// The rest of the OS: reporting, reset/boot state, cache and CPU no-ops,
// deferred "interrupt" callbacks, and the C library data MSL headers expect.
#include "platform.h"

#include <emscripten.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <utility>

extern "C" int fsa_host_alloc_depth = 0;

namespace {
std::deque<std::pair<void (*)(void*), void*>> g_pending;
bool g_servicing = false;
}  // namespace

void platform_post_callback(void (*fn)(void*), void* arg) {
  HostAllocScope host;
  g_pending.emplace_back(fn, arg);
}

void platform_service_interrupts() {
  if (g_servicing)
    return;
  g_servicing = true;
  platform_interrupt_enter();
  while (!g_pending.empty()) {
    auto [fn, arg] = g_pending.front();
    g_pending.pop_front();
    fn(arg);
  }
  platform_interrupt_leave();
  OSTime now = OSGetTime();
  platform_check_sleepers(now);
  platform_check_alarms(now);
  g_servicing = false;
  platform_reschedule();
}

void platform_log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  fflush(stdout);
}

extern "C" {

volatile int __OSTVMode = 0;  // NTSC

// --- Reporting --------------------------------------------------------------
void OSVReport(const char* msg, va_list list) {
  vfprintf(stdout, msg, list);
  fflush(stdout);
}

void OSReport(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  OSVReport(msg, ap);
  va_end(ap);
}

void OSPanic(const char* file, int line, const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fprintf(stdout, "PANIC %s:%d: ", file, line);
  vfprintf(stdout, msg, ap);
  fputc('\n', stdout);
  va_end(ap);
  fflush(stdout);
  abort();
}

void OSFatal(GXColor, GXColor, const char* msg) {
  fprintf(stdout, "FATAL: %s\n", msg);
  fflush(stdout);
  abort();
}

typedef void (*OSErrorHandler)(u16 error, OSContext* context, ...);
OSErrorHandler OSSetErrorHandler(u16, OSErrorHandler handler) { return handler; }

// --- Reset, boot and video modes ----------------------------------------------
static u8* g_saveRegionStart;
static u8* g_saveRegionEnd;
static u32 g_progressive = 0;
static u32 g_soundMode = 1;  // stereo

u32 OSGetResetCode() { return 0; }
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
  fprintf(stdout, "OSResetSystem(%d, %u, %d): reloading\n", reset, resetCode, forceMenu);
  fflush(stdout);
  EM_ASM({ location.reload(); });
  for (;;)
    emscripten_sleep(1000);
}
BOOL OSGetResetSwitchState() { return FALSE; }
void OSSetSaveRegion(void* start, void* end) {
  g_saveRegionStart = (u8*)start;
  g_saveRegionEnd = (u8*)end;
}
void OSGetSaveRegion(void** start, void** end) {
  *start = g_saveRegionStart;
  *end = g_saveRegionEnd;
}
u32 OSGetProgressiveMode() { return g_progressive; }
void OSSetProgressiveMode(u32 on) { g_progressive = on; }
u32 OSGetSoundMode() { return g_soundMode; }
void OSSetSoundMode(u32 mode) { g_soundMode = mode; }
u32 OSSetBootDol(u32) { return 0; }
void OSProtectRange(u32, void*, u32, u32) {}

// --- CPU state the game reads ---------------------------------------------------
static OSContext g_context;
OSContext* OSGetCurrentContext() { return &g_context; }
void OSFillFPUContext(OSContext*) {}
u32 __OSFpscrEnableBits = 0;
u32 OSGetStackPointer() { return (u32)(uintptr_t)__builtin_frame_address(0); }

u32 PPCMfmsr() { return 0; }
void PPCMtmsr(u32) {}
u32 PPCMfhid2() { return 0; }
void PPCMthid2(u32) {}
void PPCSync() {}

// --- Caches: coherent host memory --------------------------------------------
void DCFlushRange(void*, u32) {}
void DCFlushRangeNoSync(void*, u32) {}
void DCInvalidateRange(void*, u32) {}
void DCStoreRange(void*, u32) {}
void DCStoreRangeNoSync(void*, u32) {}
void DCZeroRange(void* addr, u32 n) { memset(addr, 0, n); }
void ICInvalidateRange(void*, u32) {}
void ICFlashInvalidate() {}

// Locked cache: the SDK's scratchpad DMA. Copies happen immediately here.
void LCEnable() {}
void LCDisable() {}
void LCQueueWait(u32) {}
u32 LCStoreData(void* dest, void* src, u32 n) {
  memmove(dest, src, n);
  return 0;
}
u32 LCLoadData(void* dest, void* src, u32 n) {
  memmove(dest, src, n);
  return 0;
}

}  // extern "C"
