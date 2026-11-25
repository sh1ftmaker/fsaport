#ifndef _JSYSTEM_J3D_J3DANMBASE_H
#define _JSYSTEM_J3D_J3DANMBASE_H

#include "JSystem/J3D/J3DFileBlock.h"
#include "JSystem/JMath/JMath.h"
#include "types.h"
#include <dolphin/gx.h>

enum J3DAnmKind {
	J3DAnmKind_Cluster        = 3,
	J3DAnmKind_ClusterFull    = 12,
	J3DAnmKind_ClusterKey     = 13,
	J3DAnmKind_Color          = 1,
	J3DAnmKind_ColorFull      = 10,
	J3DAnmKind_ColorKey       = 11,
	J3DAnmKind_TevRegKey      = 5,
	J3DAnmKind_TextureSRTKey  = 4,
	J3DAnmKind_TexPattern     = 2,
	J3DAnmKind_Transform      = 0,
	J3DAnmKind_TransformFull  = 9,
	J3DAnmKind_TransformKey   = 8,
	J3DAnmKind_VisibilityFull = 6,
	J3DAnmKind_VtxColor       = 7,
	J3DAnmKind_VtxColorFull   = 14,
	J3DAnmKind_VtxColorKey    = 15,
	J3DAnmKind_TransformFullWithLerp = 16
};

#pragma enumalwaysint off
// Intended size: byte
enum J3DAnmAttr {
	J3DAA_UNKNOWN_0 = 0,
	J3DAA_UNKNOWN_1 = 1,
	J3DAA_UNKNOWN_2 = 2,
	J3DAA_UNKNOWN_3 = 3,
	J3DAA_UNKNOWN_4 = 4,
};
#pragma enumalwaysint reset

struct J3DAnmBase {
	inline J3DAnmBase()
	    : mAttribute(0)
	    , mUnused(0)
	    , mTotalFrameCount(0)
	    , mCurrentFrame(0.0f)
	{
	}

	inline J3DAnmBase(s16 frameMax)
	    : mAttribute(0)
	    , mUnused(0)
	    , mTotalFrameCount(frameMax)
	    , mCurrentFrame(0.0f)
	{
	}

	virtual ~J3DAnmBase() { }               // _08 (weak)
	virtual J3DAnmKind getKind() const = 0; // _0C

	u8 getAttribute() const { return mAttribute; }
	s16 getTotalFrameCount() const { return mTotalFrameCount; }
	f32 getFrame() const { return mCurrentFrame; }
	void setFrame(f32 frame) { mCurrentFrame = frame; }

	// _00 = VTBL
	u8 mAttribute;        // _04
	u8 mUnused;           // _05
	s16 mTotalFrameCount; // _06
	f32 mCurrentFrame;    // _08
};

struct J3DAnmFullData : public J3DFileBlockBase {
	u8 mAttribute;        // _08
	u8 mRotationScale;    // _09
	s16 mTotalFrameCount; // _0A
	u16 mExtraInfo1;      // _0C
	u16 mExtraInfo2;      // _0E
};

struct J3DAnmKeyTableBase {
	u16 mMaxFrame; // _00
	u16 mOffset;   // _02
	u16 mType;     // _04
};

template <typename T>
inline f32 J3DHermiteInterpolation(f32, T*, T*, T*, T*, T*, T*);

template <>
inline f32 J3DHermiteInterpolation<f32>(f32 f1, f32* f2, f32* f3, f32* f4, f32* f5, f32* f6, f32* f7)
{
	return JMAHermiteInterpolation(f1, *f2, *f3, *f4, *f5, *f6, *f7);
}

template <>
inline f32 J3DHermiteInterpolation<s16>(register f32 pp1, register s16* pp2, register s16* pp3, register s16* pp4, register s16* pp5,
                                        register s16* pp6, register s16* pp7)
{
	register f32 p1 = pp1;
	register f32 ff8;
	register f32 ff7;
	register f32 ff6;
	register f32 ff5;
	register f32 ff4;
	register f32 ff3;
	register f32 ff2;
	register f32 ff0;
	register f32 fout;
	register s16* p2 = pp2;
	register s16* p3 = pp3;
	register s16* p4 = pp4;
	register s16* p5 = pp5;
	register s16* p6 = pp6;
	register s16* p7 = pp7;
#ifdef __MWERKS__ // clang-format off
	asm {
		psq_l ff2, 0(p2), 0x1, 5
		psq_l ff0, 0(p5), 0x1, 5
		psq_l ff7, 0(p3), 0x1, 5
		fsubs ff5, ff0, ff2
		psq_l ff6, 0(p6), 0x1, 5
		fsubs ff3, p1, ff2
		psq_l ff0, 0(p7), 0x1, 5
		fsubs ff4, ff6, ff7
		fdivs ff3, ff3, ff5
		psq_l fout, 0(p4), 0x1, 5
		fmadds ff0, ff0, ff5, ff7
		fmuls ff2, ff3, ff3
		fnmsubs ff4, ff5, fout, ff4
		fsubs ff0, ff0, ff6
		fsubs ff0, ff0, ff4
		fmuls ff0, ff2, ff0
		fmadds fout, ff5, fout, ff0
		fmadds fout, fout, ff3, ff7
		fmadds fout, ff4, ff2, fout
		fsubs fout, fout, ff0
	}
#else // clang-format on
	// Port: GQR5 is OSInitFastCast's s16 with no scale, so the paired loads above
	// are plain conversions; the asm's operations in its order (decomp 5f6b4a625).
	f32 t0 = *p2, v0 = *p3, tan0 = *p4, t1 = *p5, v1 = *p6, tan1 = *p7;
	f32 dt = t1 - t0;
	f32 t = (p1 - t0) / dt;
	f32 t2 = t * t;
	f32 d = (v1 - v0) - dt * tan0;
	f32 c = t2 * (((tan1 * dt + v0) - v1) - d);
	fout = dt * tan0 + c;
	fout = fout * t + v0;
	fout = d * t2 + fout;
	fout = fout - c;
#endif
	return fout;
}

#endif
