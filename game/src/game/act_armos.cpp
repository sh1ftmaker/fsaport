#include "std/math.h"
#include "game/ActorManager.h"
#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/Effect.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/act/act_armos.h"
#include "game/act/act_unknown_lnbm_801BEDBC.h"

void Armos_8049F260::init() {
    mBox.i.set(-7.0f, -7.0f);
    mBox.f.set(7.0f, 7.0f);
    mBox2.i.set(-12.0f, -16.0f);
    mBox2.f.set(12.0f, 12.0f);
    mMapBox_0x80.i.set(-7.0f, -7.0f);
    mMapBox_0x80.f.set(7.0f, 7.0f);
    field_0x300 = 0;
    field_0x30c = 0;
    field_0xb0 = 0xA040004;
    field_0x310 = -1;
    field_0x314 = -1;
    field_0x31c = (mSpawnParam_0x90 >> 4) & 1;
    if (field_0x31c != 0) {
        mBox.i.set(-12.0f, -12.0f);
        mBox.f.set(12.0f, 12.0f);
        mParamEfsz_0xc0 = 3;
    }
    field_0x94 = mSpawnParam_0x90 & 0xF;
    field_0x308 = (mSpawnParam_0x90 >> 8) & 0xFF;
    field_0x318 = 1;
    field_0x304 = 0;
    field_0xbc = 3;
    if (field_0x94 == 3 || field_0x94 == 4 || field_0x94 == 5 || field_0x94 == 7) {
        mParamEfsz_0xc0 = 3;
        mBox.i.set(-14.0f, -20.0f);
        mBox.f.set(14.0f, 6.0f);
        mMapBox_0x80.i.set(-14.0f, -20.0f);
        mMapBox_0x80.f.set(14.0f, 6.0f);
        mBox2.i.set(-16.0f, -22.0f);
        mBox2.f.set(16.0f, 8.0f);
    }
    field_0x2f8 = 3;
    switch (field_0x94) {
    case 1:
    case 4:
    case 6:
    case 7:
        field_0x196 = 1;
        mParamDstr_0x1a8 = 0;
        break;
    case 5:
        field_0x196 = 1;
        mParamDstr_0x1a8 = 0;
        field_0xb8 = 1;
        field_0x318 = 0;
        field_0x2f8 = 4;
        break;
    case 3:
        field_0x196 = 1;
        // fall through
    default:
        field_0xb8 = 1;
        field_0x2f8 = 0;
        break;
    }
}


// FIXME(shotgun): 95.40 %, missing/extra; behaviour drift.
// (partial: states 3 tail, 4, 5 not written)
// FIXME(shotgun): state 3 from the branch at 0x80141938 (moving, grid-snapping step), states 4 and 5
// (0x80141FF0, 0x80142024) are not written yet.
void Armos_8049F260::update() {
    JGeometry::TBox2f box = mBox;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    if (updateHitReaction_801F3D94(true) != 0) {
        sweepMapBox_801F2618(NULL, false);
        return;
    }
    if (field_0x2f8 == 2 || field_0x2f8 == 5) {
        if (field_0x94 != 3 && field_0x94 != 5) {
            if (checkHits_801F305C()) {
                if (field_0x2f8 != 2) {
                    field_0x318 = 0;
                    field_0x2f8 = 2;
                }
                if (mLife <= 0) {
                    startSound_801F0E34(0x400A1, 0);
                }
                sweepMapBox_801F2618(NULL, false);
                return;
            }
        } else {
            for (s32 i = 0; i < 4; i++) {
                s32 player = findOverlappingPlayer_801F285C(NULL, NULL, false, i);
                if (player < 0) {
                    continue;
                }
                JGeometry::TVec2f mid(0.5f * (box.i.x - box.f.x) + box.f.x, 0.5f * (box.i.y - box.f.y) + box.f.y);
                if (fn_80237948(player, &mid)) {
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x117, 0, 1, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x118, 0, 1, NULL, NULL, true, 1.0f);
                    startSound_801F0E34(0x400D1, 0);
                }
            }
        }
    }
    switch (field_0x2f8) {
    case 0:
        fn_80239914(&box, mLayer_0x4, mSerial);
        fn_801EF510();
        if (field_0x94 == 2) {
            if (isScreenFlagSet_0x34()) {
                startSound_801F0E34(0x400A0, 0);
                if (field_0x94 != 3) {
                    field_0xbc = 2;
                }
                field_0x2f8 = 1;
                field_0x2fc = 0x3C;
            }
            return;
        }
        for (s32 i = 0; i < 4; i++) {
            if (isPlayerActive_8023F264(i) && mLayer_0x4 == getPlayerLayer_8023DE58(i)) {
                const JGeometry::TVec3f* playerPos = getPlayerPos_8023E724(i);
                JGeometry::TVec3f d;
                d.sub(*playerPos, mPos);
                f32 threshold = 4096.0f;
                if (field_0x94 == 3) {
                    threshold = 784.0f;
                }
                if (d.squared() < threshold) {
                    startSound_801F0E34(0x400A0, 0);
                    if (field_0x94 != 3) {
                        field_0xbc = 2;
                    }
                    field_0x2f8 = 1;
                    field_0x2fc = 0x3C;
                    return;
                }
            }
        }
        break;
    case 1: {
        field_0x2fc--;
        box = mBox;
        box.i.x += mPos.x;
        box.i.y += mPos.y;
        box.f.x += mPos.x;
        box.f.y += mPos.y;
        fn_80239914(&box, mLayer_0x4, mSerial);
        if (!(field_0x2fc & 2)) {
            field_0x318 = field_0x318 ^ 1;
        }
        if (field_0x2fc < 0) {
            field_0xb8 = 1;
            if (field_0x94 != 3) {
                field_0xb0 = 0x12205;
            }
            field_0x318 = 0;
            field_0x2f8 = 2;
        }
        break;
    }
    case 2:
        if (field_0x300 > 0) {
            field_0x300--;
        }
        mVel_0x3c.z += -0.2f;
        mPos += mVel_0x3c;
        if (mPos.z <= 0.0f) {
            JGeometry::TVec3f p = mPos;
            p.y += 8.0f;
            if (field_0x304 != 0) {
                if (field_0x94 == 3 || field_0x94 == 4) {
                    spawnEmitter_8013CC50(mLayer_0x4, &p, 0x3CF, 0, 0, NULL, NULL, true, 2.0f);
                } else {
                    spawnEmitter_8013CC50(mLayer_0x4, &p, 0x3CF, 0, 0, NULL, NULL, true, 1.0f);
                }
                startSound_801F0E34(0x40007, 0);
            }
            field_0x304 = 1;
            mPos.z = 0.0f;
            if (field_0x94 == 3 || field_0x94 == 4) {
                s32 outDir = -1;
                s32 outPlayer = -1;
                findTargetPlayer_801EEC98(mSerial, NULL, &outDir, NULL, &outPlayer, 0, 0);
                bool away = false;
                if (outPlayer < 0) {
                    away = true;
                } else if (outDir != getPlayerFacing_80230CFC(outPlayer)) {
                    away = true;
                }
                if (away) {
                    field_0x304 = 0;
                    mVel_0x3c.set(0.0f, 0.0f, 0.0f);
                    box = mBox;
                    box.i.x += mPos.x;
                    box.i.y += mPos.y;
                    box.f.x += mPos.x;
                    box.f.y += mPos.y;
                    fn_80239914(&box, mLayer_0x4, mSerial);
                    fn_801EF510();
                    damagePlayersInBox_801F2B7C(-100, -1, 1);
                    sweepMapBox_801F2618(NULL, false);
                    return;
                }
            }
            getNearestPlayerDir_801EE8F4(&p, NULL, 0);
            p.scale(0.5f);
            mVel_0x3c.x = p.x;
            mVel_0x3c.y = p.y;
            mVel_0x3c.z = 2.0f;
        }
        damagePlayersInBox_801F2B7C(-100, -1, 1);
        sweepMapBox_801F2618(NULL, false);
        break;
    case 3: {
        fn_8020D58C()->addBox(mSerial);
        fn_801EF510();
        if (field_0x94 == 6 || field_0x94 == 7) {
            box = mBox;
            box.i.x += mPos.x;
            box.i.y += mPos.y;
            box.f.x += mPos.x;
            box.f.y += mPos.y;
            fn_80239914(&box, mLayer_0x4, mSerial);
            return;
        }
        if (PSVECDistance(&mPos, &field_0x320) >= 15.9f) {
            mPos.x = (f32)((((s32)mPos.x + 4) / 8) * 8);
            mPos.y = (f32)((((s32)mPos.y + 4) / 8) * 8);
            field_0x320 = mPos;
            mVel_0x3c.set(0.0f, 0.0f, 0.0f);
            JGeometry::TBox2f moved = mBox;
            moved.i.x += mPos.x;
            moved.i.y += mPos.y;
            moved.f.x += mPos.x;
            moved.f.y += mPos.y;
            fn_80239914(&moved, mLayer_0x4, mSerial);
            mPrevPos = mPos;
            field_0x300 = 8;
            return;
        }
        if ((mVel_0x3c.x != 0.0f || mVel_0x3c.y != 0.0f) && sweepMapBox_801F2618(NULL, false)) {
            mPos.x = (f32)((((s32)mPos.x + 4) / 8) * 8);
            mPos.y = (f32)((((s32)mPos.y + 4) / 8) * 8);
            field_0x320 = mPos;
            mVel_0x3c.set(0.0f, 0.0f, 0.0f);
            JGeometry::TBox2f moved = mBox;
            moved.i.x += mPos.x;
            moved.i.y += mPos.y;
            moved.f.x += mPos.x;
            moved.f.y += mPos.y;
            fn_80239914(&moved, mLayer_0x4, mSerial);
            mPrevPos = mPos;
            field_0x300 = 8;
            mVel_0x3c.set(0.0f, 0.0f, 0.0f);
        } else {
            JGeometry::TVec3f savedPos = mPos;
            JGeometry::TVec3f savedVel = mVel_0x3c;
            mVel_0x3c.normalize();
            if (field_0x314 >= 0) {
                mVel_0x3c.scale(24.0f);
            } else if (field_0x310 >= 0) {
                mVel_0x3c.scale(8.0f);
            } else {
                mVel_0x3c.set(0.0f, 0.0f, 0.0f);
            }
            mPos.x += mVel_0x3c.x;
            mPos.y += mVel_0x3c.y;
            mVel_0x3c = savedVel;
            if (fn_802265DC(&mPos, mLayer_0x4, false) == 1 || getRawTile_80226850(&mPos, mLayer_0x4) == 0x8F) {
                mVel_0x3c.set(0.0f, 0.0f, 0.0f);
            } else if (mVel_0x3c.x != 0.0f || mVel_0x3c.y != 0.0f) {
                ActorBase_804A5770* actors[1024];
                s32 count = getActorManager_801F856C()->collectActorsById_801F7818('AMOS', mLayer_0x4, actors, -1);
                for (s32 i = 0; i < count; i++) {
                    ActorBase_804A5770* actor = actors[i];
                    if (actor != NULL && actor->mSerial != mSerial) {
                        f32 dy = actor->mPos.y - mPos.y;
                        f32 dx = actor->mPos.x - mPos.x;
                        if (std::sqrtf(dx * dx + dy * dy) < 8.0f) {
                            mVel_0x3c.set(0.0f, 0.0f, 0.0f);
                            break;
                        }
                    }
                }
            }
            mPos = savedPos;
            mPos += mVel_0x3c;
        }
        if (field_0x30c < 2 && (mVel_0x3c.x != 0.0f || mVel_0x3c.y != 0.0f)) {
            field_0x30c++;
            if (field_0x30c == 2) {
                startSound_801F0E34(0x30004, 0);
            }
        }
        JGeometry::TBox2f moved = mBox;
        moved.i.x += mPos.x;
        moved.i.y += mPos.y;
        moved.f.x += mPos.x;
        moved.f.y += mPos.y;
        fn_80239914(&moved, mLayer_0x4, mSerial);
        if (field_0x300 > 0) {
            field_0x300--;
            return;
        }
        if (field_0x300 != 0) {
            return;
        }
        if (!mVel_0x3c.isZero()) {
            return;
        }
        s32 dir = -1;
        field_0x310 = fn_80239558(&moved, mLayer_0x4, mSerial);
        if (field_0x310 >= 0) {
            dir = getPlayerFacing_80230CFC(field_0x310);
            field_0x314 = -1;
        } else {
            field_0x314 = fn_80239470(&moved, mLayer_0x4, mSerial);
            if (field_0x314 >= 0) {
                field_0x310 = -1;
                dir = getPlayerFacing_80230CFC(field_0x314);
                if (dir == 0) {
                    dir = 1;
                } else if (dir == 1) {
                    dir = 0;
                } else if (dir == 2) {
                    dir = 3;
                } else if (dir == 3) {
                    dir = 2;
                }
            }
        }
        if (dir >= 0 && dir < 4) {
            static const f32 lbl_80465A98[4][3] = {
                { 0.5f, 0.0f, 0.0f },
                { -0.5f, 0.0f, 0.0f },
                { 0.0f, 0.5f, 0.0f },
                { 0.0f, -0.5f, 0.0f },
            };
            field_0x30c = 0;
            mVel_0x3c.set(lbl_80465A98[dir][0], lbl_80465A98[dir][1], lbl_80465A98[dir][2]);
            field_0x320.set(mPos);
        }
        break;
    }
    case 4:
        if (!field_0x230.fn_800FE3EC(field_0x308, mPos, mLayer_0x4)) {
            break;
        }
        field_0x2f8 = 5;
        // fall through
    case 5: {
        if ((mSpawnParam_0x90 >> 27) != 0 && isScreenFlagSet_0x34()) {
            die_0x24();
            return;
        }
        fn_801EF510();
        mVel_0x3c.z += -0.2f;
        mPos += mVel_0x3c;
        if (mPos.z <= 0.0f) {
            mPos.z = 0.0f;
            JGeometry::TVec3f p = mPos;
            p.y += 8.0f;
            if (field_0x304 != 0) {
                spawnEmitter_8013CC50(mLayer_0x4, &p, 0x3CF, 0, 0, NULL, NULL, true, 2.0f);
                startSound_801F0E34(0x40007, 0);
            }
            field_0x304 = 1;
            JGeometry::TVec3f step(0.0f, 0.0f, 0.0f);
            s32 layer = mLayer_0x4;
            u16 angle;
            JGeometry::TVec3f pos = mPos;
            field_0x230.stepAlongRail_800FDEA0(&pos, 0.3f, &step, &layer, &angle, 0x200);
            mVel_0x3c.x = step.x;
            mVel_0x3c.y = step.y;
            mVel_0x3c.z = 2.0f;
        }
        damagePlayersInBox_801F2B7C(-100, -1, 1);
        sweepMapBox_801F2618(NULL, false);
        s32 serial = findOverlappingActorById_801F8108(this, 'LNBM', -1, 32.0f);
        if (serial != 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                ((UnknownLNBM_804A4188*)actor)->fn_801BEDBC();
            }
        }
        break;
    }
    }
}

// FIXME(shotgun): 92.31 %, missing/extra + reloc-name; behaviour drift.
void Armos_8049F260::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    params.field_0x20 = 0;
    s32 flags = 0;
    s32 mode = 0;
    if (field_0x31c != 0) {
        mode = 1;
        flags = 3;
    }
    mtx[0][0] = 1.0f;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = 1.0f;
    mtx[1][2] = 0.0f;
    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = 1.0f;
    params.mMtx = mtx;
    if (field_0x94 == 3 || field_0x94 == 4 || field_0x94 == 5 || field_0x94 == 7) {
        drawSpriteScaled_801F02BC("enemy_gc_sob.bin", field_0x318 + 0x444, &params, 3, mode, false, 1.0f, 1.0f);
    } else {
        drawSpriteScaled_801F02BC("enemy_both2_sob.bin", field_0x318 + 0x152, &params, flags, mode, false, 1.0f, 1.0f);
    }
}

Armos_8049F260::~Armos_8049F260() {}

// update() (0x80141064, 0x11C0 bytes): not written (size).
