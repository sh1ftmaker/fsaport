#ifndef _DOLPHIN_GXVERT
#define _DOLPHIN_GXVERT

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GXFIFO_ADDR 0xCC008000

typedef union {
  u8 u8;
  u16 u16;
  u32 u32;
  u64 u64;
  s8 s8;
  s16 s16;
  s32 s32;
  s64 s64;
  f32 f32;
  f64 f64;
} PPCWGPipe;

#ifdef __MWERKS__
/*volatile*/ PPCWGPipe GXWGFifo : GXFIFO_ADDR;
#elif defined(FSA_PORT) && defined(__cplusplus)
// Port: the write-gather pipe is Aurora's command FIFO. Each store appends the
// value big-endian, as the hardware pipe did (platform/gx_pipe.cpp).
void fsa_gxpipe_u8(unsigned char v);
void fsa_gxpipe_u16(unsigned short v);
void fsa_gxpipe_u32(unsigned int v);
void fsa_gxpipe_f32(float v);
struct FsaGXPipeU8 { void operator=(unsigned char v) volatile { fsa_gxpipe_u8(v); } };
struct FsaGXPipeU16 { void operator=(unsigned short v) volatile { fsa_gxpipe_u16(v); } };
struct FsaGXPipeU32 { void operator=(unsigned int v) volatile { fsa_gxpipe_u32(v); } };
struct FsaGXPipeF32 { void operator=(float v) volatile { fsa_gxpipe_f32(v); } };
struct FsaGXPipe {
  FsaGXPipeU8 u8;
  FsaGXPipeU8 s8;
  FsaGXPipeU16 u16;
  FsaGXPipeU16 s16;
  FsaGXPipeU32 u32;
  FsaGXPipeU32 s32;
  FsaGXPipeF32 f32;
};
extern volatile FsaGXPipe fsa_gx_pipe;
#define GXWGFifo fsa_gx_pipe
#else
#define GXWGFifo (*(volatile PPCWGPipe*)GXFIFO_ADDR)
#endif

static inline void GXPosition2f32(const f32 x, const f32 y)
{
  GXWGFifo.f32 = x;
  GXWGFifo.f32 = y;
}

static inline void GXPosition3s16(const s16 x, const s16 y, const s16 z)
{
  GXWGFifo.s16 = x;
  GXWGFifo.s16 = y;
  GXWGFifo.s16 = z;
}

static inline void GXPosition3u16(const u16 x, const u16 y, const u16 z)
{
  GXWGFifo.u16 = x;
  GXWGFifo.u16 = y;
  GXWGFifo.u16 = z;
}

static inline void GXPosition3f32(const f32 x, const f32 y, const f32 z)
{
  GXWGFifo.f32 = x;
  GXWGFifo.f32 = y;
  GXWGFifo.f32 = z;
}

static inline void GXNormal3f32(const f32 x, const f32 y, const f32 z)
{
  GXWGFifo.f32 = x;
  GXWGFifo.f32 = y;
  GXWGFifo.f32 = z;
}

static inline void GXColor1u32(const u32 c) { GXWGFifo.u32 = c; }

static inline void GXColor4u8(const u8 r, const u8 g, const u8 b, const u8 a)
{
  GXWGFifo.u8 = r;
  GXWGFifo.u8 = g;
  GXWGFifo.u8 = b;
  GXWGFifo.u8 = a;
}

static inline void GXTexCoord2s8(const s8 u, const s8 v)
{
  GXWGFifo.s8 = u;
  GXWGFifo.s8 = v;
}

static inline void GXTexCoord2u8(const u8 s, const u8 t)
{
  GXWGFifo.u8 = s;
  GXWGFifo.u8 = t;
}

static inline void GXPosition2u16(u16 x, u16 y)
{
  GXWGFifo.u16 = x;
  GXWGFifo.u16 = y;
}

static inline void GXTexCoord2s16(const s16 u, const s16 v)
{
  GXWGFifo.s16 = u;
  GXWGFifo.s16 = v;
}

static inline void GXTexCoord2u16(const u16 u, const u16 v)
{
  GXWGFifo.u16 = u;
  GXWGFifo.u16 = v;
}

static inline void GXTexCoord2f32(const f32 u, const f32 v)
{
  GXWGFifo.f32 = u;
  GXWGFifo.f32 = v;
}

static inline void GXEnd(void) {}

#ifdef __cplusplus
}
#endif

#endif // _DOLPHIN_GXVERT
