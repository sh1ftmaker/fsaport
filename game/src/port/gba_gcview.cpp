// Deluxe: each GBA player's screen drawn by the GameCube's own renderer
// (docs/MULTIPLAYER.md, "GBA screen", approach B).
//
// In single player, when Link is in a house, cave or other sub-area, the game
// draws that view on the TV itself: TileMap_804AA858::drawPlayerPanels_802FF0EC
// -> drawPlayerPanel_802FB69C(player, field_0x11c[player], rect, viewport, ...),
// which renders a player's record exactly as the GBA would show it: the room's
// two tile layers (fn_802FAC10, mValid - 1 and mValid + 7), the sprite words and
// player marks (fn_8030275C / fn_803022C4 with the "SOB " frames), in GBA
// coordinates (an ortho box of 240 x 160) into a viewport. Multiplayer keeps the
// same records for every player (field_0x11c[p], filled by fn_802F53F4 from the
// record sent to GBA p) but only draws small corner panels, or nothing.
//
// This runs that same draw once per frame for each requested port, after the
// game's own frame, into an Aurora offscreen framebuffer (a 2 x 2 grid of
// 240 x 160 panels), and reads it back for the page. It is off unless the page
// asks for it (fsa_gba_gcview(mask)), and it leaves the game as it found it:
// the draw writes a few game objects (the camera's view box and matrices,
// the player's line-shift animation in cls_804AA8D8, TileMap queues), so those
// are saved before and restored after, byte for byte.
#include "game/Camera.h"
#include "game/GameGlobals.h"
#include "game/GameMode.h"
#include "game/GbaClient.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/tu_8030AE84.h"
#include "JSystem/JUtility/JUTVideo.h"
#include "dolphin/gx.h"
#include <string.h>

extern "C" {
void GXCreateFrameBuffer(u32 width, u32 height);
void GXRestoreFrameBuffer(void);
void AuroraReadbackFrameBuffer(u32 left, u32 top, u32 wd, u32 ht, u32 tag);
u32 AuroraReadbackLatest(u8* rgba, u32 capacity, u32* width, u32* height, u32* tag);
u32 AuroraReadbackPending(void);
u32* fsa_gba_render_record(int chan, const u32* words);  // platform/gba_view.cpp: the native view (approach A)
}

namespace {
const s32 kW = 240, kH = 160;
u32 sMask = 0;            // ports to draw (fsa_gba_gcview)
u32 sFrames = 0;          // frames drawn (the readback's tag)
u8 sPixels[kW * 2 * kH * 2 * 4];  // the latest readback, 480 x 320 RGBA
u8 sPort[4][kW * kH * 4];          // one port's panel
// The readback arrives a few frames late, and a new frame is drawn only once the
// previous one has arrived (so the panels update at the GPU's pace, and the
// tags stay in step). So that a comparison shows the same frame, each drawn
// frame keeps which ports it drew and each port's native screen (approach A)
// of that frame's record, by tag.
const s32 kRing = 4;
struct Frame {
    u32 tag;
    u32 drawn;
    u32 native[4][kW * kH];
};
Frame* sRing = NULL;
u8* sSaveMap = NULL;
u8* sSaveCam = NULL;
u8 sSaveShift[sizeof(cls_804AA8D8)];

// A sub-area record: the player is in a house / cave (mValid, layer >= 8) and
// the record is a regular view (kind 5 or 14).
bool inSubArea(const cls_804AA858Sub11C* r) {
    return r != NULL && r->f.mValid != 0 && (r->f.mKind == 5 || r->f.mKind == 14);
}
} // namespace

extern "C" {

// Called by the platform at the end of each game frame, before Aurora ends it
// (platform/vi.cpp, platform_frame_end).
void fsa_gba_gcview_frame(void) {
    if (sMask == 0 || gGameRoot2_8053AB10 == NULL || gGameRoot_8053AAF8 == NULL) {
        return;
    }
    TileMap_804AA858* map = gGameRoot2_8053AB10->field_0x20;
    Camera_804AA9E4* cam = gGameRoot2_8053AB10->field_0x24;
    if (map == NULL || cam == NULL || !isGameModeSet() || gGameRoot_8053AAF8->field_0x4c == 12) {
        return;
    }
    if (sSaveMap == NULL) {
        sSaveMap = new u8[sizeof(TileMap_804AA858)];
        sSaveCam = new u8[sizeof(Camera_804AA9E4)];
        sRing = new Frame[kRing];
        memset(sRing, 0, sizeof(Frame) * kRing);
    }
    if (AuroraReadbackPending()) {
        return;
    }
    u32 fbW = JUTVideo::getManager()->getFbWidth();
    u32 fbH = JUTVideo::getManager()->getEfbHeight();
    if (fbW < (u32)kW * 2 || fbH < (u32)kH * 2) {
        return;
    }
    memcpy(sSaveMap, map, sizeof(TileMap_804AA858));
    memcpy(sSaveCam, cam, sizeof(Camera_804AA9E4));
    // The offscreen target has the logical framebuffer's size, so viewports map 1:1.
    GXCreateFrameBuffer(fbW, fbH);
    u32 drawn = 0;
    for (s32 p = 0; p < 4; p++) {
        if (!(sMask & (1u << p)) || map->field_0x11c[p] == NULL) {
            continue;
        }
        // The record the game is sending this player's GBA now: in a sub-area
        // the GBA shows the room, which the GameCube's copy (field_0x11c) holds.
        if (!inSubArea(&lbl_80506DE8[p]) || !inSubArea(map->field_0x11c[p])) {
            continue;
        }
        cls_804AA8D8* shifts = fn_8022BCD4(p);
        if (shifts != NULL) {
            memcpy(sSaveShift, shifts, sizeof(cls_804AA8D8));
        }
        IntRect_803004C0 box;
        box.field_0x0 = 0;
        box.field_0x4 = 0;
        box.field_0x8 = kW;
        box.field_0xc = kH;
        IntRect_803004C0 view;
        view.field_0x0 = (p & 1) * kW;
        view.field_0x4 = (p >> 1) * kH;
        view.field_0x8 = kW;
        view.field_0xc = kH;
        map->drawPlayerPanel_802FB69C(p, map->field_0x11c[p], &box, &view, 1, 0, 0xFF);
        if (shifts != NULL) {
            memcpy(shifts, sSaveShift, sizeof(cls_804AA8D8));
        }
        drawn |= 1u << p;
    }
    sFrames++;
    AuroraReadbackFrameBuffer(0, 0, kW * 2, kH * 2, sFrames);
    GXRestoreFrameBuffer();
    memcpy(map, sSaveMap, sizeof(TileMap_804AA858));
    memcpy(cam, sSaveCam, sizeof(Camera_804AA9E4));
    Frame& f = sRing[sFrames % kRing];
    f.tag = sFrames;
    f.drawn = drawn;
    for (s32 p = 0; p < 4; p++) {
        if (drawn & (1u << p)) {
            memcpy(f.native[p], fsa_gba_render_record(p, (const u32*)map->field_0x11c[p]->mBytes), sizeof(f.native[p]));
        }
    }
}

// The page (through platform/gba_view.cpp's exports): which ports to draw (bit mask; 0 = off, the default).
void fsa_gba_gcview_set(u32 mask) { sMask = mask & 0xF; }

// The page: port `port`'s latest GameCube-drawn panel (240 x 160 RGBA), or NULL
// when the port is not in a sub-area (nothing drawn: the GBA shows its own
// screen then) or no frame has been read back yet. *serial: the readback's
// serial (changes when a new frame arrives).
u8* fsa_gba_gcview_get(int port, u32* serial) {
    if (port < 0 || port > 3 || sRing == NULL) {
        return NULL;
    }
    u32 w = 0, h = 0, tag = 0;
    u32 s = AuroraReadbackLatest(sPixels, sizeof(sPixels), &w, &h, &tag);
    if (serial != NULL) {
        *serial = s;
    }
    const Frame& f = sRing[tag % kRing];
    if (s == 0 || w != (u32)kW * 2 || h != (u32)kH * 2 || f.tag != tag || !(f.drawn & (1u << port))) {
        return NULL;
    }
    const u8* src = sPixels + ((port >> 1) * kH * w + (port & 1) * kW) * 4;
    for (s32 y = 0; y < kH; y++) {
        memcpy(sPort[port] + y * kW * 4, src + y * w * 4, kW * 4);
    }
    return sPort[port];
}

// The page: port `port`'s native screen (approach A) of the frame that the
// latest GameCube-drawn panel shows, or NULL (as fsa_gba_gcview_get).
u32* fsa_gba_gcview_native(int port) {
    if (port < 0 || port > 3 || sRing == NULL) {
        return NULL;
    }
    u32 tag = 0;
    if (AuroraReadbackLatest(NULL, 0, NULL, NULL, &tag) == 0) {
        return NULL;
    }
    Frame& f = sRing[tag % kRing];
    return f.tag == tag && (f.drawn & (1u << port)) ? f.native[port] : NULL;
}

} // extern "C"
