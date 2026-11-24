// Diagnostics build only (cmake -DFSA_DIAG=ON; docs/DIAGNOSTICS.md, "Virtual
// dispatch audit"): which functions the indirect calls reach, and through
// which object's vtable.
//
// tools/vdispatch_instrument.py rewrites fsa.wasm after the link. In every
// instrumented function (one that calls __cyg_profile_func_enter with its own
// table index):
//
//   - each `call_indirect` first passes its table index through
//     fsa_vd_mark(idx) (which returns it unchanged) and is followed by a store
//     of 0 to *fsa_vd_pending_ptr();
//   - when the function's first parameter is an i32, its prologue stores that
//     parameter (`this` for a member function) to *fsa_vd_this_ptr() just
//     before its own __cyg_profile_func_enter call.
//
// So when the enter hook below runs and a call is pending from the function
// on top of the shadow stack, this entry is the target of that indirect call,
// and g_this is the callee's first argument (0 when it has none or the
// callee is not rewritten). The word at `this` is the vtable pointer when the
// callee is a member function: the vtable name table fsa.diag.vt.tsv (written
// by the instrumenter from fsa.map) names the class, and the target's
// position in that vtable is the slot. Edges (caller, target, entered
// function, vtable pointer) are counted and written to the "vdispatch" stream
// at each flush, cumulative like "coverage".
//
// Nothing here may be instrumented (NOINST, no library code that is).
#ifdef FSA_DIAG
#include "platform.h"
#include "../game/port/fsa_diag.h"

#include <emscripten.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define NOINST __attribute__((no_instrument_function))

namespace {

// --- What the rewritten code writes -------------------------------------------------
unsigned g_this;         // the entered function's first i32 parameter (prologue store)
unsigned g_pending;      // table index of the indirect call about to happen, 0 = none
unsigned g_pendCaller;   // innermost instrumented function at the call
unsigned g_pendCtx;      // its shadow-stack context

uint64_t g_marks, g_matched, g_otherEntry;

// --- Vtables (fsa.diag.vt.tsv: addr \t size \t class \t mwcc __vt__ \t ...) ---------------
struct Vt {
  unsigned addr, size;
  const char* cls;
  const char* mw;
};
Vt* g_vts;
unsigned g_vtCount;
unsigned g_vtMin, g_vtMax;
int g_vtTried;

EM_JS(int, vd_js_fetch, (), {
  try {
    var x = new XMLHttpRequest();
    x.open('GET', 'fsa.diag.vt.tsv', false);
    x.send();
    if (x.status != 200) return 0;
    Module.fsaDiagVt = new TextEncoder().encode(x.responseText);
    return Module.fsaDiagVt.length;
  } catch (e) {
    return 0;
  }
});
EM_JS(void, vd_js_copy, (char* dst), {
  HEAPU8.set(Module.fsaDiagVt, dst);
  Module.fsaDiagVt = null;
});

NOINST void load_vts() {
  if (g_vtTried)
    return;
  g_vtTried = 1;
  int n = vd_js_fetch();
  if (n <= 0) {
    platform_log("[vdispatch] fsa.diag.vt.tsv not found: no receiver classes\n");
    return;
  }
  HostAllocScope host;
  char* buf = (char*)malloc(n + 1);
  vd_js_copy(buf);
  buf[n] = 0;
  unsigned lines = 0;
  for (char* p = buf; *p; p++)
    lines += *p == '\n';
  g_vts = (Vt*)calloc(lines + 1, sizeof(Vt));
  for (char* p = buf; *p;) {
    char* nl = strchr(p, '\n');
    if (nl)
      *nl = 0;
    char* f[4] = {};
    int nf = 0;
    for (char* q = p; nf < 4;) {
      f[nf++] = q;
      char* t = strchr(q, '\t');
      if (!t)
        break;
      *t = 0;
      q = t + 1;
    }
    if (nf == 4 && f[0][0] >= '0' && f[0][0] <= '9')
      g_vts[g_vtCount++] = Vt{(unsigned)strtoul(f[0], nullptr, 10), (unsigned)strtoul(f[1], nullptr, 10), f[2], f[3]};
    if (!nl)
      break;
    p = nl + 1;
  }
  // The file is sorted by address.
  if (g_vtCount) {
    g_vtMin = g_vts[0].addr;
    g_vtMax = g_vts[g_vtCount - 1].addr + g_vts[g_vtCount - 1].size;
  }
  platform_log("[vdispatch] %u vtables\n", g_vtCount);
}

// The vtable group containing addr, or -1.
NOINST int vt_find(unsigned addr) {
  unsigned lo = 0, hi = g_vtCount;
  while (lo < hi) {
    unsigned mid = (lo + hi) / 2;
    if (g_vts[mid].addr <= addr)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (!lo)
    return -1;
  const Vt& v = g_vts[lo - 1];
  return addr < v.addr + v.size ? (int)(lo - 1) : -1;
}

// --- Edges -------------------------------------------------------------------------------
constexpr unsigned kEdges = 1u << 16;
struct Edge {
  unsigned target, callee, vptr, caller;  // vptr 0: no object / not a vtable
  uint64_t calls, flushed;
  uint32_t firstFrame;
  int vt;      // index into g_vts, -1
  int slot;    // byte offset of target from vptr, -1 if not in the vtable
  int slots;   // how many entries of the group hold target
  uint8_t firstScene;
  uint8_t used;
};
Edge g_edges[kEdges];
unsigned g_edgeCount;
uint64_t g_edgeOverflow;

NOINST unsigned mem_size() { return (unsigned)__builtin_wasm_memory_size(0) << 16; }

NOINST void edge_init(Edge& e) {
  e.vt = -1;
  e.slot = -1;
  e.slots = 0;
  if (!e.vptr)
    return;
  e.vt = vt_find(e.vptr);
  if (e.vt < 0)
    return;
  const Vt& v = g_vts[e.vt];
  // Entries from vptr to the end of the group (secondary vtables of the
  // group follow the primary one; their entries count too, the slot is the
  // first match after vptr).
  for (unsigned a = e.vptr; a + 4 <= v.addr + v.size; a += 4) {
    if (*(const unsigned*)(uintptr_t)a == e.target) {
      if (e.slot < 0)
        e.slot = (int)(a - e.vptr);
      e.slots++;
    }
  }
}

NOINST void count(unsigned target, unsigned callee, unsigned vptr, unsigned caller) {
  unsigned h = (target * 0x9E3779B1u) ^ (callee * 0x85EBCA77u) ^ (vptr * 0xC2B2AE3Du) ^ (caller * 0x27D4EB2Fu);
  h ^= h >> 15;
  for (unsigned k = 0; k < 64; k++) {
    Edge& e = g_edges[(h + k) & (kEdges - 1)];
    if (!e.used) {
      if (g_edgeCount >= kEdges * 3 / 4)
        break;
      e.used = 1;
      e.target = target;
      e.callee = callee;
      e.vptr = vptr;
      e.caller = caller;
      e.calls = 1;
      e.firstFrame = fsa_diag_frame();
      e.firstScene = (uint8_t)fsa_diag_scene();
      edge_init(e);
      g_edgeCount++;
      return;
    }
    if (e.target == target && e.callee == callee && e.vptr == vptr && e.caller == caller) {
      e.calls++;
      return;
    }
  }
  g_edgeOverflow++;
}

NOINST void hook(unsigned callee, unsigned caller) {
  unsigned target = g_pending;
  if (!target)
    return;
  g_pending = 0;
  if (caller != g_pendCaller || fsa_diag_context() != g_pendCtx) {
    g_otherEntry++;
    return;
  }
  g_matched++;
  unsigned self = g_this, vptr = 0;
  if (self >= 1024 && !(self & 3) && self <= mem_size() - 4) {
    vptr = *(const unsigned*)(uintptr_t)self;
    if (vptr < g_vtMin || vptr >= g_vtMax || (vptr & 3))
      vptr = 0;
  }
  count(target, callee, vptr, caller);
}

NOINST void flush() {
  static char line[8192];
  char sc[128];
  unsigned changed = 0;
  for (unsigned i = 0; i < kEdges; i++) {
    Edge& e = g_edges[i];
    if (!e.used || e.calls == e.flushed)
      continue;
    e.flushed = e.calls;
    changed++;
    fsa_diag_scene_json(e.firstScene, sc, sizeof(sc));
    const char* cname = fsa_diag_fn_name(e.caller);
    const char* tname = fsa_diag_fn_name(e.target);
    if (strlen(cname) > 1500)
      cname = "";
    if (strlen(tname) > 1500)
      tname = "";
    int n = snprintf(line, sizeof(line),
                     "{\"caller\":%u,\"caller_sym\":\"%s\",\"caller_name\":\"%s\",\"target\":%u,\"target_sym\":\"%s\","
                     "\"target_name\":\"%s\"",
                     e.caller, fsa_diag_fn_sym(e.caller), cname, e.target, fsa_diag_fn_sym(e.target), tname);
    if (e.callee != e.target && n < (int)sizeof(line)) {
      const char* ename = fsa_diag_fn_name(e.callee);
      n += snprintf(line + n, sizeof(line) - n, ",\"entered\":%u,\"entered_name\":\"%s\"", e.callee,
                    strlen(ename) > 1500 ? "" : ename);
    }
    if (n < (int)sizeof(line)) {
      if (e.vt >= 0) {
        const Vt& v = g_vts[e.vt];
        n += snprintf(line + n, sizeof(line) - n,
                      ",\"kind\":\"%s\",\"class\":\"%s\",\"vt\":\"%s\",\"vt_addr\":%u,\"vptr_off\":%u,\"slot\":%d",
                      e.slot >= 0 ? "virtual" : "not-in-vtable", v.cls, v.mw, v.addr, e.vptr - v.addr, e.slot);
        if (e.slots > 1 && n < (int)sizeof(line))
          n += snprintf(line + n, sizeof(line) - n, ",\"slots\":%d", e.slots);
      } else {
        n += snprintf(line + n, sizeof(line) - n, ",\"kind\":\"fnptr\"");
      }
    }
    if (n < (int)sizeof(line))
      snprintf(line + n, sizeof(line) - n, ",\"calls\":%llu,\"first_scene\":%s,\"first_frame\":%u,\"frame\":%u}",
               (unsigned long long)e.calls, sc, e.firstFrame, fsa_diag_frame());
    if (n >= (int)sizeof(line) - 1)
      continue;  // cannot happen with the name caps above
    fsa_diag("vdispatch", line);
  }
  snprintf(line, sizeof(line),
           "{\"frame\":%u,\"marks\":%llu,\"matched\":%llu,\"other_entry\":%llu,\"edges\":%u,\"changed\":%u,"
           "\"edge_overflow\":%llu,\"vtables\":%u}",
           fsa_diag_frame(), (unsigned long long)g_marks, (unsigned long long)g_matched,
           (unsigned long long)g_otherEntry, g_edgeCount, changed, (unsigned long long)g_edgeOverflow, g_vtCount);
  fsa_diag("vdispatch_meta", line);
}

int g_installed;

}  // namespace

// Called by the rewritten code before each call_indirect; returns idx so the
// rewrite is a single `call` in front of the call_indirect.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE unsigned fsa_vd_mark(unsigned idx) {
  g_pending = idx;
  g_this = 0;
  g_pendCaller = fsa_diag_caller(0);
  g_pendCtx = fsa_diag_context();
  g_marks++;
  return idx;
}
// Addresses the rewritten code stores to (the instrumenter reads the constant
// out of these bodies).
extern "C" NOINST EMSCRIPTEN_KEEPALIVE unsigned* fsa_vd_this_ptr(void) { return &g_this; }
extern "C" NOINST EMSCRIPTEN_KEEPALIVE unsigned* fsa_vd_pending_ptr(void) { return &g_pending; }

// diag_cov.cpp calls this at the first retrace (this unit is linked only
// because of that reference).
extern "C" NOINST void fsa_diag_vdispatch_init(void) {
  if (g_installed)
    return;
  g_installed = 1;
  load_vts();
  fsa_diag_set_enter_hook(hook);
  fsa_diag_add_flush_hook(flush);
}
#endif  // FSA_DIAG
