// Reads real files from your own disc image through the port's big-endian structs.
//
//   tools/test_archive/run.sh path/to/G4SE01.iso
//
// Parses the disc FST, loads an archive, Yaz0-decompresses it with the port's
// JKRDecomp::decodeSZS, mounts it the way JKRMemArchive::open does (the port's
// in-place RARC swap), walks its directories, and parses resources with the
// ported structs. Prints what it finds; exits non-zero on anything implausible.
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDecomp.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/ResTIMG.h"
#include "test_formats.h"

#include <stdio.h>
#include <string.h>

// io.c: built against emscripten's libc (the MSL headers have no malloc or files).
extern "C" {
void* test_malloc(unsigned long);
void test_free(void*);
void* test_open(const char*);
int test_read(void* f, unsigned int offset, void* buf, unsigned int size);
}
#define malloc test_malloc
#define free test_free

static int gFail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            printf("FAIL: " __VA_ARGS__);     \
            printf("\n");                     \
            gFail = 1;                        \
        }                                     \
    } while (0)

static void* gDisc;

static u8* readAt(u32 offset, u32 size)
{
    u8* buf = (u8*)malloc(size);
    if (!test_read(gDisc, offset, buf, size)) {
        free(buf);
        return nullptr;
    }
    return buf;
}

// FST: 12-byte entries, big-endian; returns the file's bytes or null.
static u8* discFile(const char* name, u32* size)
{
    u8* hdr = readAt(0x424, 8);
    u8* fst = readAt(be32(hdr), be32(hdr + 4));
    u32 n = be32(fst + 8);
    const char* strtab = (const char*)fst + n * 12;
    for (u32 i = 1; i < n; i++) {
        u8* e = fst + i * 12;
        if (e[0] != 0)
            continue; // directory
        if (strcmp(strtab + (be32(e) & 0xFFFFFF), name) == 0) {
            *size = be32(e + 8);
            return readAt(be32(e + 4), *size);
        }
    }
    return nullptr;
}

struct Mounted {
    JKRArchive::SArcHeader* header;
    JKRArchive::SArcDataInfo* info;
    JKRArchive::SDIDirEntry* dirs;
    JKRArchive::SDIFileEntry* files;
    const char* strings;
    u8* data;
};

static bool mount(const char* name, Mounted* m)
{
    u32 size;
    u8* raw = discFile(name, &size);
    if (!raw) {
        printf("FAIL: %s not on the disc\n", name);
        gFail = 1;
        return false;
    }
    u8* image = raw;
    int comp = JKRDecomp::checkCompressed(raw);
    if (comp == JKRCOMPRESSION_YAZ0) {
        u32 expand = JKRDecompExpandSize(raw);
        image = (u8*)malloc(expand);
        JKRDecomp::decodeSZS(raw, image, expand, 0); // as JKRDecompress(src, dst, expandSize, 0)
        printf("%s: Yaz0, %u -> %u bytes\n", name, size, expand);
    } else {
        image = (u8*)malloc(size);
        memcpy(image, raw, size);
        printf("%s: uncompressed, %u bytes\n", name, size);
    }
    // As JKRMemArchive::open does in the port.
    m->header = (JKRArchive::SArcHeader*)image;
    if (JKRArchive::portSwapHeader(m->header))
        JKRArchive::portSwapInfoBlock((JKRArchive::SArcDataInfo*)(image + m->header->header_length));
    CHECK(m->header->signature == 'RARC', "%s: signature %08x", name, (unsigned)m->header->signature);
    m->info = (JKRArchive::SArcDataInfo*)(image + m->header->header_length);
    m->dirs = (JKRArchive::SDIDirEntry*)((u8*)&m->info->num_nodes + m->info->node_offset);
    m->files = (JKRArchive::SDIFileEntry*)((u8*)&m->info->num_nodes + m->info->file_entry_offset);
    m->strings = (const char*)((u8*)&m->info->num_nodes + m->info->string_table_offset);
    m->data = image + m->header->header_length + m->header->file_data_offset;
    // A second mount of the same buffer must not swap again.
    CHECK(!JKRArchive::portSwapHeader(m->header), "%s: header swapped twice", name);
    return true;
}

static void listArchive(Mounted* m, int maxFiles)
{
    printf("  %u directories, %u entries\n", (unsigned)m->info->num_nodes, (unsigned)m->info->num_file_entries);
    int shown = 0;
    for (u32 d = 0; d < m->info->num_nodes; d++) {
        JKRArchive::SDIDirEntry* dir = &m->dirs[d];
        char type[5] = {(char)(dir->mType >> 24), (char)(dir->mType >> 16), (char)(dir->mType >> 8), (char)dir->mType, 0};
        printf("  dir %-4s %-12s %u entries from %u\n", type, m->strings + dir->mOffset, dir->mNum, (unsigned)dir->mFirstIdx);
        CHECK(dir->mFirstIdx + dir->mNum <= m->info->num_file_entries, "directory runs past the entries");
        for (u32 f = 0; f < dir->mNum; f++) {
            JKRArchive::SDIFileEntry* e = &m->files[dir->mFirstIdx + f];
            if (e->isDirectory())
                continue;
            if (shown++ < maxFiles)
                printf("    %-28s id %3u  %7u bytes at %u\n", m->strings + e->getNameOffset(), e->mFileID,
                       (unsigned)e->mSize, (unsigned)e->mDataOffset);
            CHECK(e->mDataOffset + e->mSize <= m->header->file_data_length, "%s runs past the data",
                  m->strings + e->getNameOffset());
        }
    }
}

static JKRArchive::SDIFileEntry* findBySuffix(Mounted* m, const char* suffix, int skip = 0)
{
    size_t sl = strlen(suffix);
    for (u32 i = 0; i < m->info->num_file_entries; i++) {
        JKRArchive::SDIFileEntry* e = &m->files[i];
        if (e->isDirectory())
            continue;
        const char* n = m->strings + e->getNameOffset();
        size_t nl = strlen(n);
        if (nl >= sl && strcmp(n + nl - sl, suffix) == 0 && skip-- == 0)
            return e;
    }
    return nullptr;
}

static void checkTimg(const char* what, const ResTIMG* t)
{
    printf("  %s: format %u, %ux%u, wrap %u/%u, palette %u entries (fmt %u) at %u, mip %u, images %u, image data at %u\n",
           what, t->mTextureFormat, (unsigned)t->mSizeX, (unsigned)t->mSizeY, t->mWrapS, t->mWrapT,
           (unsigned)t->mPaletteEntryCount, t->mPaletteFormat, (unsigned)t->mPaletteOffset, t->mIsMIPmapEnabled,
           t->mTotalImageCount, (unsigned)t->mImageDataOffset);
    CHECK(t->mSizeX >= 1 && t->mSizeX <= 1024 && t->mSizeY >= 1 && t->mSizeY <= 1024, "%s: implausible size", what);
    CHECK(t->mTextureFormat <= 14, "%s: implausible format", what);
}

static void checkFont(const char* what, const ResFONT* font)
{
    char magic[9] = {0};
    u64 mg = font->mMagic;
    for (int i = 0; i < 8; i++)
        magic[i] = (char)(mg >> (56 - 8 * i));
    printf("  %s: %s, %u bytes, %u blocks\n", what, magic, (unsigned)font->mFileSize, (unsigned)font->mNumBlocks);
    CHECK(strcmp(magic, "FONTbfn1") == 0, "%s: magic", what);
    const BlockHeader* b = (const BlockHeader*)font->mData;
    for (u32 i = 0; i < font->mNumBlocks; i++, b = b->getNext()) {
        u32 m = b->mMagic;
        char tag[5] = {(char)(m >> 24), (char)(m >> 16), (char)(m >> 8), (char)m, 0};
        switch (m) {
        case 'INF1': {
            const ResFONT::InfoBlock* inf = (const ResFONT::InfoBlock*)b;
            printf("    %s type %u ascent %u descent %u width %u leading %u default 0x%x\n", tag,
                   (unsigned)inf->mFontType, (unsigned)inf->mAscent, (unsigned)inf->mDescent, (unsigned)inf->mWidth,
                   (unsigned)inf->mLeading, (unsigned)inf->mDefaultCode);
            CHECK(inf->mAscent + inf->mDescent > 0 && inf->mAscent + inf->mDescent < 256, "font height");
            break;
        }
        case 'GLY1': {
            const ResFONT::GlyphBlock* g = (const ResFONT::GlyphBlock*)b;
            printf("    %s codes 0x%x..0x%x cell %ux%u, sheet %ux%u fmt %u, %ux%u cells, %u bytes/sheet\n", tag,
                   (unsigned)g->mStartCode, (unsigned)g->mEndCode, (unsigned)g->mCellWidth, (unsigned)g->mCellHeight,
                   (unsigned)g->mTextureWidth, (unsigned)g->mTextureHeight, (unsigned)g->mTextureFormat,
                   (unsigned)g->mNumRows, (unsigned)g->mNumColumns, (unsigned)g->mTextureSize);
            CHECK(g->mTextureWidth > 0 && g->mTextureWidth <= 1024 && g->mCellWidth > 0, "glyph sheet");
            break;
        }
        case 'MAP1': {
            const ResFONT::MapBlock* mp = (const ResFONT::MapBlock*)b;
            printf("    %s method %u codes 0x%x..0x%x entries %u\n", tag, (unsigned)mp->mMappingMethod,
                   (unsigned)mp->mStartCode, (unsigned)mp->mEndCode, (unsigned)mp->mNumEntries);
            CHECK(mp->mMappingMethod <= 3, "map method");
            break;
        }
        case 'WID1': {
            const ResFONT::WidthBlock* w = (const ResFONT::WidthBlock*)b;
            printf("    %s codes 0x%x..0x%x\n", tag, (unsigned)w->mStartCode, (unsigned)w->mEndCode);
            break;
        }
        default:
            printf("    unknown block %08x\n", (unsigned)m);
            gFail = 1;
        }
        CHECK(b->mSize > 0 && b->mSize < font->mFileSize, "block size");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: test_archive G4SE01.iso\n");
        return 2;
    }
    gDisc = test_open(argv[1]);
    if (!gDisc) {
        printf("cannot open %s\n", argv[1]);
        return 2;
    }

    Mounted title;
    if (mount("title.arc", &title)) {
        listArchive(&title, 12);
        for (int i = 0; i < 3; i++) {
            JKRArchive::SDIFileEntry* e = findBySuffix(&title, ".bti", i);
            if (e)
                checkTimg(title.strings + e->getNameOffset(), (const ResTIMG*)(title.data + e->mDataOffset));
        }
        CHECK(findBySuffix(&title, ".bti") != nullptr, "title.arc has no .bti");
        testFormats(&title.data, title.strings, (void*)title.files, title.info->num_file_entries);
    }

    Mounted msg;
    if (mount("msgus.arc", &msg)) {
        listArchive(&msg, 8);
        JKRArchive::SDIFileEntry* e = findBySuffix(&msg, ".bfn");
        CHECK(e != nullptr, "msgus.arc has no .bfn");
        if (e)
            checkFont(msg.strings + e->getNameOffset(), (const ResFONT*)(msg.data + e->mDataOffset));
        testFormats(&msg.data, msg.strings, (void*)msg.files, msg.info->num_file_entries);
    }

    printf(gFail ? "\nFAILED\n" : "\nall checks passed\n");
    return gFail;
}
