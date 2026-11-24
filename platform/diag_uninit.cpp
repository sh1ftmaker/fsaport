// Uninitialised-read diagnostics (cmake -DFSA_DIAG_UNINIT=ON; see
// docs/DIAGNOSTICS.md). Not compiled into normal builds.
//
// - JKRHeap::alloc fills every fresh block with 0xA5 bytes and reports it here
//   (fsa_uninit_alloc), so blocks can be looked up by address later.
// - tools/uninit_instrument.py rewrites the game's functions in fsa.wasm:
//   loads whose value is poison call fsa_uninit_hit (read-before-write),
//   constructors are bracketed by fsa_uninit_ctor_enter/exit (after the
//   outermost constructor of a fresh block, its still-poisoned bytes are
//   never-written-after-ctor), and calls to operator new / JKRHeap::alloc store
//   the caller in g_allocSite first.
// - Findings go to the page (platform/web/fsa_uninit.js), which names the
//   functions from fsa.uninit.json and writes the "uninit" diagnostics stream.
#ifdef FSA_DIAG_UNINIT
#include <emscripten.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int fsa_diag_scene(char* buf, int n);  // game code: {"mode":..,"stage":..}

namespace {

typedef uint32_t u32;

constexpr u32 kPoison = 0xA5A5A5A5u;
constexpr int kGranuleShift = 5;      // address -> block map, 32-byte granules
constexpr int kPageShift = 20;        // map pages of 1 MiB
constexpr u32 kRecs = 1u << 21;       // live allocations (freed slots are reused)
constexpr u32 kScanMax = 0x4000;      // never-written scans look at most this far
constexpr int kMaxRuns = 64;

// One allocation. The granule map holds slot+1 of the newest block touching
// each granule; prev chains to the block that held the start granule before
// (small blocks of a solid heap share granules).
struct Rec {
  u32 ptr, size, site, ctor, seq;
  u32 scanned, prev, live;
};

Rec* g_recs;
u32 g_seq, g_cursor;
u32* g_freeSlots;
u32 g_nfree;
u32* g_pages[1u << (32 - kPageShift)];
volatile u32 g_allocSite;  // written by the instrumented call sites
bool g_inHit;

u32* granule_slot(u32 addr, bool create) {
  u32 pg = addr >> kPageShift;
  if (!g_pages[pg]) {
    if (!create)
      return nullptr;
    g_pages[pg] = (u32*)calloc(1u << (kPageShift - kGranuleShift), 4);
    if (!g_pages[pg])
      return nullptr;
  }
  return &g_pages[pg][(addr & ((1u << kPageShift) - 1)) >> kGranuleShift];
}

Rec* rec_at(u32 slot1) { return slot1 && g_recs ? &g_recs[slot1 - 1] : nullptr; }

bool contains(const Rec* r, u32 addr) {
  return r && r->live && addr >= r->ptr && addr - r->ptr < (r->size ? r->size : 1);
}

// The live block containing addr (or starting exactly there), or null.
Rec* lookup(u32 addr, bool exact = false) {
  u32* s = granule_slot(addr, false);
  if (!s)
    return nullptr;
  u32 slot = *s;
  for (int k = 0; k < 16 && slot; k++) {
    Rec* r = rec_at(slot);
    if (contains(r, addr) && (!exact || r->ptr == addr))
      return r;
    slot = r->prev;
  }
  return nullptr;
}

// ---- findings, deduplicated ------------------------------------------------
enum Kind : u32 { kRead = 1, kNever = 2, kOutside = 3 };

// One finding per (kind, reader, class, offset, size). A reader that hits
// more than kFoldAfter offsets of one class (a loop over an array) gets one
// folded finding for the rest, with the offset range.
struct Finding {
  u32 kind, where, cls, keySite, off, size, load;
  u32 count, emitted, allocSite, allocSize, emittedObjects;
  u32 minOff, maxOff;  // folded findings
  bool used, hidden;
};
constexpr u32 kFindings = 1u << 16;
constexpr u32 kGroup = 0xFFFFFFFEu, kFold = 0xFFFFFFFFu;
constexpr u32 kFoldAfter = 8;
Finding* g_find;
u32 g_findCount;
struct CtorStat { u32 ctor, objects; };
constexpr u32 kCtorStats = 1u << 14;
CtorStat* g_ctorStats;

u32 hash(u32 a, u32 b, u32 c, u32 d, u32 e) {
  const u32 v[5] = {a, b, c, d, e};
  u32 h = 2166136261u;
  for (u32 x : v) h = (h ^ x) * 16777619u;
  return h;
}

u32* ctor_objects(u32 ctor) {
  if (!g_ctorStats)
    g_ctorStats = (CtorStat*)calloc(kCtorStats, sizeof(CtorStat));
  for (u32 i = hash(ctor, 0, 0, 0, 0) & (kCtorStats - 1), n = 0; n < kCtorStats; i = (i + 1) & (kCtorStats - 1), n++) {
    if (g_ctorStats[i].ctor == ctor)
      return &g_ctorStats[i].objects;
    if (!g_ctorStats[i].ctor) {
      g_ctorStats[i].ctor = ctor;
      return &g_ctorStats[i].objects;
    }
  }
  static u32 overflow;
  return &overflow;
}

EM_JS(void, js_emit, (u32 kind, u32 where, u32 cls, u32 allocSite, u32 off, u32 size, u32 load, u32 count, u32 objects,
                      u32 allocSize, const char* scene, u32 minOff, u32 maxOff), {
  if (Module.fsaUninitEmit)
    Module.fsaUninitEmit(kind, where, cls, allocSite, off, size, load, count, objects, allocSize,
                         scene ? UTF8ToString(scene) : null, minOff, maxOff);
});

void emit(Finding& f, bool withScene) {
  char scene[160];
  scene[0] = 0;
  if (withScene) {
    g_inHit = true;  // the scene is read by (instrumented) game code
    fsa_diag_scene(scene, sizeof scene);
    g_inHit = false;
  }
  u32 objects = f.kind == kNever ? *ctor_objects(f.cls) : 0;
  js_emit(f.kind, f.where, f.cls, f.allocSite, f.off, f.size, f.load, f.count, objects, f.allocSize,
          scene[0] ? scene : nullptr, f.minOff, f.maxOff);
  f.emitted = f.count;
  f.emittedObjects = objects;
}

// The entry for a key, or null; with create, a new entry if there is room
// (*created tells).
Finding* find(u32 kind, u32 where, u32 cls, u32 keySite, u32 off, u32 size, u32 load, bool create, bool* created) {
  *created = false;
  if (!g_find)
    g_find = (Finding*)calloc(kFindings, sizeof(Finding));
  if (!g_find)
    return nullptr;
  for (u32 i = hash(kind, where, cls ? cls : keySite, off, size * 16 + load) & (kFindings - 1), n = 0; n < kFindings;
       i = (i + 1) & (kFindings - 1), n++) {
    Finding& f = g_find[i];
    if (f.used && f.kind == kind && f.where == where && f.cls == cls && f.keySite == keySite && f.off == off &&
        f.size == size && f.load == load)
      return &f;
    if (!f.used) {
      if (!create || g_findCount > kFindings * 3 / 4)
        return nullptr;  // table full: keep counting the known ones only
      g_findCount++;
      f = Finding{kind, where, cls, keySite, off, size, load, 0, 0, 0, 0, 0, off, off, true, false};
      *created = true;
      return &f;
    }
  }
  return nullptr;
}

void report(u32 kind, u32 where, u32 cls, u32 allocSite, u32 off, u32 size, u32 load, u32 allocSize) {
  // Findings in a class are one key whoever allocated it; without a class the
  // allocating function tells them apart.
  u32 keySite = cls ? 0 : allocSite;
  bool created;
  Finding* f = find(kind, where, cls, keySite, off, size, load, false, &created);
  if (f) {
    f->count++;
    return;
  }
  Finding* g = find(kind, where, cls, keySite, kGroup, size, load, true, &created);
  if (!g)
    return;
  g->hidden = true;
  if (++g->count > kFoldAfter) {
    Finding* fold = find(kind, where, cls, keySite, kFold, size, load, true, &created);
    if (!fold)
      return;
    fold->count++;
    if (created) {
      fold->minOff = fold->maxOff = off;
      fold->allocSite = allocSite;
      fold->allocSize = allocSize;
      emit(*fold, true);
    } else {
      if (off < fold->minOff) fold->minOff = off;
      if (off > fold->maxOff) fold->maxOff = off;
    }
    return;
  }
  f = find(kind, where, cls, keySite, off, size, load, true, &created);
  if (!f)
    return;
  f->count = 1;
  f->allocSite = allocSite;
  f->allocSize = allocSize;
  emit(*f, true);
}

// ---- constructors --------------------------------------------------------
constexpr int kStack = 512;
u32 g_stack[kStack], g_stackCtor[kStack];
int g_depth;

// The constructor running on a block still under construction (the first,
// i.e. most derived, entry for its address), or 0.
u32 active_ctor(u32 self) {
  int top = g_depth < kStack ? g_depth : kStack;
  for (int i = 0; i < top; i++)
    if (g_stack[i] == self)
      return g_stackCtor[i];
  return 0;
}

// The last outermost-constructed block: its never-written bytes are reported
// when the next block is constructed or allocated, after a later constructor
// running inside it (an array element, or a placement new) has cut the range
// down to the first object.
struct Pending {
  Rec* rec;
  u32 seq, ctor, limit, nruns;
  u32 runOff[kMaxRuns], runLen[kMaxRuns];
} g_pending;

void flush_pending() {
  Pending& p = g_pending;
  if (!p.rec)
    return;
  Rec* r = p.rec;
  p.rec = nullptr;
  if (!r->live || r->seq != p.seq)
    return;
  for (u32 i = 0; i < p.nruns; i++) {
    if (p.runOff[i] >= p.limit)
      break;
    u32 len = p.runLen[i];
    if (p.runOff[i] + len > p.limit)
      len = p.limit - p.runOff[i];
    report(kNever, p.ctor, p.ctor, r->site, p.runOff[i], len, 0, r->size);
  }
}

void cut_pending(u32 addr) {
  Pending& p = g_pending;
  if (p.rec && addr > p.rec->ptr && addr - p.rec->ptr < p.limit)
    p.limit = addr - p.rec->ptr;
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE u32 fsa_uninit_site_ptr() { return (u32)(uintptr_t)&g_allocSite; }

// JKRHeap::alloc, after do_alloc: poison the block and remember it.
void fsa_uninit_alloc(void* p, u32 size) {
  if (!p)
    return;
  memset(p, 0xA5, size);
  if (!g_recs) {
    g_recs = (Rec*)calloc(kRecs, sizeof(Rec));
    g_freeSlots = (u32*)malloc(kRecs * 4);
    if (!g_recs || !g_freeSlots)
      return;
  }
  u32 a = (u32)(uintptr_t)p;
  cut_pending(a);
  flush_pending();
  // A free slot, else the next one round the ring (its block is forgotten).
  u32 slot = g_nfree ? g_freeSlots[--g_nfree] : (g_cursor++ & (kRecs - 1)) + 1;
  u32* first = granule_slot(a, true);
  if (!first)
    return;
  g_recs[slot - 1] = Rec{a, size, g_allocSite, 0, ++g_seq, 0, *first == slot ? 0 : *first, 1};
  g_allocSite = 0;
  u32 last = size ? a + size - 1 : a;
  for (u32 g = a >> kGranuleShift; g <= last >> kGranuleShift; g++) {
    u32* s = granule_slot(g << kGranuleShift, true);
    if (s)
      *s = slot;
  }
}

// JKRHeap::free: the block's slot can be reused.
void fsa_uninit_free(void* p) {
  Rec* r = lookup((u32)(uintptr_t)p, true);
  if (!r)
    return;
  r->live = 0;
  if (g_nfree < kRecs)
    g_freeSlots[g_nfree++] = (u32)(r - g_recs) + 1;
}

EMSCRIPTEN_KEEPALIVE void fsa_uninit_hit(u32 addr, u32 off, u32 site, u32 size) {
  if (g_inHit)
    return;
  g_inHit = true;
  u32 ea = addr + off;
  Rec* r = lookup(ea);
  u32 load = size;  // 2, 4 or 8 bytes
  if (r) {
    u32 cls = r->ctor ? r->ctor : active_ctor(r->ptr);
    report(kRead, site, cls, cls ? 0 : r->site, ea - r->ptr, size, load, r->size);
  }
  else
    report(kOutside, site, 0, 0, ea, size, load, 0);
  g_inHit = false;
}

EMSCRIPTEN_KEEPALIVE void fsa_uninit_ctor_enter(u32 self, u32 ctor) {
  cut_pending(self);
  if (g_depth < kStack) {
    g_stack[g_depth] = self;
    g_stackCtor[g_depth] = ctor;
  }
  g_depth++;
}

EMSCRIPTEN_KEEPALIVE void fsa_uninit_ctor_exit(u32 self, u32 ctor) {
  // Threads are cooperative and a constructor can block, so entries are not
  // strictly nested: drop the newest entry for this object.
  int top = g_depth < kStack ? g_depth : kStack;
  int found = -1;
  for (int i = top - 1; i >= 0; i--)
    if (g_stack[i] == self) {
      found = i;
      break;
    }
  if (found < 0) {
    if (g_depth > 0)
      g_depth--;
    return;
  }
  for (int i = found; i < top - 1; i++) {
    g_stack[i] = g_stack[i + 1];
    g_stackCtor[i] = g_stackCtor[i + 1];
  }
  g_depth--;
  top--;
  for (int i = 0; i < top; i++)
    if (g_stack[i] == self)
      return;  // a base or member constructor: the object is not finished
  Rec* r = lookup(self, true);
  if (!r || r->scanned)
    return;
  r->scanned = 1;
  r->ctor = ctor;
  flush_pending();
  (*ctor_objects(ctor))++;
  Pending& p = g_pending;
  p.rec = r;
  p.seq = r->seq;
  p.ctor = ctor;
  p.limit = r->size < kScanMax ? r->size : kScanMax;
  p.nruns = 0;
  const uint8_t* b = (const uint8_t*)(uintptr_t)self;
  for (u32 i = 0; i < p.limit && p.nruns < kMaxRuns;) {
    if (b[i] != 0xA5) {
      i++;
      continue;
    }
    u32 j = i;
    while (j < p.limit && b[j] == 0xA5) j++;
    p.runOff[p.nruns] = i;
    p.runLen[p.nruns] = j - i;
    p.nruns++;
    i = j;
  }
}

// The page calls this every few seconds: report the pending object and send
// the findings whose counts changed (consumers keep the last line per key).
EMSCRIPTEN_KEEPALIVE void fsa_uninit_tick() {
  if (g_inHit)
    return;
  flush_pending();
  if (!g_find)
    return;
  for (u32 i = 0; i < kFindings; i++)
    if (g_find[i].used && !g_find[i].hidden && (g_find[i].count != g_find[i].emitted ||
                           (g_find[i].kind == kNever && *ctor_objects(g_find[i].cls) != g_find[i].emittedObjects)))
      emit(g_find[i], false);
}

}  // extern "C"
#endif
