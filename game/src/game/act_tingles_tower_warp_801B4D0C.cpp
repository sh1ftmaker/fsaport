#include "game/GameAudio.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Player.h"
#include "game/RotMatrix.h"
#include "game/SpriteParams.h"
#include "game/save_data_803B8C74.h"
#include "game/ActorId.h"
#include "game/GameState.h"
#include "game/Message.h"
#include "game/Map.h"
#include "JSystem/JMath/JMath.h"
#include "game/act/tu_801F9484.h"
#include "std/math.h"

// The 'TLWP' actor, TinglesTowerWarp_804A38D4.

static u16 lbl_804A37E0[20] = {
    0x051F, 0x0520, 0x0521, 0x0522, 0x0523, 0x0524, 0x0525, 0x0526, 0x0527, 0x0528,
    0x0529, 0x052A, 0x052B, 0x052C, 0x052D, 0x052E, 0x052F, 0x0530, 0x0531, 0x0532,
};
static s32 lbl_804A3808[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static s32 lbl_804A3828[8] = { 2, 2, 2, 2, 2, 2, 2, 1 };
static s32 lbl_804A3848[8] = { 0x049C, 0x049D, 0x049E, 0x049F, 0x04A0, 0x04A1, 0x04A2, 0x04A3 };
static s32 lbl_804A3868[12] = { 0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1 };
static s32 lbl_804A3898[12] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
static s32 lbl_804A38C8[3] = { 1, 1, 1 };

extern u8 lbl_8053ABF8;
s32 lbl_8053A118 = 3;

// 0x801B6170.
void TinglesTowerWarp_804A38D4::init() {
    mBox.set(-12.0f, -12.0f, 12.0f, 12.0f);
    mMapBox_0x80.set(-8.0f, -8.0f, 8.0f, 8.0f);
    field_0x240 = 0;
    field_0x244 = 0;
    field_0x248 = 0;
    for (s32 i = 0; i < 3; i++) {
        mTimers[i] = 0;
    }
    for (s32 i = 0; i < 3; i++) {
        mEmitters294[i] = NULL;
    }
    for (u32 i = 0; i < 20; i++) {
        mEmitters2A0[i] = NULL;
    }
    lbl_8053ABF8 = 0;
    field_0x25c = 1.0f;
    field_0x250 = (mSpawnParam_0x90 >> 4) & 0xF;
    field_0x254 = mSpawnParam_0x90 & 0xF;
    if (field_0x250 == 0 && field_0x254 == 0) {
        JGeometry::TBox2f box;
        box = getGameView_802D6A9C()->mVisibleBox;
        JGeometry::TVec3f pos;
        pos.set(mPos);
        pos.x = box.i.x + 0.5f * (box.f.x - box.i.x);
        pos.y = box.i.y + 0.5f * (box.f.y - box.i.y);
        pos.set(mPos);
        pos.x += 96.0f;
        pos.y += -32.0f;
        for (s32 i = 0; i < 20; i++) {
            mEmitters2A0[i] = spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_804A37E0[i], 0, 0, NULL, NULL, true, 1.0f);
        }
    }
    field_0x24c = 0;
    for (s32 i = 0; i < 4; i++) {
        field_0x264[i].x = mPos.x;
        field_0x264[i].y = mPos.y;
        field_0x264[i].z = mPos.z;
    }
    field_0x258 = 0.0f;
    field_0x260 = 0;
    if (field_0x250 == 0) {
        lbl_8053A118 = 3;
        field_0x230 = 0;
    } else {
        mPos.z = 200.0f;
        field_0x230 = 8;
    }
}

// 0x801B609C.
TinglesTowerWarp_804A38D4::~TinglesTowerWarp_804A38D4() {
    for (s32 i = 0; i < 3; i++) {
        if (mEmitters294[i] != NULL) {
            JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
            manager->forceDeleteEmitter(mEmitters294[i]);
            mEmitters294[i] = NULL;
        }
    }
    for (s32 i = 0; i < 20; i++) {
        if (mEmitters2A0[i] != NULL) {
            JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
            manager->forceDeleteEmitter(mEmitters2A0[i]);
            mEmitters2A0[i] = NULL;
        }
    }
}

// 0x801B56A0.
// FIXME(shotgun): 86.71 %, missing/extra; behaviour drift.
void TinglesTowerWarp_804A38D4::fn_801B56A0() {
    JGeometry::TVec3f target;
    JGeometry::TBox2f box = mBox;
    JGeometry::TVec3f pos = mPos;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    if (!(lbl_8050EC80.getCurrentFile().field_0x5a[0] & (u8)(1 << field_0x254))) {
        return;
    }
    if (gGameRoot_8053AAF8->field_0x7e != 0) {
        if (field_0x230 == 5 || field_0x230 == 6 || field_0x230 == 7) {
            if (lbl_8053AD30->fn_80309C4C(0x1000, -1)) {
                lbl_8053A118 = 0x309;
                field_0x230 = 7;
            }
        }
        s32 dir;
        f32 dist;
        s32 player;
        findTargetPlayer_801EEC98(mSerial, NULL, &dir, &dist, &player, 0, 0);
        switch (field_0x230) {
        case 0:
            mEmitters294[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x48B, 0, 0, NULL, NULL, true, 1.0f);
            mEmitters294[1] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x48C, 0, 0, NULL, NULL, true, 1.0f);
            field_0x230 = 1;
        case 1:
            fn_8022FEB8(&box, mLayer_0x4, mSerial, false, false);
            field_0x244 = fn_8022FD8C(mSerial);
            if (field_0x244 < 0 || !fn_802300EC(field_0x244)) {
                break;
            }
            pos.y -= 64.0f;
            if (!openMessage_8038AD24(field_0x244, lbl_804A3848[field_0x254], mSerial, mLayer_0x4, pos, 0x100)) {
                break;
            }
            field_0x230 = 2;
        case 2:
            fn_8022FE80(field_0x244, mLayer_0x4, mSerial, false, false);
            if (stepMessageWindow_803865B8(field_0x244)) {
                fn_80384C58(field_0x244, mSerial);
                switch (fn_8037EFB4(field_0x244)) {
                case 0:
                    field_0x230 = 4;
                    break;
                case 1:
                    fn_80230534(false);
                    field_0x230 = 3;
                    break;
                }
            }
            break;
        case 3: {
            s32 far = 0;
            for (s32 i = 0; i < gGameRoot_8053AAF8->getPlayerCount_80138A30(); i++) {
                f32 dx = getPlayerPos_8023E724(i)->x - mPos.x;
                f32 dy = getPlayerPos_8023E724(i)->y - mPos.y;
                if (std::sqrtf(dx * dx + dy * dy) > 33.0f) {
                    far++;
                }
            }
            if (far >= gGameRoot_8053AAF8->getPlayerCount_80138A30()) {
                field_0x230 = 1;
            }
            break;
        }
        case 4:
            if (getActorManager_801F856C()->fn_801F68E0() == 0) {
                break;
            }
            for (s32 i = 0; i < 4; i++) {
                if (isPlayerActive_8023F264(i) && getField_0x4() == getPlayerLayer_8023DE58(i)) {
                    JGeometry::TVec3f* ppos = getPlayerPos_8023E724(i);
                    field_0x264[i].x = ppos->x;
                    field_0x264[i].y = ppos->y;
                    field_0x264[i].z = ppos->z;
                    field_0x264[i].z = 0.0f;
                    if (getPlayerLeader_8022F3DC(i) < 0) {
                        fn_8022CA6C(i, 5, false);
                    }
                    if (isPlayerDead_80236B50(i)) {
                        fn_8023764C(i, 1);
                    }
                }
            }
            lbl_80529DEC.field_0x24e = 1;
            field_0x230 = 5;
        case 5: {
            target.x = mPos.x;
            target.y = mPos.y;
            target.z = mPos.z;
            s32 done = 0;
            for (s32 i = 0; i < 4; i++) {
                if (isPlayerActive_8023F264(i)) {
                    if (i != 0) {
                        target.y -= 0.2f;
                    }
                    addCalc2_801F71A4(&field_0x264[i].x, target.x, 0.3f, 1.0f);
                    addCalc2_801F71A4(&field_0x264[i].y, target.y, 0.3f, 1.0f);
                    f32 dy = field_0x264[i].y - target.y;
                    f32 dx = field_0x264[i].x - target.x;
                    if (std::sqrtf(dx * dx + dy * dy) < 1.0f) {
                        done++;
                        fn_80230498(i, &field_0x264[i], 0x3F, 0xFF, -0x80, -0x80, true);
                    } else {
                        setPlayerFacing_80230BF4(i, angleToDir_801F681C((s32)(JMath::atanTable_.atan2_(-dx, -dy) * 10430.378f)));
                        fn_802303FC(i, &field_0x264[i], true);
                    }
                } else {
                    done++;
                }
            }
            if (done >= 4) {
                field_0x230 = 6;
            }
            break;
        }
        case 6:
            if (mTimers[0] == 0) {
                target.x = mPos.x;
                target.y = mPos.y;
                target.z = mPos.z;
                for (s32 i = 0; i < 4; i++) {
                    if (isPlayerActive_8023F264(i) && mLayer_0x4 == getPlayerLayer_8023DE58(i)) {
                        if (i != 0) {
                            target.y -= 0.2f;
                        }
                        addCalc2_801F71A4(&field_0x264[i].x, target.x, 0.3f, 1.0f);
                        addCalc2_801F71A4(&field_0x264[i].y, target.y, 0.3f, 1.0f);
                        // FIXME: original also computes sqrtf(fabsf(dx)^2 + fabsf(dy)^2) and compares with 2.0f.
                        if (field_0x240 == 0) {
                            fn_80230498(i, &field_0x264[i], 0x3C, 0xFF, -0x80, -0x80, true);
                        } else {
                            fn_80230498(i, &field_0x264[i], 0x3E, 0xFF, -0x80, -0x80, true);
                        }
                    }
                }
                mTimers[0] = 8;
                field_0x240++;
                if (field_0x240 > 1) {
                    f32 offset = -48.0f;
                    for (s32 i = 0; i < 3; i++) {
                        target.x = mPos.x;
                        target.y = mPos.y;
                        target.z = mPos.z;
                        target.x += offset;
                        offset += 48.0f;
                        getActorManager_801F856C()->spawnActor_801F9484(ACTOR_TLWP, mLayer_0x4, &target, (i + 1) << 4, -1, -1);
                    }
                    field_0x230 = 7;
                }
            }
            break;
        case 7:
            if (lbl_8053A118 == 0x309) {
                gGameRoot_8053AAF8->fn_801365E8(lbl_804A3808[field_0x254], lbl_804A3828[field_0x254], 0);
            }
            break;
        }
    }
}

// 0x801B5228.
// FIXME(shotgun): 99.44 %, regalloc-only; behaviour clean.
void TinglesTowerWarp_804A38D4::fn_801B5228() {
    addCalc2_801F71A4(&mPos.z, field_0x258, 0.3f, field_0x25c);
    switch (field_0x230) {
    case 8:
        mDir_0x98 = lbl_804A38C8[field_0x250 - 1];
        for (s32 i = 0; i < 4; i++) {
            if (isPlayerActive_8023F264(i) && getField_0x4() == getPlayerLayer_8023DE58(i)) {
                JGeometry::TVec3f* pos = getPlayerPos_8023E724(i);
                field_0x264[i].x = pos->x;
                field_0x264[i].y = pos->y;
                field_0x264[i].z = pos->z;
                if (i == 0) {
                    field_0x264[i].y += 0.1f;
                }
            }
        }
        field_0x25c = 3.0f;
        field_0x230 = 9;
    case 9: {
        JGeometry::TVec3f* pos = getPlayerPos_8023E724(0);
        f32 x = pos->x;
        f32 y = pos->y;
        field_0x258 = 14.0f;
        if (field_0x250 == 1) {
            field_0x260 = 3000;
            x -= 10.0f;
            field_0x258 = 8.0f;
        }
        if (field_0x250 == 3) {
            field_0x260 = -3000;
            x += 10.0f;
            field_0x258 = 8.0f;
        }
        addCalc2_801F71A4(&mPos.x, x, 0.3f, field_0x25c);
        mPos.y = y;
        f32 dz = std::fabs(mPos.z - field_0x258);
        f32 dx = std::fabs(mPos.x - x);
        if (std::sqrtf(dx * dx + dz * dz) < 1.0f) {
            if (--lbl_8053A118 == 0) {
                for (s32 i = 0; i < 4; i++) {
                    if (isPlayerActive_8023F264(i) && getField_0x4() == getPlayerLayer_8023DE58(i)) {
                        fn_80230498(i, &field_0x264[i], 0x3E, 0xFF, -0x80, -0x80, true);
                    }
                }
                startSound_801F0E34(0x50052, 0);
            }
            field_0x230 = 10;
        }
        break;
    }
    case 10:
        if (lbl_8053A118 <= 0) {
            if (field_0x258 != 200.0f) {
                field_0x258 = 200.0f;
                field_0x25c = 2.25f;
            } else if (field_0x250 == 2) {
                f32 z = mPos.z;
                for (s32 i = 0; i < 4; i++) {
                    if (isPlayerActive_8023F264(i) && getField_0x4() == getPlayerLayer_8023DE58(i)) {
                        addCalc2_801F71A4(&field_0x264[i].z, z, 0.3f, field_0x25c);
                        fn_80230498(i, &field_0x264[i], 0xB0, 0xFF, -0x80, -0x80, true);
                    }
                }
                if (std::fabs(mPos.z - field_0x258) < 2.0f) {
                    lbl_8053A118 = 0x309;
                }
            }
        }
        break;
    }
    if (mTimers[0] == 0) {
        field_0x24c = lbl_804A3868[field_0x240 + mDir_0x98 * 4];
        mTimers[0] = lbl_804A3898[field_0x240 + mDir_0x98 * 4];
        field_0x240++;
        field_0x240 &= 3;
    }
}

// 0x801B5198.
void TinglesTowerWarp_804A38D4::update() {
    switch (field_0x250) {
    case 0:
        fn_801B56A0();
        break;
    case 1:
    case 2:
    case 3:
        fn_801B5228();
        break;
    }
    for (s32 i = 0; i < 3; i++) {
        if (mTimers[i] > 0) {
            mTimers[i]--;
        }
    }
}

// 0x801B4EC8.
// FIXME(shotgun): 96.97 %, scheduling+regalloc; behaviour clean.
void TinglesTowerWarp_804A38D4::fn_801B4EC8() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    switch (field_0x250) {
    case 2:
        params.field_0x1d = 1;
        params.field_0x0[2] = 9;
        break;
    case 1:
        params.field_0x1d = 1;
        params.field_0x0[2] = 3;
        break;
    case 3:
        params.field_0x1d = 1;
        params.field_0x0[2] = 4;
        break;
    }
    setRotScaleMtx(mtx, JGeometry::TVec3f(0.0f, (f32)field_0x260 / -10430.228f, 0.0f), 1.0f);
    params.mMtx = mtx;
    drawSprite_801F06F0("enemy_both2_sob.bin", field_0x24c + 0xC0, &params, 7, 1.0f, 1.0f);
}

// 0x801B4D0C.
void TinglesTowerWarp_804A38D4::draw() {
    switch (field_0x250) {
    case 0:
        if (lbl_8050EC80.getCurrentFile().field_0x5a[0] & (u8)(1 << field_0x254)) {
            SpriteParams_801F5AE0 params;
            params.field_0x1d = -1;
            drawSprite_801F06F0("enemy_gc_sob.bin", 0x431, &params, -1, 1.0f, 1.0f);
            drawSpriteScaled_801F02BC("enemy_gc_sob.bin", 0x431, &params, -1, 0, false, 1.0f, 1.0f);
        }
        break;
    case 1:
    case 2:
    case 3:
        fn_801B4EC8();
        break;
    }
}
