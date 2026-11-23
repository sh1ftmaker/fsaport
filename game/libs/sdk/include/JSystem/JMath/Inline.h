#ifndef JSYSTEM_JMATH_INLINE_H
#define JSYSTEM_JMATH_INLINE_H

#include <dolphin/mtx.h>
#include "types.h"

namespace JMathInlineVEC {
#ifdef __MWERKS__
inline void PSVECAdd(register const Vec* vec1, register const Vec* vec2, register Vec* dst)
{
	register f32 v1xy, v2xy, d1xy, v1z, v2z, d1z;
	// clang-format off
	__asm {            
		psq_l     v1xy, 0(vec1), 0, 0
		psq_l     v2xy, 0(vec2), 0, 0
		ps_add    d1xy, v1xy, v2xy
		psq_st    d1xy, 0(dst), 0, 0
		
		psq_l     v1z,   8(vec1), 1, 0
		psq_l     v2z,   8(vec2), 1, 0
		ps_add    d1z, v1z, v2z
		psq_st    d1z,  8(dst), 1, 0
	}
	// clang-format on
}
inline void PSVECSubtract(register const Vec* vec1, register const Vec* vec2, register Vec* dst)
{
	// clang-format off
	register f32 v1xy, v2xy, dxy, v1z, v2z, dz;
	__asm {
		psq_l     v1xy, 0(vec1), 0, 0
		psq_l     v2xy, 0(vec2), 0, 0
		ps_sub    dxy, v1xy, v2xy
		psq_st    dxy, 0(dst), 0, 0
		
		psq_l     v1z,   8(vec1), 1, 0
		psq_l     v2z,   8(vec2), 1, 0
		ps_sub    dz, v1z, v2z
		psq_st    dz,  8(dst), 1, 0            
	}
	// clang-format on
}
inline void PSVECScale(register const Vec* src, register Vec* dst, register f32 scalar)
{
	// clang-format off
	register f32 vxy, vz, rxy, rz;
	__asm {
		
		psq_l       vxy, 0(src), 0, 0
		psq_l       vz,  8(src), 1, 0
		ps_muls0    rxy, vxy, scalar
		psq_st      rxy, 0(dst), 0, 0
		ps_muls0    rz,  vz,  scalar                        
		psq_st      rz,  8(dst), 1, 0
	}
	// clang-format on
}
inline f32 PSVECDotProduct(register const Vec* vec1, register const Vec* vec2)
{
	register f32 dp, v1yz, v2yz, v2xy, v1xy;
	// clang-format off
	__asm {            
		psq_l    v1yz, 4(vec1), 0, 0
		psq_l    v2yz, 4(vec2), 0, 0
		ps_mul   v1yz, v1yz, v2yz
		psq_l    v1xy, 0(vec1), 0, 0
		psq_l    v2xy, 0(vec2), 0, 0
		ps_madd  v2yz, v1xy, v2xy, v1yz
		ps_sum0  dp, v2yz, v1yz, v1yz
	}
	// clang-format on
	return dp;
}
inline f32 PSVECSquareMag(register const Vec* src)
{
	register f32 xy, z, ret;
	// clang-format off
	__asm {
		psq_l xy, 0(src), 0, 0
		ps_mul xy, xy, xy
		lfs z, 8(src)            
		ps_madd ret, z, z, xy
		ps_sum0 ret, ret, xy, xy            
	}
	// clang-format on
	return ret;
}
#else
// Port: plain C for the paired-single versions above (same operation order).
inline void PSVECAdd(const Vec* a, const Vec* b, Vec* d) { d->x = a->x + b->x; d->y = a->y + b->y; d->z = a->z + b->z; }
inline void PSVECSubtract(const Vec* a, const Vec* b, Vec* d) { d->x = a->x - b->x; d->y = a->y - b->y; d->z = a->z - b->z; }
inline void PSVECScale(const Vec* s, Vec* d, f32 k) { d->x = s->x * k; d->y = s->y * k; d->z = s->z * k; }
inline f32 PSVECDotProduct(const Vec* a, const Vec* b) { return (a->x * b->x + a->y * b->y) + a->z * b->z; }
inline f32 PSVECSquareMag(const Vec* s) { return (s->z * s->z + s->x * s->x) + s->y * s->y; }
#endif
} // namespace JMathInlineVEC

#endif
