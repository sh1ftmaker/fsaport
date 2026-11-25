// VI: retrace "interrupts" come from the page's requestAnimationFrame loop
// (paced to 59.94 Hz in platform/web/fsa_sched.js, whatever the display's
// refresh rate), and each game frame is one Aurora frame.
#include "platform.h"
#include "port/endian_diag.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <emscripten.h>

namespace {
u32 g_retraceCount = 0;
VIRetraceCallback g_preRetrace = nullptr;
VIRetraceCallback g_postRetrace = nullptr;
void* g_nextFb = nullptr;
void* g_currentFb = nullptr;
bool g_inFrame = false;
OSThreadQueue g_retraceQueue;
}  // namespace

u32 platform_retrace_count() { return g_retraceCount; }

u32 g_framesBegun = 0, g_framesDrawn = 0;
double g_frameStart = 0, g_frameMsSum = 0, g_endMsSum = 0, g_beginMsSum = 0;

void platform_retrace() {
  platform_interrupt_enter();
  g_retraceCount++;
  if (g_retraceCount % 60 == 0) {
    const AuroraStats* st = aurora_get_stats();
    static u32 lastDrawn = 0;
    u32 n = g_framesDrawn - lastDrawn;
    platform_log("[vi] retrace %u, frames %u; game %.2f ms/frame, aurora_begin %.2f ms, aurora_end %.2f ms; draws %u\n", g_retraceCount,
                 n, n ? g_frameMsSum / n : 0.0, n ? g_beginMsSum / n : 0.0, n ? g_endMsSum / n : 0.0, st->drawCallCount);
    lastDrawn = g_framesDrawn;
    g_frameMsSum = g_endMsSum = g_beginMsSum = 0;
  }
#ifdef FSA_ENDIAN_DIAG
  if (g_retraceCount % 300 == 0)
    fsa_endian_report();
#endif
  if (g_preRetrace)
    g_preRetrace(g_retraceCount);
  g_currentFb = g_nextFb;
  if (g_postRetrace)
    g_postRetrace(g_retraceCount);
  platform_interrupt_leave();
  OSWakeupThread(&g_retraceQueue);
  platform_service_interrupts();
}

extern "C" void fsa_view_frame_begin();  // platform/view.cpp: applies a pending picture change

void platform_frame_begin() {
  if (g_inFrame)
    return;
  HostAllocScope host;
  fsa_view_frame_begin();
  if (g_framesBegun++ < 3)
    platform_log("[vi] frame %u begin\n", g_framesBegun);
  double tb = emscripten_get_now();
  aurora_update();
  g_inFrame = aurora_begin_frame();
  g_frameStart = emscripten_get_now();
  g_beginMsSum += g_frameStart - tb;
  if (g_framesBegun <= 3)
    platform_log("[vi] frame %u begun=%d\n", g_framesBegun, (int)g_inFrame);
}

extern "C" void fsa_gba_gcview_frame(void);  // game/src/port/gba_gcview.cpp (deluxe; off by default)

void platform_frame_end() {
  if (!g_inFrame)
    return;
  fsa_gba_gcview_frame();  // GBA views drawn by the game's renderer, when the page asked for them
  HostAllocScope host;
  if (g_framesDrawn < 3)
    platform_log("[vi] frame %u: game drawing done, aurora_end_frame\n", g_framesDrawn + 1);
  double t0 = emscripten_get_now();
  g_frameMsSum += t0 - g_frameStart;
  aurora_end_frame();
  g_endMsSum += emscripten_get_now() - t0;
  g_inFrame = false;
  if (g_framesDrawn++ < 3)
    platform_log("[vi] frame %u ended\n", g_framesDrawn);
}

// The page calls this once per (paced) animation frame.
// Entry points called by the page are "interrupts": nothing in them may switch
// threads (that would suspend inside a browser callback); platform_kick then
// resumes a thread if the scheduler was idle.
#ifdef FSA_DIAG
extern "C" void fsa_diag_retrace(void);  // diag_cov.cpp: scene sample, periodic flush
#endif
extern "C" EMSCRIPTEN_KEEPALIVE void fsa_retrace() {
  platform_interrupt_enter();
  platform_retrace();
#ifdef FSA_DIAG
  fsa_diag_retrace();
#endif
  platform_interrupt_leave();
  platform_kick();
}

// Next timer deadline in ms from now (for the page's setTimeout), or -1.
extern "C" EMSCRIPTEN_KEEPALIVE double fsa_next_timer_ms() {
  OSTime next = platform_next_alarm();
  if (!next || platform_lockstep())  // lockstep: alarms fire at retraces only
    return -1;
  OSTime now = OSGetTime();
  if (next <= now)
    return 0;
  return (double)(next - now) * 1000.0 / (double)OS_TIMER_CLOCK;
}

extern "C" EMSCRIPTEN_KEEPALIVE void fsa_timer() {
  if (platform_lockstep())
    return;
  platform_interrupt_enter();
  platform_service_interrupts();
  platform_interrupt_leave();
  platform_kick();
}

extern "C" {

void VIWaitForRetrace() {
  u32 start = g_retraceCount;
  while (g_retraceCount == start)
    OSSleepThread(&g_retraceQueue);
}
void* VIGetNextFrameBuffer() { return g_nextFb; }
void* VIGetCurrentFrameBuffer() { return g_currentFb; }
void VISetNextFrameBuffer(void* fb) { g_nextFb = fb; }
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) {
  VIRetraceCallback old = g_preRetrace;
  g_preRetrace = cb;
  return old;
}
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) {
  VIRetraceCallback old = g_postRetrace;
  g_postRetrace = cb;
  return old;
}
void VISetBlack(BOOL) {}
u32 VIGetRetraceCount() {
  platform_poll_point();
  return g_retraceCount;
}
u32 VIGetNextField() { return 1; }
u32 VIGetCurrentLine() { return 0; }
u32 VIGetDTVStatus() { return 0; }
void __VIGetCurrentPosition(s16* x, s16* y) { *x = *y = 0; }

}  // extern "C"
