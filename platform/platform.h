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

// Log through OSReport's sink (console.log in the browser).
void platform_log(const char* fmt, ...);
