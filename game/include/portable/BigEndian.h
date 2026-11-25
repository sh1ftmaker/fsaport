#ifndef PORTABLE_BIGENDIAN_H
#define PORTABLE_BIGENDIAN_H

#include "dolphin/types.h"

// Reads of big-endian values from game data that may be unaligned (message
// tag arguments, file headers). On the GameCube these are plain loads, and
// they stay macros there so the code MWCC generates is exactly that load. A
// portable build assembles the bytes, so host byte order and alignment don't
// matter. See docs/PORTABLE.md.
// The MWCC casts carry no const: a load through a const-qualified cast moved
// registers in THPAudioDecode (the lYn1/lYn2 reads), the plain cast doesn't.
#ifdef __MWERKS__
#define be_read_s32(p) (*(s32*)(p))
#define be_read_u32(p) (*(u32*)(p))
#define be_read_s16(p) (*(s16*)(p))
#define be_read_u16(p) (*(u16*)(p))
#else
#ifdef FSA_DIAG
// Port: hot byte-order helpers without a decomp symbol, not counted by the
// diagnostics build (as port/be.h; docs/DIAGNOSTICS.md).
#pragma clang attribute push(__attribute__((no_instrument_function)), apply_to = function)
#endif
static inline u32 be_read_u32(const void* p)
{
    const u8* b = (const u8*)p;
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | (u32)b[3];
}
static inline s32 be_read_s32(const void* p) { return (s32)(signed int)be_read_u32(p); }
static inline u16 be_read_u16(const void* p)
{
    const u8* b = (const u8*)p;
    return (u16)(((u32)b[0] << 8) | (u32)b[1]);
}
static inline s16 be_read_s16(const void* p) { return (s16)be_read_u16(p); }
#ifdef FSA_DIAG
#pragma clang attribute pop
#endif
#endif

#endif
