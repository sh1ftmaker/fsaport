#include "game/GameAudio.h"

#include "game/GameGlobals.h"
#include "game/ViewLayer.h"
#include "game/Map.h"
#include "game/GameState.h"

// Raw tile codes (12).
static const u16 lbl_8046AAB0[12] = {
    0x095, 0x096, 0x097, 0x098, 0x0A4, 0x0A5, 0x0A6, 0x0A7,
    0x0B4, 0x0B5, 0x0B6, 0x0B7,
};

// Collision codes (nine and the 0 that ends them).
static const u16 lbl_8046AAC8[10] = {
    0x00D, 0x03B, 0x03C, 0x03D, 0x05C, 0x05D, 0x05E, 0x05F,
    0x03E, 0x000,
};

// Five collision codes and the 0 that ends them.
static const u16 lbl_8046AADC[6] = {
    0x039, 0x058, 0x059, 0x05A, 0x05B, 0x000,
};

// Raw tile codes by stage, each list ended by 0: 13, 13 and 5.
static const u16 lbl_8046AAE8[14] = {
    0x270, 0x271, 0x272, 0x274, 0x275, 0x27B, 0x27D,
    0x27F, 0x2A0, 0x2A1, 0x2A2, 0x2A5, 0x2A7, 0x000,
};

static const u16 lbl_8046AB04[14] = {
    0x270, 0x271, 0x272, 0x27D, 0x27F, 0x2A0, 0x2A1,
    0x2A2, 0x2A5, 0x2A7, 0x056, 0x057, 0x361, 0x000,
};

static const u16 lbl_8046AB20[6] = {
    0x262, 0x265, 0x267, 0x26A, 0x26D, 0x000,
};

// Raw tile codes (0x5E).
static const u16 lbl_8046AB2C[0x5E] = {
    0x09B, 0x09C, 0x2E0, 0x2E1, 0x2F0, 0x2F1, 0x2E2, 0x2F2,
    0x2E3, 0x2F3, 0x30A, 0x300, 0x301, 0x30B, 0x31A, 0x310,
    0x311, 0x31B, 0x30C, 0x302, 0x312, 0x31C, 0x30D, 0x303,
    0x313, 0x31D, 0x308, 0x318, 0x309, 0x319, 0x348, 0x349,
    0x358, 0x359, 0x34A, 0x35A, 0x34B, 0x35B, 0x30A, 0x304,
    0x305, 0x30B, 0x31A, 0x314, 0x315, 0x31B, 0x30C, 0x306,
    0x316, 0x31C, 0x30D, 0x307, 0x317, 0x31D, 0x330, 0x331,
    0x322, 0x323, 0x325, 0x335, 0x326, 0x336, 0x330, 0x30E,
    0x30E, 0x331, 0x322, 0x31E, 0x31E, 0x323, 0x325, 0x30F,
    0x30F, 0x335, 0x326, 0x31F, 0x31F, 0x336, 0x3A8, 0x3A9,
    0x3B8, 0x3B9, 0x3AA, 0x3BA, 0x3AB, 0x3BB, 0x390, 0x391,
    0x382, 0x383, 0x385, 0x395, 0x386, 0x396,
};

// Two pairs of raw tile codes.
static const u16 lbl_8053FF48[2] = { 0x074, 0x146 };
static const u16 lbl_8053FF4C[2] = { 0x08A, 0x231 };

// Clamps a box-relative coordinate to the 9-bit cell range [0, 0x200).
static inline void clampCell(s32& v) {
    if (v < 0) {
        v = 0;
    } else if (v >= 0x200) {
        v = 0x1FF;
    }
}

template <class V>
static inline bool insideBox(const JGeometry::TBox2f& b, const V* p) {
    return p->x >= b.i.x && p->y >= b.i.y && p->x < b.f.x && p->y < b.f.y;
}

// Whether code is one of the first n entries of list.
static inline bool inList(u16 code, const u16* list, s32 n) {
    for (s32 i = 0; i < n; i++) {
        if (code == list[i]) {
            return true;
        }
    }
    return false;
}

// inList for the two-entry lists: their callers keep the answer in a flag and leave the loop.
static inline bool inListFlag(u16 code, const u16* list, s32 n) {
    bool result = false;
    for (s32 i = 0; i < n; i++) {
        if (code == list[i]) {
            result = true;
            break;
        }
    }
    return result;
}

// The raw tile code at pos in the layer's box (0xF outside), as getRawTile_80226850 computes it.
// layer is an int here: the original copies it before handing it on as the tile map's s32.
template <class V>
static inline u16 rawTileAt(const V* pos, int layer) {
    GameView_804A9950* view = getGameView_802D6A9C();
    const JGeometry::TBox2f* src = layer < 8 ? &view->mVisibleBox : &view->field_0xdc;
    JGeometry::TBox2f box;
    box = *src;
    if (!insideBox(box, pos)) {
        return 0xF;
    }
    s32 x = (s32)(pos->x - box.i.x);
    s32 y = (s32)(pos->y - box.i.y);
    clampCell(x);
    clampCell(y);
    return gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, ((y & 0x100) << 1) | (x & 0x100) | (y & 0xF0) | ((x >> 4) & 0xF)) & 0x3FF;
}

// field_0x60[row][col] == value for the current screen.
static inline bool screenValueIs(s32 value) {
    // The original reads the column, fetches the view for the row, then the view for the table.
    s32 col = getGameView_802D6A9C()->mScreenX_0x14;
    GameView_804A9950* view = getGameView_802D6A9C();
    return getGameView_802D6A9C()->field_0x60[view->mScreenY_0x18][col] == value;
}

// The tile's collision code: the raw tile code (as getRawTile_80226850) through lbl_8046CB88, adjusted
// when the layer's GameView byte 0x131DF + layer is set (0 -> 2, 0x61 -> 0) and on stage (7, 2) /
// (7, 3) (0xE -> 0x51).
template <class V>
static inline int collisionCodeAt(const V* pos, int layer) {
    u16 code = lbl_8046CB88[rawTileAt(pos, layer)];
    if (viewLayerFlag(layer) != 0) {
        if (code == 0) {
            return 2;
        } else if (code == 0x61) {
            return 0;
        }
        return code;
    } else if (code == 0xE && gGameRoot_8053AAF8->field_0x7f != 0) {
        if ((getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 2) ||
            (getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 3 &&
             screenValueIs(0x10) &&
             pos->y > 2470.0f)) {
            return 0x51;
        }
    }
    return code;
}

// The corner of cell `index` (the inverse of getCellIndexVec3_80226AE0); z is cleared.
// FIXME(shotgun): 84.33 %, scheduling+regalloc; behaviour clean.
void cellToPos_80226CA8(s32 index, s32 layer, JGeometry::TVec3f* out) {
    JGeometry::TVec3f p = *out;
    GameView_804A9950* view = getGameView_802D6A9C();
    const JGeometry::TBox2f* src = layer < 8 ? &view->mVisibleBox : &view->field_0xdc;
    s32 yOff = ((index >> 1) & 0x100) + (index & 0xF0);
    s32 xOff = (index & 0x100) + ((index & 0xF) << 4);
    JGeometry::TBox2f box;
    box = *src;
    p.set((f32)xOff + box.i.x, (f32)yOff + box.i.y, 0.0f);
    *out = p;
}

// The packed cell index of pos (no lookup; clamped like the queries, no outside test).
// The y term is written first to match the original's instruction order.
s32 getCellIndex_80226BC4(const JGeometry::TVec2f* pos, s32 layer) {
    GameView_804A9950* view = getGameView_802D6A9C();
    const JGeometry::TBox2f* src = layer < 8 ? &view->mVisibleBox : &view->field_0xdc;
    JGeometry::TBox2f box;
    box = *src;
    s32 x = (s32)(pos->x - box.i.x);
    s32 y = (s32)(pos->y - box.i.y);
    clampCell(x);
    clampCell(y);
    return ((y & 0x100) << 1) | (x & 0x100) | (y & 0xF0) | ((x >> 4) & 0xF);
}

// The packed cell index of pos (as getCellIndex_80226BC4, on a Vec3).
s32 getCellIndexVec3_80226AE0(const JGeometry::TVec3f* pos, s32 layer) {
    GameView_804A9950* view = getGameView_802D6A9C();
    const JGeometry::TBox2f* src = layer < 8 ? &view->mVisibleBox : &view->field_0xdc;
    JGeometry::TBox2f box;
    box = *src;
    s32 x = (s32)(pos->x - box.i.x);
    s32 y = (s32)(pos->y - box.i.y);
    clampCell(x);
    clampCell(y);
    return ((y & 0x100) << 1) | (x & 0x100) | (y & 0xF0) | ((x >> 4) & 0xF);
}

// The same as getRawTile_80226850 (only the register allocation differs in the original).
// FIXME(shotgun): 99.15 %, regalloc-only; behaviour clean.
u16 getTileAt_80226998(const JGeometry::TVec3f* pos, s32 layer) {
    return rawTileAt(pos, layer);
}

// The raw tile code at pos, 0xF outside the layer's box.
u16 getRawTile_80226850(const JGeometry::TVec3f* pos, s32 layer) {
    return rawTileAt(pos, layer);
}

// The same result as getTileCollisionCode_8022461C (compiled branch-free in the original).
// FIXME: 96.07 %, scheduling+regalloc; behaviour clean.
u16 getTileCollisionCodeAlt_802265FC(const JGeometry::TVec3f* pos, s32 layer) {
    u16 raw = rawTileAt(pos, layer);
    u16 code = lbl_8046CB88[raw];
    if (viewLayerFlag(layer) != 0) {
        if (code == 0) {
            return 2;
        }
        return code == 0x61 ? 0 : code;
    } else if (code == 0xE && gGameRoot_8053AAF8->field_0x7f != 0) {
        if ((getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 2) ||
            (getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 3 &&
             screenValueIs(0x10) &&
             pos->y > 2470.0f)) {
            return 0x51;
        }
    }
    return code;
}

bool fn_802265DC(const JGeometry::TVec3f* pos, s32 layer, bool flag) {
    return fn_802260DC((const JGeometry::TVec2f*)pos, layer, flag);
}

// Whether the tile at pos blocks, from its collision code (and the position inside the cell for
// the half-cell codes); `flag` selects further cases.
// FIXME(shotgun): 99.56 %, scheduling+regalloc; behaviour clean.
bool fn_802260DC(const JGeometry::TVec2f* pos, s32 layer, bool flag) {
    s32 code = (u16)collisionCodeAt(pos, layer);
    if (code == 0 || code == 2 || code == 0x50) {
        return false;
    }
    if (code == 0x22 && getGameView_802D6A9C()->field_0x170 == 0) {
        return false;
    }
    if (flag == 1 && (code == 0xC || code == 0x39)) {
        return false;
    }
    if (flag == 1 && (code == 3 || code == 0xE || code == 0x3A)) {
        return false;
    }
    if (flag == 1 && code >= 0x4C && code <= 0x4F) {
        return false;
    }
    if (code == 0x25) {
        return (u8)!getGameView_802D6A9C()->field_0x171;
    }
    if (code == 0x26) {
        return (u8)(getGameView_802D6A9C()->field_0x171 == 1);
    }
    // Half and quarter cells: which 8-unit part of the 16-unit cell is solid.
    if (code == 4 && ((s32)pos->y & 0xF) >= 8) {
        return false;
    }
    if (code == 5 && ((s32)pos->y & 0xF) < 8) {
        return false;
    }
    if (code == 6 && ((s32)pos->x & 0xF) >= 8) {
        return false;
    }
    if (code == 7 && ((s32)pos->x & 0xF) < 8) {
        return false;
    }
    if (code == 0x18 && (((s32)pos->x & 0xF) >= 8 || ((s32)pos->y & 0xF) >= 8)) {
        return false;
    }
    if (code == 0x19 && (((s32)pos->x & 0xF) < 8 || ((s32)pos->y & 0xF) >= 8)) {
        return false;
    }
    if (code == 0x1A && (((s32)pos->x & 0xF) >= 8 || ((s32)pos->y & 0xF) < 8)) {
        return false;
    }
    if (code == 0x1B && (((s32)pos->x & 0xF) < 8 || ((s32)pos->y & 0xF) < 8)) {
        return false;
    }
    return true;
}

// Whether the raw tile at pos is one of the codes of lbl_8046AAB0.
bool fn_80225EF4(const JGeometry::TVec3f* pos, s32 layer) {
    return inList(rawTileAt(pos, layer), lbl_8046AAB0, 12);
}

// Whether the collision code at pos is one of the first nine of lbl_8046AAC8.
// FIXME(shotgun): 98.62 %, scheduling+regalloc; behaviour clean.
bool fn_80225BE4(const JGeometry::TVec3f* pos, s32 layer) {
    return inList(collisionCodeAt(pos, layer), lbl_8046AAC8, 9);
}

// FIXME(shotgun): 98.62 %, scheduling+regalloc; behaviour clean.
bool fn_802258D4(const JGeometry::TVec3f* pos, s32 layer) {
    return inList(collisionCodeAt(pos, layer), lbl_8046AAC8, 9);
}

// Whether the collision code at pos is one of the first five of lbl_8046AADC.
// FIXME(shotgun): 98.47 %, scheduling+regalloc; behaviour clean.
bool fn_80225614(const JGeometry::TVec3f* pos, s32 layer) {
    return inList(collisionCodeAt(pos, layer), lbl_8046AADC, 5);
}

bool fn_802255F4(const JGeometry::TVec3f* pos, s32 layer) {
    return fn_802252F4(pos, layer);
}

// Not auto-inlined: fn_802255F4 calls fn_802252F4 out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// Tests the raw tile at pos against the code lists of the current stage.
bool fn_802252F4(const JGeometry::TVec3f* pos, int layer) {
    u16 raw = rawTileAt(pos, layer);
    if (gGameRoot_8053AAF8->field_0x7f != 0) {
        if (getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 0) {
            return inList(raw, lbl_8046AAE8, 13);
        }
        if (getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 2) {
            if (viewLayerFlag(layer) != 0) {
                // Both loops are written out: a miss in either shares the `return false` below.
                for (s32 i = 0; i < 5; i++) {
                    if (raw == lbl_8046AB20[i]) {
                        return true;
                    }
                }
            } else {
                for (s32 i = 0; i < 13; i++) {
                    if (raw == lbl_8046AB04[i]) {
                        return true;
                    }
                }
            }
            return false;
        }
        return false;
    }
    if (lbl_80529DEC.field_0x12c + (lbl_80529DEC.field_0x128 << 2) == 4 ||
        lbl_80529DEC.field_0x12c + (lbl_80529DEC.field_0x128 << 2) == 9) {
        return inList(raw, lbl_8046AAE8, 13);
    }
    return false;
}
#pragma auto_inline reset

// Whether the raw tile at pos is one of the two codes of lbl_8053FF48.
bool fn_8022517C(const JGeometry::TVec3f* pos, s32 layer) {
    return inListFlag(rawTileAt(pos, layer), lbl_8053FF48, 2);
}

bool fn_80225000(const JGeometry::TVec3f* pos, s32 layer) {
    u16 code = rawTileAt(pos, layer);
    for (s32 i = 0; i < 2; i++) {
        if (code == lbl_8053FF48[i]) {
            return true;
        }
    }
    return false;
}

// Whether the raw tile at pos is one of the two codes of lbl_8053FF4C.
bool fn_80224E88(const JGeometry::TVec3f* pos, s32 layer) {
    return inListFlag(rawTileAt(pos, layer), lbl_8053FF4C, 2);
}

bool fn_80224D0C(const JGeometry::TVec3f* pos, s32 layer) {
    u16 code = rawTileAt(pos, layer);
    for (s32 i = 0; i < 2; i++) {
        if (code == lbl_8053FF4C[i]) {
            return true;
        }
    }
    return false;
}

// On the upper layers, whether the tile is 0x1B0 / 0x1BB while GameView byte 0x131F1 is set.
bool fn_80224B88(const JGeometry::TVec3f* pos, s32 layer) {
    u16 code = rawTileAt(pos, layer);
    if (layer < 8 && getGameView_802D6A9C()->field_0x131f1 != 0 && (code == 0x1B0 || code == 0x1BB)) {
        return true;
    }
    return false;
}

bool fn_80224A04(const JGeometry::TVec3f* pos, s32 layer) {
    u16 code = rawTileAt(pos, layer);
    if (layer < 8 && getGameView_802D6A9C()->field_0x131f1 != 0 && (code == 0x1B0 || code == 0x1BB)) {
        return true;
    }
    return false;
}

// Whether the raw tile at pos is one of the 0x5E codes of lbl_8046AB2C (tile table in this
// unit's data, not in the source).
bool fn_8022486C(const JGeometry::TVec3f* pos, s32 layer) {
    return inList(rawTileAt(pos, layer), lbl_8046AB2C, 0x5E);
}

// The tile's collision code: the raw tile code (as getRawTile_80226850) through lbl_8046CB88, adjusted
// when the layer's GameView byte 0x131DF + layer is set (0 -> 2, 0x61 -> 0) and on stage (7, 2) /
// (7, 3) (0xE -> 0x51).
// FIXME(shotgun): 97.30 %, missing/extra; behaviour near.
u16 getTileCollisionCode_8022461C(const JGeometry::TVec3f* pos, s32 layer) {
    return collisionCodeAt(pos, layer);
}
