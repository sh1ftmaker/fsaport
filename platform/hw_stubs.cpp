// Hardware the port does not have yet, made to look present and idle:
// the DSP's mailbox and GX calls Aurora does not implement (the GBA link is
// platform/gba_link.cpp). Each is a placeholder until its real replacement exists.
#include "platform.h"

#include <dolphin/gx.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" {

u32 __OSBusClock = 162000000;
u32 __OSCoreClock = 486000000;

// --- DSP: its mixing is done on the CPU (audio_dsp.cpp; the AI is audio.cpp),
// so the DSP itself is never booted or mailed.
void DSPInit() {}
u32 DSPCheckMailToDSP() { return 0; }
u32 DSPCheckMailFromDSP() { return 0; }
u32 DSPReadMailFromDSP() { return 0; }
void DSPSendMailToDSP(u32) {}
void DSPAssertInt() {}
void __DSP_boot_task(void*) {}

// --- GBA link: platform/gba_link.cpp (deluxe). A native endpoint per port; with
// no GBA plugged in it answers as the stub here did (GBA_NOT_READY).

BOOL __PADDisableRecalibration(BOOL) { return FALSE; }

// --- GX calls Aurora does not implement ---------------------------------------
void GXAbortFrame() {}
OSThread* GXSetCurrentGXThread() { return OSGetCurrentThread(); }
OSThread* GXGetCurrentGXThread() { return OSGetCurrentThread(); }
void GXSetMisc(GXMiscToken, u32) {}
void GXSetCopyClamp(GXFBClamp) {}
void GXSetZScaleOffset(f32, f32) {}
void GXInitTexCacheRegion(GXTexRegion*, GXBool, u32, GXTexCacheSize, u32, GXTexCacheSize) {}
void GXPeekARGB(u16, u16, u32* color) { *color = 0; }
void GXReadXfRasMetric(u32* a, u32* b, u32* c, u32* d) { *a = *b = *c = *d = 0; }
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) { return (f32)xfbHeight / (f32)efbHeight; }
u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) { return (u16)((f32)efbHeight * yScale); }

// --- MSL bits the decompiled code reaches ------------------------------------
int __float_epsilon[1] = {0x34000000};  // FLT_EPSILON
int __float_max[1] = {0x7F7FFFFF};      // FLT_MAX
int __float_huge[1] = {0x7F800000};     // +inf
int __float_nan[1] = {0x7FFFFFFF};
unsigned char __lower_map[256];
unsigned char __upper_map[256];
// MSL's FILE table; fiprintf below ignores which stream it is given.
unsigned char __files[4 * 0x50];

__attribute__((constructor)) static void init_ctype_maps() {
  for (int i = 0; i < 256; i++) {
    __lower_map[i] = (i >= 'A' && i <= 'Z') ? i + 32 : i;
    __upper_map[i] = (i >= 'a' && i <= 'z') ? i - 32 : i;
  }
}

int siprintf(char* buf, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsprintf(buf, fmt, ap);
  va_end(ap);
  return n;
}


}  // extern "C"
