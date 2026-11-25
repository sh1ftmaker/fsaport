#include "game/GameAudio.h"
#include <math.h>
#include "game/GameGlobals.h"
#include "game/ActorManager.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/act/act_pengator.h"
#include "game/tu_80212AC0.h"

// .data tables indexed by state * 4 + field_0x240 (update's state 2 and 3).
static s32 lbl_804A1808[16] = {0, 1, 0, 2, 3, 4, 3, 5, 6, 7, 8, 7, 10, 9, 11, 9};
static s32 lbl_804A1848[16] = {4, 9, 4, 9, 4, 9, 4, 9, 9, 4, 9, 4, 9, 4, 9, 4};

// The hit reaction update() runs after updateHitReaction_801F3D94 or checkHits_801F305C reports a
// hit; the original has this code twice (inlined at both checks).
static inline void hitReaction(Pengator_804A1888* self) {
    self->field_0xbc = 2;
    if (self->mPos.z > 0.0f) {
        self->mVel_0x3c.z -= 1.0f;
    } else {
        self->mPos.z = 0.0f;
        self->mVel_0x3c.z = 0.0f;
    }
    self->sweepMapBox_801F2618(NULL, false);
}

// FIXME: 99.00 %, regalloc-only; behaviour clean.
// Regalloc only: the zero literal of the 0x234-0x248 stores
// is in r7 in the original and r8 here (it swaps with the gGameRoot_8053AAF8
// pointer loaded after it).
void Pengator_804A1888::init() {
    mBox.i.x = -8.0f;
    mBox.i.y = -8.0f;
    mBox.f.x = 8.0f;
    mBox.f.y = 8.0f;
    mMapBox_0x80.i.x = -8.0f;
    mMapBox_0x80.i.y = -8.0f;
    mMapBox_0x80.f.x = 8.0f;
    mMapBox_0x80.f.y = 8.0f;
    field_0x240 = 0;
    field_0x244 = 0;
    field_0x248 = 0;
    field_0x234[0] = 0;
    field_0x234[1] = 0;
    field_0x234[2] = 0;
    mDir_0x98 = (s32)(3.99f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
    field_0x24c = mDir_0x98 << 2;
    field_0x258.zero();
    mVel_0x3c.zero();
    field_0xb0 = 0x11101;
    field_0xb8 = 1;
    field_0x195 = 1;
    field_0x254 = 0.0f;
    field_0x250 = 1.0f;
    field_0x230 = 0;
}

void Pengator_804A1888::checkPlayerContact_0x18() {
    fn_80212AC0()->fn_80212AF0(&mPos);
    ActorBase_804A5770::checkPlayerContact_0x18();
}

// FIXME: 99.82 %, regalloc-only; behaviour clean.
// What differs: register choices only (gGameRoot_8053AAF8 in the random
// calls, the frame table index).
void Pengator_804A1888::update() {
    // Entry copies: the box and the position's x, y are read before the calls.
    JGeometry::TBox2f box(mMapBox_0x80);
    JGeometry::TVec2f pos(mPos.x, mPos.y);
    u16 outAngle = 0;
    f32 outDist = 0.0f;
    s32 outPlayer = -1;
    s32 outDir = -1;
    JGeometry::TVec2f center;

    if (updateHitReaction_801F3D94(1)) {
        hitReaction(this);
        return;
    }
    if (checkHits_801F305C()) {
        hitReaction(this);
        return;
    }

    addCalc2_801F71A4(&mVel_0x3c.x, field_0x258.x, 0.1f, field_0x250);
    addCalc2_801F71A4(&mVel_0x3c.y, field_0x258.y, 0.1f, field_0x250);
    mPos += mVel_0x3c;
    fn_80212AC0()->fn_80212AF0(&mPos);
    damagePlayersInBox_801F2B7C(-100, -1, 1);
    findTargetPlayer_801EEC98(mSerial, &outAngle, &outDir, &outDist, &outPlayer, 0, 0);
    u8 moved = sweepMapBox_801F2618(NULL, false);

    box.addPos(pos);
    center.x = box.f.x + 0.5f * (box.i.x - box.f.x);
    center.y = box.f.y + 0.5f * (box.i.y - box.f.y);
    u16 tile = getTileCollisionCodeAlt_802265FC((const JGeometry::TVec3f*)&center, mLayer_0x4);

    field_0x250 = 1.0f;
    if (tile == 0x50) {
        field_0x250 = 0.01f;
    }

    switch (field_0x230) {
    case 0:
        field_0x234[0] = 0;
        field_0x240 = 0;
        field_0xbc = 2;
        field_0x230 = 1;
        // fall through
    case 1:
        if (field_0x234[2] == 0 && mDir_0x98 == outDir) {
            if (((!(mDir_0x98 & 2) && FABS(getPlayerPos_8023E724(outPlayer)->y - mPos.y) < 24.0f) ||
                 ((mDir_0x98 & 2) && FABS(getPlayerPos_8023E724(outPlayer)->x - mPos.x) < 24.0f)) &&
                !fn_801F23BC(mLayer_0x4, &mPos, getPlayerPos_8023E724(outPlayer), &mMapBox_0x80, NULL, NULL, false)) {
                field_0x230 = 2;
                break;
            }
        }
        if (moved || field_0x234[1] == 0) {
            u8 flag = moved;
            if (moved || tile != 0x50) {
                mVel_0x3c.zero();
            }
            field_0x234[1] = (s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x32;
            if (!moved) {
                if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.7f) {
                    if (outDir != -1) {
                        if (mDir_0x98 == outDir) {
                            break;
                        }
                        mDir_0x98 = outDir;
                    }
                } else {
                    field_0x234[0] = 8;
                    flag = 1;
                }
            }
            if (flag) {
                if (!(mDir_0x98 & 2)) {
                    if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.5f) {
                        mDir_0x98 = 2;
                    } else {
                        mDir_0x98 = 3;
                    }
                } else {
                    if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.5f) {
                        mDir_0x98 = 0;
                    } else {
                        mDir_0x98 = 1;
                    }
                }
            }
        }
        switch (mDir_0x98) {
        case 0:
            field_0x258.set(0.7f, 0.0f, 0.0f);
            break;
        case 1:
            field_0x258.set(-0.7f, 0.0f, 0.0f);
            break;
        case 2:
            field_0x258.set(0.0f, 0.7f, 0.0f);
            break;
        case 3:
            field_0x258.set(0.0f, -0.7f, 0.0f);
            break;
        }
        if (field_0x234[0] == 0) {
            field_0x24c = lbl_804A1808[mDir_0x98 * 4 + field_0x240];
            field_0x234[0] = lbl_804A1848[mDir_0x98 * 4 + field_0x240];
            field_0x240++;
            field_0x240 &= 3;
        }
        break;
    case 2:
        mVel_0x3c.zero();
        field_0x258.zero();
        field_0x234[0] = 0;
        field_0x240 = 0;
        field_0x24c = mDir_0x98 + 0xc;
        startSound_801F0E34(0x400AB, 0);
        field_0x254 = 5.0f;
        if (tile != 0x50) {
            field_0x254 = 3.0f;
        }
        switch (mDir_0x98) {
        case 0:
            field_0x258.set(field_0x254, 0.0f, 0.0f);
            mVel_0x3c.set(0.3f * field_0x254, 0.0f, 5.0f);
            break;
        case 1:
            field_0x258.set(-field_0x254, 0.0f, 0.0f);
            mVel_0x3c.set(-(0.3f * field_0x254), 0.0f, 5.0f);
            break;
        case 2:
            field_0x258.set(0.0f, field_0x254, 0.0f);
            mVel_0x3c.set(0.0f, 0.3f * field_0x254, 5.0f);
            break;
        case 3:
            field_0x258.set(0.0f, -field_0x254, 0.0f);
            mVel_0x3c.set(0.0f, -(0.3f * field_0x254), 5.0f);
            break;
        }
        field_0xbc = 3;
        field_0x230 = 3;
        break;
    case 3: {
        f32 rate = 0.05f;
        if (tile != 0x50) {
            rate = 0.1f;
        }
        if (mPos.z > 0.0f) {
            mVel_0x3c.z -= 1.0f;
        }
        addCalc2_801F71A4(&field_0x258.x, 0.0f, 0.1f, rate);
        addCalc2_801F71A4(&field_0x258.y, 0.0f, 0.1f, rate);
        if (mPos.z <= 0.0f) {
            mVel_0x3c.z = 0.0f;
            if (moved || (FABS(mVel_0x3c.x) < 0.1f && field_0x258.x < 0.1f &&
                          FABS(mVel_0x3c.y) < 0.1f && field_0x258.y < 0.1f)) {
                mVel_0x3c.zero();
                field_0x234[2] = (s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x32;
                field_0x230 = 0;
            }
        }
        break;
    }
    }

    for (s32 i = 0; i < 3; i++) {
        if (field_0x234[i] > 0) {
            field_0x234[i]--;
        }
    }
}

void Pengator_804A1888::draw() {
    drawSprite_801F06F0("enemy_gc_sob.bin", field_0x24c + 0x145, NULL, 7, 1.0f, 1.0f);
}

Pengator_804A1888::~Pengator_804A1888() {}
