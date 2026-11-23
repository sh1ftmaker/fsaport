// BMG, J3D and JPA checks through the ported accessors and structs.
#include "test_extra.h"

#include "JSystem/JMessage/data.h"
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

void testExtra(const char* name, const u8* file, u32 size)
{
    if (endsWith(name, "_text_e.bmg"))
        checkBmg(name, file);
}
