// BLO, BMG, J3D and JPA checks through the ported structs.
#include "test_formats.h"

#include "JSystem/J2D/J2DManage.h"
#include "JSystem/J2D/J2DMaterialFactory.h"
#include "JSystem/J2D/J2DPane.h"
#include "JSystem/J3D/J3DFileBlock.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "test_extra.h"

#include <stdio.h>
#include <string.h>

int gFormatFail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            printf("FAIL: " __VA_ARGS__);     \
            printf("\n");                     \
            gFormatFail = 1;                  \
        }                                     \
    } while (0)

static void tag4(u32 m, char* out)
{
    out[0] = (char)(m >> 24);
    out[1] = (char)(m >> 16);
    out[2] = (char)(m >> 8);
    out[3] = (char)m;
    out[4] = 0;
}

static void checkBlo(const char* name, const u8* file)
{
    const J3DFileHeader* hdr = (const J3DFileHeader*)file;
    char a[5], b[5];
    tag4(hdr->mJ3dVersion, a);
    tag4(hdr->mFileVersion, b);
    printf("  %s: %s%s\n", name, a, b);
    CHECK(hdr->mJ3dVersion == 'SCRN' && (hdr->mFileVersion == 'blo1' || hdr->mFileVersion == 'blo2'), "%s: header", name);
    const u8* p = file + sizeof(J3DFileHeader);
    int panes = 0, pictures = 0, texts = 0, windows = 0, depth = 0, shown = 0;
    for (int guard = 0; guard < 10000; guard++) {
        const J2DScrnBlockHeader* blk = (const J2DScrnBlockHeader*)p;
        u32 type = blk->mBloBlockType;
        char t[5];
        tag4(type, t);
        CHECK(blk->mBlockLength >= 8 && blk->mBlockLength < 0x100000, "%s: block %s length %d", name, t, (int)blk->mBlockLength);
        if (blk->mBlockLength < 8)
            return;
        if (type == 'INF1') {
            const J2DScreenInfoBlock* inf = (const J2DScreenInfoBlock*)blk;
            printf("    INF1 %ux%u colour %08x\n", (unsigned)inf->mWidth, (unsigned)inf->mHeight, (unsigned)inf->mColor);
            CHECK(inf->mWidth > 0 && inf->mWidth <= 1024 && inf->mHeight > 0 && inf->mHeight <= 1024, "screen size");
        } else if (type == 'TEX1' || type == 'FNT1') {
            const J2DResReference* ref = (const J2DResReference*)(p + 0x10);
            printf("    %s %u references:", t, (unsigned)ref->mCount);
            for (u16 i = 0; i < ref->mCount && i < 6; i++) {
                const u8* e = (const u8*)ref + ref->mOffsets[i];
                printf(" %.*s", e[1], (const char*)e + 2);
            }
            printf("\n");
            CHECK(ref->mCount < 512, "reference count");
        } else if (type == 'MAT1') {
            const J2DMaterialBlock* mat = (const J2DMaterialBlock*)blk;
            u16 count = be16(p + 8);
            const J2DMaterialInitData* init =
                (const J2DMaterialInitData*)((const u8*)mat + mat->mMatInitDataOffset);
            const BE<u16>* index = (const BE<u16>*)((const u8*)mat + mat->mMatIndexTableOffset);
            const BE<u16>* texNo = (const BE<u16>*)((const u8*)mat + mat->mTexNoOffset);
            const J2DMaterialInitData& m0 = init[index[0]];
            printf("    MAT1 %u materials; material 0: mode %u, tex index %u -> texture %u, blend info %u\n", count,
                   m0.mMode, (unsigned)m0.mTexNoIdx[0],
                   m0.mTexNoIdx[0] != 0xFFFF ? (unsigned)texNo[m0.mTexNoIdx[0]] : 0xFFFF, (unsigned)m0.mBlendInfoIdx);
            CHECK(count > 0 && count < 1024 && index[0] < count, "material count / index table");
            if (mat->_14) {
                const ResNTAB* names = (const ResNTAB*)((const u8*)mat + mat->_14);
                printf("      names: %u, first \"%s\"\n", (unsigned)names->mEntryNum,
                       (const char*)names + names->mEntries[0].mOffs);
                CHECK(names->mEntryNum == count, "material name count");
            }
        } else if (type == 'PAN2' || type == 'PIC2' || type == 'TBX2' || type == 'WIN2') {
            // PIC2/TBX2/WIN2 carry their PAN2 block right after their own header.
            const J2DPaneExBlock* pane = (const J2DPaneExBlock*)(type == 'PAN2' ? p : p + 8);
            if (type != 'PAN2') {
                CHECK(pane->mBloBlockType == 'pan2', "%s: embedded block is not pan2", t);
            }
            if (type == 'PIC2' && pictures < 3) {
                const J2DScrnBlockPictureParameter* pic =
                    (const J2DScrnBlockPictureParameter*)(p + 8 + pane->mBlockLength);
                JGeometry::TVec2<s16> tc0 = pic->mTexCoords[0], tc3 = pic->mTexCoords[3];
                printf("      picture: material %u, texcoords (%d,%d)..(%d,%d), corner colour %08x\n",
                       (unsigned)pic->mMaterialID, tc0.x, tc0.y, tc3.x, tc3.y, (unsigned)pic->mCornerColor[0]);
                CHECK(pic->mMaterialID < 64, "picture material id");
            }
            if (shown++ < 4) {
                u64 tg = pane->mTag;
                char tagName[9] = {0};
                for (int i = 0; i < 8; i++)
                    tagName[i] = (char)(tg >> (56 - 8 * i));
                for (int i = 0; i < 8; i++)
                    if (tagName[i] == 0)
                        tagName[i] = '.';
                printf("    %s tag %s pos %.0f,%.0f size %.0fx%.0f scale %.2f,%.2f angle %.1f\n", t, tagName,
                       (float)pane->mOffsetX, (float)pane->mOffsetY, (float)pane->mWidth, (float)pane->mHeight,
                       (float)pane->mWidthScale, (float)pane->mHeightScale, (float)pane->mAngleZ);
            }
            CHECK(pane->mWidth >= 0 && pane->mWidth < 4096 && pane->mHeight >= 0 && pane->mHeight < 4096,
                  "%s: pane size", t);
            if (type == 'PAN2')
                panes++;
            else if (type == 'PIC2')
                pictures++;
            else if (type == 'TBX2')
                texts++;
            else
                windows++;
        } else if (type == 'BGN1') {
            depth++;
        } else if (type == 'END1') {
            depth--;
        } else if (type == 'EXT1') {
            break;
        } else {
            printf("    other block %s, %d bytes\n", t, (int)blk->mBlockLength);
        }
        p += blk->mBlockLength;
    }
    printf("    %d panes, %d pictures, %d text boxes, %d windows; nesting back to %d\n", panes, pictures, texts, windows,
           depth);
    CHECK(depth == 0, "%s: BGN1/END1 unbalanced", name);
}

void testFormats(u8** data, const char* strings, void* filesv, u32 numFiles)
{
    JKRArchive::SDIFileEntry* files = (JKRArchive::SDIFileEntry*)filesv;
    int blos = 0;
    for (u32 i = 0; i < numFiles; i++) {
        JKRArchive::SDIFileEntry* e = &files[i];
        if (e->isDirectory())
            continue;
        const char* n = strings + e->getNameOffset();
        size_t l = strlen(n);
        const u8* file = *data + e->mDataOffset;
        if (l > 4 && strcmp(n + l - 4, ".blo") == 0 && blos++ < 2)
            checkBlo(n, file);
        testExtra(n, file, e->mSize);
    }
    // Screen animations: each J2D .btk next to a .blo of the same name.
    for (u32 i = 0; i < numFiles; i++) {
        JKRArchive::SDIFileEntry* e = &files[i];
        const char* n = strings + e->getNameOffset();
        size_t l = strlen(n);
        if (e->isDirectory() || l < 5 || strcmp(n + l - 4, ".btk") != 0)
            continue;
        for (u32 j = 0; j < numFiles; j++) {
            const char* m = strings + files[j].getNameOffset();
            if (!files[j].isDirectory() && strlen(m) == l && strncmp(m, n, l - 4) == 0 && strcmp(m + l - 4, ".blo") == 0)
                checkScreenAnm(n, *data + e->mDataOffset, *data + files[j].mDataOffset);
        }
    }
}
