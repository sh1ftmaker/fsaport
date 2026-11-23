#ifndef _JSYSTEM_J3D_J3DFILEBLOCK_H
#define _JSYSTEM_J3D_J3DFILEBLOCK_H

#include "port/be.h"
#include "types.h"

struct J3DFileBlockBase;

/**
 * @size{0x20}
 */
struct J3DFileHeader {
	BE<u32> mJ3dVersion;  // _00
	BE<u32> mFileVersion; // _04
	u8 _08[4];        // _08
	BE<u32> mBlockCount;  // _0C
	u8 _10[0x10];     // _10

	const J3DFileBlockBase* getFirstBlock() const { return reinterpret_cast<const J3DFileBlockBase*>(this + 1); }
};

/**
 * @size{0x8}
 */
struct J3DFileBlockBase {
	BE<u32> mBlockType; // _00
	BE<int> mSize;      // _04

	const J3DFileBlockBase* getNext() const
	{
		return reinterpret_cast<const J3DFileBlockBase*>(reinterpret_cast<const u8*>(this) + this->mSize);
	}
};

enum J3DFileBlockType {
	J3DFBT_Draw        = 'DRW1',
	J3DFBT_Envelope    = 'EVP1',
	J3DFBT_Info        = 'INF1',
	J3DFBT_Joint       = 'JNT1',
	J3DFBT_MaterialV21 = 'MAT2',
	J3DFBT_Material    = 'MAT3',
	J3DFBT_MaterialDL  = 'MDL3',
	J3DFBT_Texture     = 'TEX1',
	J3DFBT_Shape       = 'SHP1',
	J3DFBT_Vertex      = 'VTX1',

	//  Anm Block Types:
	J3DFBT_AnmTexPattern     = 'TPT1',
	J3DFBT_AnmClusterFull    = 'CLF1',
	J3DFBT_AnmClusterKey     = 'CLK1',
	J3DFBT_AnmTransformFull  = 'ANF1',
	J3DFBT_AnmTransformKey   = 'ANK1',
	J3DFBT_AnmColorFull      = 'PAF1',
	J3DFBT_AnmColorKey       = 'PAK1',
	J3DFBT_AnmVtxColorFull   = 'VCF1',
	J3DFBT_AnmVtxColorKey    = 'VCK1',
	J3DFBT_AnmVisibilityFull = 'VAF1',
	J3DFBT_AnmTextureSRTKey  = 'TTK1',
	J3DFBT_AnmTevRegKey      = 'TRK1',
};

/**
 *    Block members courtesy of https://wiki.cloudmodding.com/tww/BMD_and_BDL
 */

struct J3DDrawBlock : J3DFileBlockBase {
	BE<u16> mCount;                   // _08
	BEOffset mMatrixTypeArrayOffset; // _0C
	BEOffset mDataArrayOffset;       // _14
};

struct J3DEnvelopeBlock : J3DFileBlockBase {
	BE<u16> mCount;                   // _08
	BEOffset mJointCountTableOffset; // _0C
	BEOffset mIndexTableOffset;      // _10
	BEOffset mWeightTableOffset;     // _14
	BEOffset mInvBindTableOffset;    // _18
};

struct J3DJointBlock : J3DFileBlockBase {
	BE<u16> mCount;            // _08
	BE<u32> mJointInitData;    // _0C
	BE<u32> mRemapTableOffset; // _10
	BE<u32> mNameTableOffset;  // _14
};

struct J3DMaterialBlock : J3DFileBlockBase {
	BE<u16> mNumMaterials;                 // _08
	BEOffset mMatEntryDataOffset;         // _0C
	BEOffset mMatRemapTableOffset;        // _10
	BEOffset mStringTableOffset;          // _14
	BEOffset mIndTextureInfoOffset;       // _18
	BEOffset mCullModeInfoOffset;         // _1C
	BEOffset mMatColorsOffset;            // _20
	BEOffset mNumColorChansOffset;        // _24
	BEOffset mColorChanInfoOffset;        // _28
	BEOffset mAmbientColorOffset;         // _2C
	BEOffset mLightInfoOffset;            // _30
	BEOffset mNumTexCoordsOffset;         // _34
	BEOffset mTexCoordInfoOffset;         // _38
	BEOffset mTexCoord2InfoOffset;        // _3C
	BEOffset mTexMtxInfoOffset;           // _40
	BEOffset mTexMtxInfo2Offset;          // _44
	BEOffset mTextureRemapTableOffset;    // _48
	BEOffset mTevOrderInfoOffset;         // _4C
	BEOffset mTevColorsOffset;            // _50
	BEOffset mTevKColorsOffset;           // _54
	BEOffset mNumTevStagesOffset;         // _58
	BEOffset mTevStageInfoOffset;         // _5C
	BEOffset mTevSwapModeInfoOffset;      // _60
	BEOffset mTevSwapModeTableInfoOffset; // _64
	BEOffset mFogInfoOffset;              // _68
	BEOffset mAlphaCompInfoOffset;        // _6C
	BEOffset mBlendInfoOffset;            // _70
	BEOffset mZModeInfoOffset;            // _74
	BEOffset mZCompareInfoOffset;         // _78
	BEOffset mDitherInfoOffset;           // _7C
	BEOffset mNBTScaleInfoOffset;         // _80
};

struct J3DMaterialBlock_v21 : J3DFileBlockBase {
	BE<u16> mNumMaterials;                 // _08
	BEOffset mMatEntryDataOffset;         // _0C
	BEOffset mMatRemapTableOffset;        // _10
	BEOffset mIndTexInfoOffset;           // _14
	BEOffset mCullModeInfoOffset;         // _18
	BEOffset mMatColorsOffset;            // _1C
	BEOffset mNumColorChansOffset;        // _20
	BEOffset mColorChanInfoOffset;        // _24
	BEOffset mNumTexCoordsOffset;         // _28
	BEOffset mTexCoordInfoOffset;         // _2C
	BEOffset mTexCoord2InfoOffset;        // _30
	BEOffset mTexMtxInfoOffset;           // _34
	BEOffset mTexMtxInfo2Offset;          // _38
	BEOffset mTextureRemapTableOffset;    // _3C
	BEOffset mTevOrderInfoOffset;         // _40
	BEOffset mTevColorsOffset;            // _44
	BEOffset mTevKColorsOffset;           // _48
	BEOffset mNumTevStagesOffset;         // _4C
	BEOffset mTevStageInfoOffset;         // _50
	BEOffset mTevSwapModeInfoOffset;      // _54
	BEOffset mTevSwapModeTableInfoOffset; // _58
	BEOffset mFogInfoOffset;              // _5C
	BEOffset mAlphaCompInfoOffset;        // _60
	BEOffset mBlendInfoOffset;            // _64
	BEOffset mZModeInfoOffset;            // _68
	BEOffset mZCompareInfoOffset;         // _6C
	BEOffset mDitherInfoOffset;           // _70
	BEOffset mNBTScaleInfoOffset;         // _74
};

struct J3DMaterialDLBlock : J3DFileBlockBase {
	BE<u16> mEntries;                   // _08
	BEOffset mPacketOffset;            // _0C
	BEOffset mSubPacketLocationOffset; // _10
	BEOffset mMatrixIndexOffset;       // _14
	BEOffset mPixelEngineModesOffset;  // _18
	BEOffset mIndexesOffset;           // _1C
	BEOffset mStringTableOffset;       // _20
};

struct J3DModelInfoBlock : J3DFileBlockBase {
	BE<u16> mFlags;                 // _08
	BE<u32> mMatrixGroupCount;      // _0C
	BE<u32> mVertexCount;           // _10
	BEOffset mHierarchyDataOffset; // _14
};

struct J3DShapeBlock : J3DFileBlockBase {
	BE<u16> mShapeNum;             // _08
	BE<u32> mShapeDataOffset;      // _0C
	BE<u32> mRemapTableOffset;     // _10
	BE<u32> mNameTableOffset;      // _14
	BE<u32> mAttribTableOffset;    // _18
	BE<u32> mMatrixTableOffset;    // _1C
	BE<u32> mPrimDataOffset;       // _20
	BE<u32> mMatrixInitDataOffset; // _24
	BE<u32> mMtxGroupTableOffset;  // _28
};

struct J3DTextureBlock : J3DFileBlockBase {
	BE<u16> mTextureCount;      // _08
	BEOffset mTexHeaderOffset; // _0C
	BEOffset mTexNameOffset;   // _10
};

struct J3DVertexBlock : J3DFileBlockBase {
	BEOffset mVertexFormatOffset;    // _08
	BEOffset mPositionDataOffset;    // _0C
	BEOffset mNormalDataOffset;      // _10
	BEOffset mNBTDataOffset;         // _14
	BEOffset mColorDataOffset[2];    // _18
	BEOffset mTexCoordDataOffset[8]; // _20
};

#endif
