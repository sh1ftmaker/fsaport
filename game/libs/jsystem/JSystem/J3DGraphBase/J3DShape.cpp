#include "JSystem/J3D/J3DShape.h"
#include <dolphin/gd.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>
#include "JSystem/J3D/J3DGD.h"
#include "JSystem/J3D/J3DPacket.h"
#include "JSystem/J3D/J3DSys.h"
#include "JSystem/J3D/J3DVertexData.h"
#include "types.h"

/**
 * @note Address: 0x80060850
 * @note Size: 0x70
 */
void J3DShape::initialize()
{
	mMaterial    = nullptr;
	mId          = 0xFFFF;
	mMtxGroupNum = 0;
	mFlags       = 0;
	mRadius      = 0.0f;
	mMin.x       = 0.0f;
	mMin.y       = 0.0f;
	mMin.z       = 0.0f;
	mMax.x       = 0.0f;
	mMax.y       = 0.0f;
	mMax.z       = 0.0f;
	mVtxDesc           = nullptr;
	mShapeMtx          = nullptr;
	mShapeDraw         = nullptr;
	mVtxData           = nullptr;
	mDrawMtxData       = nullptr;
	mScaleFlagArray    = nullptr;
	mDrawMtx           = nullptr;
	mNrmMtx            = nullptr;
	mCurrentViewNumber = reinterpret_cast<u32*>(&j3dDefaultViewNo);
	mHasNBT            = false;
	mHasPNMTXIdx       = false;
}

/**
 * @note Address: 0x800608C0
 * @note Size: 0x78
 */
void J3DShape::calcNBTScale(const Vec& scale, Mtx33* srcArray, Mtx33* dstArray)
{
	for (u16 i = 0; i < getMtxGroupNum(); i++) {
		getShapeMtx(i)->calcNBTScale(scale, srcArray, dstArray);
	}
}

/**
 * @note Address: 0x80060938
 * @note Size: 0x80
 */
u32 J3DShape::countBumpMtxNum() const
{
	u32 count = 0;
	for (u16 i = 0; i < mMtxGroupNum; i++) {
		count += mShapeMtx[i]->getUseMtxNum();
	}
	return count;
}

/**
 * @note Address: 0x800609B8
 * @note Size: 0xEC
 */
bool J3DShape::isSameVcdVatCmd(J3DShape* other)
{
	u8* otherVatCmd = other->mVcdVatCmd;
	u8* thisVatCmd  = mVcdVatCmd;

	for (int i = 0; i < kVcdVatDLSize; i++) {
		if (otherVatCmd[i] != thisVatCmd[i]) {
			return false;
		}
	}
	return true;
}

/**
 * @note Address: 0x80060AA4
 * @note Size: 0x2FC
 */
void J3DShape::makeVtxArrayCmd()
{
	GXVtxAttrFmtList* vtxAttr = mVtxData->getVtxAttrFmtList();

	u8 stride[0x0C];
	void* array[0x0C];
	for (u32 i = 0; i < 0x0C; i++) {
		stride[i] = 0;
		array[i]  = 0;
	}

	for (; vtxAttr->attr != GX_VA_NULL; vtxAttr++) {
		switch (vtxAttr->attr) {
		case GX_VA_POS: {
			if (vtxAttr->type == GX_F32)
				stride[vtxAttr->attr - GX_VA_POS] = 0x0C;
			else
				stride[vtxAttr->attr - GX_VA_POS] = 0x06;
			array[vtxAttr->attr - GX_VA_POS] = mVtxData->getVtxPosArray();
			mVtxData->setVtxPosFrac(vtxAttr->frac);
			mVtxData->setVtxPosType(vtxAttr->type);
		} break;
		case GX_VA_NRM: {
			if (vtxAttr->type == GX_F32)
				stride[vtxAttr->attr - GX_VA_POS] = 0x0C;
			else
				stride[vtxAttr->attr - GX_VA_POS] = 0x06;
			array[vtxAttr->attr - GX_VA_POS] = mVtxData->getVtxNrmArray();
			mVtxData->setVtxNrmFrac(vtxAttr->frac);
			mVtxData->setVtxNrmType(vtxAttr->type);
		} break;
		case GX_VA_CLR0:
		case GX_VA_CLR1: {
			stride[vtxAttr->attr - GX_VA_POS] = 0x04;
			array[vtxAttr->attr - GX_VA_POS]  = mVtxData->getVtxColorArray(vtxAttr->attr - GX_VA_CLR0);
		} break;
		case GX_VA_TEX0:
		case GX_VA_TEX1:
		case GX_VA_TEX2:
		case GX_VA_TEX3:
		case GX_VA_TEX4:
		case GX_VA_TEX5:
		case GX_VA_TEX6:
		case GX_VA_TEX7: {
			if (vtxAttr->type == GX_F32)
				stride[vtxAttr->attr - GX_VA_POS] = 0x08;
			else
				stride[vtxAttr->attr - GX_VA_POS] = 0x04;
			array[vtxAttr->attr - GX_VA_POS] = mVtxData->getVtxTexCoordArray(vtxAttr->attr - GX_VA_TEX0);
		} break;
		default:
			break;
		}
	}

	GXVtxDescList* vtxDesc = mVtxDesc;
	mHasPNMTXIdx           = false;
	for (; vtxDesc->attr != GX_VA_NULL; vtxDesc++) {
		if (vtxDesc->attr == GX_VA_NBT && vtxDesc->type != GX_NONE) {
			mHasNBT = true;
			stride[GX_VA_NRM - GX_VA_POS] *= 3;
			array[GX_VA_NRM - GX_VA_POS] = mVtxData->getVtxNBTArray();
		} else if (vtxDesc->attr == GX_VA_PNMTXIDX && vtxDesc->type != GX_NONE) {
			mHasPNMTXIdx = true;
		}
	}

#ifdef FSA_PORT
	// Each array runs up to the next one in the VTX1 block; the last is bounded
	// by the largest element count.
	u32 maxCount = mVtxData->getVtxNum();
	if (mVtxData->getNrmNum() > maxCount)
		maxCount = mVtxData->getNrmNum();
	if (mVtxData->getColNum() > maxCount)
		maxCount = mVtxData->getColNum();
	for (u32 i = 0; i < 0x0C; i++) {
		mPortArray[i]       = array[i];
		mPortArrayStride[i] = stride[i];
		mPortArraySize[i]   = 0;
		if (!array[i])
			continue;
		u32 next = 0;
		for (u32 j = 0; j < 0x0C; j++)
			if ((u32)array[j] > (u32)array[i] && (!next || (u32)array[j] < next))
				next = (u32)array[j];
		mPortArraySize[i] = next ? next - (u32)array[i] : stride[i] * (maxCount ? maxCount * 2 : 0x10000);
	}
#else
	for (u32 i = 0; i < 0x0C; i++) {
		if (array[i] != 0)
			GDSetArray((GXAttr)(i + GX_VA_POS), array[i], stride[i]);
		else
			GDSetArrayRaw((GXAttr)(i + GX_VA_POS), nullptr, stride[i]);
	}
#endif
}

#ifdef FSA_PORT
void J3DShape::portLoadArrays() const
{
	for (u32 i = 0; i < 0x0C; i++)
		if (mPortArray[i])
			GXSetArray((GXAttr)(i + GX_VA_POS), mPortArray[i], mPortArraySize[i], mPortArrayStride[i], false);
}

// Per-draw array base (J3DLoadArrayBasePtr): the file's array is big-endian;
// any other buffer (CPU-skinned positions/normals) holds host floats.
void J3DShape::portSetArray(GXAttr attr, void* data) const
{
	u32 i = (attr == GX_VA_NBT) ? 1 : (attr - GX_VA_POS);
	if (!data)
		return;
	bool fromFile = data == mPortArray[i];
	GXSetArray((GXAttr)(i + GX_VA_POS), data, mPortArraySize[i], mPortArrayStride[i], !fromFile);
}
#endif

/**
 * @note Address: 0x80060DA0
 * @note Size: 0xA0
 */
void J3DShape::makeVcdVatCmd()
{
	static s32 sInterruptFlag;
	static s8 init;

	if (!init) {
		sInterruptFlag = OSDisableInterrupts();
		init           = true;
	}
	OSDisableScheduler();

	GDCurrentDL gdl_obj;
	GDInitGDLObj(&gdl_obj, mVcdVatCmd, kVcdVatDLSize);
	__GDSetCurrent(&gdl_obj);
	GDSetVtxDescv(mVtxDesc);
	makeVtxArrayCmd();
	J3DGDSetVtxAttrFmtv(GX_VTXFMT0, mVtxData->getVtxAttrFmtList(), mHasNBT);
	GDPadCurr32();
	GDFlushCurrToMem();
	__GDSetCurrent(nullptr);
	OSEnableScheduler();
	OSRestoreInterrupts(sInterruptFlag);
}

u8* J3DShape::sOldVcdVatCmd;

/**
 * @note Address: 0x80060E40
 * @note Size: 0xA0
 */
void J3DShape::loadPreDrawSetting() const
{
	if (sOldVcdVatCmd != mVcdVatCmd) {
		GXCallDisplayList(mVcdVatCmd, kVcdVatDLSize);
#ifdef FSA_PORT
		portLoadArrays();
#endif
		sOldVcdVatCmd = mVcdVatCmd;
	}
	mCurrentMtx.load();
}

u8 J3DShape::sEnvelopeFlag;

void J3DLoadCPCmd(u8 cmd, u32 param)
{
	GXWGFifo.u8  = GX_CMD_LOAD_CP_REG;
	GXWGFifo.u8  = cmd;
	GXWGFifo.u32 = param;
}

#ifdef FSA_PORT
#define J3D_PORT_LOAD_ARRAY(attr, data) portSetArray(attr, data)
#else
#define J3D_PORT_LOAD_ARRAY(attr, data) J3DLoadArrayBasePtr(attr, data)
#endif
static void J3DLoadArrayBasePtr(_GXAttr attr, void* data)
{
	u32 idx = (attr == GX_VA_NBT) ? 1 : (attr - GX_VA_POS);
	J3DLoadCPCmd(0xA0 + idx, ((u32)data & 0x7FFFFFFF));
}

/**
 * @note Address: 0x80060EE0
 * @note Size: 0x278
 */
void J3DShape::drawFast() const
{
	if (sOldVcdVatCmd != mVcdVatCmd) {
		GXCallDisplayList(mVcdVatCmd, kVcdVatDLSize);
#ifdef FSA_PORT
		portLoadArrays();
#endif
		sOldVcdVatCmd = mVcdVatCmd;
	}

	if (sEnvelopeFlag != 0 && !mHasPNMTXIdx)
		mCurrentMtx.load();

	// start of setArrayAndBindPipeline();
	J3DShapeMtx::setCurrentPipeline(getPipeline());

	// start of loadVtxArray
	J3D_PORT_LOAD_ARRAY(GX_VA_POS, j3dSys.getVtxPos());
	if (!mHasNBT) {
		J3D_PORT_LOAD_ARRAY(GX_VA_NRM, j3dSys.getVtxNrm());
	}
	J3D_PORT_LOAD_ARRAY(GX_VA_CLR0, j3dSys.getVtxCol());
	// end of loadVtxArray

	j3dSys.setModelDrawMtx(mDrawMtx[*mCurrentViewNumber]);
	j3dSys.setModelNrmMtx((Mtx*)mNrmMtx[*mCurrentViewNumber]);
	J3DShapeMtx::sCurrentScaleFlag = mScaleFlagArray;
	J3DShapeMtx::sNBTFlag          = mHasNBT;
	sEnvelopeFlag                  = mHasPNMTXIdx;
	J3DShapeMtx::sTexMtxLoadType   = getTexMtxLoadType();
	// end of setArrayAndBindPipeline();

	if (!checkFlag(J3DShape_NoMtx)) {
		// LOD flag shenanigans
		if (J3DShapeMtx::getLODFlag() != 0)
			J3DShapeMtx::resetMtxLoadCache();
		u32 n = mMtxGroupNum;
		for (u16 i = 0; i < n; i++) {
			if (getShapeMtx(i) != nullptr)
				getShapeMtx(i)->load();
			if (getShapeDraw(i) != nullptr)
				getShapeDraw(i)->draw();
		}
	} else {
		J3DFifoLoadPosMtxImm(*j3dSys.getShapePacket()->getBaseMtxPtr(), GX_PNMTX0);
		J3DFifoLoadNrmMtxImm(*j3dSys.getShapePacket()->getBaseMtxPtr(), GX_PNMTX0);
		u32 n = mMtxGroupNum;
		for (u16 i = 0; i < n; i++)
			if (getShapeDraw(i) != nullptr)
				getShapeDraw(i)->draw();
	}
}

/**
 * @note Address: 0x80061158
 * @note Size: 0xB8
 * draw__8J3DShapeCFv
 */
void J3DShape::draw() const
{
	sOldVcdVatCmd = NULL;
	loadPreDrawSetting();
	drawFast();
}

/**
 * @note Address: 0x80061210
 * @note Size: 0x160
 */
void J3DShape::simpleDraw() const
{
	sOldVcdVatCmd = NULL;
	loadPreDrawSetting();
	J3DShapeMtx::setCurrentPipeline(getPipeline());

	// start of loadVtxArray
	J3D_PORT_LOAD_ARRAY(GX_VA_POS, j3dSys.getVtxPos());
	if (!mHasNBT) {
		J3D_PORT_LOAD_ARRAY(GX_VA_NRM, j3dSys.getVtxNrm());
	}
	J3D_PORT_LOAD_ARRAY(GX_VA_CLR0, j3dSys.getVtxCol());
	// end of loadVtxArray

	u16 i = 0;
	u32 n = getMtxGroupNum();
	for (i; i < n; i++) {
		if (getShapeDraw(i) != nullptr) {
			getShapeDraw(i)->draw();
		}
	}
}

/**
 * @note Address: 0x80061370
 * @note Size: 0x170
 */
void J3DShape::simpleDrawCache() const
{
	if (sOldVcdVatCmd != mVcdVatCmd) {
		GXCallDisplayList(mVcdVatCmd, kVcdVatDLSize);
#ifdef FSA_PORT
		portLoadArrays();
#endif
		sOldVcdVatCmd = mVcdVatCmd;
	}

	if (J3DShape::sEnvelopeFlag != 0 && !mHasPNMTXIdx) {
		mCurrentMtx.load();
	}

	// start of loadVtxArray
	J3D_PORT_LOAD_ARRAY(GX_VA_POS, j3dSys.getVtxPos());
	if (!mHasNBT) {
		J3D_PORT_LOAD_ARRAY(GX_VA_NRM, j3dSys.getVtxNrm());
	}
	J3D_PORT_LOAD_ARRAY(GX_VA_CLR0, j3dSys.getVtxCol());
	// end of loadVtxArray
	u16 i = 0;
	u32 n = getMtxGroupNum();
	for (i; i < n; i++)
		if (getShapeDraw(i) != NULL)
			getShapeDraw(i)->draw();
}
