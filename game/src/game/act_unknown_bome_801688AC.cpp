#include "game/GameAudio.h"
#include "game/act/act_unknown_bome_801688AC.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Player.h"
#include "game/Map.h"
#include "game/BgRewrite.h"
#include "game/Camera.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/Vec2Ops.h"
#include "JSystem/JMath/JMath.h"
#include "std/math.h"

extern s32 lbl_804A0718[]; // per-variant angle (s16 units)

// Scans from the 8-pixel grid cell of pos along each axis for a wall tile
// (tile codes 8-15 with bit 0 set), at most 64 steps, and stores the four edges in out;
// layer 0 gives the view box.

// 0x80169DCC.
// FIXME(shotgun): 99.86 %, regalloc-only; behaviour clean.
void UnknownBOME_804A0738::init() {
    mDir_0x98 = (mSpawnParam_0x90 >> 4) & 7;
    field_0x25c = (mSpawnParam_0x90 >> 8) & 1;
    mSpawnParam_0x90 = mSpawnParam_0x90 & 3;
    mMapBox_0x80.i.x = -4.0f;
    mMapBox_0x80.i.y = -4.0f;
    mMapBox_0x80.f.x = 4.0f;
    mMapBox_0x80.f.y = 4.0f;
    field_0x244 = 0;
    field_0x248 = 0;
    field_0x24c = 0;
    field_0x234 = 0;
    field_0x238 = 0;
    field_0x23c = 0;
    field_0x240 = 0;
    for (s32 i = 0; i < 5; i++) {
        field_0x274[i].p0.x = mPos.x;
        field_0x274[i].p0.y = mPos.y;
        field_0x274[i].layer0 = mLayer_0x4;
    }
    for (s32 i = 0; i < 5; i++) {
        JGeometry::TVec3f* pos = getPlayerPos_8023E724(mSpawnParam_0x90);
        field_0x2ec[i].p1.x = pos->x;
        field_0x2ec[i].p1.y = pos->y;
        field_0x2ec[i].layer1 = getPlayerLayer_8023DE58(mSpawnParam_0x90);
    }
    field_0x264 = 0;
    field_0x268 = 0;
    field_0x26c = 0;
    field_0x270 = 0;
    field_0x250 = 0;
    field_0x364.z = 0.0f;
    field_0x364.y = 0.0f;
    field_0x364.x = 0.0f;
    mVel_0x3c.z = 0.0f;
    mVel_0x3c.y = 0.0f;
    mVel_0x3c.x = 0.0f;
    fn_8016890C(&mPos, mLayer_0x4, (JGeometry::TBox2f*)field_0x370);
    s32 angle = lbl_804A0718[mDir_0x98];
    JGeometry::TVec2f dir;
    dir.x = JMASinShort(angle);
    dir.y = JMACosShort(angle);
    dir.scale(1000.0f);
    dir.normalize();
    if (field_0x25c) {
        mVel_0x3c.x = 9.0f * dir.x;
        mVel_0x3c.y = 9.0f * dir.y;
    } else {
        mVel_0x3c.x = 6.0f * dir.x;
        mVel_0x3c.y = 6.0f * dir.y;
    }
    field_0x94 = 0;
    field_0x23c = 4;
    if (mDir_0x98 > 3) {
        mVel_0x3c.x *= 1.2f;
        mVel_0x3c.y *= 1.2f;
    }
    field_0x254 = 0;
    field_0x260 = 0;
    field_0x261 = 0;
    field_0x258 = 300;
    field_0x180 = 60;
    field_0x184 = 80.0f;
    field_0x230 = 0;
}

// Same box-margin check as isOutsideView_801F6758 (actor_manager_801F6058.cpp), on an arbitrary TBox2f.
static inline bool isOutsideBox(const JGeometry::TVec3f* pos, const JGeometry::TBox2f* box, f32 margin) {
    if (pos->x <= box->i.x - margin || pos->x >= box->f.x + margin ||
        pos->y <= box->i.y - margin || pos->y >= box->f.y + margin) {
        return true;
    }
    return false;
}

// Turns d into a direction and sets the velocity: 9 per frame for the fast variant, else 6.
#define AIM_VELOCITY(d, fast)       \
    do {                            \
        (d).normalize();            \
        if (fast) {                 \
            mVel_0x3c.x = 9.0f * (d).x; \
            mVel_0x3c.y = 9.0f * (d).y; \
        } else {                    \
            mVel_0x3c.x = 6.0f * (d).x; \
            mVel_0x3c.y = 6.0f * (d).y; \
        }                           \
    } while (0)

// The |dx|, |dy| <= 16 tests and the other fabs conditions were re-checked against the full asm.
// 0x80169030. A chaser: it follows the trail of positions the player recorded, then the
// player, and is caught by the player (state 1) or lost when it leaves the room.
// FIXME(shotgun): 92.43 %, missing/extra; behaviour clean.
void UnknownBOME_804A0738::update() {
    s32 target = mSpawnParam_0x90;
    if (fn_8022F478(mSpawnParam_0x90) == 1 && hasPlayerLeader_8022F514(mSpawnParam_0x90) == 1) {
        target = getPlayerLeader_8022F3DC(mSpawnParam_0x90);
    }
    if (field_0x23c == 0) {
        if (std::fabsf(mVel_0x3c.x - field_0x364.x) > 0.01f) {
            addCalc2_801F71A4(&mVel_0x3c.x, field_0x364.x, 0.1f, 0.2f);
        } else {
            field_0x364.x = mVel_0x3c.x;
        }
        if (std::fabsf(mVel_0x3c.y - field_0x364.y) > 0.01f) {
            addCalc2_801F71A4(&mVel_0x3c.y, field_0x364.y, 0.1f, 0.2f);
        } else {
            field_0x364.y = mVel_0x3c.y;
        }
    }
    if (isOutsideView_801F6758(&mPos, mLayer_0x4, 300.0f)) {
        mAlive = false;
    }
    if (field_0x268 < 5) {
        JGeometry::TVec3f p;
        p.set(0.0f, 0.0f, 0.0f);
        const JGeometry::TVec2f& p1 = field_0x2ec[field_0x268].p1;
        p.x = p1.x;
        p.y = p1.y;
        if (fn_8022BC38(target, field_0x2ec[field_0x268].layer1, &p) == 1) {
            field_0x2ec[field_0x268].p1 = xy(*getPlayerPos_8023E724(target));
        } else {
            field_0x2ec[field_0x268].layer0 = getPlayerLayer_8023DE58(target);
            field_0x2ec[field_0x268].p0 = xy(*getPlayerPos_8023E724(target));
            field_0x268++;
            if (field_0x268 < 5) {
                field_0x2ec[field_0x268].layer1 = getPlayerLayer_8023DE58(target);
                field_0x2ec[field_0x268].p1 = xy(*getPlayerPos_8023E724(target));
            }
        }
    }
    mPos += mVel_0x3c;
    switch (field_0x230) {
    case 0:
        if (std::fabsf(mVel_0x3c.x - field_0x364.x) <= 1.0f && std::fabsf(mVel_0x3c.y - field_0x364.y) <= 1.0f) {
            field_0x174 = field_0x180;
            mVel_0x3c.x = field_0x364.x;
            mVel_0x3c.y = field_0x364.y;
            mVel_0x3c.z = field_0x364.z;
            if (field_0x244) {
                field_0x26c = field_0x264;
                field_0x230 = 1;
                field_0x174 = 0;
                if (mLayer_0x4 != getPlayerLayer_8023DE58(mSpawnParam_0x90)) {
                    JGeometry::TVec2f d;
                    if (field_0x264 > 0) {
                        d = field_0x274[field_0x264 - 1].p1 - xy(mPos);
                    } else if (field_0x268 > 0) {
                        d = field_0x2ec[0].p1 - xy(mPos);
                    } else {
                        field_0x26c = field_0x264;
                        field_0x230 = 1;
                        field_0x174 = 0;
                    }
                    AIM_VELOCITY(d, field_0x25c != 0);
                }
                break;
            }
            JGeometry::TVec2f d = xy(*getPlayerPos_8023E724(mSpawnParam_0x90)) - xy(mPos);
            if (d.squared() < 240.0f) {
                const JGeometry::TVec3f* pp = getPlayerPos_8023E724(mSpawnParam_0x90);
                mPos = *pp;
                mPos.z = 0.0f;
                mAlive = false;
                break;
            }
        } else if (field_0x264 < 5) {
            if (!field_0x17d) {
                field_0x274[field_0x264].p0 = xy(mPos);
            } else {
                field_0x274[field_0x264].layer1 = mLayer_0x4;
                field_0x274[field_0x264].p1 = xy(mPos);
                field_0x264++;
                if (field_0x264 < 5) {
                    field_0x274[field_0x264].layer0 = mLayer_0x4;
                    field_0x274[field_0x264].p0 = xy(mPos);
                }
                fn_8016890C(&mPos, mLayer_0x4, (JGeometry::TBox2f*)field_0x370);
            }
        }
        if ((mLayer_0x4 == 0 && isOutsideView_801F6758(&mPos, mLayer_0x4, -16.0f) == 1) ||
            (mLayer_0x4 >= 8 && isOutsideBox(&mPos, (const JGeometry::TBox2f*)field_0x370, -9.0f) == 1)) {
            mVel_0x3c.zero();
            field_0x364.z = 0.0f;
            field_0x364.y = 0.0f;
            field_0x364.x = 0.0f;
        }
        if (!fn_8022C274(target)) {
            field_0x244 = 1;
        }
        if (!(field_0x8 == getPlayerLayer_8023DE58(mSpawnParam_0x90) && fn_802373EC(mSpawnParam_0x90) == 3)) {
            field_0x244 = 1;
        }
        if (fn_8022CAF8(mSpawnParam_0x90) == 4 && fn_8022F478(mSpawnParam_0x90) == 1) {
            field_0x244 = 1;
        }
        if (fn_80168C38() == 1 || field_0x254 != 0) {
            mVel_0x3c.scale(-1.0f);
            if (field_0x254 == 0) {
                field_0x254 = 1;
            }
            if (mLayer_0x4 != getPlayerLayer_8023DE58(mSpawnParam_0x90)) {
                JGeometry::TVec2f d;
                if (field_0x264 > 0) {
                    d = field_0x274[field_0x264 - 1].p1 - xy(mPos);
                } else if (field_0x268 > 0) {
                    d = field_0x2ec[0].p1 - xy(mPos);
                } else {
                    field_0x174 = 0;
                    field_0x26c = field_0x264;
                    field_0x230 = 1;
                    break;
                }
                AIM_VELOCITY(d, field_0x25c != 0);
            }
            field_0x174 = 0;
            field_0x26c = field_0x264;
            field_0x230 = 1;
        }
        break;
    case 1:
        if (fn_8022BC38(mSpawnParam_0x90, mLayer_0x4, &mPos) == 1 &&
            (fn_8022D75C(target) != 1 || (fn_8022CAF8(mSpawnParam_0x90) == 4 && target != (s32)mSpawnParam_0x90))) {
            JGeometry::TVec2f d = xy(*getPlayerPos_8023E724(mSpawnParam_0x90)) - xy(mPos);
            if (field_0x260 == 1) {
                mAlive = false;
            } else if (d.squared() < 240.0f) {
                const JGeometry::TVec3f* pp = getPlayerPos_8023E724(mSpawnParam_0x90);
                mPos = *pp;
                mPos.z = 0.0f;
                field_0x260 = 1;
            } else {
                AIM_VELOCITY(d, field_0x25c != 0 && field_0x261 == 0);
            }
        } else {
            JGeometry::TVec2f d;
            if (field_0x26c > 0) {
                d = field_0x274[field_0x26c - 1].p1 - xy(mPos);
            } else if (field_0x270 < field_0x268) {
                d = field_0x2ec[field_0x270].p1 - xy(mPos);
            } else {
                if (mLayer_0x4 != getPlayerLayer_8023DE58(target)) {
                    break;
                }
                d = xy(*getPlayerPos_8023E724(target)) - xy(mPos);
            }
            AIM_VELOCITY(d, field_0x25c > 0 && field_0x261 == 0);
            if ((field_0x264 > 0 && field_0x26c > 0) || (field_0x268 > 0 && field_0x270 < field_0x268)) {
                bool fromFirst;
                Elem_80203594* pt;
                if (field_0x264 > 0 && field_0x26c > 0) {
                    pt = &field_0x274[field_0x26c - 1];
                    fromFirst = true;
                } else {
                    pt = &field_0x2ec[field_0x270];
                    fromFirst = false;
                }
                if (std::fabsf(pt->p1.x - mPos.x) <= 16.0f && std::fabsf(pt->p1.y - mPos.y) <= 16.0f) {
                    JGeometry::TVec3f p;
                    p.set(0.0f, 0.0f, 0.0f);
                    p.x = pt->p0.x;
                    p.y = pt->p0.y;
                    mLayer_0x4 = pt->layer0;
                    mPos = p;
                    if (fromFirst == 1) {
                        field_0x26c--;
                    } else {
                        field_0x270++;
                    }
                }
            }
        }
        fn_80168C38();
        break;
    }
    {
        JGeometry::TVec3f focus(mPos.x, mPos.y - mPos.z, 0.0f);
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&focus);
    }
    if (field_0x234 > 0) {
        field_0x234--;
    }
    if (field_0x238 > 0) {
        field_0x238--;
    }
    if (field_0x23c > 0) {
        field_0x23c--;
    }
    if (field_0x240 > 0) {
        field_0x240--;
    }
    if (field_0x244) {
        field_0x258--;
        if (field_0x258 <= 0) {
            mAlive = false;
        }
    }
    if (field_0x234 == 0) {
        field_0x250++;
        field_0x250 &= 3;
        field_0x234 = 2;
        if (field_0x250 == 0) {
            startSound_801F0E34(0x3008D, 0);
        }
    }
    if (!isOutsideView_801F6758(&mPos, mLayer_0x4, 0.0f)) {
        gBgRewrite_8053AC98->fn_802A20F0(&mPos, mSpawnParam_0x90, mLayer_0x4, 0, 0);
    }
}

// 0x80168E78.
void UnknownBOME_804A0738::draw() {
    SpriteParams_801F5AE0 params;
    params.field_0x30 = 0x11;
    s32 frame = field_0x250 + 0x69;
    if (mLayer_0x4 < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
        gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&mPos, res, frame, &params, 1.0f, 1.0f);
    }
    showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, field_0x250 + 0x69, 0, 2, -1, false, 0, 0);
}


// 0x80168C38. Whether a player standing in the (grown) hit box gets caught; every other
// player is then pushed along.
// FIXME(shotgun): 87.06 %, missing/extra; behaviour drift.
bool UnknownBOME_804A0738::fn_80168C38() {
    const JGeometry::TVec3f* first = getPlayerPos_8023E724(mSpawnParam_0x90);
    s32 found = -1;
    JGeometry::TVec2f d = xy(*first) - xy(mPos);
    JGeometry::TBox2f box(mBox);
    box.addPos(xy(mPos));
    box.i.x -= 4.0f;
    box.i.y -= 4.0f;
    box.f.x += 4.0f;
    box.f.y += 4.0f;
    for (s32 i = 0; i < 4; i++) {
        if (fn_8023EA28(i) != 1) {
            if (!arePlayersInSameGroup_8022E438(i, mSpawnParam_0x90) ||
                (arePlayersInSameGroup_8022E438(i, mSpawnParam_0x90) == 1 && i != getField_0x90() && fn_80235580(i) == 1)) {
                if (fn_8023B0F4(i, &box, mLayer_0x4, false, 0.0f)) {
                    found = i;
                }
            }
        }
    }
    if (found >= 0) {
        if (fn_8022CAF8(found) != 4) {
            for (s32 i = 0; i < 4; i++) {
                if (arePlayersInSameGroup_8022E438(i, found) == 1) {
                    field_0x261 = 1;
                    fn_8022E23C(i, mSerial);
                    fn_8022CC20(i);
                    if (hasPlayerLeader_8022F514(i) == 1) {
                        JGeometry::TVec3f d = mPos;
                        d -= *getPlayerPos_8023E724(i);
                        fn_8023DFF4(i, &d);
                    }
                }
            }
        } else {
            field_0x261 = 1;
            return fn_8022E23C(found, mSerial);
        }
        return true;
    }
    return false;
}

// 0x8016890C.
void UnknownBOME_804A0738::fn_8016890C(const JGeometry::TVec3f* pos, s32 layer, JGeometry::TBox2f* out) {
    if (layer == 0) {
        GameView_804A9950* view = getGameView_802D6A9C();
        const JGeometry::TBox2f& box = (layer < 8) ? view->mVisibleBox : view->field_0xdc;
        out->set(box);
        return;
    }
    JGeometry::TVec3f p = *pos;
    u32 gx = (u32)p.x & ~7;
    u32 gy = (u32)p.y & ~7;
    p.x = (f32)gx;
    p.y = (f32)gy;
    p.z = 0.0f;
    for (s32 n = 0; n < 64; n++) {
        if (p.x >= 512.0f) {
            p.x = 512.0f;
            break;
        }
        u16 tile = getRawTile_80226850(&p, layer);
        if (tile >= 8 && tile < 16 && (tile & 1)) {
            break;
        }
        p.x += 8.0f;
    }
    out->f.x = p.x;
    p.x = (f32)gx;
    for (s32 n = 0; n < 64; n++) {
        if (p.x < 8.0f) {
            p.x = 0.0f;
            break;
        }
        u16 tile = getRawTile_80226850(&p, layer);
        if (tile >= 8 && tile < 16 && (tile & 1)) {
            p.x += 8.0f;
            break;
        }
        p.x -= 8.0f;
    }
    out->i.x = p.x;
    p.x = (f32)gx;
    p.y = (f32)gy;
    p.z = 0.0f;
    for (s32 n = 0; n < 64; n++) {
        if (p.y >= 512.0f) {
            p.y = 512.0f;
            break;
        }
        u16 tile = getRawTile_80226850(&p, layer);
        if (tile >= 8 && tile < 16 && (tile & 1)) {
            break;
        }
        p.y += 8.0f;
    }
    out->f.y = p.y;
    p.y = (f32)gy;
    for (s32 n = 0; n < 64; n++) {
        if (p.y < 8.0f) {
            p.y = 0.0f;
            break;
        }
        u16 tile = getRawTile_80226850(&p, layer);
        if (tile >= 8 && tile < 16 && (tile & 1)) {
            p.y += 8.0f;
            break;
        }
        p.y -= 8.0f;
    }
    out->i.y = p.y;
}

// The original calls both of these with bl (update -> fn_80168C38, init/update -> fn_8016890C);
// our deferred build would inline them.
UnknownBOME_804A0738::~UnknownBOME_804A0738() {}
