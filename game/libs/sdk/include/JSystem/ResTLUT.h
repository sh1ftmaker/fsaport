#ifndef _JSYSTEM_RESTLUT_H
#define _JSYSTEM_RESTLUT_H

#include <dolphin/gx.h>
#include "types.h"
#include "port/be.h"

struct ResTLUT
{
    u8 mFormat;       // _00, GXTlutFormat
    u8 mTransparency; // _01, JUTTransparency
    BE<u16> mNumColors;   // _02
    u8 _04[0x1C];     // _04
    BE<u32> _20;          // _20
};

#endif