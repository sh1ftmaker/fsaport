#ifndef PORTABLE_FLOATCONV_H
#define PORTABLE_FLOATCONV_H

// Float to u16 the way the GameCube converts: fctiwz to a 32-bit integer, then
// the low 16 bits, so a negative angle wraps (-1.0f -> 0xFFFF). Other targets
// clamp or leave it undefined (wasm gives 0). Where a plain (u16)(s32) changes
// MWCC's code, use this: the plain cast under MWCC, through s32 elsewhere.
// See docs/PORTABLE.md.
#ifdef __MWERKS__
#define gc_f32_to_u16(x) ((u16)(x))
#else
#define gc_f32_to_u16(x) ((u16)(s32)(x))
#endif

#endif
