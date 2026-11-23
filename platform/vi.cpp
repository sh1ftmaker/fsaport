// VI: retrace "interrupts" come from the page's requestAnimationFrame loop
// (paced to 59.94 Hz in platform/web/fsa_sched.js, whatever the display's
// refresh rate), and each game frame is one Aurora frame.
#include "platform.h"

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

u32 g_framesBegun = 0, g_framesDrawn = 0;

void platform_retrace() {
  platform_interrupt_enter();
  g_retraceCount++;
  if (g_retraceCount % 60 == 0) {
    const AuroraStats* st = aurora_get_stats();
    platform_log("[vi] retrace %u, frames begun %u drawn %u; draws %u (merged %u) pipelines %u/%u verts %u tex %u\n",
                 g_retraceCount, g_framesBegun, g_framesDrawn, st->drawCallCount, st->mergedDrawCallCount,
                 st->createdPipelines, st->queuedPipelines, st->lastVertSize, st->lastTextureUploadSize);
  }
  if (g_preRetrace)
    g_preRetrace(g_retraceCount);
  g_currentFb = g_nextFb;
  if (g_postRetrace)
    g_postRetrace(g_retraceCount);
  platform_interrupt_leave();
  OSWakeupThread(&g_retraceQueue);
  platform_service_interrupts();
}

void platform_frame_begin() {
  if (g_inFrame)
    return;
  HostAllocScope host;
  if (g_framesBegun++ < 3)
    platform_log("[vi] frame %u begin\n", g_framesBegun);
  aurora_update();
  g_inFrame = aurora_begin_frame();
  if (g_framesBegun <= 3)
    platform_log("[vi] frame %u begun=%d\n", g_framesBegun, (int)g_inFrame);
}

void platform_frame_end() {
  if (!g_inFrame)
    return;
  HostAllocScope host;
  if (g_framesDrawn < 3)
    platform_log("[vi] frame %u: game drawing done, aurora_end_frame\n", g_framesDrawn + 1);
  aurora_end_frame();
  g_inFrame = false;
  if (g_framesDrawn++ < 3)
    platform_log("[vi] frame %u ended\n", g_framesDrawn);
}

// The page calls this once per (paced) animation frame.
// Entry points called by the page are "interrupts": nothing in them may switch
// threads (that would suspend inside a browser callback); platform_kick then
// resumes a thread if the scheduler was idle.
extern "C" EMSCRIPTEN_KEEPALIVE void fsa_retrace() {
  platform_interrupt_enter();
  platform_retrace();
  platform_interrupt_leave();
  platform_kick();
}

// Next timer deadline in ms from now (for the page's setTimeout), or -1.
extern "C" EMSCRIPTEN_KEEPALIVE double fsa_next_timer_ms() {
  OSTime next = platform_next_alarm();
  if (!next)
    return -1;
  OSTime now = OSGetTime();
  if (next <= now)
    return 0;
  return (double)(next - now) * 1000.0 / (double)OS_TIMER_CLOCK;
}

extern "C" EMSCRIPTEN_KEEPALIVE void fsa_timer() {
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
