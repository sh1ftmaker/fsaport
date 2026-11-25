// ?allocprobe=1 (docs/DIAGNOSTICS.md, "Host allocations in the game's heaps"):
// a check that host code (Aurora, libc++) does not allocate from the game's JKR
// heaps. The game's operator new (JKRHeap.cpp) sends every allocation made while
// fsa_host_alloc_depth == 0 to the current JKR heap, so an Aurora entry point
// the game calls that is not wrapped in AURORA_HOST_ALLOC
// (extern/aurora/lib/host_alloc.hpp) puts Aurora's objects in MEM1.
//
// With the probe on, every allocation operator new sends to a JKR heap while an
// Aurora or std:: function is on the call stack is counted, and each distinct
// call stack is logged once ("[allocprobe] ..."). The counts also go into the
// "heap" diagnostics stream with the JKR heap tree's free space, at every
// harness sync point (game/src/port/harness_state.cpp). Off by default: normal
// runs only test a null function pointer per operator new.
#include "platform.h"

#include <emscripten.h>

#include <cstring>

// Weak: deluxe's platform/netplay.cpp (?netprobe=1) defines the same hook.
extern "C" {
__attribute__((weak)) void (*fsa_jkr_new_probe)(size_t) = nullptr;
int fsa_alloc_probe_on = 0;
u32 fsa_alloc_probe_hits = 0;     // host allocations that went to a JKR heap
u32 fsa_alloc_probe_bytes = 0;    // their bytes (allocated, not live)
u32 fsa_alloc_probe_stacks = 0;   // distinct call stacks among them
}

namespace {
char g_stack[8192];
u32 g_seen[4096];
u32 g_seenCount = 0;
bool g_inProbe = false;

void probe(size_t n) {
  if (g_inProbe)
    return;
  g_inProbe = true;
  HostAllocScope host;
  int len = emscripten_get_callstack(EM_LOG_C_STACK, g_stack, sizeof(g_stack));
  if (len > 0 && (strstr(g_stack, "aurora") || strstr(g_stack, "std::"))) {
    fsa_alloc_probe_hits++;
    fsa_alloc_probe_bytes += (u32)n;
    u32 h = 2166136261u;
    for (int i = 0; i < len && g_stack[i]; i++)
      h = (h ^ (u8)g_stack[i]) * 16777619u;
    bool seen = false;
    for (u32 i = 0; i < g_seenCount && !seen; i++)
      seen = g_seen[i] == h;
    if (!seen) {
      if (g_seenCount < sizeof(g_seen) / sizeof(g_seen[0]))
        g_seen[g_seenCount++] = h;
      fsa_alloc_probe_stacks++;
      platform_log("[allocprobe] host allocation of %u bytes in a JKR heap (stack %u):\n%s\n", (unsigned)n,
                   fsa_alloc_probe_stacks, g_stack);
    }
  }
  g_inProbe = false;
}
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void fsa_alloc_probe(int on) {
  fsa_alloc_probe_on = on;
  fsa_jkr_new_probe = on ? probe : nullptr;
}

extern "C" EMSCRIPTEN_KEEPALIVE void fsa_alloc_probe_report() {
  platform_log("[allocprobe] %u host allocations in JKR heaps, %u bytes, %u distinct stacks\n", fsa_alloc_probe_hits,
               fsa_alloc_probe_bytes, fsa_alloc_probe_stacks);
}
