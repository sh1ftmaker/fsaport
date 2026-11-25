// The native GBA view, first phase: a debug view of what each GBA is sent
// (deluxe; docs/MULTIPLAYER.md §3).
//
// The game sends each player's GBA two things over the link (game/src/game/
// tu_80301CD0.cpp, cls_804AA8B8::fn_80304FB0):
//   files   at boot (fn_80303CE4) the client's data: OBJ tiles and palettes,
//           sprite definitions ("*_sob"), sound, the font; on entering a room
//           the room's BG files for 0x0600C000 / 0x0600E000. Mostly Yaz0.
//   records one per frame (0x68 bytes at most, cls_804AA858Sub11C): kind 5 is
//           the regular view (the four players' marks, the 9-bit BG scroll,
//           up to 19 sprite words, the layer mask), 6-8 carry a file, 10-12
//           a message's characters, 9/13/14 are "nothing to show".
// This keeps a GBA memory image per port (EWRAM, palette RAM, VRAM) with the
// files placed at their addresses, the latest record decoded, and draws a
// 240 x 160 picture of it: the backdrop, the two BG files if present, the
// sprite words as boxes and the player marks as crosses. Which tiles and
// palettes the client program pairs with which sprite id is not known yet
// (the client program is not decompiled); the boxes are where its sprites
// would be drawn.
#include "gba_link.h"

#include "platform.h"

#include <emscripten.h>

#include <cstring>

namespace {
constexpr int kW = 240, kH = 160;

struct View {
  u8 ewram[0x40000];
  u8 pal[0x400];
  u8 vram[0x18000];
  u32 record[0x1A];
  int recordWords = 0;
  u32 records = 0;
  u32 kindCount[16] = {};
  // The files received, newest last (address, size after Yaz0, flag).
  struct File {
    u32 addr, size, packed;
    int flag;
  } files[32];
  int fileCount = 0;
  u16 message[256];
  int messageLen = 0;
  bool bgC000 = false, bgE000 = false;
  u32 rgba[kW * kH];
};
View g_view[FSA_GBA_CHANNELS];

// Yaz0 into dst (at most cap bytes); returns the bytes written.
u32 yaz0(const u8* src, u32 srcSize, u8* dst, u32 cap) {
  if (srcSize < 16 || memcmp(src, "Yaz0", 4) != 0)
    return 0;
  u32 size = (u32)src[4] << 24 | (u32)src[5] << 16 | (u32)src[6] << 8 | src[7];
  if (size > cap)
    size = cap;
  u32 s = 16, d = 0;
  u8 code = 0;
  int bits = 0;
  while (d < size && s < srcSize) {
    if (bits == 0) {
      code = src[s++];
      bits = 8;
    }
    if (code & 0x80) {
      dst[d++] = src[s++];
    } else {
      if (s + 1 >= srcSize)
        break;
      u8 b1 = src[s++], b2 = src[s++];
      u32 dist = ((b1 & 0xF) << 8 | b2) + 1;
      u32 n = b1 >> 4;
      if (n == 0) {
        if (s >= srcSize)
          break;
        n = src[s++] + 0x12;
      } else {
        n += 2;
      }
      if (dist > d)
        break;
      for (u32 k = 0; k < n && d < size; k++, d++)
        dst[d] = dst[d - dist];
    }
    code <<= 1;
    bits--;
  }
  return d;
}

// A GBA address to this view's memory (nullptr outside EWRAM/palette/VRAM).
u8* gba_mem(View& v, u32 addr, u32* room) {
  if (addr >= 0x02000000 && addr < 0x02040000) {
    *room = 0x02040000 - addr;
    return v.ewram + (addr - 0x02000000);
  }
  if (addr >= 0x05000000 && addr < 0x05000400) {
    *room = 0x05000400 - addr;
    return v.pal + (addr - 0x05000000);
  }
  if (addr >= 0x06000000 && addr < 0x06018000) {
    *room = 0x06018000 - addr;
    return v.vram + (addr - 0x06000000);
  }
  return nullptr;
}

u32 bgr555(u16 c) {
  u32 r = (c & 0x1F) << 3, g = ((c >> 5) & 0x1F) << 3, b = ((c >> 10) & 0x1F) << 3;
  return 0xFF000000u | b << 16 | g << 8 | r;  // RGBA in memory order
}

void put(View& v, int x, int y, u32 c) {
  if (x >= 0 && x < kW && y >= 0 && y < kH)
    v.rgba[y * kW + x] = c;
}

void box(View& v, int x, int y, int w, int h, u32 c) {
  for (int i = 0; i < w; i++) {
    put(v, x + i, y, c);
    put(v, x + i, y + h - 1, c);
  }
  for (int j = 0; j < h; j++) {
    put(v, x, y + j, c);
    put(v, x + w - 1, y + j, c);
  }
}

// A GBA text BG (32 x 32 entries of 16 bits) at `map`, 4bpp tiles at `chars`,
// BG palette RAM, scrolled.
void draw_bg(View& v, const u8* map, const u8* chars, u32 charRoom, int sx, int sy) {
  for (int y = 0; y < kH; y++) {
    for (int x = 0; x < kW; x++) {
      int px = (x + sx) & 255, py = (y + sy) & 255;
      const u8* e = map + ((py >> 3) * 32 + (px >> 3)) * 2;
      u16 entry = (u16)(e[0] | e[1] << 8);
      int tile = entry & 0x3FF, pal = entry >> 12;
      int tx = px & 7, ty = py & 7;
      if (entry & 0x400)
        tx = 7 - tx;
      if (entry & 0x800)
        ty = 7 - ty;
      u32 off = (u32)tile * 32 + ty * 4 + (tx >> 1);
      if (off >= charRoom)
        continue;
      int idx = (chars[off] >> ((tx & 1) * 4)) & 0xF;
      if (idx == 0)
        continue;
      const u8* c = v.pal + (pal * 16 + idx) * 2;
      v.rgba[y * kW + x] = bgr555((u16)(c[0] | c[1] << 8));
    }
  }
}

const u32 kPlayerColour[4] = {0xFF30D040, 0xFF3030E0, 0xFFE06030, 0xFFD040C0};  // green, red, blue, purple (RGBA LE)

void render(int chan) {
  View& v = g_view[chan];
  u32 backdrop = bgr555((u16)(v.pal[0] | v.pal[1] << 8));
  if ((backdrop & 0xFFFFFF) == 0)
    backdrop = 0xFF202020;
  for (int i = 0; i < kW * kH; i++)
    v.rgba[i] = backdrop;
  const u32* w = v.record;
  int n = v.recordWords;
  if (n < 7)
    return;
  int kind = (w[0] >> 24) & 0xF;
  const u8* b = (const u8*)w;
  // The marks' bytes hold the BG scroll (game/Map.h, drawPlayerPanel_802FB69C).
  auto scroll = [&](int e0, int e1) {
    u8 a = b[8 + e0 * 4 + 3], c = b[8 + e1 * 4 + 3];
    return (a & 0xF) << 5 | ((a >> 4) & 1) << 4 | (c & 0xF);
  };
  int sx = scroll(0, 1), sy = scroll(2, 3);
  int valid = (w[0] >> 19) & 7;  // mValid: the player is in a sub-area (layer >= 8)
  if (kind != 5 || valid == 0) {
    // Nothing for the GBA screen to show (the player is on the overworld, or
    // no regular record yet): a sheet of the tiles it holds instead, BG
    // characters (VRAM 0x06000000, BG palette 0) above, OBJ characters
    // (0x06010000, OBJ palette 0) below, so the data path can be seen.
    for (int half = 0; half < 2; half++) {
      const u8* chars = v.vram + (half ? 0x10000 : 0);
      const u8* pal = v.pal + (half ? 0x200 : 0);
      for (int t = 0; t < 300; t++) {
        int tx = (t % 30) * 8, ty = half * 80 + (t / 30) * 8;
        for (int py = 0; py < 8; py++)
          for (int px = 0; px < 8; px++) {
            int idx = (chars[t * 32 + py * 4 + (px >> 1)] >> ((px & 1) * 4)) & 0xF;
            if (idx)
              v.rgba[(ty + py) * kW + tx + px] = bgr555((u16)(pal[idx * 2] | pal[idx * 2 + 1] << 8));
          }
      }
    }
  } else if (kind == 5) {
    if (v.bgE000)
      draw_bg(v, v.vram + 0xE000, v.vram + 0xC000, 0x2000, sx, sy);
    // Sprite words: words 6 .. n-2 (the last is the layer mask).
    for (int i = 6; i < n - 1; i++) {
      const u8* s = b + i * 4;
      int y = s[1], x = s[2] | ((s[3] >> 7) & 1) << 8;
      if (y == 0xE0 || x == 0x180)
        continue;
      if (x >= 256)
        x -= 512;
      int sk = s[3] & 7;
      static const u32 kKind[8] = {0xFFFFFFFF, 0xFF00FFFF, 0xFFFF00FF, 0xFFFFFF00, 0xFF8080FF, 0xFF80FF80, 0xFFFF8080, 0xFF808080};
      box(v, x - 4, y - 4, 8, 8, kKind[sk]);
    }
  }
  // Player marks (entries 0-3): x 9 bits, y 8 bits; hidden at (0x180, 0xE0).
  for (int i = 0; i < 4; i++) {
    const u8* e = b + 8 + i * 4;
    int y = e[1], x = e[2] | ((e[3] >> 7) & 1) << 8;
    if (y == 0xE0 || x == 0x180)
      continue;
    if (x >= 256)
      x -= 512;
    for (int d = -3; d <= 3; d++) {
      put(v, x + d, y, kPlayerColour[i]);
      put(v, x, y + d, kPlayerColour[i]);
    }
  }
}
}  // namespace

extern "C" {

void FsaGbaViewReset(int chan) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return;
  View& v = g_view[chan];
  memset(v.ewram, 0, sizeof(v.ewram));
  memset(v.pal, 0, sizeof(v.pal));
  memset(v.vram, 0, sizeof(v.vram));
  v.recordWords = 0;
  v.records = 0;
  memset(v.kindCount, 0, sizeof(v.kindCount));
  v.fileCount = 0;
  v.messageLen = 0;
  v.bgC000 = v.bgE000 = false;
}

void FsaGbaViewRecord(int chan, const u32* words, int count) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return;
  View& v = g_view[chan];
  if (count > 0x1A)
    count = 0x1A;
  int kind = (words[0] >> 24) & 0xF;
  v.records++;
  v.kindCount[kind]++;
  if (kind >= 10 && kind <= 12) {
    // Message characters: halfword pairs, byte-swapped by the game (fn_80304FB0).
    if (kind == 10)
      v.messageLen = 0;
    for (int i = 2; i < count; i++) {
      u32 x = words[i];
      x = (x >> 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | (x << 24);
      for (int h = 0; h < 2 && v.messageLen < 256; h++)
        v.message[v.messageLen++] = (u16)(h ? x >> 16 : x);
    }
    return;
  }
  if (kind == 5 || kind == 9 || kind == 13 || kind == 14) {
    memcpy(v.record, words, count * 4);
    v.recordWords = count;
  }
}

void FsaGbaViewFile(int chan, u32 address, const u8* bytes, u32 size, int flag) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return;
  View& v = g_view[chan];
  u32 addr = address & ~1u;  // 0x05000201: palette RAM 0x200 (the low bit is a flag)
  u32 room = 0;
  u8* dst = gba_mem(v, addr, &room);
  u32 out = 0;
  if (dst) {
    if (flag == 1)
      out = yaz0(bytes, size, dst, room);
    if (out == 0) {
      out = size < room ? size : room;
      memcpy(dst, bytes, out);
    }
    if (addr == 0x0600C000)
      v.bgC000 = true;
    if (addr == 0x0600E000)
      v.bgE000 = true;
  }
  if (v.fileCount == 32) {
    memmove(v.files, v.files + 1, sizeof(v.files[0]) * 31);
    v.fileCount--;
  }
  v.files[v.fileCount++] = {address, out, size, flag};
  platform_log("[gba] port %d: file for %08x, %u bytes sent, %u placed%s\n", chan, address, size, out,
               flag == 1 && out && memcmp(bytes, "Yaz0", 4) == 0 ? " (Yaz0)" : "");
}

// The page: draws port `chan`'s view and returns its 240 x 160 RGBA pixels.
EMSCRIPTEN_KEEPALIVE u32* fsa_gba_render(int chan) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return nullptr;
  render(chan);
  return g_view[chan].rgba;
}

// The panel's numbers: records, then the count per kind (16), the latest
// record's word count and words (26), the file count and files (addr, size,
// packed size, flag), the message length and characters.
EMSCRIPTEN_KEEPALIVE int fsa_gba_view_info(int chan, u32* out) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return 0;
  const View& v = g_view[chan];
  int k = 0;
  out[k++] = v.records;
  for (int i = 0; i < 16; i++)
    out[k++] = v.kindCount[i];
  out[k++] = v.recordWords;
  for (int i = 0; i < 0x1A; i++)
    out[k++] = i < v.recordWords ? v.record[i] : 0;
  out[k++] = v.fileCount;
  for (int i = 0; i < 32; i++) {
    const View::File& f = v.files[i];
    out[k++] = i < v.fileCount ? f.addr : 0;
    out[k++] = i < v.fileCount ? f.size : 0;
    out[k++] = i < v.fileCount ? f.packed : 0;
    out[k++] = i < v.fileCount ? (u32)f.flag : 0;
  }
  out[k++] = v.messageLen;
  for (int i = 0; i < 256; i++)
    out[k++] = i < v.messageLen ? v.message[i] : 0;
  return k;
}

// The view's GBA memory (0: EWRAM 256 KiB, 1: palette RAM 1 KiB, 2: VRAM 96 KiB).
EMSCRIPTEN_KEEPALIVE u8* fsa_gba_memory(int chan, int region) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return nullptr;
  View& v = g_view[chan];
  return region == 0 ? v.ewram : region == 1 ? v.pal : v.vram;
}

}  // extern "C"
