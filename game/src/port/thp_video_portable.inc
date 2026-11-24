// Port: a portable THP video frame decoder, replacing THPDec.c's (whose
// Huffman decoding, inverse DCT and row stores are paired-single asm with no
// C body, so every movie frame came out as garbage). #included by
// game/thp_read_thread_80427D2C.cpp under FSA_PORT in place of THPDec.c.
//
// A THP frame is baseline JPEG data: SOI, DQT, DHT, SOF0 (8-bit, 3
// components, Y sampled 2x2 and U / V 1x1), optional DRI, SOS, then the
// entropy-coded MCUs. Unlike JPEG the entropy data has no 0xFF00 byte
// stuffing and a restart interval has no RSTn marker: the reader just moves
// to the next byte boundary and the DC predictors reset (as THPDec.c does).
//
// Output: three GX_TF_I8 textures (Y: width x height, U / V: half each way)
// in GX's tiled layout (8 x 4 pixel tiles of 32 bytes, row-major tiles).

#include <math.h>
#include <string.h>

namespace fsa_thp {

struct Huff {
    bool valid;
    u8 count[17];     // codes of each length 1-16
    u8 values[256];
    s32 maxCode[18];  // largest code of each length, -1 if none
    s32 valOffset[17];
    // Fast path: the first 9 bits -> value and length (0: not a short code).
    u8 fastLen[512];
    u8 fastVal[512];
};

struct Decoder {
    const u8* p;
    const u8* end;
    u32 bitBuf;
    s32 bitCount;
    u16 width, height;
    u8 quantSel[3], dcSel[3], acSel[3];
    s32 predDC[3];
    float quant[4][64];   // natural order
    Huff huff[8];         // index: id * 2 + class (0 DC, 1 AC)
    u16 restartInterval;
};

static const u8 kZigzag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

static Decoder sDec;
static float sCos[8][8];  // sCos[x][u] = C(u) / 2 * cos((2x + 1) u pi / 16)
static bool sInit;

static void initTables() {
    for (int x = 0; x < 8; x++) {
        for (int u = 0; u < 8; u++) {
            float c = u == 0 ? (float)(1.0 / sqrt(2.0)) : 1.0f;
            sCos[x][u] = 0.5f * c * (float)cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0);
        }
    }
    sInit = true;
}

static void buildHuff(Huff* h) {
    s32 code = 0, k = 0;
    memset(h->fastLen, 0, sizeof(h->fastLen));
    for (int len = 1; len <= 16; len++) {
        h->valOffset[len] = k - code;
        if (h->count[len]) {
            for (int i = 0; i < h->count[len]; i++, k++, code++) {
                if (len <= 9) {
                    s32 shift = 9 - len;
                    for (s32 f = code << shift; f < ((code + 1) << shift); f++) {
                        h->fastLen[f] = (u8)len;
                        h->fastVal[f] = h->values[k];
                    }
                }
            }
            h->maxCode[len] = code - 1;
        } else {
            h->maxCode[len] = -1;
        }
        code <<= 1;
    }
    h->maxCode[17] = 0x7FFFFFFF;
    h->valid = true;
}

static inline void fill(Decoder* d) {
    while (d->bitCount <= 24) {
        u32 b = d->p < d->end ? *d->p++ : 0;
        d->bitBuf |= b << (24 - d->bitCount);
        d->bitCount += 8;
    }
}

static inline u32 getBits(Decoder* d, s32 n) {
    if (n == 0) {
        return 0;
    }
    fill(d);
    u32 v = d->bitBuf >> (32 - n);
    d->bitBuf <<= n;
    d->bitCount -= n;
    return v;
}

// Moves to the next byte boundary (a restart): drops the partial byte.
static void alignByte(Decoder* d) {
    s32 drop = d->bitCount & 7;
    d->bitBuf <<= drop;
    d->bitCount -= drop;
}

static inline s32 decodeHuff(Decoder* d, const Huff* h) {
    fill(d);
    u32 peek = d->bitBuf >> 23;
    if (h->fastLen[peek]) {
        s32 len = h->fastLen[peek];
        d->bitBuf <<= len;
        d->bitCount -= len;
        return h->fastVal[peek];
    }
    s32 code = 0;
    for (int len = 1; len <= 16; len++) {
        code = (code << 1) | (s32)getBits(d, 1);
        if (code <= h->maxCode[len]) {
            return h->values[code + h->valOffset[len]];
        }
    }
    return 0;  // corrupt data: treat as EOB / zero
}

static inline s32 extend(u32 v, s32 s) {
    return s == 0 ? 0 : (v < (1u << (s - 1)) ? (s32)v - (1 << s) + 1 : (s32)v);
}

// Decodes one 8x8 block of component c into out (natural order, dequantised).
static void decodeBlock(Decoder* d, int c, float* out) {
    const Huff* dc = &d->huff[d->dcSel[c] * 2];
    const Huff* ac = &d->huff[d->acSel[c] * 2 + 1];
    const float* q = d->quant[d->quantSel[c] & 3];
    for (int i = 0; i < 64; i++) {
        out[i] = 0.0f;
    }
    s32 s = decodeHuff(d, dc);
    d->predDC[c] += extend(getBits(d, s), s);
    out[0] = d->predDC[c] * q[0];
    for (int k = 1; k < 64;) {
        s32 rs = decodeHuff(d, ac);
        s32 r = rs >> 4, sz = rs & 15;
        if (sz == 0) {
            if (r != 15) {
                break;  // EOB
            }
            k += 16;    // ZRL
            continue;
        }
        k += r;
        if (k > 63) {
            break;
        }
        s32 z = kZigzag[k];
        out[z] = extend(getBits(d, sz), sz) * q[z];
        k++;
    }
}

// Inverse DCT of a block, level shift, clamp, stored at (bx, by) of an I8
// texture `width` pixels wide in GX's 8 x 4 tiled layout.
static void storeBlock(const float* in, u8* tex, u32 width, u32 bx, u32 by) {
    float tmp[64];
    for (int y = 0; y < 8; y++) {
        for (int u = 0; u < 8; u++) {
            float s = 0.0f;
            for (int v = 0; v < 8; v++) {
                s += sCos[y][v] * in[v * 8 + u];
            }
            tmp[y * 8 + u] = s;
        }
    }
    u32 tilesPerRow = width / 8;
    for (int y = 0; y < 8; y++) {
        u32 py = by + y;
        u8* row = tex + (py / 4) * tilesPerRow * 32 + (py % 4) * 8;
        for (int x = 0; x < 8; x++) {
            float s = 0.0f;
            for (int u = 0; u < 8; u++) {
                s += sCos[x][u] * tmp[y * 8 + u];
            }
            s32 v = (s32)floor(s + 128.5f);
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            u32 px = bx + x;
            row[(px / 8) * 32 + (px % 8)] = (u8)v;
        }
    }
}

static inline u16 be16(const u8* p) { return (u16)(p[0] << 8 | p[1]); }

}  // namespace fsa_thp

bool THPInit() {
    if (!fsa_thp::sInit) {
        fsa_thp::initTables();
    }
    return true;
}

// The frame `file` (its THP component data) into tileY / tileU / tileV.
// Returns 0, or THPDec.c's error codes.
s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV, void* work) {
    using namespace fsa_thp;
    (void)work;
    if (file == NULL) {
        return 25;
    }
    if (tileY == NULL || tileU == NULL || tileV == NULL) {
        return 27;
    }
    if (!sInit) {
        return 29;
    }
    Decoder* d = &sDec;
    memset(d->huff, 0, sizeof(d->huff));
    d->restartInterval = 0;
    d->width = d->height = 0;
    const u8* c = (const u8*)file;
    // A frame is at most a few hundred KB; the bound only stops a runaway read.
    d->end = c + (4 << 20);
    for (;;) {
        if (*c++ != 0xFF) {
            return 3;
        }
        while (*c == 0xFF) {
            c++;
        }
        u8 m = *c++;
        if (m == 0xD8) {
            continue;  // SOI
        }
        if (m == 0xC4) {  // DHT
            s32 len = be16(c) - 2;
            c += 2;
            while (len > 0) {
                u8 tc = *c++;
                Huff* h = &d->huff[((tc & 15) << 1) + (tc >> 4)];
                s32 total = 0;
                h->count[0] = 0;
                for (int i = 1; i <= 16; i++) {
                    h->count[i] = *c++;
                    total += h->count[i];
                }
                memcpy(h->values, c, total > 256 ? 256 : total);
                c += total;
                buildHuff(h);
                len -= 17 + total;
            }
        } else if (m == 0xC0) {  // SOF0
            if (c[2] != 8) {
                return 10;
            }
            d->height = be16(c + 3);
            d->width = be16(c + 5);
            if (c[7] != 3) {
                return 12;
            }
            for (int i = 0; i < 3; i++) {
                u8 samp = c[8 + i * 3 + 1];
                if ((i == 0 && samp != 0x22) || (i > 0 && samp != 0x11)) {
                    return 19;
                }
                d->quantSel[i] = c[8 + i * 3 + 2];
            }
            c += be16(c);
        } else if (m == 0xDD) {  // DRI
            d->restartInterval = be16(c + 2);
            c += 4;
        } else if (m == 0xDB) {  // DQT
            s32 len = be16(c) - 2;
            c += 2;
            while (len > 0) {
                u8 id = *c++ & 3;
                for (int i = 0; i < 64; i++) {
                    d->quant[id][kZigzag[i]] = (float)*c++;
                }
                len -= 65;
            }
        } else if (m == 0xDA) {  // SOS
            if (c[2] != 3) {
                return 12;
            }
            for (int i = 0; i < 3; i++) {
                u8 t = c[3 + i * 2 + 1];
                d->dcSel[i] = t >> 4;
                d->acSel[i] = t & 15;
                if (!d->huff[d->dcSel[i] * 2].valid || !d->huff[d->acSel[i] * 2 + 1].valid) {
                    return 15;
                }
            }
            c += be16(c);
            break;
        } else if ((m >= 0xE0 && m <= 0xEF) || m == 0xFE) {
            c += be16(c);
        } else {
            return 11;
        }
    }
    if (d->width == 0 || d->height == 0 || (d->width & 15) || (d->height & 15)) {
        return 19;
    }

    d->p = c;
    d->bitBuf = 0;
    d->bitCount = 0;
    d->predDC[0] = d->predDC[1] = d->predDC[2] = 0;
    u32 mcusPerRow = d->width / 16;
    u32 mcuRows = d->height / 16;
    u32 untilRestart = d->restartInterval;
    float block[64];
    for (u32 my = 0; my < mcuRows; my++) {
        for (u32 mx = 0; mx < mcusPerRow; mx++) {
            u32 x = mx * 16, y = my * 16;
            decodeBlock(d, 0, block);
            storeBlock(block, (u8*)tileY, d->width, x, y);
            decodeBlock(d, 0, block);
            storeBlock(block, (u8*)tileY, d->width, x + 8, y);
            decodeBlock(d, 0, block);
            storeBlock(block, (u8*)tileY, d->width, x, y + 8);
            decodeBlock(d, 0, block);
            storeBlock(block, (u8*)tileY, d->width, x + 8, y + 8);
            decodeBlock(d, 1, block);
            storeBlock(block, (u8*)tileU, d->width / 2, x / 2, y / 2);
            decodeBlock(d, 2, block);
            storeBlock(block, (u8*)tileV, d->width / 2, x / 2, y / 2);
            if (d->restartInterval && --untilRestart == 0) {
                untilRestart = d->restartInterval;
                alignByte(d);
                d->predDC[0] = d->predDC[1] = d->predDC[2] = 0;
            }
        }
    }
    return 0;
}
