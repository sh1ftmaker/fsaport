#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/J3D/J3DMaterialFactory.h"
#include "JSystem/J3D/J3DModel.h"
#include "JSystem/J3D/J3DModelLoader.h"
#include "JSystem/J3D/J3DMtxCalc.h"
#include "JSystem/J3D/J3DTypes.h"
#include "JSystem/J3D/J3DShapeFactory.h"
#include "JSystem/J3D/J3DJointFactory.h"
#include "JSystem/JSupport.h"

#ifdef FSA_PORT
// ---------------------------------------------------------------------------
// Port: J3D model files (BMD/BDL/BMT) are big-endian. The loaders keep pointers
// into the file's tables (hierarchy, envelopes, draw matrices, joints, shapes,
// materials) that the runtime reads every frame, so those tables are swapped to
// host order once, in place, before the first loader touches them. Block
// headers stay big-endian (J3DFileBlock.h declares them BE), as do name tables
// (ResNTAB), texture headers (ResTIMG), display lists and vertex arrays (Aurora
// reads those big-endian). A marker in the header padding keeps a file from
// being swapped twice when it is loaded again.
#include "J3DPortSwapTables.h"

// GX structs that are typedefs of unnamed structs: every field is a u32 enum / s16.
static const unsigned short kPortSwap_GXVtxAttrFmtList[] = {0, 4, 4, 4, 8, 4};
static const unsigned int kPortSwapCount_GXVtxAttrFmtList_ = 3;
static const unsigned short kPortSwap_GXVtxDescList[] = {0, 4, 4, 4};
static const unsigned int kPortSwapCount_GXVtxDescList_ = 2;
static const unsigned short kPortSwap_GXColorS10[] = {0, 2, 2, 2, 4, 2, 6, 2};
static const unsigned int kPortSwapCount_GXColorS10_ = 4;

static void portSwapBytes(u8* p, u32 size)
{
	if (size == 2) {
		u8 t = p[0]; p[0] = p[1]; p[1] = t;
	} else if (size == 4) {
		u8 t = p[0]; p[0] = p[3]; p[3] = t;
		t = p[1]; p[1] = p[2]; p[2] = t;
	} else if (size == 8) {
		for (int i = 0; i < 4; i++) {
			u8 t = p[i]; p[i] = p[7 - i]; p[7 - i] = t;
		}
	}
}

// Swaps every whole element of `elemSize` bytes in [p, p + bytes).
static void portSwapElements(void* p, u32 bytes, const unsigned short* table, u32 count, u32 elemSize)
{
	if (p == nullptr || elemSize == 0 || count == 0)
		return;
	u8* base = (u8*)p;
	for (u32 e = 0; e + elemSize <= bytes; e += elemSize)
		for (u32 i = 0; i < count; i++)
			portSwapBytes(base + e + table[2 * i], table[2 * i + 1]);
}

static void portSwapWords(void* p, u32 bytes, u32 width)
{
	if (p == nullptr)
		return;
	for (u32 o = 0; o + width <= bytes; o += width)
		portSwapBytes((u8*)p + o, width);
}

#define PORT_SWAP(T, p, bytes) portSwapElements((p), (bytes), kPortSwap_##T, kPortSwapCount_##T, kPortSwapSize_##T)

// The tables of one block: [offset, next offset in the block or the block end).
struct PortBlockTables {
	const u8* mBlock;
	u32 mSize;
	u32 mOffsets[40];
	u32 mCount;

	PortBlockTables(const void* block, u32 size) : mBlock((const u8*)block), mSize(size), mCount(0) {}
	void add(u32 off)
	{
		if (off != 0 && off < mSize && mCount < 40)
			mOffsets[mCount++] = off;
	}
	u8* at(u32 off) const { return off ? (u8*)mBlock + off : nullptr; }
	u32 length(u32 off) const
	{
		u32 end = mSize;
		for (u32 i = 0; i < mCount; i++)
			if (mOffsets[i] > off && mOffsets[i] < end)
				end = mOffsets[i];
		return off ? end - off : 0;
	}
};

static void portSwapBlock(const J3DFileBlockBase* blk)
{
	u32 size = (u32)blk->mSize;
	switch ((u32)blk->mBlockType) {
	case J3DFBT_Info: {
		const J3DModelInfoBlock* b = (const J3DModelInfoBlock*)blk;
		PortBlockTables t(b, size);
		u32 h = (u32)b->mHierarchyDataOffset;
		t.add(h);
		PORT_SWAP(J3DModelHierarchy, t.at(h), t.length(h));
		break;
	}
	case J3DFBT_Vertex: {
		const J3DVertexBlock* b = (const J3DVertexBlock*)blk;
		PortBlockTables t(b, size);
		u32 fmt = (u32)b->mVertexFormatOffset;
		t.add(fmt);
		t.add((u32)b->mPositionDataOffset);
		t.add((u32)b->mNormalDataOffset);
		t.add((u32)b->mNBTDataOffset);
		for (int i = 0; i < 2; i++)
			t.add((u32)b->mColorDataOffset[i]);
		for (int i = 0; i < 8; i++)
			t.add((u32)b->mTexCoordDataOffset[i]);
		// Only the format list; the vertex arrays stay big-endian for the GPU.
		u8* p = t.at(fmt);
		for (u32 o = 0; p && o + 16 <= t.length(fmt); o += 16) {
			portSwapElements(p + o, 16, kPortSwap_GXVtxAttrFmtList, kPortSwapCount_GXVtxAttrFmtList_, 16);
			if (*(u32*)(p + o) == GX_VA_NULL)
				break;
		}
		break;
	}
	case J3DFBT_Envelope: {
		const J3DEnvelopeBlock* b = (const J3DEnvelopeBlock*)blk;
		PortBlockTables t(b, size);
		u32 cnt = (u32)b->mJointCountTableOffset, idx = (u32)b->mIndexTableOffset;
		u32 wgt = (u32)b->mWeightTableOffset, inv = (u32)b->mInvBindTableOffset;
		t.add(cnt);
		t.add(idx);
		t.add(wgt);
		t.add(inv);
		portSwapWords(t.at(idx), t.length(idx), 2);
		portSwapWords(t.at(wgt), t.length(wgt), 4);
		portSwapWords(t.at(inv), t.length(inv), 4);
		break;
	}
	case J3DFBT_Draw: {
		const J3DDrawBlock* b = (const J3DDrawBlock*)blk;
		PortBlockTables t(b, size);
		u32 flags = (u32)b->mMatrixTypeArrayOffset, data = (u32)b->mDataArrayOffset;
		t.add(flags);
		t.add(data);
		portSwapWords(t.at(data), t.length(data), 2);
		break;
	}
	case J3DFBT_Joint: {
		const J3DJointBlock* b = (const J3DJointBlock*)blk;
		PortBlockTables t(b, size);
		u32 init = b->mJointInitData, remap = b->mRemapTableOffset, names = b->mNameTableOffset;
		t.add(init);
		t.add(remap);
		t.add(names);
		PORT_SWAP(J3DJointInitData, t.at(init), t.length(init));
		portSwapWords(t.at(remap), t.length(remap), 2);
		break;
	}
	case J3DFBT_Shape: {
		const J3DShapeBlock* b = (const J3DShapeBlock*)blk;
		PortBlockTables t(b, size);
		u32 data = b->mShapeDataOffset, remap = b->mRemapTableOffset, names = b->mNameTableOffset;
		u32 attr = b->mAttribTableOffset, mtx = b->mMatrixTableOffset, prim = b->mPrimDataOffset;
		u32 mtxInit = b->mMatrixInitDataOffset, draw = b->mMtxGroupTableOffset;
		t.add(data);
		t.add(remap);
		t.add(names);
		t.add(attr);
		t.add(mtx);
		t.add(prim);
		t.add(mtxInit);
		t.add(draw);
		PORT_SWAP(J3DShapeInitData, t.at(data), t.length(data));
		portSwapWords(t.at(remap), t.length(remap), 2);
		portSwapElements(t.at(attr), t.length(attr), kPortSwap_GXVtxDescList, kPortSwapCount_GXVtxDescList_, 8);
		portSwapWords(t.at(mtx), t.length(mtx), 2);
		PORT_SWAP(J3DShapeMtxInitData, t.at(mtxInit), t.length(mtxInit));
		PORT_SWAP(J3DShapeDrawInitData, t.at(draw), t.length(draw));
		break;
	}
	case J3DFBT_Material: {
		const J3DMaterialBlock* b = (const J3DMaterialBlock*)blk;
		PortBlockTables t(b, size);
		// Every table, in header order (the string table only bounds the others).
		const BEOffset* offs = &b->mMatEntryDataOffset;
		for (int i = 0; i < 30; i++)
			t.add((u32)offs[i]);
		u32 o;
		o = (u32)b->mMatEntryDataOffset;      PORT_SWAP(J3DMaterialInitData, t.at(o), t.length(o));
		o = (u32)b->mMatRemapTableOffset;     portSwapWords(t.at(o), t.length(o), 2);
		o = (u32)b->mIndTextureInfoOffset;    PORT_SWAP(J3DIndInitData, t.at(o), t.length(o));
		o = (u32)b->mCullModeInfoOffset;      portSwapWords(t.at(o), t.length(o), 4);
		o = (u32)b->mTexMtxInfoOffset;        PORT_SWAP(J3DTexMtxInfo, t.at(o), t.length(o));
		o = (u32)b->mTexMtxInfo2Offset;       PORT_SWAP(J3DTexMtxInfo, t.at(o), t.length(o));
		o = (u32)b->mTextureRemapTableOffset; portSwapWords(t.at(o), t.length(o), 2);
		o = (u32)b->mTevColorsOffset;
		portSwapElements(t.at(o), t.length(o), kPortSwap_GXColorS10, kPortSwapCount_GXColorS10_, 8);
		o = (u32)b->mFogInfoOffset;           PORT_SWAP(J3DFogInfo, t.at(o), t.length(o));
		o = (u32)b->mNBTScaleInfoOffset;      PORT_SWAP(J3DNBTScaleInfo, t.at(o), t.length(o));
		// Light info: J3DLightInfo is an empty placeholder in the decomp; the file
		// entries are tp's layout (0x34 bytes: position and direction f32[6], a
		// GXColor, attenuation f32[6]).
		o = (u32)b->mLightInfoOffset;
		if (u8* p = t.at(o)) {
			for (u32 e = 0; e + 0x34 <= t.length(o); e += 0x34) {
				portSwapWords(p + e, 0x18, 4);
				portSwapWords(p + e + 0x1C, 0x18, 4);
			}
		}
		// Not swapped: the byte tables (colours, counts, TEV/blend/z/alpha settings).
		break;
	}
	case J3DFBT_MaterialV21: {
		const J3DMaterialBlock_v21* b = (const J3DMaterialBlock_v21*)blk;
		PortBlockTables t(b, size);
		const BEOffset* offs = &b->mMatEntryDataOffset;
		for (int i = 0; i < 27; i++)
			t.add((u32)offs[i]);
		u32 o;
		o = (u32)b->mMatEntryDataOffset;      PORT_SWAP(J3DMaterialInitData_v21, t.at(o), t.length(o));
		o = (u32)b->mMatRemapTableOffset;     portSwapWords(t.at(o), t.length(o), 2);
		o = (u32)b->mIndTexInfoOffset;        PORT_SWAP(J3DIndInitData, t.at(o), t.length(o));
		o = (u32)b->mCullModeInfoOffset;      portSwapWords(t.at(o), t.length(o), 4);
		o = (u32)b->mTexMtxInfoOffset;        PORT_SWAP(J3DTexMtxInfo, t.at(o), t.length(o));
		o = (u32)b->mTexMtxInfo2Offset;       PORT_SWAP(J3DTexMtxInfo, t.at(o), t.length(o));
		o = (u32)b->mTextureRemapTableOffset; portSwapWords(t.at(o), t.length(o), 2);
		o = (u32)b->mTevColorsOffset;
		portSwapElements(t.at(o), t.length(o), kPortSwap_GXColorS10, kPortSwapCount_GXColorS10_, 8);
		o = (u32)b->mFogInfoOffset;           PORT_SWAP(J3DFogInfo, t.at(o), t.length(o));
		o = (u32)b->mNBTScaleInfoOffset;      PORT_SWAP(J3DNBTScaleInfo, t.at(o), t.length(o));
		break;
	}
	case J3DFBT_MaterialDL: {
		const J3DMaterialDLBlock* b = (const J3DMaterialDLBlock*)blk;
		PortBlockTables t(b, size);
		const BEOffset* offs = &b->mPacketOffset;
		for (int i = 0; i < 6; i++)
			t.add((u32)offs[i]);
		u32 o;
		o = (u32)b->mPacketOffset;            PORT_SWAP(J3DDisplayListInit, t.at(o), t.length(o));
		o = (u32)b->mSubPacketLocationOffset; PORT_SWAP(J3DPatchingInfo, t.at(o), t.length(o));
		o = (u32)b->mMatrixIndexOffset;       PORT_SWAP(J3DCurrentMtxInfo, t.at(o), t.length(o));
		o = (u32)b->mIndexesOffset;           portSwapWords(t.at(o), t.length(o), 2);
		break;
	}
	default:
		// TEX1 (ResTIMG and names are declared big-endian) and anything else:
		// left alone.
		break;
	}
}

void J3DPortSwapModelFile(const void* stream)
{
	if (stream == nullptr)
		return;
	J3DFileHeader* header = (J3DFileHeader*)stream;
	if (header->mJ3dVersion != 'J3D2' && header->mJ3dVersion != 'J3D1')
		return;
	u8* mark = header->_10;
	if (mark[0] == 'P' && mark[1] == 'O' && mark[2] == 'R' && mark[3] == 'T')
		return; // already in host order
	const J3DFileBlockBase* blk = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		portSwapBlock(blk);
		blk = blk->getNext();
	}
	mark[0] = 'P';
	mark[1] = 'O';
	mark[2] = 'R';
	mark[3] = 'T';
}
#endif

/**
 * @note Address: 0x8006F894
 * @note Size: 0x154
 */
J3DModelData* J3DModelLoaderDataBase::load(const void* stream, u32 flags)
{
#ifdef FSA_PORT
	J3DPortSwapModelFile(stream); // port: host-order tables, once per file
#endif
	if (stream == nullptr) {
		return nullptr;
	}
	J3DFileHeader* header = (J3DFileHeader*)stream;
	if (header->mJ3dVersion == 'J3D1' && header->mFileVersion == 'bmd1') {
		return nullptr;
	}
	if (header->mJ3dVersion == 'J3D2' && header->mFileVersion == 'bmd2') {
		J3DModelLoader_v21 loader;
		return loader.load(stream, flags);
	}
	if (header->mJ3dVersion == 'J3D2' && header->mFileVersion == 'bmd3') {
		J3DModelLoader_v26 loader;
		return loader.load(stream, flags);
	}
	return nullptr;
}

/**
 * @note Address: 0x8006FAE8
 * @note Size: 0xBC
 */
J3DModelData* J3DModelLoaderDataBase::loadBinaryDisplayList(const void* stream, u32 flags)
{
#ifdef FSA_PORT
	J3DPortSwapModelFile(stream); // port: host-order tables, once per file
#endif
	if (stream == nullptr) {
		return nullptr;
	}
	J3DFileHeader* header = (J3DFileHeader*)stream;
	if (header->mJ3dVersion == 'J3D2' && (header->mFileVersion == 'bdl3' || header->mFileVersion == 'bdl4')) {
		J3DModelLoader_v26 loader;
		return loader.loadBinaryDisplayList(stream, flags);
	}
	return nullptr;
}

/**
 * @note Address: 0x8006FBA4
 * @note Size: 0x2BC
 * load__14J3DModelLoaderFPCvUl
 */
J3DModelData* J3DModelLoader::load(const void* stream, u32 flags)
{
#ifdef FSA_PORT
	J3DPortSwapModelFile(stream); // port: host-order tables, once per file
#endif
	JKRHeap::getCurrentHeap()->getTotalFreeSize();
	mModelData = new J3DModelData;
	mModelData->clear();
	const J3DFileHeader* header           = reinterpret_cast<const J3DFileHeader*>(stream);
	mModelData->mBmd                      = (u8*)stream;
	const J3DFileBlockBase* nextBlock     = header->getFirstBlock();
	mModelData->mJointTree.mModelDataType = 0;
	mMaterialTable                        = &mModelData->mMaterialTable;
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_Info:
			readInformation((const J3DModelInfoBlock*)nextBlock, flags);
			break;
		case J3DFBT_Vertex:
			readVertex((const J3DVertexBlock*)nextBlock);
			break;
		case J3DFBT_Envelope:
			readEnvelop((const J3DEnvelopeBlock*)nextBlock);
			break;
		case J3DFBT_Draw:
			readDraw((const J3DDrawBlock*)nextBlock);
			break;
		case J3DFBT_Joint:
			readJoint((const J3DJointBlock*)nextBlock);
			break;
		case J3DFBT_Material:
			readMaterial((const J3DMaterialBlock*)nextBlock, flags);
			break;
		case J3DFBT_MaterialV21:
			readMaterial_v21((const J3DMaterialBlock_v21*)nextBlock, flags);
			break;
		case J3DFBT_Shape:
			readShape((const J3DShapeBlock*)nextBlock, flags);
			break;
		case J3DFBT_Texture:
			readTexture((const J3DTextureBlock*)nextBlock);
			break;
		}
		nextBlock = nextBlock->getNext();
	}
	mModelData->init(mModelData->mJointTree.mHierarchy);
	mModelData->mShapeTable.sortVcdVatCmd();
	mModelData->mJointTree.findImportantMtxIndex();
	setupBBoardInfo();
	if (mModelData->mModelLoaderFlags & J3DMLF_NoMatrixTransform) {
		for (u16 i = 0; i < mModelData->getShapeNum(); i++) {
			mModelData->mShapeTable.getItem(i)->onFlag(J3DShape_NoMtx);
		}
	}
	return mModelData;
}

/**
 * @note Address: 0x8006FE68
 * @note Size: 0x148
 * loadMaterialTable__14J3DModelLoaderFPCv
 */
J3DMaterialTable* J3DModelLoader::loadMaterialTable(const void* stream)
{
#ifdef FSA_PORT
	J3DPortSwapModelFile(stream); // port: host-order tables, once per file
#endif
	mMaterialTable = new J3DMaterialTable();
	mMaterialTable->clear();
	const J3DFileHeader* header       = reinterpret_cast<const J3DFileHeader*>(stream);
	const J3DFileBlockBase* nextBlock = header->getFirstBlock();
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_Material:
			readMaterialTable((const J3DMaterialBlock*)nextBlock, 0x51100000);
			break;
		case J3DFBT_MaterialV21:
			readMaterialTable_v21((const J3DMaterialBlock_v21*)nextBlock, 0x51100000);
			break;
		case J3DFBT_Texture:
			readTextureTable((const J3DTextureBlock*)nextBlock);
			break;
		default:
			break;
		}
		nextBlock = nextBlock->getNext();
	}
	if (mMaterialTable->mTextures == nullptr) {
		mMaterialTable->mTextures = new J3DTexture(0, nullptr);
	}
	return mMaterialTable;
}

/**
 * @note Address: 0x8006FFB8
 * @note Size: 0x2A4
 * loadBinaryDisplayList__14J3DModelLoaderFPCvUl
 */
J3DModelData* J3DModelLoader::loadBinaryDisplayList(const void* stream, u32 flags)
{
#ifdef FSA_PORT
	J3DPortSwapModelFile(stream); // port: host-order tables, once per file
#endif
	mModelData = new J3DModelData();
	mModelData->clear();
	const J3DFileHeader* header           = reinterpret_cast<const J3DFileHeader*>(stream);
	mModelData->mBmd                      = stream;
	const J3DFileBlockBase* nextBlock     = header->getFirstBlock();
	mModelData->mJointTree.mModelDataType = 0x1;
	mMaterialTable                        = &mModelData->mMaterialTable;
	for (u32 i = 0; i < header->mBlockCount; i++) {
		switch (nextBlock->mBlockType) {
		case J3DFBT_Info:
			readInformation((const J3DModelInfoBlock*)nextBlock, flags);
			break;
		case J3DFBT_Vertex:
			readVertex((const J3DVertexBlock*)nextBlock);
			break;
		case J3DFBT_Envelope:
			readEnvelop((const J3DEnvelopeBlock*)nextBlock);
			break;
		case J3DFBT_Draw:
			readDraw((const J3DDrawBlock*)nextBlock);
			break;
		case J3DFBT_Joint:
			readJoint((const J3DJointBlock*)nextBlock);
			break;
		case J3DFBT_Shape:
			readShape((const J3DShapeBlock*)nextBlock, flags);
			break;
		case J3DFBT_Texture:
			readTexture((const J3DTextureBlock*)nextBlock);
			break;
		case J3DFBT_MaterialDL:
			readMaterialDL((const J3DMaterialDLBlock*)nextBlock, flags);
			modifyMaterial(flags);
			break;
		case J3DFBT_Material: {
			u32 matFlags = 0x50100000;
			matFlags |= flags & 0x03000000;
			const J3DMaterialBlock* matBlock = (const J3DMaterialBlock*)nextBlock;
			mMaterialBlock                   = matBlock;
			u32 matType                      = getBdlFlag_MaterialType(flags);
			if (matType == 0) {
				readMaterial(matBlock, matFlags);
			} else if (matType == 0x2000) {
				readPatchedMaterial(matBlock, matFlags);
			}
			break;
		}
		}
		nextBlock = nextBlock->getNext();
	}
	mModelData->init(mModelData->mJointTree.mHierarchy);
	mModelData->mShapeTable.sortVcdVatCmd();
	mModelData->mJointTree.findImportantMtxIndex();
	setupBBoardInfo();
	mModelData->indexToPtr();
	return mModelData;
}

/**
 * @note Address: 0x8007025C
 * @note Size: 0x14C
 */
void J3DModelLoader::setupBBoardInfo()
{
	for (u16 i = 0; i < mModelData->getJointNum(); i++) {
		J3DMaterial* material = mModelData->mJointTree.mJoints[i]->mMaterial;
		if (material) {
			u16 id                     = material->mShape->mId;
			u16* initDataIndexToIDMap  = JSUConvertOffsetToPtr<u16>(mShapeBlock, mShapeBlock->mRemapTableOffset);
			J3DShapeInitData* initData = JSUConvertOffsetToPtr<J3DShapeInitData>(mShapeBlock, mShapeBlock->mShapeDataOffset);
			J3DJoint* joint;
			switch (initData[initDataIndexToIDMap[id]].mShapeMtxType) {
			case J3DShapeMtx_Base:
				joint = mModelData->getJointNodePointer(i);
				joint->resetMtxType();
				break;
			case J3DShapeMtx_BBoard:
				joint = mModelData->getJointNodePointer(i);
				joint->setMtxType(1);
				mModelData->mBillboardFlag = 1;
				break;
			case J3DShapeMtx_Y_BBoard:
				joint = mModelData->getJointNodePointer(i);
				joint->setMtxType(2);
				mModelData->mBillboardFlag = 1;
				break;
			case J3DShapeMtx_Multi:
				joint = mModelData->getJointNodePointer(i);
				joint->resetMtxType();
				break;
			}
		}
	}
}

/**
 * @note Address: 0x800703A8
 * @note Size: 0x168
 */
void J3DModelLoader::readInformation(const J3DModelInfoBlock* block, u32 flags)
{
	J3DMtxCalc* calc              = nullptr;
	mModelData->mModelLoaderFlags = flags | block->mFlags;
	mModelData->mJointTree.mFlags = mModelData->mModelLoaderFlags;
	switch (mModelData->mModelLoaderFlags & J3DMLF_MtxTypeMask) {
	case 0: // Basic
		calc = new J3DMtxCalcNoAnm<J3DMtxCalcCalcTransformBasic, J3DMtxCalcJ3DSysInitBasic>;
		break;
	case J3DMLF_MtxSoftImageCalc:
		calc = new J3DMtxCalcNoAnm<J3DMtxCalcCalcTransformSoftimage, J3DMtxCalcJ3DSysInitSoftimage>;
		break;
	case J3DMLF_MtxMayaCalc:
		calc = new J3DMtxCalcNoAnm<J3DMtxCalcCalcTransformMaya, J3DMtxCalcJ3DSysInitMaya>;
		break;
	}
	mModelData->mJointTree.mTransformCalc = calc;
	mModelData->mVertexData.mPacketNum    = block->mMatrixGroupCount;
	mModelData->mVertexData.mVtxNum       = block->mVertexCount;
	mModelData->mJointTree.mHierarchy     = JSUConvertOffsetToPtr<J3DModelHierarchy>(block, block->mHierarchyDataOffset);
}

/**
 * @note Address: N/A
 * @note Size: 0x2C
 */
GXCompType getFmtType(GXVtxAttrFmtList* fmtList, GXAttr i_attr)
{
	for (; fmtList->attr != GX_VA_NULL; fmtList++) {
		if (fmtList->attr == i_attr) {
			return fmtList->type;
		}
	}
	return GX_F32;
}

/**
 * @note Address: 0x8007056C
 * @note Size: 0x238
 * readVertex__14J3DModelLoaderFPC14J3DVertexBlock
 */
void J3DModelLoader::readVertex(const J3DVertexBlock* block)
{
	J3DModelData* model       = mModelData;
	J3DVertexData* vertData   = model->getVertexData();
	vertData->mVtxAttrFmtList = JSUConvertOffsetToPtr<GXVtxAttrFmtList>(block, block->mVertexFormatOffset);
	vertData->mVtxPos         = JSUConvertOffsetToPtr<void>(block, block->mPositionDataOffset);
	vertData->mVtxNorm        = JSUConvertOffsetToPtr<void>(block, block->mNormalDataOffset);
	vertData->mVtxNBT         = JSUConvertOffsetToPtr<void>(block, block->mNBTDataOffset);
	for (int i = 0; i < 2; i++) {
		vertData->mVtxColor[i] = (GXColor*)JSUConvertOffsetToPtr<void>(block, block->mColorDataOffset[i]);
	}
	for (int i = 0; i < 8; i++) {
		vertData->mVtxTexCoord[i] = JSUConvertOffsetToPtr<void>(block, block->mTexCoordDataOffset[i]);
	}

	_GXCompType nrm_type = getFmtType(vertData->mVtxAttrFmtList, GX_VA_NRM);
	u32 nrm_size         = nrm_type == GX_F32 ? 12 : 6;

	void* nrm_end = nullptr;
	if (vertData->mVtxNBT != nullptr) {
		nrm_end = vertData->mVtxNBT;
	} else if (vertData->mVtxColor[0] != nullptr) {
		nrm_end = vertData->mVtxColor[0];
	} else if (vertData->mVtxTexCoord[0] != nullptr) {
		nrm_end = vertData->mVtxTexCoord[0];
	}

	if (vertData->mVtxNorm == nullptr) {
		vertData->mNormNum = 0;
	} else if (nrm_end != nullptr) {
		vertData->mNormNum = ((u32)nrm_end - (u32)vertData->mVtxNorm) / nrm_size + 1;
	} else {
		vertData->mNormNum = (block->mSize - (u32)block->mNormalDataOffset) / nrm_size + 1;
	}

	void* color0_end = nullptr;
	if (vertData->mVtxColor[1] != nullptr) {
		color0_end = vertData->mVtxColor[1];
	} else if (vertData->mVtxTexCoord[0] != nullptr) {
		color0_end = vertData->mVtxTexCoord[0];
	}

	if (vertData->mVtxColor[0] == nullptr) {
		vertData->mColorNum = 0;
	} else if (color0_end != nullptr) {
		vertData->mColorNum = ((u32)color0_end - (u32)vertData->mVtxColor[0]) / 4 + 1;
	} else {
		vertData->mColorNum = (block->mSize - (u32)block->mColorDataOffset[0]) / 4 + 1;
	}

	if (vertData->mVtxTexCoord[0] == nullptr) {
		vertData->mTexCoordNum = 0;
	} else {
		vertData->mTexCoordNum = (block->mSize - (u32)block->mTexCoordDataOffset[0]) / 8 + 1;
	}
}

/**
 * @note Address: 0x800707A4
 * @note Size: 0x98
 */
void J3DModelLoader::readEnvelop(const J3DEnvelopeBlock* block)
{
	mModelData->getJointTree().mEnvelopeCnt       = block->mCount;
	mModelData->getJointTree().mEnvelopeMixCnt    = JSUConvertOffsetToPtr<u8>(block, block->mJointCountTableOffset);
	mModelData->getJointTree().mEnvelopeMixIdx    = JSUConvertOffsetToPtr<u16>(block, block->mIndexTableOffset);
	mModelData->getJointTree().mEnvelopeMixWeight = JSUConvertOffsetToPtr<f32>(block, block->mWeightTableOffset);
	mModelData->getJointTree().mInvJointMtx       = JSUConvertOffsetToPtr<Mtx>(block, block->mInvBindTableOffset);
}

/**
 * @note Address: 0x8007083C
 * @note Size: 0xB8
 */
void J3DModelLoader::readDraw(const J3DDrawBlock* block)
{
	J3DJointTree& tree   = mModelData->getJointTree();
	tree.mMtxData.mCount = block->mCount - tree.mEnvelopeCnt;

	tree.mMtxData.mDrawMtxFlag = JSUConvertOffsetToPtr<u8>(block, block->mMatrixTypeArrayOffset);
	tree.mMtxData.mDrawMtxIdx  = JSUConvertOffsetToPtr<u16>(block, block->mDataArrayOffset);

	u16 i;
	for (i = 0; i < tree.mMtxData.mCount; i++) {
		if (tree.mMtxData.mDrawMtxFlag[i] == 1) {
			break;
		}
	}

	tree.mMtxData.mDrawMtxCount = i;

	mModelData->getJointTree().mEnvelopeImptIdx = new u16[tree.mMtxData.mCount];
}

/**
 * @note Address: 0x800708F4
 * @note Size: 0xEC
 */
void J3DModelLoader::readJoint(const J3DJointBlock* block)
{
	J3DJointFactory factory(*block);
	mModelData->getJointTree().mJointCnt = block->mCount;
	if (block->mNameTableOffset != nullptr) {
		mModelData->getJointTree().mNametab = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, (u32)block->mNameTableOffset));
	} else {
		mModelData->getJointTree().mNametab = nullptr;
	}
	mModelData->getJointTree().mJoints = new J3DJoint*[mModelData->getJointTree().mJointCnt];

	for (u16 i = 0; i < mModelData->getJointNum(); i++) {
		mModelData->getJointTree().mJoints[i] = factory.create(i);
	}
}

/**
 * @note Address: 0x800709E0
 * @note Size: 0x274
 */
void J3DModelLoader_v26::readMaterial(const J3DMaterialBlock* block, u32 flags)
{
	J3DMaterialFactory factory(*block);
	mMaterialTable->mMaterialNum       = block->mNumMaterials;
	mMaterialTable->mUniqueMaterialNum = factory.countUniqueMaterials();
	if (block->mStringTableOffset != nullptr) {
		mMaterialTable->mMaterialNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mStringTableOffset));
	} else {
		mMaterialTable->mMaterialNames = nullptr;
	}

	mMaterialTable->mMaterials = new J3DMaterial*[mMaterialTable->mMaterialNum];
	if (flags & J3DMLF_UseUniqueMaterials) {
		mMaterialTable->mUniqueMaterials = new (0x20) J3DMaterial[mMaterialTable->mUniqueMaterialNum];
	} else {
		mMaterialTable->mUniqueMaterials = nullptr;
	}
	if (flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mMaterialTable->mUniqueMaterialNum; i++) {
			factory.create(&mMaterialTable->mUniqueMaterials[i], J3DMaterialFactory::NORMAL, i, flags);
			mMaterialTable->mUniqueMaterials[i].mDiffFlag = (u32)&mMaterialTable->mUniqueMaterials[i] >> 4;
		}
	}
	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i] = factory.create(nullptr, J3DMaterialFactory::NORMAL, i, flags);
	}
	if (flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			mMaterialTable->mMaterials[i]->mDiffFlag     = (u32)&mMaterialTable->mUniqueMaterials[factory.getMaterialID(i)] >> 4;
			mMaterialTable->mMaterials[i]->mOrigMaterial = &mMaterialTable->mUniqueMaterials[factory.getMaterialID(i)];
		}
	} else {
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			mMaterialTable->mMaterials[i]->mDiffFlag = ((u32)mMaterialTable->mMaterials >> 4) + factory.getMaterialID(i);
		}
	}
}

/**
 * @note Address: 0x80070CA8
 * @note Size: 0x25C
 */
void J3DModelLoader_v21::readMaterial_v21(const J3DMaterialBlock_v21* block, u32 flags)
{
	J3DMaterialFactory_v21 factory(*block);
	mMaterialTable->mMaterialNum       = block->mNumMaterials;
	mMaterialTable->mUniqueMaterialNum = factory.countUniqueMaterials();
	if (block->mIndTexInfoOffset != nullptr) {
		mMaterialTable->mMaterialNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mIndTexInfoOffset));
	} else {
		mMaterialTable->mMaterialNames = nullptr;
	}

	mMaterialTable->mMaterials = new J3DMaterial*[mMaterialTable->mMaterialNum];
	if (flags & J3DMLF_UseUniqueMaterials) {
		mMaterialTable->mUniqueMaterials = new (0x20) J3DMaterial[mMaterialTable->mUniqueMaterialNum];
	} else {
		mMaterialTable->mUniqueMaterials = nullptr;
	}
	if (flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mMaterialTable->mUniqueMaterialNum; i++) {
			factory.create(&mMaterialTable->mUniqueMaterials[i], i, flags);
			mMaterialTable->mUniqueMaterials[i].mDiffFlag = (u32)&mMaterialTable->mUniqueMaterials[i] >> 4;
		}
	}
	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i] = factory.create(nullptr, i, flags);
	}
	if (flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			mMaterialTable->mMaterials[i]->mDiffFlag     = (u32)&mMaterialTable->mUniqueMaterials[factory.getMaterialID(i)] >> 4;
			mMaterialTable->mMaterials[i]->mOrigMaterial = &mMaterialTable->mUniqueMaterials[factory.getMaterialID(i)];
		}
	} else {
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			mMaterialTable->mMaterials[i]->mDiffFlag = 0xC0000000;
		}
	}
}

/**
 * @note Address: 0x80070F04
 * @note Size: 0x11C
 */
void J3DModelLoader::readShape(const J3DShapeBlock* block, u32 flags)
{
	mShapeBlock = block;

	J3DModelData* model = mModelData;
	J3DShapeFactory factory(*block);

	model->mShapeTable.mCount = block->mShapeNum;
	if (block->mNameTableOffset) {
		model->mShapeTable.mNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, (u32)block->mNameTableOffset));
	} else {
		model->mShapeTable.mNames = nullptr;
	}
	model->mShapeTable.mItems = new J3DShape*[model->mShapeTable.mCount];
	factory.allocVcdVatCmdBuffer(model->mShapeTable.mCount);

	J3DModelHierarchy const* hierarchy_entry = model->getHierarchy();
	GXVtxDescList* vtxDescList               = nullptr;
	for (J3DModelHierarchy* hier = mModelData->mJointTree.mHierarchy; hier->mType; hier++) {
		if (hier->mType == 0x12) {
			model->mShapeTable.mItems[hier->mValue] = factory.create(hier->mValue, flags, vtxDescList);
			vtxDescList                             = factory.getVtxDescList(hier->mValue);
		}
	}
}

/**
 * @note Address: 0x80071020
 * @note Size: 0xC4
 */
void J3DModelLoader::readTexture(const J3DTextureBlock* block)
{
	const u16 count          = block->mTextureCount;
	ResTIMG* resTextureImage = JSUConvertOffsetToPtr<ResTIMG>(block, block->mTexHeaderOffset);
	if (block->mTexNameOffset) {
		mMaterialTable->mTextureNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mTexNameOffset));
	} else {
		mMaterialTable->mTextureNames = nullptr;
	}
	mMaterialTable->setTexture(new J3DTexture(count, resTextureImage));
}

/**
 * @note Address: 0x800710E4
 * @note Size: 0x14C
 * readMaterialTable__18J3DModelLoader_v26FPC16J3DMaterialBlockUl
 */
void J3DModelLoader_v26::readMaterialTable(const J3DMaterialBlock* block, u32 flags)
{
	J3DMaterialFactory factory(*block);
	mMaterialTable->mMaterialNum = block->mNumMaterials;
	if (block->mStringTableOffset) {
		mMaterialTable->mMaterialNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mStringTableOffset));
	} else {
		mMaterialTable->mMaterialNames = nullptr;
	}
	mMaterialTable->mMaterials = new J3DMaterial*[mMaterialTable->mMaterialNum];
	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i] = factory.create(nullptr, J3DMaterialFactory::NORMAL, i, flags);
	}
	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i]->mDiffFlag = (u32)(mMaterialTable->mMaterials) + factory.getMaterialID(i);
	}
}

/**
 * @note Address: 0x80071230
 * @note Size: 0x14C
 */
void J3DModelLoader_v21::readMaterialTable_v21(const J3DMaterialBlock_v21* block, u32 flags)
{
	J3DMaterialFactory_v21 factory(*block);
	mMaterialTable->mMaterialNum = block->mNumMaterials;
	if (block->mIndTexInfoOffset) {
		mMaterialTable->mMaterialNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mIndTexInfoOffset));
	} else {
		mMaterialTable->mMaterialNames = nullptr;
	}
	mMaterialTable->mMaterials = new J3DMaterial*[mMaterialTable->mMaterialNum];

	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i] = factory.create(nullptr, i, flags);
	}

	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i]->mDiffFlag = ((u32)(mMaterialTable->mMaterials) >> 4) + factory.getMaterialID(i);
	}
}

/**
 * @note Address: 0x8007137C
 * @note Size: 0xC4
 */
void J3DModelLoader::readTextureTable(const J3DTextureBlock* block)
{
	const u16 count          = block->mTextureCount;
	ResTIMG* resTextureImage = JSUConvertOffsetToPtr<ResTIMG>(block, block->mTexHeaderOffset);
	if (block->mTexNameOffset) {
		mMaterialTable->mTextureNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mTexNameOffset));
	} else {
		mMaterialTable->mTextureNames = nullptr;
	}
	mMaterialTable->setTexture(new J3DTexture(count, resTextureImage));
}

/**
 * @note Address: 0x80071440
 * @note Size: 0x150
 */
void J3DModelLoader::readPatchedMaterial(const J3DMaterialBlock* block, u32 flags)
{
	J3DMaterialFactory factory(*block);
	mMaterialTable->mMaterialNum       = block->mNumMaterials;
	mMaterialTable->mUniqueMaterialNum = factory.countUniqueMaterials();
	if (block->mStringTableOffset) {
		mMaterialTable->mMaterialNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mStringTableOffset));
	} else {
		mMaterialTable->mMaterialNames = nullptr;
	}
	mMaterialTable->mMaterials       = new J3DMaterial*[mMaterialTable->mMaterialNum];
	mMaterialTable->mUniqueMaterials = nullptr;

	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i]            = factory.create(nullptr, J3DMaterialFactory::PATCHED, i, flags);
		mMaterialTable->mMaterials[i]->mDiffFlag = ((u32)(mMaterialTable->mMaterials) >> 4) + factory.getMaterialID(i);
	}
}

/**
 * @note Address: 0x80071590
 * @note Size: 0x1BC
 */
void J3DModelLoader::readMaterialDL(const J3DMaterialDLBlock* block, u32 flags)
{
	J3DMaterialFactory factory(*block);
	if (mMaterialTable->getMaterialNum() == 0) {
		mMaterialTable->_1C                = 1;
		mMaterialTable->mMaterialNum       = block->mEntries;
		mMaterialTable->mUniqueMaterialNum = block->mEntries;
		if (block->mStringTableOffset) {
			mMaterialTable->mMaterialNames = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mStringTableOffset));
		} else {
			mMaterialTable->mMaterialNames = nullptr;
		}
		mMaterialTable->mMaterials       = new J3DMaterial*[mMaterialTable->mMaterialNum];
		mMaterialTable->mUniqueMaterials = nullptr;
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			mMaterialTable->mMaterials[i] = factory.create(nullptr, J3DMaterialFactory::LOCKED, i, flags);
		}
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			mMaterialTable->mMaterials[i]->mDiffFlag = 0xc0000000;
		}
		return;
	}

	for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
		mMaterialTable->mMaterials[i] = factory.create(mMaterialTable->mMaterials[i], J3DMaterialFactory::LOCKED, i, flags);
	}
}

/**
 * @note Address: 0x8007174C
 * @note Size: 0x7C
 */
void J3DModelLoader::modifyMaterial(u32 flags)
{
	if ((flags & 0x2000) != 0) {
		J3DMaterialFactory factory(*mMaterialBlock);
		for (u16 i = 0; i < mMaterialTable->mMaterialNum; i++) {
			factory.modifyPatchedCurrentMtx(mMaterialTable->mMaterials[i], i);
		}
	}
}
