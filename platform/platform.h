// Internal interface between the platform layer's parts.
#pragma once

#include <dolphin/os.h>
#include <dolphin/dvd.h>
#include <dolphin/vi.h>

// os_thread.cpp
void platform_threads_init();
void platform_interrupt_enter();
void platform_interrupt_leave();
void platform_kick();                       // resume a ready thread if idle
bool platform_is_idle();
void platform_reschedule();                 // let a readied higher-priority thread run
void platform_check_alarms(OSTime now);
void platform_check_sleepers(OSTime now);
OSTime platform_next_alarm();               // 0 = none
void platform_sleep_ticks(OSTime ticks);
void platform_poll_point();                 // see os_thread.cpp

// os_misc.cpp: run pending "interrupts" (DVD callbacks, due alarms). Safe to
// call from any thread context; callbacks never switch threads.
void platform_service_interrupts();
void platform_post_callback(void (*fn)(void*), void* arg);

// dvd.cpp
void platform_dvd_init();

// vi.cpp
void platform_retrace();                    // one VI retrace "interrupt"
void platform_frame_begin();                // JFWDisplay::beginRender
void platform_frame_end();                  // JFWDisplay::endFrame
u32 platform_retrace_count();               // VI retraces so far (no poll point)

// harness.cpp: the behaviour harness (docs/HARNESS.md). In lockstep mode time
// is virtual (retrace count), retraces only arrive while every thread is
// blocked and polling loops yield by call count, not wall time.
bool platform_lockstep();
struct PADStatus;
void platform_harness_pad(struct PADStatus* status);  // every JUTGamePad::read, after the touch pad

// Host allocations (libc++, Aurora, this layer) must not land in the game's
// JKR heaps: JSystem's operator new sends them to malloc while this is > 0.
extern "C" int fsa_host_alloc_depth;
struct HostAllocScope {
  HostAllocScope() { fsa_host_alloc_depth++; }
  ~HostAllocScope() { fsa_host_alloc_depth--; }
  HostAllocScope(const HostAllocScope&) = delete;
  HostAllocScope& operator=(const HostAllocScope&) = delete;
};

// Log through OSReport's sink (console.log in the browser).
void platform_log(const char* fmt, ...);
