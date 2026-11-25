// The native GBA view (deluxe; docs/MULTIPLAYER.md, "GBA screen", approach A):
// each GBA's screen rebuilt from what the game sends it.
//
// The game sends each player's GBA two things over the link (game/src/game/
// tu_80301CD0.cpp, cls_804AA8B8::fn_80304FB0):
//   files   at boot (fn_80303CE4) the client's data: OBJ tiles and palettes,
//           sprite definitions ("*_sob"), sound, the font, the BG metatiles;
//           at a stage start its BG characters and palettes; on entering a
//           room the room's two tile grids for 0x0600C000 / 0x0600E000. Mostly Yaz0.
//   records one per frame (0x68 bytes at most, cls_804AA858Sub11C): kind 5 is
//           the regular view (the four players' marks, the 9-bit BG scroll,
//           up to 19 sprite words, the layer mask), 6-8 carry a file, 10-12
//           a message's characters, 9/13/14 are "nothing to show".
// This keeps a GBA memory image per port (EWRAM, palette RAM, VRAM) with the
// files placed at their addresses, the latest record decoded, and draws the
// 240 x 160 screen from them (render(), below). The client program that does
// this on a real GBA is not decompiled; the drawing rules are the GameCube's
// drawing of the same records, and what neither tells is listed in
// docs/GBA-CLIENT.md.
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
  int markState[4] = {};  // the players' 2-bit states (record word 1, odd phases): the marks' draw keys
  // The sprite table: two halves of 19 words; each regular record in a room
  // replaces the half its phase (mMode bit 0) selects (as the GameCube's own
  // copy, fn_803068D0 into lbl_80506F88, which its drawing reads).
  u32 sprites[38];
  int ownMark = 0;        // this GBA's own mark (its port)
  u32 messageAge = 0;     // records since the latest message record
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
  auto x = [](u32 v) { return v << 3 | v >> 2; };  // 5 bits to 8, full range
  u32 r = x(c & 0x1F), g = x((c >> 5) & 0x1F), b = x((c >> 10) & 0x1F);
  return 0xFF000000u | b << 16 | g << 8 | r;  // RGBA in memory order
}

// ---------------------------------------------------------------------------
// The native GBA renderer (approach A, docs/MULTIPLAYER.md "GBA screen").
//
// Everything here comes from what the game sent this GBA; the drawing rules
// are the GameCube's own drawing of the same records (TileMap_804AA858::
// drawPlayerPanel_802FB69C and what it calls), translated to the GBA's memory:
//
//   room layers   the two room files are tile grids in the GameCube's layout
//                 (u16 metatile ids, 2 x 2 blocks of 16 x 16 metatiles = 512 x
//                 512 pixels): 0x0600E000 the room (fn_802FAC10(mValid - 1)),
//                 0x0600C000 the overlay drawn above the low sprites when the
//                 record's overlay flag (mark 3's scroll bit) is set
//                 (fn_802FAC10(mValid + 7)).
//   metatiles     bg_a_spl (EWRAM 0x0203C000): four GBA BG map entries per
//                 metatile id (char | hflip 0x400 | vflip 0x800 | palette << 12),
//                 top-left, top-right, bottom-left, bottom-right.
//   characters    the stage's BG file at VRAM 0x06000000 (4bpp), BG palettes at
//                 0x05000000; colour 0 of each palette is transparent; the
//                 backdrop is BG colour 0.
//   sprites       the player marks and the sprite table (each record's words
//                 replace the half of 38 its phase selects, as the GameCube's
//                 own copy does, fn_803068D0), each word the frame
//                 `id` of a "SOB " resource (player_sob 0x02010000 for marks and
//                 kinds >= 3, enemy_both_sob 0x02021000 for kind 2,
//                 enemy_both2_sob 0x02018E00 for kinds 0-1): u16 frame offsets
//                 at +2, each frame a list of 6-byte GBA OAM entries (attr0-2)
//                 relative to the word's (x, y), drawn last to first. The OAM
//                 priority bits pick the sheet, as on the GameCube: 0 = OBJ VRAM
//                 0x06010000 (gb_ch_obj_base, boss tiles at +0x7000), 2 =
//                 player_sch (EWRAM 0x02034000, tiles 512-1023), 3 = a sheet the
//                 GBA is never sent (drawn as an outline). 2D mapping, 32 tiles
//                 per row. OBJ palettes at 0x05000200; a word of kind 4-7 and
//                 the marks recolour palette 7 to the player's (12 + n).
//   order         marks and words sorted by (kind key, y): keys < 2 under the
//                 overlay layer, keys >= 2 above it (fn_8030275C, fn_803022C4,
//                 fn_803074A4).
//
// Not known without the client program (docs/GBA-CLIENT.md): the overworld
// screen (record mValid 0), the message window, the line-shift and tint
// effects, animated BG tiles, the spin of "rotating" words, sheet 3, and how
// the client really fits the player sheet into OBJ VRAM.
inline u16 rd16(const u8* p) { return (u16)(p[0] | p[1] << 8); }

u32 colour(const View& v, int palBase, int pal, int idx) {
  const u8* c = v.pal + palBase + (pal * 16 + idx) * 2;
  return bgr555(rd16(c));
}

// One room layer: grid (GameCube layout) at `grid`, scrolled.
void draw_layer(View& v, const u8* grid, int sx, int sy) {
  const u8* defs = v.ewram + 0x3C000;
  for (int y = 0; y < kH; y++) {
    int py = (y + sy) & 511;
    for (int x = 0; x < kW; x++) {
      int px = (x + sx) & 511;
      int mx = px >> 4, my = py >> 4;
      int block = (my >> 4) * 2 + (mx >> 4);
      u16 tile = rd16(grid + ((block * 16 + (my & 15)) * 16 + (mx & 15)) * 2);
      if (tile == 0 || tile >= 0x400)
        continue;
      int sub = ((py >> 3) & 1) * 2 + ((px >> 3) & 1);
      u16 e = rd16(defs + (tile * 4 + sub) * 2);
      int ch = e & 0x3FF, pal = e >> 12;
      int tx = px & 7, ty = py & 7;
      if (e & 0x400)
        tx = 7 - tx;
      if (e & 0x800)
        ty = 7 - ty;
      int idx = (v.vram[ch * 32 + ty * 4 + (tx >> 1)] >> ((tx & 1) * 4)) & 0xF;
      if (idx)
        v.rgba[y * kW + x] = colour(v, 0, pal, idx);
    }
  }
}

// The GBA OBJ sizes in tiles by shape and size.
const u8 kObjSize[3][4][2] = {
    {{1, 1}, {2, 2}, {4, 4}, {8, 8}},
    {{2, 1}, {4, 1}, {4, 2}, {8, 4}},
    {{1, 2}, {1, 4}, {2, 4}, {4, 8}},
};

struct SobDraw {
  int ax, ay;         // the word's position
  int scale;          // 1 or 2
  float xscale;       // horizontal squash (spinning words), 1 otherwise
  bool vflip;         // mirrored (kind 2, id >= 0x200)
  bool translucent;   // alpha 0xA0
  int palMap[16];     // OAM palette -> OBJ palette
};

// Frame `frame` of the SOB resource at EWRAM offset `res` (size `size`).
// Returns false when the frame does not exist.
bool draw_sob(View& v, u32 res, u32 size, int frame, const SobDraw& d) {
  const u8* data = v.ewram + res;
  if ((u32)(frame * 2 + 6) > size)
    return false;
  int start = rd16(data + frame * 2 + 2), end = rd16(data + frame * 2 + 4);
  if (end < start || (u32)end > size)
    return false;
  int count = (end - start) / 6;
  for (int i = count - 1; i >= 0; i--) {
    const u8* o = data + start + i * 6;
    int ey = (s8)o[0];
    int shape = (o[1] >> 6) & 3, sz = (o[3] >> 6) & 3;
    int ex = (s8)o[2];  // bit 8 of x is the sign (as the GameCube reads it)
    bool hf = (o[3] >> 4) & 1, vf = (o[3] >> 5) & 1;
    int tile = o[4] | (o[5] & 3) << 8;
    int pri = (o[5] >> 2) & 3;
    int pal = d.palMap[(o[5] >> 4) & 0xF];
    if (shape == 3)
      continue;
    int tw = kObjSize[shape][sz][0], th = kObjSize[shape][sz][1];
    const u8* sheet;
    int base = tile;
    if (pri == 0) {
      sheet = v.vram + 0x10000;
    } else if (pri == 2 && tile >= 512) {
      sheet = v.ewram + 0x34000;
      base = tile - 512;
    } else {
      sheet = nullptr;
    }
    int w = tw * 8, h = th * 8;
    // Screen rectangle: offsets scaled about the anchor; a mirrored word flips
    // the frame vertically about it.
    float left = d.ax + ex * d.scale * d.xscale;
    float top = d.vflip ? d.ay - (ey + h) * d.scale : d.ay + ey * d.scale;
    float dw = w * d.scale * d.xscale;
    int dh = h * d.scale;
    if (dw < 0)
      dw = -dw;
    int x0 = (int)left, x1 = (int)(left + dw);
    for (int sy = 0; sy < dh; sy++) {
      int yy = (int)top + sy;
      if (yy < 0 || yy >= kH)
        continue;
      int iy = sy / d.scale;
      if (d.vflip)
        iy = h - 1 - iy;
      if (vf)
        iy = h - 1 - iy;
      for (int xx = x0; xx < x1; xx++) {
        if (xx < 0 || xx >= kW)
          continue;
        int ix = (int)((xx - left) * w / dw);
        if (ix < 0 || ix >= w)
          continue;
        if (hf)
          ix = w - 1 - ix;
        u32 c;
        if (sheet == nullptr) {
          // A sheet the GBA does not have: the outline.
          if (sy != 0 && sy != dh - 1 && xx != x0 && xx != x1 - 1)
            continue;
          c = 0xFF808080;
        } else {
          int t = base + (iy >> 3) * 32 + (ix >> 3);
          if (t < 0 || t >= 1024 || (sheet == v.ewram + 0x34000 && t >= 512))
            continue;
          int px = ix & 7, py = iy & 7;
          int idx = (sheet[t * 32 + py * 4 + (px >> 1)] >> ((px & 1) * 4)) & 0xF;
          if (idx == 0)
            continue;
          c = colour(v, 0x200, pal, idx);
        }
        u32& dst = v.rgba[yy * kW + xx];
        if (d.translucent) {
          // ~5/8 of the sprite over the background (GBA alpha blending).
          u32 r = ((c & 0xFF) * 5 + (dst & 0xFF) * 3) >> 3;
          u32 g = (((c >> 8) & 0xFF) * 5 + ((dst >> 8) & 0xFF) * 3) >> 3;
          u32 b = (((c >> 16) & 0xFF) * 5 + ((dst >> 16) & 0xFF) * 3) >> 3;
          c = 0xFF000000u | b << 16 | g << 8 | r;
        }
        dst = c;
      }
    }
  }
  return true;
}

// A record's sprite word's kind key (fn_803074A4).
int sprite_key(int kind, int id) {
  if (kind == 2) {
    if (id >= 0x39 && id < 0x45)
      return 4;
    if (id < 0x25)
      return -1;
    if (id >= 0x200)
      return 1;
    return 0;
  }
  if (kind == 0 || kind == 1) {
    if ((id >= 0xC0 && id < 0xD3) || (id >= 0x155 && id < 0x158))
      return 3;
    if (id >= 0x37 && id < 0x41)
      return 2;
    if (id == 0x3CD)
      return -1;
    if ((id >= 0x2A3 && id < 0x2AA) || (id >= 0x34C && id < 0x37C))
      return 4;
    return kind == 1 ? 1 : 0;
  }
  return 0;
}

struct Item {
  int key, y;
  int order;     // stable tie-break
  bool mark;
  int index;     // mark number or word number
};

void draw_item(View& v, const u8* rec, const Item& it, int mask) {
  const u8* table = (const u8*)v.sprites;
  SobDraw d;
  d.scale = 1;
  d.xscale = 1.0f;
  d.vflip = false;
  d.translucent = false;
  for (int i = 0; i < 16; i++)
    d.palMap[i] = i;
  if (it.mark) {
    // fn_80302C34: a player's mark, in player_sob.
    const u8* e = rec + 8 + it.index * 4;
    int x = e[2] | ((e[3] >> 7) & 1) << 8, y = e[1];
    if (x == 0x180 || y == 0xE0)
      return;
    if (x >= 0x180)
      x -= 0x200;
    if (y >= 0xE0)
      y -= 0x100;
    int id = e[0] | ((e[3] >> 5) & 3) << 8;
    if ((mask >> it.index) & 1) {
      for (int i = 0; i < 16; i++)
        d.palMap[i] = 7;
    } else {
      d.palMap[7] = 12 + it.index;
    }
    d.ax = x;
    d.ay = y;
    draw_sob(v, 0x10000, 35284, id, d);
    return;
  }
  // fn_80302F9C: a sprite word of the table.
  const u8* s = table + it.index * 4;
  int x = s[2] | ((s[3] >> 7) & 1) << 8, y = s[1];
  if (x == 0x180 || y == 0xE0)
    return;
  if (x >= 0x180)
    x -= 0x200;
  if (y >= 0xE0)
    y -= 0x100;
  int id = s[0] | ((s[3] >> 5) & 3) << 8;
  int kind = s[3] & 7;
  bool big = (s[3] >> 3) & 1, spin = (s[3] >> 4) & 1;
  d.ax = x;
  d.ay = y;
  if (kind == 2 && id >= 0x200) {
    id &= 0x1FF;
    d.vflip = true;
    d.scale = big ? 2 : 1;
  } else if (spin) {
    // Turning about the vertical axis: the GameCube steps the angle 45 degrees a
    // frame (lbl_80506D78); the GBA does its own (not known), so this squashes
    // the frame by a cosine of our frame count.
    static const float kCos[8] = {1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f};
    d.xscale = kCos[v.records & 7] * (big ? 1.5f : 0.75f);
    if (d.xscale > -0.1f && d.xscale < 0.1f)
      d.xscale = 0.1f;
    for (int i = 0; i < 16; i++)
      d.palMap[i] = 7;
  } else if (big || (kind == 2 && id >= 0x39 && id < 0x45)) {
    d.scale = 2;
  }
  if (kind < 2) {
    if ((id >= 0x37 && id < 0x41) || id == 0x3CD)
      d.translucent = true;
    else if (id == 0x2A8 || id == 0x2A9)
      id -= 3;
    draw_sob(v, 0x18E00, 30830, id, d);
  } else if (kind == 2) {
    draw_sob(v, 0x21000, 15586, id, d);
  } else {
    if (kind & 4)
      d.palMap[7] = (kind & 3) + 12;
    draw_sob(v, 0x10000, 35284, id, d);
  }
}

// The message window: the characters of the latest message (records 10-12) in
// the font the GBA is sent (moji_english_sch, EWRAM 0x02038000). How the
// client lays the window out is not known; this draws a two-line box at the
// bottom with 8 x 16 cells (character code - 0x20 -> cell, two tiles high).
void draw_message(View& v) {
  if (v.messageLen == 0 || v.messageAge > 600)
    return;
  int top = kH - 40;
  for (int y = top; y < kH; y++)
    for (int x = 0; x < kW; x++) {
      u32& p = v.rgba[y * kW + x];
      p = 0xFF000000u | ((p >> 2) & 0x3F3F3F);
    }
  const u8* font = v.ewram + 0x38000;
  int cx = 4, cy = top + 4;
  for (int i = 0; i < v.messageLen; i++) {
    u16 c = v.message[i];
    if (c == 0 || c == 0x0A || c == 0xFFFF) {
      cx = 4;
      cy += 16;
      if (cy + 16 > kH)
        break;
      continue;
    }
    if (c < 0x20 || c >= 0x20 + 256)
      continue;
    int cell = c - 0x20;
    // 32 cells per row of the 256-pixel sheet, each 8 x 16 (two tile rows).
    int t0 = (cell / 32) * 64 + (cell % 32);
    for (int yy = 0; yy < 16; yy++)
      for (int xx = 0; xx < 8; xx++) {
        int t = t0 + (yy >> 3) * 32;
        if (t >= 512)
          continue;
        int idx = (font[t * 32 + (yy & 7) * 4 + (xx >> 1)] >> ((xx & 1) * 4)) & 0xF;
        if (idx && cx + xx < kW && cy + yy < kH)
          v.rgba[(cy + yy) * kW + cx + xx] = idx == 1 ? 0xFF000000u : 0xFFFFFFFFu;
      }
    cx += 8;
    if (cx + 8 > kW - 4) {
      cx = 4;
      cy += 16;
      if (cy + 16 > kH)
        break;
    }
  }
}

void render(int chan) {
  View& v = g_view[chan];
  u32 backdrop = bgr555(rd16(v.pal));
  for (int i = 0; i < kW * kH; i++)
    v.rgba[i] = backdrop;
  const u32* w = v.record;
  int n = v.recordWords;
  if (n < 7) {
    draw_message(v);
    return;
  }
  const u8* b = (const u8*)w;
  int kind = b[3] & 0xF;
  int valid = (b[2] >> 3) & 7;  // mValid: the player is in a sub-area (layer mValid + 7)
  if ((kind != 5 && kind != 14) || valid == 0) {
    // The overworld (or a menu, a cutscene): the player looks at the TV. What
    // the client shows then is not known (docs/GBA-CLIENT.md); the backdrop.
    draw_message(v);
    return;
  }
  // The marks' spare bits hold the 9-bit scroll (drawPlayerPanel_802FB69C).
  auto scroll = [&](int e0, int e1) {
    u8 a = b[8 + e0 * 4 + 3], c = b[8 + e1 * 4 + 3];
    return (a & 0xF) << 5 | ((a >> 4) & 1) << 4 | (c & 0xF);
  };
  int sx = scroll(0, 1), sy = scroll(2, 3);
  bool overlay = (b[8 + 3 * 4 + 3] >> 4) & 1;
  int mask = b[1] & 0xF;
  draw_layer(v, v.vram + 0xE000, sx, sy);
  // Marks and words in (key, y) order.
  Item items[4 + 38];
  int count = 0;
  for (int i = 0; i < 4; i++) {
    // A mark's key: the player's state (lbl_80506DA8, from the odd-phase word 1)
    // when the mark is set like the player's own, else -1.
    int key = ((mask >> i) & 1) == ((mask >> v.ownMark) & 1) ? v.markState[i] : -1;
    items[count++] = {key, b[8 + i * 4 + 1], count, true, i};
  }
  for (int i = 0; i < 38; i++) {
    const u8* s = (const u8*)&v.sprites[i];
    int id = s[0] | ((s[3] >> 5) & 3) << 8;
    items[count++] = {sprite_key(s[3] & 7, id), s[1], count, false, i};
  }
  for (int i = 1; i < count; i++)
    for (int j = i; j > 0; j--) {
      const Item& a = items[j - 1];
      const Item& c = items[j];
      if (a.key > c.key || (a.key == c.key && (a.y > c.y || (a.y == c.y && a.order > c.order)))) {
        Item t = items[j - 1];
        items[j - 1] = items[j];
        items[j] = t;
      }
    }
  for (int i = 0; i < count; i++)
    if (items[i].key < 2)
      draw_item(v, b, items[i], mask);
  if (overlay)
    draw_layer(v, v.vram + 0xC000, sx, sy);
  for (int i = 0; i < count; i++)
    if (items[i].key >= 2)
      draw_item(v, b, items[i], mask);
  draw_message(v);
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
  memset(v.markState, 0, sizeof(v.markState));
  for (u32& w : v.sprites)
    w = 0x80u << 16 | 0x80u << 24;
  v.messageAge = 0;
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
  v.messageAge++;
  if ((kind == 5 || kind == 14) && ((words[0] >> 19) & 7) != 0 && count >= 8) {
    // fn_803068D0: the words go to the record's half, the rest of it is hidden,
    // and the other half's kind-2 words below id 0x45 are hidden.
    const u32 hidden = 0x80u << 16 | 0x80u << 24;  // x = 0x180
    int half = (words[0] >> 16) & 1;
    u32* dst = v.sprites + half * 19;
    int i = 0;
    for (; i < count - 7 && i < 19; i++)
      dst[i] = words[6 + i];
    for (; i < 19; i++)
      dst[i] = (dst[i] & 0x7F00FFFFu) | hidden;
    u32* other = v.sprites + (half ^ 1) * 19;
    for (i = 0; i < 19; i++) {
      const u8* b = (const u8*)&other[i];
      int id = b[0] | ((b[3] >> 5) & 3) << 8;
      if ((b[3] & 7) == 2 && id < 0x45)
        other[i] = (other[i] & 0x7F00FFFFu) | hidden;
    }
  }
  v.ownMark = chan;
  if (kind >= 10 && kind <= 12) {
    v.messageAge = 0;
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
    const u8* b = (const u8*)words;
    if ((kind == 5 || kind == 14) && (b[2] & 1))  // mMode 1 / 3: word 1 holds the players' states
      for (int i = 0; i < 4; i++)
        v.markState[i] = (b[6] >> (i * 2)) & 3;
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

// Approach B's comparisons: port `chan`'s native screen of the record `words`
// (the GameCube's copy of what it sends that GBA this frame) instead of the
// latest received one.
EMSCRIPTEN_KEEPALIVE u32* fsa_gba_render_record(int chan, const u32* words) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return nullptr;
  View& v = g_view[chan];
  u32 saved[0x1A];
  memcpy(saved, v.record, sizeof(saved));
  int n = v.recordWords < 7 ? 0x1A : v.recordWords;
  memcpy(v.record, words, n * 4);
  int savedN = v.recordWords;
  v.recordWords = n;
  render(chan);
  memcpy(v.record, saved, sizeof(saved));
  v.recordWords = savedN;
  return v.rgba;
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

// Approach B (game/src/port/gba_gcview.cpp): the GameCube's own drawing of each
// port's GBA view. fsa_gba_gcview(mask) turns it on for those ports (0 = off);
// fsa_gba_gcview_pixels(port, &serial) returns the latest 240 x 160 RGBA panel,
// or NULL while that port is not in a sub-area.
u8* fsa_gba_gcview_get(int port, u32* serial);
void fsa_gba_gcview_set(u32 mask);
EMSCRIPTEN_KEEPALIVE void fsa_gba_gcview(u32 mask) { fsa_gba_gcview_set(mask); }
EMSCRIPTEN_KEEPALIVE u8* fsa_gba_gcview_pixels(int port, u32* serial) { return fsa_gba_gcview_get(port, serial); }
// The native screen of the same frame as that panel (for side-by-side comparisons).
u32* fsa_gba_gcview_native(int port);
EMSCRIPTEN_KEEPALIVE u32* fsa_gba_gcview_frame_native(int port) { return fsa_gba_gcview_native(port); }

// The view's GBA memory (0: EWRAM 256 KiB, 1: palette RAM 1 KiB, 2: VRAM 96 KiB).
EMSCRIPTEN_KEEPALIVE u8* fsa_gba_memory(int chan, int region) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return nullptr;
  View& v = g_view[chan];
  return region == 0 ? v.ewram : region == 1 ? v.pal : v.vram;
}

}  // extern "C"
