// JAudio checks: walks JaiInit.baa with the port's JAUAudioArcInterpreter and reads
// its banks (IBNK), wave systems (WSYS), sound table (BST) and sequence
// collection (BSC) through the ported structs.
#include "test_extra.h"

#include "JSystem/JAudio/JAUAudioArchive.h"
#include "JSystem/JAudio/JAUSeqCollection.h"
#include "JSystem/JAudio/JAUSoundTable.h"
#include "JSystem/JAudio/System/JASBNKParser.h"
#include "JSystem/JAudio/System/JASWSParser.h"
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

static void tag4(u32 m, char* out)
{
    for (int i = 0; i < 4; i++)
        out[i] = (char)(m >> (24 - 8 * i));
    out[4] = 0;
}

struct TestInterpreter : public JAUAudioArcInterpreter {
    int mWs, mBnk, mCommands;
    TestInterpreter() : mWs(0), mBnk(0), mCommands(0) {}

    void readWS(u32 bankNo, const void* p, u32 id)
    {
        using namespace JASWSParser;
        const THeader* h = (const THeader*)p;
        char t[5];
        tag4(h->magic, t);
        TCtrlGroup* group = h->ctrlGroupOffset.ptr(h);
        TWaveArchiveBank* arcs = h->archiveBankOffset.ptr(h);
        TWaveArchive* arc = arcs->archiveOffsets[0].ptr(h);
        TCtrl* ctrl = group->ctrlSceneOffsets[0].ptr(h)->ctrlOffset.ptr(h);
        if (mWs++ < 3) {
            TWave* w = arc->waveOffsets[0].ptr(h);
            printf("    ws %u: %s %u bytes, %u groups; group 0: %u waves in \"%s\"; wave 0 rate %.0f, %u bytes at %u, loop %d..%d\n",
                   bankNo, t, (unsigned)h->size, (unsigned)group->ctrlGroupCount, (unsigned)ctrl->waveCount,
                   arc->fileName, (float)w->_04, (unsigned)w->_0C, (unsigned)w->offset, (int)w->_14, (int)w->_18);
            CHECK((float)w->_04 >= 4000.0f && (float)w->_04 <= 48000.0f, "wave sample rate");
        }
        CHECK(h->magic == 'WSYS', "WSYS magic");
        CHECK(group->ctrlGroupCount > 0 && group->ctrlGroupCount < 256, "WSYS group count");
        CHECK(ctrl->waveCount > 0 && ctrl->waveCount < 4096, "WSYS wave count");
    }
    void readBNK(u32 bankNo, const void* p)
    {
        using namespace JASBNKParser;
        const TFileHeader* h = (const TFileHeader*)p;
        char t[5];
        tag4(h->id, t);
        int chunks = 0, insts = 0;
        const u8* end = (const u8*)p + h->mSize;
        const Ver1::TChunk* c = (const Ver1::TChunk*)((const u8*)p + 0x20);
        // As Ver1::findChunk walks: ((chunk + 0xb + size) & ~3).
        while ((const u8*)c < end && chunks < 64) {
            if (c->mID == 'LIST') {
                const Ver1::TListChunk* l = (const Ver1::TListChunk*)c;
                for (u32 i = 0; i < l->count; i++)
                    if (l->mOffsets[i] != 0)
                        insts++;
            }
            chunks++;
            c = (const Ver1::TChunk*)(((u32)c + 0xb + c->mSize) & ~3);
        }
        if (mBnk++ < 3)
            printf("    bnk %u: %s %u bytes, global id %u, version %u, %d chunks, %d instruments\n", bankNo, t,
                   (unsigned)h->mSize, (unsigned)h->mGlobalId, (unsigned)h->mVersion, chunks, insts);
        CHECK(h->id == 'IBNK' && h->mVersion <= 1, "IBNK header");
        CHECK(chunks > 0 && chunks < 64, "IBNK chunk walk ends");
    }
    void readBSC(const void* p, u32 size)
    {
        JAUSeqCollection c;
        c.init(p);
        printf("    bsc: %u bytes, valid %d, %u groups, size %d\n", (unsigned)size, c.isValid(), c.groupId_, c.size_);
    }
    void readBST(const void* p, u32 size)
    {
        JAUSoundTable_<JAUSoundTableRoot, JAUSoundTableSection, JAUSoundTableGroup, void> t;
        t.init(p);
        CHECK(t.mRes != NULL, "BST magic");
        if (!t.mRes)
            return;
        u32 sections = t._4->mSectionNumber, groups = 0, items = 0;
        for (u32 s = 0; s < sections; s++) {
            JAUSoundTableSection* sec = t.getSection(s);
            if (!sec)
                continue;
            for (u32 g = 0; g < sec->mNumGroups; g++) {
                JAUSoundTableGroup* grp = t.getGroup(sec, g);
                if (grp) {
                    groups++;
                    items += grp->mNumItems;
                }
            }
        }
        JAUSoundTableGroup* g0 = t.getGroup(t.getSection(0), 0);
        printf("    bst: %u bytes, %u sections, %u groups, %u sounds; section 0 group 0: %u items, first type %u at %u\n",
               (unsigned)size, (unsigned)sections, (unsigned)groups, (unsigned)items, (unsigned)g0->mNumItems,
               g0->getTypeID(0), (unsigned)g0->getItemOffset(0));
        CHECK(sections > 0 && sections < 64 && items > 0 && items < 100000, "BST counts");
    }
    void readBSTN(const void*, u32 size) { printf("    bstn: %u bytes\n", (unsigned)size); }
    void readBMS(u32 id, const void*, u32 size) { printf("    bms %u: %u bytes\n", (unsigned)id, (unsigned)size); }
    void readBMS_fromArchive(u32 id) { printf("    bmsa %u\n", (unsigned)id); }
    void newVoiceBank(u32 w, u32 b) { printf("    vbnk wave bank %u -> bank %u\n", (unsigned)w, (unsigned)b); }
    void newDynamicSeqBlock(u32 size) { printf("    dsqb %u bytes\n", (unsigned)size); }
    void readBSFT(const void*) { printf("    bsft\n"); }
    void readMaxSeCategory(int c, int a, int i) { printf("    sect category %d: %d active / %d inactive\n", c, a, i); }
    void beginBNKList(u32 off, u32 n) { printf("    bank list: %u entries\n", (unsigned)n); }
    void endBNKList() {}
};

void testAudio(const u8* baa, u32 size)
{
    printf("  JaiInit.baa: %u bytes\n", (unsigned)size);
    TestInterpreter it;
    bool ok = it.parse(baa);
    printf("    parse %s, %d wave systems, %d banks, ended %u bytes in\n", ok ? "ok" : "FAILED", it.mWs, it.mBnk,
           (unsigned)(it.mReadPtr - baa));
    CHECK(ok, "JaiInit.baa parse");
    CHECK(it.mWs > 0 && it.mBnk > 0, "JaiInit.baa has wave systems and banks");
}
