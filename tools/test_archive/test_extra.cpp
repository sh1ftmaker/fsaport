// BMG, J3D and JPA checks through the ported accessors and structs.
#include "test_extra.h"

#include "JSystem/J3D/J3DAnmLoader.h"
#include "JSystem/J3D/J3DAnmTevRegKey.h"
#include "JSystem/J3D/J3DAnmTextureSRTKey.h"
#include "JSystem/J3D/J3DAnmTransform.h"
#include "JSystem/J3D/J3DFileBlock.h"
#include "JSystem/J3D/J3DJointFactory.h"
#include "JSystem/J3D/J3DMaterialFactory.h"
#include "JSystem/J3D/J3DModel.h"
#include "JSystem/J3D/J3DModelLoader.h"
#include "JSystem/J3D/J3DShape.h"
#include "JSystem/J3D/J3DShapeFactory.h"
#include "JSystem/JMessage/data.h"
#include "JSystem/JParticle/JPABlock.h"
#include "JSystem/JParticle/JPAShape.h"
#include "JSystem/ResTIMG.h"
#include "port/be.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            printf("FAIL: " __VA_ARGS__);     \
            printf("\n");                     \
            gFormatFail = 1;                  \
        }                                     \
    } while (0)

static bool endsWith(const char* n, const char* s)
{
    size_t a = strlen(n), b = strlen(s);
    return a >= b && strcmp(n + a - b, s) == 0;
}

static void tag4(u32 m, char* out)
{
    for (int i = 0; i < 4; i++)
        out[i] = (char)(m >> (24 - 8 * i));
    out[4] = 0;
}

// Prints a message's text, showing control tags (0x1A len group code...) as {g:code}.
static void printMessage(const char* text, int maxChars)
{
    printf("\"");
    for (int i = 0; i < maxChars && *text; i++) {
        unsigned char c = (unsigned char)*text;
        if (c == 0x1A) {
            const unsigned char* t = (const unsigned char*)text;
            printf("{%u:%u}", t[2], (t[3] << 8) | t[4]);
            text += t[1];
            continue;
        }
        if (c == '\n')
            printf("\\n");
        else if (c >= 0x20 && c < 0x7F)
            printf("%c", c);
        else
            printf("\\x%02x", c);
        text++;
    }
    printf("\"");
}

static void checkBmg(const char* name, const u8* file)
{
    using namespace JMessage::data;
    TParse_THeader header(file);
    char sig[5];
    tag4(be32(header.getSignature()), sig);
    printf("  %s: %s, type %08x, %u blocks, encoding %u\n", name, sig, (unsigned)header.getType(),
           (unsigned)header.getBlockNumber(), header.getEncoding());
    CHECK(be32(header.getSignature()) == 'MESG' && header.getType() == 'bmg1', "%s: header", name);
    const void* p = header.getContent();
    const char* dat = nullptr;
    const u8* inf = nullptr;
    for (u32 i = 0; i < header.getBlockNumber(); i++) {
        TParse_TBlock blk(p);
        char t[5];
        tag4(blk.get_type(), t);
        printf("    %s %u bytes", t, (unsigned)blk.get_size());
        CHECK(blk.get_size() >= 8 && blk.get_size() < 0x100000, "%s: block size", t);
        if (blk.get_type() == 'INF1') {
            TParse_TBlock_info info(p);
            printf(": %u messages, %u-byte entries, group %u", info.getMessageEntryNumber(),
                   info.getMessageEntrySize(), info.getGroupID());
            CHECK(info.getMessageEntrySize() >= 4 && info.getMessageEntrySize() <= 32, "INF1 entry size");
            inf = (const u8*)p;
        } else if (blk.get_type() == 'DAT1') {
            dat = (const char*)p + 8;
        } else if (blk.get_type() == 'MID1') {
            TParse_TBlock_messageID mid(p);
            printf(": %u ids, ordered %d, first %u", mid.get_number(), mid.get_isOrdered(),
                   (unsigned)be32(mid.getContent()));
        }
        printf("\n");
        if (blk.get_size() < 8)
            return;
        p = blk.getNext();
    }
    if (inf && dat) {
        TParse_TBlock_info info(inf);
        const u8* entries = (const u8*)info.getContent();
        int shown = 0;
        for (u16 i = 0; i < info.getMessageEntryNumber() && shown < 4; i++) {
            // As TResource::getMessageText_messageEntry does in the port.
            s32 off = be32s(entries + i * info.getMessageEntrySize());
            CHECK(off >= 0 && off < 0x100000, "message %u offset %d", i, off);
            if (off <= 0 || dat[off] == 0)
                continue;
            printf("    message %u: ", i);
            printMessage(dat + off, 60);
            printf("\n");
            shown++;
        }
    }
}

static void checkBmd(const char* name, const u8* file)
{
    const J3DFileHeader* hdr = (const J3DFileHeader*)file;
    char a[5], b[5];
    tag4(hdr->mJ3dVersion, a);
    tag4(hdr->mFileVersion, b);
    J3DPortSwapModelFile(file);
    J3DPortSwapModelFile(file); // a second load must not swap again
    printf("  %s: %s%s, %u blocks\n", name, a, b, (unsigned)hdr->mBlockCount);
    CHECK(hdr->mJ3dVersion == 'J3D2', "%s: header", name);
    const J3DFileBlockBase* blk = hdr->getFirstBlock();
    for (u32 i = 0; i < hdr->mBlockCount; i++, blk = blk->getNext()) {
        char t[5];
        tag4(blk->mBlockType, t);
        printf("    %s %6d bytes", t, (int)blk->mSize);
        switch ((u32)blk->mBlockType) {
        case J3DFBT_Info: {
            const J3DModelInfoBlock* inf = (const J3DModelInfoBlock*)blk;
            const J3DModelHierarchy* h = (const J3DModelHierarchy*)((const u8*)blk + (u32)inf->mHierarchyDataOffset);
            int n = 0;
            for (; n < 4096 && h[n].mType != 0; n++) {}
            printf(": flags %x, %u packets, %u vertices, hierarchy %d nodes (first %u/%u)", (unsigned)inf->mFlags,
                   (unsigned)inf->mMatrixGroupCount, (unsigned)inf->mVertexCount, n, h[0].mType, h[0].mValue);
            CHECK(n > 0 && n < 4096 && h[0].mType <= 0x12, "hierarchy");
            break;
        }
        case J3DFBT_Vertex: {
            const J3DVertexBlock* v = (const J3DVertexBlock*)blk;
            const u32* fmt = (const u32*)((const u8*)blk + (u32)v->mVertexFormatOffset);
            printf(": formats");
            for (int k = 0; k < 16 && fmt[k * 4] != GX_VA_NULL; k++)
                printf(" %u/%u/%u/%u", (unsigned)fmt[k * 4], (unsigned)fmt[k * 4 + 1], (unsigned)fmt[k * 4 + 2],
                       ((const u8*)&fmt[k * 4 + 3])[0]);
            CHECK(fmt[0] == GX_VA_POS && fmt[2] <= 4, "vertex format list");
            break;
        }
        case J3DFBT_Joint: {
            const J3DJointBlock* j = (const J3DJointBlock*)blk;
            const J3DJointInitData* init = (const J3DJointInitData*)((const u8*)blk + j->mJointInitData);
            const J3DTransformInfo& tf = init[0].mTransformInfo;
            printf(": %u joints; joint 0 scale %.2f,%.2f,%.2f trans %.1f,%.1f,%.1f radius %.1f",
                   (unsigned)j->mCount, tf.mScale.x, tf.mScale.y, tf.mScale.z, tf.mTranslation.x, tf.mTranslation.y,
                   tf.mTranslation.z, init[0].mRadius);
            CHECK(tf.mScale.x > 0.0f && tf.mScale.x < 1000.0f, "joint scale");
            break;
        }
        case J3DFBT_Shape: {
            const J3DShapeBlock* s = (const J3DShapeBlock*)blk;
            const J3DShapeInitData* sh = (const J3DShapeInitData*)((const u8*)blk + s->mShapeDataOffset);
            const J3DShapeDrawInitData* draw = (const J3DShapeDrawInitData*)((const u8*)blk + s->mMtxGroupTableOffset);
            const u32* desc = (const u32*)((const u8*)blk + s->mAttribTableOffset);
            printf(": %u shapes; shape 0 mtx type %u, %u groups, radius %.1f, bbox %.0f..%.0f; desc %u/%u; draw 0: %u bytes at %u",
                   (unsigned)s->mShapeNum, sh[0].mShapeMtxType, sh[0].mMtxGroupNum, sh[0].mRadius, sh[0].mMin.x,
                   sh[0].mMax.x, (unsigned)desc[0], (unsigned)desc[1], (unsigned)draw[0].mDisplayListSize,
                   (unsigned)draw[0].mDisplayListIndex);
            CHECK(sh[0].mMtxGroupNum > 0 && sh[0].mMtxGroupNum < 1024, "shape matrix groups");
            CHECK(draw[0].mDisplayListSize > 0 && draw[0].mDisplayListSize < 0x100000 && (draw[0].mDisplayListSize & 31) == 0,
                  "display list size");
            break;
        }
        case J3DFBT_Material: {
            const J3DMaterialBlock* m = (const J3DMaterialBlock*)blk;
            const J3DMaterialInitData* init = (const J3DMaterialInitData*)((const u8*)blk + (u32)m->mMatEntryDataOffset);
            const u16* remap = (const u16*)((const u8*)blk + (u32)m->mMatRemapTableOffset);
            printf(": %u materials; material 0 = init %u: PE mode %u, colour index %u", (unsigned)m->mNumMaterials, remap[0],
                   init[remap[0]].mPixelEngineMode, init[remap[0]].mMatColorIndex[0]);
            CHECK(remap[0] < m->mNumMaterials, "material remap");
            break;
        }
        case J3DFBT_Texture: {
            const J3DTextureBlock* t = (const J3DTextureBlock*)blk;
            const ResTIMG* timg = (const ResTIMG*)((const u8*)blk + (u32)t->mTexHeaderOffset);
            printf(": %u textures; texture 0 %ux%u format %u", (unsigned)t->mTextureCount, (unsigned)timg->mSizeX,
                   (unsigned)timg->mSizeY, timg->mTextureFormat);
            CHECK(timg->mSizeX > 0 && timg->mSizeX <= 1024, "texture size");
            break;
        }
        }
        printf("\n");
    }
}

static void checkAnm(const char* name, const u8* file)
{
    const J3DFileHeader* hdr = (const J3DFileHeader*)file;
    char a[5], b[5];
    tag4(hdr->mJ3dVersion, a);
    tag4(hdr->mFileVersion, b);
    J3DPortSwapAnmFile(file);
    J3DPortSwapAnmFile(file); // must not swap twice
    const J3DFileBlockBase* blk = hdr->getFirstBlock();
    char t[5];
    tag4(blk->mBlockType, t);
    printf("  %s: %s%s, block %s\n", name, a, b, t);
    if (blk->mBlockType == J3DFBT_AnmTextureSRTKey) {
        const J3DAnmTextureSRTKeyData* d = (const J3DAnmTextureSRTKeyData*)blk;
        const J3DAnmTransformKeyTable* tab = (const J3DAnmTransformKeyTable*)((const u8*)d + d->mTableOffset);
        const f32* scale = (const f32*)((const u8*)d + d->mScaleValOffset);
        const f32* trans = (const f32*)((const u8*)d + d->mTransValOffset);
        const u16* mat = (const u16*)((const u8*)d + d->mUpdateMatIDOffset);
        printf("    %d frames, %u materials (first %u); table 0 scale %u keys at %u, trans %u keys at %u (type %u); "
               "scale[0] %.3f, trans[0..2] %.3f %.3f %.3f\n",
               d->mTotalFrameCount, (unsigned)d->mExtraInfo1, mat[0], tab[0].mScaleInfo.mMaxFrame,
               tab[0].mScaleInfo.mOffset, tab[0].mTranslationInfo.mMaxFrame, tab[0].mTranslationInfo.mOffset,
               tab[0].mTranslationInfo.mType, scale[0], trans[0], trans[1], trans[2]);
        CHECK(d->mTotalFrameCount > 0 && d->mTotalFrameCount < 100000, "%s: frame count", name);
        CHECK(scale[0] > -100.0f && scale[0] < 100.0f, "%s: scale value", name);
    } else if (blk->mBlockType == J3DFBT_AnmTevRegKey) {
        const J3DAnmTevRegKeyData* d = (const J3DAnmTevRegKeyData*)blk;
        const J3DAnmCRegKeyTable* tab = (const J3DAnmCRegKeyTable*)((const u8*)d + d->mCRegTableOffset);
        const s16* red = (const s16*)((const u8*)d + d->mCRValuesOffset);
        printf("    %d frames, %u C-reg / %u K-reg materials; C table 0 red %u keys at %u (type %u); red keys %d %d %d %d\n",
               d->mTotalFrameCount, d->mCRegUpdateMaterialNum, d->mKRegUpdateMaterialNum, tab[0].mTables[0].mMaxFrame,
               tab[0].mTables[0].mOffset, tab[0].mTables[0].mType, red[0], red[1], red[2], red[3]);
        CHECK(d->mTotalFrameCount > 0 && d->mTotalFrameCount < 100000, "%s: frame count", name);
        CHECK(d->mCRegUpdateMaterialNum + d->mKRegUpdateMaterialNum > 0, "%s: materials", name);
    }
}

// Walks a jpc the way JPAResourceLoader::load_jpc does, through the ported structs.
static void checkJpc(const char* name, const u8* file)
{
    char ver[9] = {0};
    memcpy(ver, file, 8);
    u16 resNum = be16(file + 8), texNum = be16(file + 0xA);
    u32 texOff = be32(file + 0xC);
    printf("  %s: %s, %u resources, %u textures at %u\n", name, ver, resNum, texNum, (unsigned)texOff);
    CHECK(be32s(file + 4) == '2-10', "%s: version", name);
    u32 off = 0x10;
    int shown = 0, blocks = 0;
    for (u16 r = 0; r < resNum; r++) {
        const u8* res = file + off;
        u16 blockNum = be16(res + 2);
        off += 8;
        for (u16 b = 0; b < blockNum; b++, blocks++) {
            const u8* blk = file + off;
            u32 type = be32(blk);
            s32 len = be32s(blk + 4);
            CHECK(len >= 8 && len < 0x100000, "%s: resource %u block %u length %d", name, r, b, (int)len);
            if (len < 8)
                return;
            if (shown < 6) {
                if (type == 'BEM1') {
                    const JPADynamicsBlockData* d = (const JPADynamicsBlockData*)blk;
                    JGeometry::TVec3f scl = d->mEmitterScl;
                    printf("    res %u BEM1: scale %.2f,%.2f,%.2f rate %.2f life %d max frame %d volume %u\n", r, scl.x,
                           scl.y, scl.z, (float)d->mRate, (int)d->mLifeTime, (int)d->mMaxFrame, (unsigned)d->mVolumeSize);
                    CHECK(d->mLifeTime >= 0 && d->mLifeTime < 10000, "emitter life time");
                    shown++;
                } else if (type == 'BSP1') {
                    const JPABaseShapeData* s = (const JPABaseShapeData*)blk;
                    printf("    res %u BSP1: base size %.2fx%.2f, blend %04x, colour frames %d\n", r, (float)s->mBaseSizeX,
                           (float)s->mBaseSizeY, (unsigned)s->mBlendModeCfg, (int)s->mClrAnmFrmMax);
                    CHECK(s->mBaseSizeX >= 0.0f && s->mBaseSizeX < 10000.0f, "base size");
                    shown++;
                } else if (type == 'KFA1') {
                    const JPAKeyBlockData* k = (const JPAKeyBlockData*)blk;
                    const BE<f32>* keys = (const BE<f32>*)(blk + 0xC);
                    printf("    res %u KFA1: %u keys, first (%.2f, %.3f)\n", r, k->mKeyFrameCount, (float)keys[0],
                           (float)keys[1]);
                    shown++;
                }
            }
            off += len;
        }
    }
    printf("    %d blocks walked\n", blocks);
    // Textures: TEX1 blocks holding a ResTIMG at 0x20.
    u32 t = texOff;
    for (u16 i = 0; i < texNum && i < 2; i++) {
        const ResTIMG* timg = (const ResTIMG*)(file + t + 0x20);
        printf("    texture %u \"%.20s\": %ux%u format %u\n", i, (const char*)file + t + 0xC, (unsigned)timg->mSizeX,
               (unsigned)timg->mSizeY, timg->mTextureFormat);
        CHECK(timg->mSizeX > 0 && timg->mSizeX <= 1024, "jpc texture size");
        t += be32(file + t + 4);
    }
}

void testExtra(const char* name, const u8* file, u32 size)
{
    static int bmds = 0, btks = 0, brks = 0;
    if (endsWith(name, ".jpc"))
        checkJpc(name, file);
    if (endsWith(name, ".btk") && btks++ < 1)
        checkAnm(name, file);
    if (endsWith(name, ".brk") && brks++ < 1)
        checkAnm(name, file);
    if (endsWith(name, "_text_e.bmg"))
        checkBmg(name, file);
    if ((endsWith(name, ".bmd") || endsWith(name, ".bdl")) && bmds++ < 2)
        checkBmd(name, file);
}
