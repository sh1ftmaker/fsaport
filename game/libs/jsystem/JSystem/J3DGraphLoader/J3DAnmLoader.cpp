#include "JSystem/J3D/J3DAnmLoader.h"
#include "JSystem/J3D/J3DAnmBase.h"
#include "JSystem/J3D/J3DAnmCluster.h"
#include "JSystem/J3D/J3DAnmColor.h"
#include "JSystem/J3D/J3DAnmTevRegKey.h"
#include "JSystem/J3D/J3DAnmTexPattern.h"
#include "JSystem/J3D/J3DAnmTextureSRTKey.h"
#include "JSystem/J3D/J3DAnmTransform.h"
#include "JSystem/J3D/J3DAnmVisibilityFull.h"
#include "JSystem/J3D/J3DAnmVtxColor.h"
#include "JSystem/J3D/J3DFileBlock.h"
#include "JSystem/J3D/J3DMaterial.h"
#include "JSystem/JGeometry.h"
#include "JSystem/JSupport.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "types.h"

#ifdef FSA_PORT
#define offsetof(t, m) __builtin_offsetof(t, m)
// ---------------------------------------------------------------------------
// Port: J3D animation files (BCK, BCA, BPK, BPA, BRK, BTK, BTP, BVA, BLK, BLA,
// vertex colour) are big-endian. The loaders keep pointers into their tables and
// value arrays, which the runtime reads every frame, so each block's header
// fields and arrays are swapped to host order once, in place, before the first
// loader reads them (as J3DPortSwapModelFile does for models). Name tables
// (ResNTAB) stay big-endian by type, byte arrays need nothing. A marker in the
// file header padding keeps a file from being swapped twice.
#include "J3DPortSwapTables.h"

namespace {

// PW_REGTAB: J3DAnmCRegKeyTable / J3DAnmKRegKeyTable entries (0x1C: four
// 6-byte key table bases, then the u8 register id and 3 pad bytes, which
// must stay put: swapped as u16s the id became the 0xFF pad and the TEV
// register animations indexed mTevColorAnm[255] / mTevKColorAnm[255]).
enum { PW_BYTES = 0, PW_NAMES = -1, PW_VTXIDX = -2, PW_REGTAB = -3 };
struct PortAnmTable {
	u16 mField; // offset of the s32 offset in the block header
	s8 mWidth;  // element width to swap: 2, 4, PW_BYTES, PW_NAMES, PW_VTXIDX, PW_REGTAB
};

#define T(type, field, width) { (u16)offsetof(type, field), width }
const PortAnmTable kTransformFull[] = { T(J3DAnmTransformFullData, mTableOffset, 2), T(J3DAnmTransformFullData, mScaleValOffset, 4),
	                                     T(J3DAnmTransformFullData, mRotValOffset, 2), T(J3DAnmTransformFullData, mTransValOffset, 4) };
const PortAnmTable kTransformKey[] = { T(J3DAnmTransformKeyData, mTableOffset, 2), T(J3DAnmTransformKeyData, mScaleOffset, 4),
	                                    T(J3DAnmTransformKeyData, mRotationOffset, 2), T(J3DAnmTransformKeyData, mTranslateOffset, 4) };
const PortAnmTable kColorFull[] = { T(J3DAnmColorFullData, mTableOffset, 2), T(J3DAnmColorFullData, mUpdateMaterialIDOffset, 2),
	                                 T(J3DAnmColorFullData, mNameTabOffset, PW_NAMES), T(J3DAnmColorFullData, mRValuesOffset, PW_BYTES),
	                                 T(J3DAnmColorFullData, mGValuesOffset, PW_BYTES), T(J3DAnmColorFullData, mBValuesOffset, PW_BYTES),
	                                 T(J3DAnmColorFullData, mAValuesOffset, PW_BYTES) };
const PortAnmTable kColorKey[] = { T(J3DAnmColorKeyData, mTableOffset, 2), T(J3DAnmColorKeyData, mUpdateMaterialIDOffset, 2),
	                                T(J3DAnmColorKeyData, mNameTabOffset, PW_NAMES), T(J3DAnmColorKeyData, mRValOffset, 2),
	                                T(J3DAnmColorKeyData, mGValOffset, 2), T(J3DAnmColorKeyData, mBValOffset, 2),
	                                T(J3DAnmColorKeyData, mAValOffset, 2) };
const PortAnmTable kTevRegKey[] = { T(J3DAnmTevRegKeyData, mCRegTableOffset, PW_REGTAB), T(J3DAnmTevRegKeyData, mKRegTableOffset, PW_REGTAB),
	                                 T(J3DAnmTevRegKeyData, mCRegUpdateMaterialIDOffset, 2),
	                                 T(J3DAnmTevRegKeyData, mKRegUpdateMaterialIDOffset, 2),
	                                 T(J3DAnmTevRegKeyData, mCRegNameTabOffset, PW_NAMES),
	                                 T(J3DAnmTevRegKeyData, mKRegNameTabOffset, PW_NAMES), T(J3DAnmTevRegKeyData, mCRValuesOffset, 2),
	                                 T(J3DAnmTevRegKeyData, mCGValuesOffset, 2), T(J3DAnmTevRegKeyData, mCBValuesOffset, 2),
	                                 T(J3DAnmTevRegKeyData, mCAValuesOffset, 2), T(J3DAnmTevRegKeyData, mKRValuesOffset, 2),
	                                 T(J3DAnmTevRegKeyData, mKGValuesOffset, 2), T(J3DAnmTevRegKeyData, mKBValuesOffset, 2),
	                                 T(J3DAnmTevRegKeyData, mKAValuesOffset, 2) };
const PortAnmTable kTexPatternFull[] = { T(J3DAnmTexPatternFullData, mTableOffset, 2), T(J3DAnmTexPatternFullData, mValuesOffset, 2),
	                                      T(J3DAnmTexPatternFullData, mUpdateMaterialIDOffset, 2),
	                                      T(J3DAnmTexPatternFullData, mNameTabOffset, PW_NAMES) };
const PortAnmTable kVisibilityFull[] = { T(J3DAnmVisibilityFullData, mTableOffset, 2),
	                                      T(J3DAnmVisibilityFullData, mValuesOffset, PW_BYTES) };
const PortAnmTable kClusterFull[] = { T(J3DAnmClusterFullData, mTablesOffset, 2), T(J3DAnmClusterFullData, mWeightsOffset, 4) };
const PortAnmTable kClusterKey[] = { T(J3DAnmClusterKeyData, mTableOffset, 2), T(J3DAnmClusterKeyData, mWeightsOffset, 4) };
const PortAnmTable kVtxColorFull[] = { T(J3DAnmVtxColorFullData, mTableOffsets[0], 2), T(J3DAnmVtxColorFullData, mTableOffsets[1], 2),
	                                    T(J3DAnmVtxColorFullData, mVtxColorIndexDataOffsets[0], PW_VTXIDX),
	                                    T(J3DAnmVtxColorFullData, mVtxColorIndexDataOffsets[1], PW_VTXIDX),
	                                    T(J3DAnmVtxColorFullData, mVtxColorIndexPointerOffsets[0], 2),
	                                    T(J3DAnmVtxColorFullData, mVtxColorIndexPointerOffsets[1], 2),
	                                    T(J3DAnmVtxColorFullData, mRValuesOffset, PW_BYTES), T(J3DAnmVtxColorFullData, mGValuesOffset, PW_BYTES),
	                                    T(J3DAnmVtxColorFullData, mBValuesOffset, PW_BYTES), T(J3DAnmVtxColorFullData, mAValuesOffset, PW_BYTES) };
const PortAnmTable kVtxColorKey[] = { T(J3DAnmVtxColorKeyData, mTableOffsets[0], 2), T(J3DAnmVtxColorKeyData, mTableOffsets[1], 2),
	                                   T(J3DAnmVtxColorKeyData, mVtxColoIndexDataOffset[0], PW_VTXIDX),
	                                   T(J3DAnmVtxColorKeyData, mVtxColoIndexDataOffset[1], PW_VTXIDX),
	                                   T(J3DAnmVtxColorKeyData, mVtxColorIndexPointerOffset[0], 2),
	                                   T(J3DAnmVtxColorKeyData, mVtxColorIndexPointerOffset[1], 2),
	                                   T(J3DAnmVtxColorKeyData, mRValOffset, 2), T(J3DAnmVtxColorKeyData, mGValOffset, 2),
	                                   T(J3DAnmVtxColorKeyData, mBValOffset, 2), T(J3DAnmVtxColorKeyData, mAValOffset, 2) };
// The post-transform set (_50.._58 are its scale, rotation and translation values, as in tp).
const PortAnmTable kTextureSRTKey[] = { T(J3DAnmTextureSRTKeyData, mTableOffset, 2), T(J3DAnmTextureSRTKeyData, mUpdateMatIDOffset, 2),
	                                     T(J3DAnmTextureSRTKeyData, mNameTab1Offset, PW_NAMES),
	                                     T(J3DAnmTextureSRTKeyData, mUpdateTexMtxIDOffset, PW_BYTES),
	                                     T(J3DAnmTextureSRTKeyData, mSrtCenterOffset, 4), T(J3DAnmTextureSRTKeyData, mScaleValOffset, 4),
	                                     T(J3DAnmTextureSRTKeyData, mRotValOffset, 2), T(J3DAnmTextureSRTKeyData, mTransValOffset, 4),
	                                     T(J3DAnmTextureSRTKeyData, mTransformKeyTableOffset, 2),
	                                     T(J3DAnmTextureSRTKeyData, mPostUpdateMaterialIDOffset, 2),
	                                     T(J3DAnmTextureSRTKeyData, mNameTab2Offset, PW_NAMES),
	                                     T(J3DAnmTextureSRTKeyData, mPostUpdateTexMtxIDOffset, PW_BYTES),
	                                     T(J3DAnmTextureSRTKeyData, mPostSRTCenterOffset, 4), T(J3DAnmTextureSRTKeyData, _50, 4),
	                                     T(J3DAnmTextureSRTKeyData, _54, 2), T(J3DAnmTextureSRTKeyData, _58, 4) };
#undef T

void portSwap(u8* p, u32 size)
{
	for (u32 i = 0; i < size / 2; i++) {
		u8 t = p[i];
		p[i] = p[size - 1 - i];
		p[size - 1 - i] = t;
	}
}

void portSwapStruct(u8* p, const unsigned short* table, u32 count)
{
	for (u32 i = 0; i < count; i++)
		portSwap(p + table[2 * i], table[2 * i + 1]);
}

void portSwapAnmBlock(u8* blk, const unsigned short* header, u32 headerCount, const PortAnmTable* tables, u32 tableCount,
                      const u8* fileEnd)
{
	u32 size = (u32)((J3DFileBlockBase*)blk)->mSize;
	// A block's mSize can run past the end of the file (the last table then
	// swapped the first bytes of whatever follows the file in memory: in an
	// archive, the next file's header, e.g. yami_yougan.bmd's 'J3D2', which
	// then failed to load).
	if (blk + size > fileEnd)
		size = fileEnd > blk ? (u32)(fileEnd - blk) : 0;
	portSwapStruct(blk, header, headerCount); // now the offsets are host order
	for (u32 t = 0; t < tableCount; t++) {
		u32 off = *(u32*)(blk + tables[t].mField);
		if (off == 0 || off >= size)
			continue;
		u32 end = size; // up to the next table in the block
		for (u32 k = 0; k < tableCount; k++) {
			u32 o = *(u32*)(blk + tables[k].mField);
			if (o > off && o < end)
				end = o;
		}
		u8* p = blk + off;
		switch (tables[t].mWidth) {
		case 2:
		case 4:
			for (u32 o = 0; o + tables[t].mWidth <= end - off; o += tables[t].mWidth)
				portSwap(p + o, tables[t].mWidth);
			break;
		case PW_REGTAB:
			for (u32 o = 0; o + sizeof(J3DAnmCRegKeyTable) <= end - off; o += sizeof(J3DAnmCRegKeyTable))
				for (u32 k = 0; k < offsetof(J3DAnmCRegKeyTable, _18); k += 2)
					portSwap(p + o + k, 2);
			break;
		case PW_VTXIDX:
			for (u32 o = 0; o + kPortSwapSize_J3DAnmVtxColorIndexData <= end - off; o += kPortSwapSize_J3DAnmVtxColorIndexData)
				portSwapStruct(p + o, kPortSwap_J3DAnmVtxColorIndexData, kPortSwapCount_J3DAnmVtxColorIndexData);
			break;
		default: // bytes, or a name table (big-endian by type)
			break;
		}
	}
}

} // namespace

#define PORT_ANM(T, tables) \
	portSwapAnmBlock(blk, kPortSwap_##T, kPortSwapCount_##T, tables, sizeof(tables) / sizeof(tables[0]), fileEnd)

void J3DPortSwapAnmFile(const void* stream)
{
	if (stream == nullptr)
		return;
	J3DFileHeader* header = (J3DFileHeader*)stream;
	if (header->mJ3dVersion != 'J3D1' && header->mJ3dVersion != 'J3D2')
		return;
	u8* mark = header->_10;
	if (mark[0] == 'P' && mark[1] == 'O' && mark[2] == 'R' && mark[3] == 'T')
		return; // already in host order
	const u8* fileEnd = (const u8*)stream + ((u32)header->_08[0] << 24 | (u32)header->_08[1] << 16 |
	                                         (u32)header->_08[2] << 8 | header->_08[3]); // file size, big-endian
	const J3DFileBlockBase* next = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		u8* blk = (u8*)next;
		next = next->getNext(); // read before the block changes
		switch ((u32)((J3DFileBlockBase*)blk)->mBlockType) {
		case J3DFBT_AnmTransformFull:  PORT_ANM(J3DAnmTransformFullData, kTransformFull); break;
		case J3DFBT_AnmTransformKey:   PORT_ANM(J3DAnmTransformKeyData, kTransformKey); break;
		case J3DFBT_AnmColorFull:      PORT_ANM(J3DAnmColorFullData, kColorFull); break;
		case J3DFBT_AnmColorKey:       PORT_ANM(J3DAnmColorKeyData, kColorKey); break;
		case J3DFBT_AnmTevRegKey:      PORT_ANM(J3DAnmTevRegKeyData, kTevRegKey); break;
		case J3DFBT_AnmTexPattern:     PORT_ANM(J3DAnmTexPatternFullData, kTexPatternFull); break;
		case J3DFBT_AnmVisibilityFull: PORT_ANM(J3DAnmVisibilityFullData, kVisibilityFull); break;
		case J3DFBT_AnmClusterFull:    PORT_ANM(J3DAnmClusterFullData, kClusterFull); break;
		case J3DFBT_AnmClusterKey:     PORT_ANM(J3DAnmClusterKeyData, kClusterKey); break;
		case J3DFBT_AnmVtxColorFull:   PORT_ANM(J3DAnmVtxColorFullData, kVtxColorFull); break;
		case J3DFBT_AnmVtxColorKey:    PORT_ANM(J3DAnmVtxColorKeyData, kVtxColorKey); break;
		case J3DFBT_AnmTextureSRTKey:  PORT_ANM(J3DAnmTextureSRTKeyData, kTextureSRTKey); break;
		default: break;
		}
	}
	mark[0] = 'P';
	mark[1] = 'O';
	mark[2] = 'R';
	mark[3] = 'T';
}
#endif

/**
 * @note Address: 0x800725A0
 * @note Size: 0x8C4
 */
J3DAnmBase* J3DAnmLoaderDataBase::load(const void* stream, J3DAnmLoaderDataBaseFlag flag)
{
#ifdef FSA_PORT
	J3DPortSwapAnmFile(stream); // port: host-order tables, once per file
#endif
	if (stream == nullptr) {
		return nullptr;
	}
	J3DFileHeader* header = (J3DFileHeader*)stream;
	if (header->mJ3dVersion == 'J3D1') {
		switch (header->mFileVersion) {
		case 'bck1': {
			J3DAnmKeyLoader_v15 loader;
			loader.mAnimation = new J3DAnmTransformKey();
			return loader.load(stream);
		} break;
		case 'bpk1': {
			J3DAnmKeyLoader_v15 loader;
			loader.mAnimation = new J3DAnmColorKey();
			return loader.load(stream);
		} break;
		case 'blk1': {
			J3DAnmKeyLoader_v15 loader;
			loader.mAnimation = new J3DAnmClusterKey();
			return loader.load(stream);
		} break;
		case 'btk1': {
			J3DAnmKeyLoader_v15 loader;
			loader.mAnimation = new J3DAnmTextureSRTKey();
			return loader.load(stream);
		} break;
		case 'brk1': {
			J3DAnmKeyLoader_v15 loader;
			loader.mAnimation = new J3DAnmTevRegKey();
			return loader.load(stream);
		} break;
		case 'bxk1': {
			J3DAnmKeyLoader_v15 loader;
			loader.mAnimation = new J3DAnmVtxColorKey();
			return loader.load(stream);
		} break;
		case 'bca1': {
			J3DAnmFullLoader_v15 loader;
			if (flag & J3DLOADER_UNK_FLAG1) {
				loader.mAnimation = new J3DAnmTransformFullWithLerp();
			} else {
				loader.mAnimation = new J3DAnmTransformFull();
			}
			return loader.load(stream);
		} break;
		case 'bpa1': {
			J3DAnmFullLoader_v15 loader;
			loader.mAnimation = new J3DAnmColorFull();
			return loader.load(stream);
		} break;
		case 'btp1': {
			J3DAnmFullLoader_v15 loader;
			loader.mAnimation = new J3DAnmTexPattern();
			return loader.load(stream);
		} break;
		case 'bva1': {
			J3DAnmFullLoader_v15 loader;
			loader.mAnimation = new J3DAnmVisibilityFull();
			return loader.load(stream);
		} break;
		case 'bla1': {
			J3DAnmFullLoader_v15 loader;
			loader.mAnimation = new J3DAnmClusterFull();
			return loader.load(stream);
		} break;
		case 'bxa1': {
			J3DAnmFullLoader_v15 loader;
			loader.mAnimation = new J3DAnmVtxColorFull();
			return loader.load(stream);
		} break;
		default:
			return nullptr;
		}
	}
	return nullptr;
}

/**
 * @note Address: 0x80072E64
 * @note Size: 0x1C
 * __ct__20J3DAnmFullLoader_v15Fv
 */
J3DAnmFullLoader_v15::J3DAnmFullLoader_v15()
{
}

/**
 * @note Address: 0x80072E80
 * @note Size: 0x48
 * __dt__12J3DAnmLoaderFv
 */
J3DAnmLoader::~J3DAnmLoader()
{
}

/**
 * @note Address: 0x80072EC8
 * @note Size: 0x5C
 * __dt__20J3DAnmFullLoader_v15Fv
 */
J3DAnmFullLoader_v15::~J3DAnmFullLoader_v15()
{
}

/**
 * @note Address: 0x80072F24
 * @note Size: 0x1C
 * __ct__19J3DAnmKeyLoader_v15Fv
 */
J3DAnmKeyLoader_v15::J3DAnmKeyLoader_v15()
{
}

/**
 * @note Address: 0x80072F40
 * @note Size: 0x5C
 * __dt__19J3DAnmKeyLoader_v15Fv
 */
J3DAnmKeyLoader_v15::~J3DAnmKeyLoader_v15()
{
}

/**
 * @note Address: 0x80072F9C
 * @note Size: 0x128
 * load__20J3DAnmFullLoader_v15FPCv
 */
J3DAnmBase* J3DAnmFullLoader_v15::load(const void* stream)
{
#ifdef FSA_PORT
	J3DPortSwapAnmFile(stream); // port: host-order tables, once per file
#endif
	const J3DFileHeader* header = reinterpret_cast<const J3DFileHeader*>(stream);
	const J3DFileBlockBase* nextBlock = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_AnmTransformFull:
			readAnmTransform((const J3DAnmTransformFullData*)nextBlock);
			break;
		case J3DFBT_AnmColorFull:
			readAnmColor((const J3DAnmColorFullData*)nextBlock);
			break;
		case J3DFBT_AnmTexPattern:
			readAnmTexPattern((const J3DAnmTexPatternFullData*)nextBlock);
			break;
		case J3DFBT_AnmClusterFull:
			readAnmCluster((const J3DAnmClusterFullData*)nextBlock);
			break;
		case J3DFBT_AnmVisibilityFull:
			readAnmVisibility((const J3DAnmVisibilityFullData*)nextBlock);
			break;
		case J3DFBT_AnmVtxColorFull:
			readAnmVtxColor((const J3DAnmVtxColorFullData*)nextBlock);
			break;
		}
		nextBlock = nextBlock->getNext();
	}
	return mAnimation;
}

/**
 * @note Address: 0x800730C4
 * @note Size: 0x140
 * setResource__20J3DAnmFullLoader_v15FP10J3DAnmBasePCv
 */
void J3DAnmFullLoader_v15::setResource(J3DAnmBase* resource, const void* stream)
{
#ifdef FSA_PORT
	J3DPortSwapAnmFile(stream); // port: host-order tables, once per file
#endif
	const J3DFileHeader* header       = reinterpret_cast<const J3DFileHeader*>(stream);
	const J3DFileBlockBase* nextBlock = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_AnmTransformFull:
			setAnmTransform((J3DAnmTransformFull*)resource, (const J3DAnmTransformFullData*)nextBlock);
			break;
		case J3DFBT_AnmColorFull:
			setAnmColor((J3DAnmColorFull*)resource, (const J3DAnmColorFullData*)nextBlock);
			break;
		case J3DFBT_AnmTexPattern:
			setAnmTexPattern((J3DAnmTexPattern*)resource, (const J3DAnmTexPatternFullData*)nextBlock);
			break;
		case J3DFBT_AnmClusterFull:
			setAnmCluster((J3DAnmClusterFull*)resource, (const J3DAnmClusterFullData*)nextBlock);
			break;
		case J3DFBT_AnmVisibilityFull:
			setAnmVisibility((J3DAnmVisibilityFull*)resource, (const J3DAnmVisibilityFullData*)nextBlock);
			break;
		case J3DFBT_AnmVtxColorFull:
			setAnmVtxColor((J3DAnmVtxColorFull*)resource, (const J3DAnmVtxColorFullData*)nextBlock);
			break;
		}
		nextBlock = nextBlock->getNext();
	}
}

/**
 * @note Address: 0x80073204
 * @note Size: 0x28
 */
void J3DAnmFullLoader_v15::readAnmTransform(const J3DAnmTransformFullData* data)
{
	setAnmTransform((J3DAnmTransformFull*)mAnimation, data);
}

/**
 * @note Address: 0x8007322C
 * @note Size: 0x94
 */
void J3DAnmFullLoader_v15::setAnmTransform(J3DAnmTransformFull* animation, const J3DAnmTransformFullData* data)
{
	animation->mUpdateMaterialNum = data->mExtraInfo1;
	animation->mTotalFrameCount   = data->mTotalFrameCount;
	animation->mAttribute         = data->mAttribute;
	animation->mCurrentFrame      = 0.0f;

	animation->mTable           = JSUConvertOffsetToPtr<J3DAnmTransformFullTable>(data, (void*)data->mTableOffset);
	animation->mScaleVals       = JSUConvertOffsetToPtr<f32>(data, (void*)data->mScaleValOffset);
	animation->mRotationVals    = JSUConvertOffsetToPtr<s16>(data, (void*)data->mRotValOffset);
	animation->mTranslationVals = JSUConvertOffsetToPtr<f32>(data, (void*)data->mTransValOffset);
}

/**
 * @note Address: 0x800732C0
 * @note Size: 0x28
 */
void J3DAnmFullLoader_v15::readAnmColor(const J3DAnmColorFullData* data)
{
	setAnmColor((J3DAnmColorFull*)mAnimation, data);
}

/**
 * @note Address: 0x800732E8
 * @note Size: 0xCC
 */
void J3DAnmFullLoader_v15::setAnmColor(J3DAnmColorFull* animation, const J3DAnmColorFullData* data)
{
	animation->mTotalFrameCount   = data->mFrameMax;
	animation->mAttribute         = data->_08;
	animation->mCurrentFrame      = 0.0f;
	animation->mUpdateMaterialNum = data->mUpdateMaterialNum;

	animation->mTable            = JSUConvertOffsetToPtr<J3DAnmColorFullTable>(data, (void*)data->mTableOffset);
	animation->mRedVals          = JSUConvertOffsetToPtr<u8>(data, (void*)data->mRValuesOffset);
	animation->mGreenVals        = JSUConvertOffsetToPtr<u8>(data, (void*)data->mGValuesOffset);
	animation->mBlueVals         = JSUConvertOffsetToPtr<u8>(data, (void*)data->mBValuesOffset);
	animation->mAlphaVals        = JSUConvertOffsetToPtr<u8>(data, (void*)data->mAValuesOffset);
	animation->mUpdateMaterialID = JSUConvertOffsetToPtr<u16>(data, (void*)data->mUpdateMaterialIDOffset);
	animation->mNameTab.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mNameTabOffset));
}

/**
 * @note Address: 0x800733B4
 * @note Size: 0x28
 */
void J3DAnmFullLoader_v15::readAnmTexPattern(const J3DAnmTexPatternFullData* data)
{
	setAnmTexPattern((J3DAnmTexPattern*)mAnimation, data);
}

/**
 * @note Address: 0x800733DC
 * @note Size: 0xA4
 */
void J3DAnmFullLoader_v15::setAnmTexPattern(J3DAnmTexPattern* animation, const J3DAnmTexPatternFullData* data)
{
	animation->mTotalFrameCount   = data->mTotalFrameCount;
	animation->mAttribute         = data->mAttribute;
	animation->mCurrentFrame      = 0.0f;
	animation->mUpdateMaterialNum = data->mExtraInfo1;
	animation->_14                = data->mExtraInfo2;

	animation->mAnmTable         = JSUConvertOffsetToPtr<J3DAnmTexPatternFullTable>(data, (void*)data->mTableOffset);
	animation->_0C               = JSUConvertOffsetToPtr<u16>(data, (void*)data->mValuesOffset);
	animation->mUpdateMaterialID = JSUConvertOffsetToPtr<u16>(data, (void*)data->mUpdateMaterialIDOffset);
	animation->mNameTab.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mNameTabOffset));
}

/**
 * @note Address: 0x80073480
 * @note Size: 0x28
 */
void J3DAnmFullLoader_v15::readAnmVisibility(const J3DAnmVisibilityFullData* data)
{
	setAnmVisibility((J3DAnmVisibilityFull*)mAnimation, data);
}

/**
 * @note Address: 0x800734A8
 * @note Size: 0x7C
 */
void J3DAnmFullLoader_v15::setAnmVisibility(J3DAnmVisibilityFull* animation, const J3DAnmVisibilityFullData* data)
{
	animation->mTotalFrameCount = data->mTotalFrameCount;
	animation->mAttribute       = data->mAttribute;
	animation->mCurrentFrame    = 0.0f;
	animation->_0C              = data->mExtraInfo1;
	animation->_0E              = data->mExtraInfo2;

	animation->mTable  = JSUConvertOffsetToPtr<J3DAnmVisibilityFullTable>(data, (void*)data->mTableOffset);
	animation->mValues = JSUConvertOffsetToPtr<u8>(data, (void*)data->mValuesOffset);
}

/**
 * @note Address: 0x80073524
 * @note Size: 0x28
 */
void J3DAnmFullLoader_v15::readAnmCluster(const J3DAnmClusterFullData* data)
{
	setAnmCluster((J3DAnmClusterFull*)mAnimation, data);
}

/**
 * @note Address: 0x8007354C
 * @note Size: 0x6C
 */
void J3DAnmFullLoader_v15::setAnmCluster(J3DAnmClusterFull* animation, const J3DAnmClusterFullData* data)
{
	animation->mTotalFrameCount = data->mTotalFrameCount;
	animation->mAttribute       = data->mAttribute;
	animation->mCurrentFrame    = 0.0f;

	animation->mTables  = JSUConvertOffsetToPtr<J3DAnmClusterFullTable>(data, data->mTablesOffset);
	animation->mWeights = JSUConvertOffsetToPtr<f32>(data, data->mWeightsOffset);
}

/**
 * @note Address: 0x800735B8
 * @note Size: 0x28
 */
void J3DAnmFullLoader_v15::readAnmVtxColor(const J3DAnmVtxColorFullData* data)
{
	setAnmVtxColor((J3DAnmVtxColorFull*)mAnimation, data);
}

/**
 * @note Address: 0x800735E0
 * @note Size: 0x170
 * setAnmVtxColor__20J3DAnmFullLoader_v15FP18J3DAnmVtxColorFullPC22J3DAnmVtxColorFullData
 */
void J3DAnmFullLoader_v15::setAnmVtxColor(J3DAnmVtxColorFull* animation, const J3DAnmVtxColorFullData* data)
{
	animation->mTotalFrameCount = data->mTotalFrameCount;
	animation->mAttribute       = data->mAttribute;
	animation->mCurrentFrame    = 0.0f;
	animation->mAnmTableNum[0]  = data->mExtraInfo1;
	animation->mAnmTableNum[1]  = data->mExtraInfo2;

	animation->mTable[0]                = JSUConvertOffsetToPtr<J3DAnmColorFullTable>(data, (void*)data->mTableOffsets[0]);
	animation->mTable[1]                = JSUConvertOffsetToPtr<J3DAnmColorFullTable>(data, (void*)data->mTableOffsets[1]);
	animation->mAnmVtxColorIndexData[0] = JSUConvertOffsetToPtr<J3DAnmVtxColorIndexData>(data, (void*)data->mVtxColorIndexDataOffsets[0]);
	animation->mAnmVtxColorIndexData[1] = JSUConvertOffsetToPtr<J3DAnmVtxColorIndexData>(data, (void*)data->mVtxColorIndexDataOffsets[1]);
	u16* v1                             = JSUConvertOffsetToPtr<u16>(data, (void*)data->mVtxColorIndexPointerOffsets[0]);
	u16* v2                             = JSUConvertOffsetToPtr<u16>(data, (void*)data->mVtxColorIndexPointerOffsets[1]);

	for (int i = 0; i < animation->mAnmTableNum[0]; i++) {
		animation->mAnmVtxColorIndexData[0][i].mData = (void*)(v1 + (u32)animation->mAnmVtxColorIndexData[0][i].mData);
	}
	for (int i = 0; i < animation->mAnmTableNum[1]; i++) {
		animation->mAnmVtxColorIndexData[1][i].mData = (void*)(v2 + (u32)animation->mAnmVtxColorIndexData[1][i].mData);
	}

	animation->mRedVals   = JSUConvertOffsetToPtr<u8>(data, (void*)data->mRValuesOffset);
	animation->mGreenVals = JSUConvertOffsetToPtr<u8>(data, (void*)data->mGValuesOffset);
	animation->mBlueVals  = JSUConvertOffsetToPtr<u8>(data, (void*)data->mBValuesOffset);
	animation->mAlphaVals = JSUConvertOffsetToPtr<u8>(data, (void*)data->mAValuesOffset);
}

/**
 * @note Address: 0x80073750
 * @note Size: 0x128
 * load__19J3DAnmKeyLoader_v15FPCv
 */
J3DAnmBase* J3DAnmKeyLoader_v15::load(const void* stream)
{
#ifdef FSA_PORT
	J3DPortSwapAnmFile(stream); // port: host-order tables, once per file
#endif
	const J3DFileHeader* header = reinterpret_cast<const J3DFileHeader*>(stream);
	const J3DFileBlockBase* nextBlock = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_AnmTransformKey:
			readAnmTransform((const J3DAnmTransformKeyData*)nextBlock);
			break;
		case J3DFBT_AnmColorKey:
			readAnmColor((const J3DAnmColorKeyData*)nextBlock);
			break;
		case J3DFBT_AnmClusterKey:
			readAnmCluster((const J3DAnmClusterKeyData*)nextBlock);
			break;
		case J3DFBT_AnmTextureSRTKey:
			readAnmTextureSRT((const J3DAnmTextureSRTKeyData*)nextBlock);
			break;
		case J3DFBT_AnmTevRegKey:
			readAnmTevReg((const J3DAnmTevRegKeyData*)nextBlock);
			break;
		case J3DFBT_AnmVtxColorKey:
			readAnmVtxColor((const J3DAnmVtxColorKeyData*)nextBlock);
			break;
		}
		nextBlock = nextBlock->getNext();
	}
	return mAnimation;
}

/**
 * @note Address: 0x80073878
 * @note Size: 0x140
 * setResource__19J3DAnmKeyLoader_v15FP10J3DAnmBasePCv
 */
void J3DAnmKeyLoader_v15::setResource(J3DAnmBase* resource, const void* stream)
{
#ifdef FSA_PORT
	J3DPortSwapAnmFile(stream); // port: host-order tables, once per file
#endif
	const J3DFileHeader* header       = reinterpret_cast<const J3DFileHeader*>(stream);
	const J3DFileBlockBase* nextBlock = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_AnmTransformKey:
			setAnmTransform((J3DAnmTransformKey*)resource, (const J3DAnmTransformKeyData*)nextBlock);
			break;
		case J3DFBT_AnmColorKey:
			setAnmColor((J3DAnmColorKey*)resource, (const J3DAnmColorKeyData*)nextBlock);
			break;
		case J3DFBT_AnmClusterKey:
			setAnmCluster((J3DAnmClusterKey*)resource, (const J3DAnmClusterKeyData*)nextBlock);
			break;
		case J3DFBT_AnmTextureSRTKey:
			setAnmTextureSRT((J3DAnmTextureSRTKey*)resource, (const J3DAnmTextureSRTKeyData*)nextBlock);
			break;
		case J3DFBT_AnmTevRegKey:
			setAnmTevReg((J3DAnmTevRegKey*)resource, (const J3DAnmTevRegKeyData*)nextBlock);
			break;
		case J3DFBT_AnmVtxColorKey:
			setAnmVtxColor((J3DAnmVtxColorKey*)resource, (const J3DAnmVtxColorKeyData*)nextBlock);
			break;
		}
		nextBlock = nextBlock->getNext();
	}
}

/**
 * @note Address: 0x800739B8
 * @note Size: 0x28
 * readAnmTransform__19J3DAnmKeyLoader_v15FPC22J3DAnmTransformKeyData
 */
void J3DAnmKeyLoader_v15::readAnmTransform(const J3DAnmTransformKeyData* data)
{
	setAnmTransform((J3DAnmTransformKey*)mAnimation, data);
}

/**
 * @note Address: 0x800739E0
 * @note Size: 0x9C
 * setAnmTransform__19J3DAnmKeyLoader_v15FP18J3DAnmTransformKeyPC22J3DAnmTransformKeyData
 */
void J3DAnmKeyLoader_v15::setAnmTransform(J3DAnmTransformKey* animation, const J3DAnmTransformKeyData* data)
{
	animation->mUpdateMaterialNum = data->mExtraInfo1;
	animation->mTotalFrameCount   = data->mTotalFrameCount;
	animation->mAttribute         = data->mAttribute;
	animation->_20                = data->mRotationScale;
	animation->mCurrentFrame      = 0.0f;

	animation->mTable           = JSUConvertOffsetToPtr<J3DAnmTransformKeyTable>(data, (void*)data->mTableOffset);
	animation->mScaleVals       = JSUConvertOffsetToPtr<f32>(data, (void*)data->mScaleOffset);
	animation->mRotationVals    = JSUConvertOffsetToPtr<s16>(data, (void*)data->mRotationOffset);
	animation->mTranslationVals = JSUConvertOffsetToPtr<f32>(data, (void*)data->mTranslateOffset);
}

/**
 * @note Address: 0x80073A7C
 * @note Size: 0x28
 * readAnmTextureSRT__19J3DAnmKeyLoader_v15FPC23J3DAnmTextureSRTKeyData
 */
void J3DAnmKeyLoader_v15::readAnmTextureSRT(const J3DAnmTextureSRTKeyData* data)
{
	setAnmTextureSRT((J3DAnmTextureSRTKey*)mAnimation, data);
}

/**
 * @note Address: 0x80073AA4
 * @note Size: 0x1D8
 * setAnmTextureSRT__19J3DAnmKeyLoader_v15FP19J3DAnmTextureSRTKeyPC23J3DAnmTextureSRTKeyData
 */
void J3DAnmKeyLoader_v15::setAnmTextureSRT(J3DAnmTextureSRTKey* animation, const J3DAnmTextureSRTKeyData* data)
{
	animation->mTrackNum        = data->mExtraInfo1;
	animation->mTotalFrameCount = data->mTotalFrameCount;
	animation->mAttribute       = data->mAttribute;
	animation->mRotationScale   = data->mRotationScale;
	animation->mCurrentFrame    = 0.0f;
	animation->mTrackNum        = data->mExtraInfo1;
	animation->mScaleNum        = data->mExtraInfo2;
	animation->mRotNum          = data->mRotationNum;
	animation->mTransNum        = data->mTranslationNum;

	animation->mTable1           = JSUConvertOffsetToPtr<J3DAnmTransformKeyTable>(data, (void*)data->mTableOffset);
	animation->mUpdateMaterialID = JSUConvertOffsetToPtr<u16>(data, (void*)data->mUpdateMatIDOffset);
	animation->mUpdateMaterialName.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mNameTab1Offset));
	animation->mUpdateTexMtxID   = JSUConvertOffsetToPtr<u8>(data, (void*)data->mUpdateTexMtxIDOffset);
	animation->mSRTCenter        = JSUConvertOffsetToPtr<Vec>(data, (void*)data->mSrtCenterOffset);
	animation->mScale1Vals       = JSUConvertOffsetToPtr<f32>(data, (void*)data->mScaleValOffset);
	animation->mRotation1Vals    = JSUConvertOffsetToPtr<s16>(data, (void*)data->mRotValOffset);
	animation->mTranslation1Vals = JSUConvertOffsetToPtr<f32>(data, (void*)data->mTransValOffset);
	if (data->mNameTab2Offset != nullptr) {
		animation->mPostUpdateMaterialName.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mNameTab2Offset));
	}
	animation->mPostTrackNum         = data->mPostTrackNum;
	animation->_44                   = data->_36;
	animation->_46                   = data->_38;
	animation->_48                   = data->_3A;
	animation->mTransformKeyTable    = JSUConvertOffsetToPtr<J3DAnmTransformKeyTable>(data, (void*)data->mTransformKeyTableOffset);
	animation->mPostUpdateMaterialID = JSUConvertOffsetToPtr<u16>(data, (void*)data->mPostUpdateMaterialIDOffset);
	animation->mPostUpdateTexMtxID   = JSUConvertOffsetToPtr<u8>(data, (void*)data->mPostUpdateTexMtxIDOffset);
	animation->mPostSRTCenter        = JSUConvertOffsetToPtr<Vec>(data, (void*)data->mPostSRTCenterOffset);
	animation->_4C                   = JSUConvertOffsetToPtr<f32>(data, (void*)data->_50);
	animation->_50                   = JSUConvertOffsetToPtr<s16>(data, (void*)data->_54);
	animation->_54                   = JSUConvertOffsetToPtr<f32>(data, (void*)data->_58);

	switch (data->mTexMtxCalcType) {
	case 0:
	case 1:
		animation->mTexMtxCalcType = data->mTexMtxCalcType;
		break;
	default:
		animation->mTexMtxCalcType = 0;
	}
}

/**
 * @note Address: 0x80073C7C
 * @note Size: 0x28
 * readAnmColor__19J3DAnmKeyLoader_v15FPC18J3DAnmColorKeyData
 */
void J3DAnmKeyLoader_v15::readAnmColor(const J3DAnmColorKeyData* data)
{
	setAnmColor((J3DAnmColorKey*)mAnimation, data);
}

/**
 * @note Address: 0x80073CA4
 * @note Size: 0xEC
 * setAnmColor__19J3DAnmKeyLoader_v15FP14J3DAnmColorKeyPC18J3DAnmColorKeyData
 */
void J3DAnmKeyLoader_v15::setAnmColor(J3DAnmColorKey* animation, const J3DAnmColorKeyData* data)
{
	animation->mTotalFrameCount   = data->mTotalFrameCount;
	animation->mAttribute         = data->mAttribute;
	animation->mCurrentFrame      = 0.0f;
	animation->mUpdateMaterialNum = data->mUpdateMaterialNum;
	animation->_0C                = data->_10;
	animation->_0E                = data->_12;
	animation->_10                = data->_14;
	animation->_12                = data->_16;
	animation->mTable             = JSUConvertOffsetToPtr<J3DAnmColorKeyTable>(data, (void*)data->mTableOffset);
	animation->mRedValue          = JSUConvertOffsetToPtr<s16>(data, (void*)data->mRValOffset);
	animation->mGreenValue        = JSUConvertOffsetToPtr<s16>(data, (void*)data->mGValOffset);
	animation->mBlueValue         = JSUConvertOffsetToPtr<s16>(data, (void*)data->mBValOffset);
	animation->mAlphaValue        = JSUConvertOffsetToPtr<s16>(data, (void*)data->mAValOffset);
	animation->mUpdateMaterialID  = JSUConvertOffsetToPtr<u16>(data, (void*)data->mUpdateMaterialIDOffset);

	animation->mNameTab.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mNameTabOffset));
}

/**
 * @note Address: 0x80073D90
 * @note Size: 0x28
 * readAnmCluster__19J3DAnmKeyLoader_v15FPC20J3DAnmClusterKeyData
 */
void J3DAnmKeyLoader_v15::readAnmCluster(const J3DAnmClusterKeyData* data)
{
	setAnmCluster((J3DAnmClusterKey*)mAnimation, data);
}

/**
 * @note Address: 0x80073DB8
 * @note Size: 0x6C
 * setAnmCluster__19J3DAnmKeyLoader_v15FP16J3DAnmClusterKeyPC20J3DAnmClusterKeyData
 */
void J3DAnmKeyLoader_v15::setAnmCluster(J3DAnmClusterKey* animation, const J3DAnmClusterKeyData* data)
{
	animation->mTotalFrameCount = data->mTotalFrameCount;
	animation->mAttribute       = data->mAttribute;
	animation->mCurrentFrame    = 0.0f;
	animation->mTables          = JSUConvertOffsetToPtr<J3DAnmClusterKeyTable>(data, data->mTableOffset);
	animation->mWeights         = JSUConvertOffsetToPtr<f32>(data, (void*)data->mWeightsOffset);
}

/**
 * @note Address: 0x80073E24
 * @note Size: 0x28
 * readAnmTevReg__19J3DAnmKeyLoader_v15FPC19J3DAnmTevRegKeyData
 */
void J3DAnmKeyLoader_v15::readAnmTevReg(const J3DAnmTevRegKeyData* data)
{
	setAnmTevReg((J3DAnmTevRegKey*)mAnimation, data);
}

/**
 * @note Address: 0x80073E4C
 * @note Size: 0x18C
 * setAnmTevReg__19J3DAnmKeyLoader_v15FP15J3DAnmTevRegKeyPC19J3DAnmTevRegKeyData
 */
void J3DAnmKeyLoader_v15::setAnmTevReg(J3DAnmTevRegKey* animation, const J3DAnmTevRegKeyData* data)
{
	animation->mTotalFrameCount       = data->mTotalFrameCount;
	animation->mAttribute             = data->mAttribute;
	animation->mCurrentFrame          = 0.0f;
	animation->mCRegUpdateMaterialNum = data->mCRegUpdateMaterialNum;
	animation->mCRegKeyTable          = JSUConvertOffsetToPtr<J3DAnmCRegKeyTable>(data, (void*)data->mCRegTableOffset);
	animation->mCRegUpdateMaterialID  = JSUConvertOffsetToPtr<u16>(data, (void*)data->mCRegUpdateMaterialIDOffset);
	animation->mCRegNameTable.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mCRegNameTabOffset));
	animation->mKRegUpdateMaterialNum = data->mKRegUpdateMaterialNum;
	animation->mKRegKeyTable          = JSUConvertOffsetToPtr<J3DAnmKRegKeyTable>(data, (void*)data->mKRegTableOffset);
	animation->mKRegUpdateMaterialID  = JSUConvertOffsetToPtr<u16>(data, (void*)data->mKRegUpdateMaterialIDOffset);
	animation->mKRegNameTable.setResource(JSUConvertOffsetToPtr<ResNTAB>(data, (void*)data->mKRegNameTabOffset));
	animation->_10         = data->_10;
	animation->_12         = data->_12;
	animation->_14         = data->_14;
	animation->_16         = data->_16;
	animation->mCRedVals   = JSUConvertOffsetToPtr<s16>(data, (void*)data->mCRValuesOffset);
	animation->mCGreenVals = JSUConvertOffsetToPtr<s16>(data, (void*)data->mCGValuesOffset);
	animation->mCBlueVals  = JSUConvertOffsetToPtr<s16>(data, (void*)data->mCBValuesOffset);
	animation->mCAlphaVals = JSUConvertOffsetToPtr<s16>(data, (void*)data->mCAValuesOffset);
	animation->_18         = data->_18;
	animation->_1A         = data->_1A;
	animation->_1C         = data->_1C;
	animation->_1E         = data->_1E;
	animation->mKRedVals   = JSUConvertOffsetToPtr<s16>(data, (void*)data->mKRValuesOffset);
	animation->mKGreenVals = JSUConvertOffsetToPtr<s16>(data, (void*)data->mKGValuesOffset);
	animation->mKBlueVals  = JSUConvertOffsetToPtr<s16>(data, (void*)data->mKBValuesOffset);
	animation->mKAlphaVals = JSUConvertOffsetToPtr<s16>(data, (void*)data->mKAValuesOffset);
}

/**
 * @note Address: 0x80073FD8
 * @note Size: 0x28
 * readAnmVtxColor__19J3DAnmKeyLoader_v15FPC21J3DAnmVtxColorKeyData
 */
void J3DAnmKeyLoader_v15::readAnmVtxColor(const J3DAnmVtxColorKeyData* data)
{
	setAnmVtxColor((J3DAnmVtxColorKey*)mAnimation, data);
}

/**
 * @note Address: 0x80074000
 * @note Size: 0x170
 * setAnmVtxColor__19J3DAnmKeyLoader_v15FP17J3DAnmVtxColorKeyPC21J3DAnmVtxColorKeyData
 */
void J3DAnmKeyLoader_v15::setAnmVtxColor(J3DAnmVtxColorKey* animation, const J3DAnmVtxColorKeyData* data)
{
	animation->mTotalFrameCount = data->mTotalFrameCount;
	animation->mAttribute       = data->mAttribute;
	animation->mCurrentFrame    = 0.0f;
	animation->mAnmTableNum[0]  = data->mAnmTableNum[0];
	animation->mAnmTableNum[1]  = data->mAnmTableNum[1];

	animation->mTable[0]                = JSUConvertOffsetToPtr<J3DAnmColorKeyTable>(data, (void*)data->mTableOffsets[0]);
	animation->mTable[1]                = JSUConvertOffsetToPtr<J3DAnmColorKeyTable>(data, (void*)data->mTableOffsets[1]);
	animation->mAnmVtxColorIndexData[0] = JSUConvertOffsetToPtr<J3DAnmVtxColorIndexData>(data, (void*)data->mVtxColoIndexDataOffset[0]);
	animation->mAnmVtxColorIndexData[1] = JSUConvertOffsetToPtr<J3DAnmVtxColorIndexData>(data, (void*)data->mVtxColoIndexDataOffset[1]);

	u16* vtxColors0 = JSUConvertOffsetToPtr<u16>(data, (void*)data->mVtxColorIndexPointerOffset[0]);
	u16* vtxColors1 = JSUConvertOffsetToPtr<u16>(data, (void*)data->mVtxColorIndexPointerOffset[1]);

	for (int i = 0; i < animation->mAnmTableNum[0]; i++) {
		animation->mAnmVtxColorIndexData[0][i].mData = (void*)(vtxColors0 + (u32)animation->mAnmVtxColorIndexData[0][i].mData);
	}

	for (int i = 0; i < animation->mAnmTableNum[1]; i++) {
		animation->mAnmVtxColorIndexData[1][i].mData = (void*)(vtxColors1 + (u32)animation->mAnmVtxColorIndexData[1][i].mData);
	}

	animation->mRedVals   = JSUConvertOffsetToPtr<s16>(data, (void*)data->mRValOffset);
	animation->mGreenVals = JSUConvertOffsetToPtr<s16>(data, (void*)data->mGValOffset);
	animation->mBlueVals  = JSUConvertOffsetToPtr<s16>(data, (void*)data->mBValOffset);
	animation->mAlphaVals = JSUConvertOffsetToPtr<s16>(data, (void*)data->mAValOffset);
}
