#include "../../host_alloc.hpp"
#include <dolphin/gd.h>
#include <dolphin/os.h>
#include "gd.hpp"

GDLObj* __GDCurrentDL = NULL;
static GDOverflowCb overflowcb = NULL;

void GDInitGDLObj(GDLObj* dl, void* start, u32 length) {
  AURORA_HOST_ALLOC;
    ASSERTMSGLINE(40, !((u32)start & 0x1F), "start must be aligned to 32 bytes");
    ASSERTMSGLINE(41, !((u32)length & 0x1F), "length must be aligned to 32 bytes");
    dl->start = (u8*)start;
    dl->ptr = (u8*)start;
    dl->top = (u8*)start + length;
    dl->length = length;
}

void GDFlushCurrToMem(void) {
  AURORA_HOST_ALLOC;
    DCFlushRange(__GDCurrentDL->start, __GDCurrentDL->length);
}

void GDPadCurr32(void) {
  AURORA_HOST_ALLOC;
    uintptr_t n;

    n = (uintptr_t)__GDCurrentDL->ptr & 0x1F;
    if (n != 0) {
        for (; n < 32; n = n + 1) {
            __GDWrite(0);
        }
    }
}

void GDOverflowed(void) {
  AURORA_HOST_ALLOC;
    if (overflowcb) {
        (*overflowcb)();
    } else {
        Log.fatal("GDWrite: display list overflowed");
    }
}

void GDSetOverflowCallback(GDOverflowCb callback) {
  AURORA_HOST_ALLOC;
    overflowcb = callback;
}

GDOverflowCb GDGetOverflowCallback(void) {
  AURORA_HOST_ALLOC;
    return overflowcb;
}
