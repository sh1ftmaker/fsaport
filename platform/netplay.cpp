// Netplay (deluxe; docs/NETPLAY.md): every browser runs the whole game and only
// inputs travel. This file is the C side: the two hooks in the pad read (where
// the timeline's inputs replace the four PADStatus, and where a rollback
// restores an earlier frame), and the snapshot ring.
//
// The index is the harness's: read n is the n-th JUTGamePad::read, one per
// main-loop frame (docs/HARNESS.md). Lockstep is on (virtual time, retraces
// only while every thread is blocked), so a frame is a pure function of the
// state at its read and the four pads given at it.
//
// A snapshot is taken at the top of read n, before anything of frame n has
// happened. It holds:
//   - the static memory of the simulation: the game's and JSystem's .data and
//     .bss, and the platform statics that are game-visible state (the region
//     table tools/net_regions.py derives from the linker map, given by the page);
//   - MEM1 (the game's JKR heaps; Aurora's own allocations stay in malloc,
//     JKRHeap.cpp's operator new / HostAllocScope);
//   - ARAM, copy-on-write: AR.cpp calls aurora_aram_write_hook before a write
//     and the 64 KiB blocks a live snapshot still needs are saved then;
//   - every coroutine's live shadow stack and the thread layer's scheduling
//     state (platform_threads_save, os_thread.cpp).
// What it cannot hold is the JSPI continuations (the wasm stacks of suspended
// threads are not memory). A restore is therefore only valid when every
// coroutine is suspended where it was at the snapshot; platform_threads_restore
// checks that (same coroutines, same switch-out stack pointers) and refuses
// otherwise. At a pad read the game thread is in JUTGamePad::read and the
// others are blocked at their usual waits, so in practice it holds.
//
// Everything the page decides (timeline, prediction, when to roll back) is in
// platform/web/fsa_net.js; it calls fsa_net_snapshot / fsa_net_restore from
// inside fsa_js_net_begin.
#include "platform.h"

#include <dolphin/pad.h>
#include <emscripten.h>
#include <emscripten/stack.h>

#include <cstdlib>
#include <cstring>

extern void* MEM1Start;
extern void* MEM1End;
extern "C" void* ARGetStorageAddress();
extern "C" u32 ARGetSize(void);
extern "C" void (*aurora_aram_write_hook)(u32 offset, u32 length);
extern "C" int aurora_gx_reusable_ids;  // GXTexture.cpp

int platform_threads_save(u8* buf, int cap, uintptr_t cur_sp);
int platform_threads_restore(const u8* buf, int len, uintptr_t cur_sp);
int platform_thread_stacks(uintptr_t* out, int max, uintptr_t cur_sp);
int platform_thread_layout(u32* out, int max, uintptr_t cur_sp);

namespace {
constexpr int kMaxSlots = 64;
constexpr int kMaxRegions = 256;
constexpr u32 kAramBlock = 64 * 1024;
constexpr int kMaxStacks = 64;
constexpr int kThreadsCap = 64 * 1024;

struct Region {
  uintptr_t lo, hi;
};

struct Slot {
  s32 frame = -1;
  u8* buf = nullptr;     // statics, MEM1
  size_t cap = 0;
  u8* stacks = nullptr;  // stack bytes, then the (lo, hi) table
  size_t stacksCap = 0;
  size_t stacksLen = 0;
  int stackCount = 0;
  uintptr_t stackRanges[2 * kMaxStacks];
  u8* threads = nullptr;
  int threadsLen = 0;
  u8* aram[256] = {};    // saved ARAM blocks (before a later write), null = unchanged
  double hash = 0;       // rbtest: of the statics + MEM1 + stacks
  uintptr_t mem[2 * 40] = {}; // the MEM1 parts saved: [lo, hi) ranges
  int memCount = 0;
  size_t memBytes() const {
    size_t n = 0;
    for (int i = 0; i < memCount; i++) n += mem[2 * i + 1] - mem[2 * i];
    return n;
  }
};

bool g_on = false;
bool g_resim = false;
Region g_regions[kMaxRegions];
int g_regionCount = 0;
uintptr_t g_mem1Lo = 0, g_mem1Hi = 0;
Slot g_slots[kMaxSlots];
int g_slotCount = 0;
uintptr_t g_hookSp = 0;
u32 g_aramBlocks = 0;
// Timings for the page (ms): the latest snapshot / restore.
double g_snapMs = 0, g_restoreMs = 0;

size_t static_bytes() {
  size_t n = 0;
  for (int i = 0; i < g_regionCount; i++)
    n += g_regions[i].hi - g_regions[i].lo;
  return n;
}

void aram_write(u32 offset, u32 length) {
  if (!length)
    return;
  u8* aram = (u8*)ARGetStorageAddress();
  if (!aram)
    return;
  u32 first = offset / kAramBlock, last = (offset + length - 1) / kAramBlock;
  for (int s = 0; s < g_slotCount; s++) {
    Slot& slot = g_slots[s];
    if (slot.frame < 0)
      continue;
    for (u32 b = first; b <= last && b < g_aramBlocks; b++) {
      if (slot.aram[b])
        continue;
      HostAllocScope host;
      slot.aram[b] = (u8*)malloc(kAramBlock);
      memcpy(slot.aram[b], aram + (size_t)b * kAramBlock, kAramBlock);
    }
  }
}

extern "C" int fsa_net_heap_ranges(u32* out, int max, u32* rootStart, u32* rootEnd);  // game/src/port/harness_state.cpp
bool g_mem1Trim = true;
constexpr int kMaxParts = 40;

// The parts of MEM1 a snapshot saves now: what lies outside the root heap, and
// inside it the heap tree's used parts (fsa_net_heap_ranges), page-aligned;
// all of MEM1 when the heap cannot be read (or ?mem1= / ?mem1trim=0).
int mem1_parts(uintptr_t* r) {
  uintptr_t lo = g_mem1Lo ? g_mem1Lo : (uintptr_t)MEM1Start, hi = g_mem1Hi ? g_mem1Hi : (uintptr_t)MEM1End;
  u32 parts[2 * (kMaxParts - 2)], rs = 0, re = 0;
  int n = (g_mem1Trim && !g_mem1Lo) ? fsa_net_heap_ranges(parts, kMaxParts - 2, &rs, &re) : 0;
  if (n <= 0 || rs < lo || re > hi) {
    r[0] = lo;
    r[1] = hi;
    return 1;
  }
  int k = 0;
  auto add = [&](uintptr_t a, uintptr_t b) {
    a &= ~(uintptr_t)4095;
    b = (b + 4095) & ~(uintptr_t)4095;
    if (b > hi) b = hi;
    if (a < lo) a = lo;
    if (b <= a) return;
    if (k && a <= r[2 * k - 1]) {
      if (b > r[2 * k - 1]) r[2 * k - 1] = b;
      return;
    }
    r[2 * k] = a;
    r[2 * k + 1] = b;
    k++;
  };
  add(lo, rs);
  for (int i = 0; i < n; i++) add(parts[2 * i], parts[2 * i + 1]);
  add(re, hi);
  return k;
}

void slot_clear_aram(Slot& slot) {
  for (u32 b = 0; b < 256; b++) {
    free(slot.aram[b]);
    slot.aram[b] = nullptr;
  }
}

// A 64-bit multiply-xor hash, returned as a double of its top 52 bits (for JS).
u64 hash_bytes(u64 h, const u8* p, size_t n) {
  const u64 k = 0x9E3779B97F4A7C15ull;
  size_t i = 0;
  for (; i + 8 <= n; i += 8) {
    u64 v;
    memcpy(&v, p + i, 8);
    h = (h ^ v) * k;
    h ^= h >> 29;
  }
  for (; i < n; i++)
    h = (h ^ p[i]) * k;
  return h;
}
}  // namespace

bool platform_net_on() { return g_on; }

// ?netprobe=1: every allocation the game's operator new sends to a JKR heap
// while a host (Aurora, libc++) function is on the call stack is logged once
// per distinct stack. Such memory is in MEM1, so a restore would roll it back
// under the host's feet.
extern "C" void (*fsa_jkr_new_probe)(size_t);
void (*fsa_jkr_new_probe)(size_t) = nullptr;
namespace {
char g_probeStack[8192];
u32 g_probeSeen[1024];
int g_probeCount = 0;
void jkr_probe(size_t n) {
  int len = emscripten_get_callstack(EM_LOG_C_STACK, g_probeStack, sizeof(g_probeStack));
  if (len <= 0)
    return;
  // Host frames: Aurora's namespaces and GX/GD entry points, the STL.
  if (!strstr(g_probeStack, "aurora") && !strstr(g_probeStack, "std::"))
    return;
  u32 h = 2166136261u;
  for (int i = 0; i < len && g_probeStack[i]; i++)
    h = (h ^ (u8)g_probeStack[i]) * 16777619u;
  for (int i = 0; i < g_probeCount; i++)
    if (g_probeSeen[i] == h)
      return;
  if (g_probeCount < 1024)
    g_probeSeen[g_probeCount++] = h;
  HostAllocScope host;
  platform_log("[netprobe] host allocation of %u bytes in a JKR heap:\n%s\n", (unsigned)n, g_probeStack);
}
}  // namespace
extern "C" EMSCRIPTEN_KEEPALIVE void fsa_net_probe(int on) { fsa_jkr_new_probe = on ? jkr_probe : nullptr; }
bool platform_net_resim() { return g_resim; }

// JS: fsa_net.js decides at the top of each pad read (rollback / snapshot).
EM_JS(void, fsa_js_net_begin, (u32 n), { Module.fsaNet.begin(n); });
// JS: the four pads of frame n (written into status); may wait for the network.
EM_ASYNC_JS(void, fsa_js_net_inputs, (PADStatus* status, u32 n), { await Module.fsaNet.inputs(n); });

// platform/harness.cpp, first thing in every pad read (before g_reads moves).
void platform_net_read_begin(u32 n) {
  if (!g_on)
    return;
  g_hookSp = emscripten_stack_get_current();
  fsa_js_net_begin(n);
}

// The page's view of the four pads: per port (a, b, err) with
//   a = button | (u8)stickX << 16 | (u8)stickY << 24
//   b = (u8)substickX | (u8)substickY << 8 | triggerL << 16 | triggerR << 24
// (analogA/B are not carried, as in the harness's input files).
u32 g_io[PAD_CHANMAX * 3];

// platform/harness.cpp, after the script (whose output is the local input) and
// before the inputs are recorded.
void platform_net_inputs(PADStatus* status, u32 n) {
  if (!g_on)
    return;
  for (int i = 0; i < PAD_CHANMAX; i++) {
    const PADStatus& p = status[i];
    g_io[3 * i] = (u32)p.button | (u32)(u8)p.stickX << 16 | (u32)(u8)p.stickY << 24;
    g_io[3 * i + 1] = (u32)(u8)p.substickX | (u32)(u8)p.substickY << 8 | (u32)p.triggerLeft << 16 | (u32)p.triggerRight << 24;
    g_io[3 * i + 2] = (u32)(s32)p.err;
  }
  fsa_js_net_inputs(status, n);
  for (int i = 0; i < PAD_CHANMAX; i++) {
    PADStatus& p = status[i];
    u32 a = g_io[3 * i], b = g_io[3 * i + 1];
    memset(&p, 0, sizeof(p));
    p.err = (s8)(s32)g_io[3 * i + 2];
    if (p.err != PAD_ERR_NONE)
      continue;
    p.button = (u16)a;
    p.stickX = (s8)(a >> 16);
    p.stickY = (s8)(a >> 24);
    p.substickX = (s8)b;
    p.substickY = (s8)(b >> 8);
    p.triggerLeft = (u8)(b >> 16);
    p.triggerRight = (u8)(b >> 24);
  }
}

extern "C" {

// The page, before main(): the static regions (pairs of addresses), the ring size.
EMSCRIPTEN_KEEPALIVE int fsa_net_config(const u32* pairs, int count, int slots) {
  if (count > kMaxRegions || slots > kMaxSlots || slots < 1)
    return 0;
  g_regionCount = count;
  for (int i = 0; i < count; i++)
    g_regions[i] = {pairs[2 * i], pairs[2 * i + 1]};
  g_slotCount = slots;
  aurora_aram_write_hook = aram_write;
  aurora_gx_reusable_ids = 1;
  g_on = true;
  return 1;
}

EMSCRIPTEN_KEEPALIVE void fsa_net_set_resim(int on) { g_resim = on != 0; }
EMSCRIPTEN_KEEPALIVE u32* fsa_net_io() { return g_io; }

// MEM1's part of a snapshot: [lo, hi) (0, 0 = all of MEM1).
EMSCRIPTEN_KEEPALIVE void fsa_net_set_mem1(u32 lo, u32 hi) {
  g_mem1Lo = lo;
  g_mem1Hi = hi;
}

// For the page: MEM1 and the parts a snapshot would save now (fsaNetMemStats):
// out[0] = count, then (lo, hi) pairs.
EMSCRIPTEN_KEEPALIVE void fsa_net_heap_info(u32* out) {
  uintptr_t parts[2 * kMaxParts];
  int n = mem1_parts(parts);
  out[0] = (u32)n;
  for (int i = 0; i < 2 * n; i++) out[1 + i] = (u32)parts[i];
}
extern "C" void fsa_net_heap_dump();
EMSCRIPTEN_KEEPALIVE void fsa_net_dump_heaps() { fsa_net_heap_dump(); }
EMSCRIPTEN_KEEPALIVE void fsa_net_set_mem1_trim(int on) { g_mem1Trim = on != 0; }
EMSCRIPTEN_KEEPALIVE u32 fsa_net_mem1_start() { return (u32)(uintptr_t)MEM1Start; }
EMSCRIPTEN_KEEPALIVE u32 fsa_net_mem1_end() { return (u32)(uintptr_t)MEM1End; }
EMSCRIPTEN_KEEPALIVE double fsa_net_snap_ms() { return g_snapMs; }
EMSCRIPTEN_KEEPALIVE double fsa_net_restore_ms() { return g_restoreMs; }

// Bytes of one snapshot (statics + MEM1 part + the latest stacks).
EMSCRIPTEN_KEEPALIVE double fsa_net_snapshot_bytes(int s) {
  if (s < 0 || s >= g_slotCount)
    return 0;
  const Slot& slot = g_slots[s];
  double aram = 0;
  for (u32 b = 0; b < 256; b++)
    aram += slot.aram[b] ? kAramBlock : 0;
  return (double)(static_bytes() + slot.memBytes() + slot.stacksLen + slot.threadsLen) + aram;
}

// Take a snapshot of the state at the top of read `frame` into slot s. Only
// from inside fsa_js_net_begin. Returns 1, or 0 when the thread layer did not fit.
EMSCRIPTEN_KEEPALIVE int fsa_net_snapshot(int s, int frame) {
  if (s < 0 || s >= g_slotCount)
    return 0;
  double t0 = emscripten_get_now();
  HostAllocScope host;
  Slot& slot = g_slots[s];
  uintptr_t parts[2 * kMaxParts];
  int partCount = mem1_parts(parts);
  size_t memNeed = 0;
  for (int i = 0; i < partCount; i++) memNeed += parts[2 * i + 1] - parts[2 * i];
  size_t need = static_bytes() + memNeed;
  if (slot.cap < need) {
    free(slot.buf);
    slot.buf = (u8*)malloc(need);
    slot.cap = need;
  }
  u8* p = slot.buf;
  for (int i = 0; i < g_regionCount; i++) {
    size_t n = g_regions[i].hi - g_regions[i].lo;
    memcpy(p, (const void*)g_regions[i].lo, n);
    p += n;
  }
  slot.memCount = partCount;
  for (int i = 0; i < partCount; i++) {
    slot.mem[2 * i] = parts[2 * i];
    slot.mem[2 * i + 1] = parts[2 * i + 1];
    memcpy(p, (const void*)parts[2 * i], parts[2 * i + 1] - parts[2 * i]);
    p += parts[2 * i + 1] - parts[2 * i];
  }
  slot.stackCount = platform_thread_stacks(slot.stackRanges, kMaxStacks, g_hookSp);
  size_t stacks = 0;
  for (int k = 0; k < slot.stackCount; k++)
    stacks += slot.stackRanges[2 * k + 1] - slot.stackRanges[2 * k];
  if (slot.stacksCap < stacks) {
    free(slot.stacks);
    slot.stacks = (u8*)malloc(stacks);
    slot.stacksCap = stacks;
  }
  u8* q = slot.stacks;
  for (int k = 0; k < slot.stackCount; k++) {
    size_t n = slot.stackRanges[2 * k + 1] - slot.stackRanges[2 * k];
    memcpy(q, (const void*)slot.stackRanges[2 * k], n);
    q += n;
  }
  slot.stacksLen = stacks;
  if (!slot.threads)
    slot.threads = (u8*)malloc(kThreadsCap);
  slot.threadsLen = platform_threads_save(slot.threads, kThreadsCap, g_hookSp);
  slot_clear_aram(slot);
  g_aramBlocks = (ARGetSize() + kAramBlock - 1) / kAramBlock;
  slot.frame = slot.threadsLen > 0 ? frame : -1;
  g_snapMs = emscripten_get_now() - t0;
  return slot.frame >= 0;
}

// Restore slot s (only from inside fsa_js_net_begin). 1 = restored; <= 0 =
// refused, nothing changed: 0 no such snapshot, -1..-5 platform_threads_restore's
// reasons. The saved stacks belong to coroutines that were running at the
// snapshot, which the thread layer has checked are switched out at the same
// place now, so they go back to the same addresses.
EMSCRIPTEN_KEEPALIVE int fsa_net_restore(int s, int keepNewer) {
  if (s < 0 || s >= g_slotCount || g_slots[s].frame < 0)
    return 0;
  double t0 = emscripten_get_now();
  Slot& slot = g_slots[s];
  int r = platform_threads_restore(slot.threads, slot.threadsLen, g_hookSp);
  if (r != 0)
    return r;
  const u8* p = slot.buf;
  for (int i = 0; i < g_regionCount; i++) {
    size_t n = g_regions[i].hi - g_regions[i].lo;
    memcpy((void*)g_regions[i].lo, p, n);
    p += n;
  }
  for (int i = 0; i < slot.memCount; i++) {
    size_t n = slot.mem[2 * i + 1] - slot.mem[2 * i];
    memcpy((void*)slot.mem[2 * i], p, n);
    p += n;
  }
  const u8* q = slot.stacks;
  for (int k = 0; k < slot.stackCount; k++) {
    size_t n = slot.stackRanges[2 * k + 1] - slot.stackRanges[2 * k];
    memcpy((void*)slot.stackRanges[2 * k], q, n);
    q += n;
  }
  u8* aram = (u8*)ARGetStorageAddress();
  for (u32 b = 0; b < 256; b++)
    if (slot.aram[b] && aram)
      memcpy(aram + (size_t)b * kAramBlock, slot.aram[b], kAramBlock);
  // Snapshots newer than this one are about the abandoned future; the ARAM
  // blocks this one saved are back in place and it stays valid as it is.
  for (int t = 0; t < g_slotCount && !keepNewer; t++)
    if (t != s && g_slots[t].frame > slot.frame) {
      g_slots[t].frame = -1;
      slot_clear_aram(g_slots[t]);
    }
  g_restoreMs = emscripten_get_now() - t0;
  return 1;
}

// Coroutine stack k of slot s: its (low, high) range.
EMSCRIPTEN_KEEPALIVE u32 fsa_net_slot_stack(int s, int k, int hi) {
  if (s < 0 || s >= g_slotCount || k < 0 || k >= g_slots[s].stackCount)
    return 0;
  return (u32)g_slots[s].stackRanges[2 * k + (hi ? 1 : 0)];
}

// The coroutine layout now (only from inside fsa_js_net_begin): (id, sp, top)
// triples into out; returns the count.
EMSCRIPTEN_KEEPALIVE int fsa_net_layout(u32* out, int max) { return platform_thread_layout(out, max, g_hookSp); }

EMSCRIPTEN_KEEPALIVE int fsa_net_slot_frame(int s) { return s >= 0 && s < g_slotCount ? g_slots[s].frame : -1; }

// A hash of slot s's statics + MEM1 + stacks (not ARAM), for checks.
EMSCRIPTEN_KEEPALIVE double fsa_net_slot_hash(int s) {
  if (s < 0 || s >= g_slotCount || g_slots[s].frame < 0)
    return -1;
  const Slot& slot = g_slots[s];
  u64 h = hash_bytes(1469598103934665603ull, slot.buf, static_bytes() + slot.memBytes());
  h = hash_bytes(h, slot.stacks, slot.stacksLen);
  return (double)(h >> 12);
}

// Where two slots differ first, by part: writes up to `max` (address, part,
// word in a, word in b) for differing 4 KiB pages into out (part: region index,
// -1 MEM1, -2 - k coroutine stack k); returns how many pages differ in all.
EMSCRIPTEN_KEEPALIVE int fsa_net_slot_diff(int a, int b, u32* out, int max) {
  if (a < 0 || b < 0 || a >= g_slotCount || b >= g_slotCount)
    return -1;
  const Slot& A = g_slots[a];
  const Slot& B = g_slots[b];
  int k = 0, total = 0;
  auto cmp = [&](const u8* x, const u8* y, size_t n, uintptr_t base, int part) {
    for (size_t off = 0; off < n; off += 4096) {
      size_t len = n - off < 4096 ? n - off : 4096;
      if (memcmp(x + off, y + off, len) != 0) {
        // The first differing byte's address.
        size_t i = 0;
        while (i < len && x[off + i] == y[off + i])
          i++;
        if (k < max) {
          size_t w = (off + i) & ~(size_t)3;
          u32 va = 0, vb = 0;
          memcpy(&va, x + w, w + 4 <= n ? 4 : 1);
          memcpy(&vb, y + w, w + 4 <= n ? 4 : 1);
          out[4 * k] = (u32)(base + off + i);
          out[4 * k + 1] = (u32)part;
          out[4 * k + 2] = va;
          out[4 * k + 3] = vb;
          k++;
        }
        total++;
      }
    }
  };
  size_t at = 0;
  for (int i = 0; i < g_regionCount; i++) {
    size_t n = g_regions[i].hi - g_regions[i].lo;
    cmp(A.buf + at, B.buf + at, n, g_regions[i].lo, i);
    at += n;
  }
  if (A.memCount == B.memCount && memcmp(A.mem, B.mem, sizeof(A.mem)) == 0) {
    for (int i = 0; i < A.memCount; i++) {
      size_t n = A.mem[2 * i + 1] - A.mem[2 * i];
      cmp(A.buf + at, B.buf + at, n, A.mem[2 * i], -1);
      at += n;
    }
  } else {
    total = -1000;  // different MEM1 extents: not compared
  }
  if (A.stacksLen == B.stacksLen && A.stackCount == B.stackCount) {
    // By address (part -2 - k for coroutine stack k), so a frame can be found.
    size_t off = 0;
    for (int k = 0; k < A.stackCount; k++) {
      size_t n = A.stackRanges[2 * k + 1] - A.stackRanges[2 * k];
      cmp(A.stacks + off, B.stacks + off, n, A.stackRanges[2 * k], -2 - k);
      off += n;
    }
  }
  return total;
}

}  // extern "C"
