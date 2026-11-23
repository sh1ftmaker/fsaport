#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/ActorManager.h"
#include "game/ActorId.h"
#include "game/Effect.h"
#include "game/Player.h"
#include "game/Message.h"
#include "game/SpriteParams.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/act/act_great_fairy_bfry.h"

// Sprite frames of the fairy kinds (.rodata 0x804755E0 / 0x80475600).
extern const s32 lbl_804755E0[8] = {
    0x08, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x40, 0x48,
};
extern const s32 lbl_80475600[];

GreatFairy_804AF338::~GreatFairy_804AF338() {
    releaseEmitter_801EE434(field_0x2a0[0]);
    releaseEmitter_801EE434(field_0x2a0[1]);
    if (mSpawnParam_0x90 != 3) {
        for (s32 i = 0; i < 4; i++) {
            if (field_0x24c[i] == 1) {
                fn_803037D4(i, 0x12);
                field_0x24c[i] = 0;
            }
        }
    }
}

// FIXME(shotgun): 98.63 %, scheduling+regalloc; behaviour drift.
void GreatFairy_804AF338::init() {
    mBox.i.x = -8.0f;
    mBox.i.y = -8.0f;
    mBox.f.x = 8.0f;
    mBox.f.y = 8.0f;
    mVel_0x3c.set(0.0f, 0.0f, 0.0f);
    field_0x234 = -1;
    field_0x238 = -1;
    field_0x260 = 0;
    field_0x268 = 0;
    field_0x270 = -1;
    field_0x29c = 0;
    field_0x284.z = 0.0f;
    field_0x284.y = 0.0f;
    field_0x284.x = 0.0f;
    field_0x290.z = 0.0f;
    field_0x290.y = 0.0f;
    field_0x290.x = 0.0f;
    field_0x274[0] = mSpawnParam_0x90 >> 27;
    field_0x274[1] = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x274[2] = (mSpawnParam_0x90 >> 17) & 0x1F;
    field_0x274[3] = (mSpawnParam_0x90 >> 12) & 0x1F;
    if ((mSpawnParam_0x90 & 0xF) == 3) {
        field_0x29c = (mSpawnParam_0x90 >> 4) & 0xF;
        field_0x234 = (mSpawnParam_0x90 >> 8) & 3;
        if (field_0x29c == 0xB) {
            JGeometry::TVec3f p = *getPlayerPos_8023E724(field_0x234);
            field_0x284.x = mPos.x;
            field_0x284.y = mPos.y;
            field_0x284.z = mPos.z;
            field_0x284.z += 32.0f;
            p.z = 8.0f;
            field_0x290.set(
                (p.x - field_0x284.x) / 120.0f,
                (p.y - field_0x284.y) / 120.0f,
                (p.z - field_0x284.z) / 120.0f
            );
            mState = 9;
        } else {
            field_0x264 = 0x78;
            mState = 7;
        }
        switch (field_0x29c) {
        case 0:
            break;
        case 1:
            field_0x260 = 0x260;
            break;
        case 2:
            field_0x260 = 0x261;
            break;
        case 3:
            field_0x260 = 0x262;
            break;
        case 4:
            field_0x260 = 0x263;
            break;
        case 5:
            field_0x260 = 0x264;
            break;
        case 6:
            field_0x260 = 0x268;
            break;
        case 8:
            field_0x260 = 0x269;
            break;
        case 7:
            field_0x260 = 0x26A;
            break;
        case 9:
            u32 mode = (gGameRoot_8053AAF8->field_0x83 >> (fn_8022F340(field_0x234) * 2)) & 3;
            if (mode == 2) {
                field_0x260 = 0x27F;
            } else {
                field_0x260 = 0x284;
            }
            break;
        case 10:
            field_0x260 = 0x265;
            break;
        case 11:
            field_0x260 = 0x266;
            break;
        case 12:
            field_0x260 = 0x267;
            break;
        }
    } else {
        mState = 0;
    }
    field_0x240 = 2;
    field_0x244 = 0;
    field_0x23c = 0;
    field_0x248 = 0;
    field_0x24c[0] = 0;
    field_0x24c[1] = 0;
    field_0x24c[2] = 0;
    field_0x24c[3] = 0;
    field_0x25c = 0x1E;
    field_0x264 = 0;
    field_0x26c = 0;
    field_0x2a0[0] = NULL;
    field_0x2a0[1] = NULL;
    mSpawnParam_0x90 = mSpawnParam_0x90 & 0xF;
}

// FIXME(shotgun): 93.33 %, missing/extra + reloc-name; behaviour drift.
// (partial)
// FIXME(shotgun): states 2-13 (0x8038E900 onward) and the tail of state 1 (0x8038E8D4 and the
// end of the function at 0x8038F31C) are not written.
void GreatFairy_804AF338::update() {
    if (field_0x248 != 1) {
        JGeometry::TBox2f box;
        if (mSpawnParam_0x90 != 3) {
            if (gGameRoot_8053AAF8->getPlayerCount_80138A30() > 1 && mLayer_0x4 >= 8) {
                for (s32 i = 0; i < 4; i++) {
                    if (getPlayerLayer_8023DE58(i) == mLayer_0x4 && !fn_8022F478(i) && !isPlayerDead_80236B50(i) &&
                        field_0x24c[fn_8022F340(i)] == 0)
                    {
                        fn_803037D4(fn_8022F340(i), 0x11);
                        field_0x24c[fn_8022F340(i)] = 1;
                    }
                }
            }
            box.i.x = mBox.i.x;
            box.i.y = mBox.i.y;
            box.f.x = mBox.f.x;
            box.f.y = mBox.f.y;
            box.i.x += mPos.x;
            box.i.y += mPos.y;
            box.f.x += mPos.x;
            box.f.y += mPos.y;
            fn_80239914(&box, mLayer_0x4, mSerial);
        }
        JGeometry::TBox2f near(-24.0f, -24.0f, 24.0f, 24.0f);
        JGeometry::TBox2f far(-36.0f, -36.0f, 36.0f, 36.0f);
        near.i.x += mSpawnPos.x;
        near.f.x += mSpawnPos.x;
        far.i.x += mSpawnPos.x;
        far.f.x += mSpawnPos.x;
        near.i.y += mSpawnPos.y;
        near.f.y += mSpawnPos.y;
        far.i.y += mSpawnPos.y;
        far.f.y += mSpawnPos.y;
        switch (mState) {
        case 0: {
            for (s32 i = 0; i < 4; i++) {
                if (field_0x238 != i && getPlayerPos_8023E724(i)->z == 0.0f && !fn_8023EFEC(i) && !fn_8023EA28(i) &&
                    !isPlayerDead_80236B50(i))
                {
                    if (!(gGameRoot_8053AAF8->field_0x7f == 0 && fn_80237060(fn_8022F340(i)) >= 2 &&
                          (fn_8023725C(fn_8022F340(i)) >= 2 || fn_8023725C(fn_8022F340(i)) == 0)))
                    {
                        fn_8022FF8C(i, &near, mLayer_0x4, mSerial, false, true);
                    }
                } else if (!fn_8023B0F4(i, &far, mLayer_0x4, false, 500.0f)) {
                    field_0x238 = -1;
                }
            }
            field_0x234 = fn_8022FD8C(mSerial);
            if (field_0x234 >= 0 && getPlayerPos_8023E724(field_0x234)->z == 0.0f) {
                switch (mSpawnParam_0x90) {
                case 0:
                    field_0x260 = 0x25E;
                    break;
                default:
                    field_0x260 = 0x25F;
                    break;
                }
                if (openMessage_8038AD24(field_0x234, field_0x260, mSerial, mLayer_0x4, mPos, 0x2080)) {
                    field_0x238 = field_0x234;
                    mState = 1;
                }
            }
            break;
        }
        case 1:
            fn_8022FEB8(&near, mLayer_0x4, mSerial, false, true);
            if (stepMessageWindow_803865B8(field_0x234)) {
                switch (mSpawnParam_0x90) {
                case 0:
                    if (fn_80388CAC(field_0x234, mSerial)) {
                        healPlayerGroup_80236554(fn_8022F340(field_0x234), getPlayerMaxLife_80237810(fn_8022F340(field_0x234)), false);
                        s32 layer = mLayer_0x4;
                        if (layer >= 8) {
                            getActorManager_801F856C()->spawnActor_801F9484(
                                'PEFF', layer, &mPos, (field_0x234 & 3) | 4, -1, -1);
                        }
                        startSound_801F0E34(0x40090, 0);
                        mState = 3;
                    }
                    break;
                case 1:
                    fn_8038DE74();
                    break;
                case 2:
                    if (fn_80236E3C(fn_8022F340(field_0x234)) ||
                        (field_0x274[fn_8022F340(field_0x234)] != 0 &&
                         getGameView_802D6A9C()->isScreenFlagSet_802D800C(field_0x274[fn_8022F340(field_0x234)]) == 1))
                    {
                        field_0x260 = 0x267;
                        fn_8038470C(field_0x234, field_0x260);
                        mState = 2;
                    } else if (fn_80388CAC(field_0x234, mSerial)) {
                        field_0x270 = getActorManager_801F856C()->spawnActor_801F9484(
                            ACTOR_BFRY, mLayer_0x4, &mPos,
                            ((field_0x29c << 4) & 0xF0) | 3 | ((field_0x234 << 8) & 0x300), -1, -1);
                        if (field_0x270 >= 0) {
                            field_0x29c = 0xB;
                            mState = 5;
                            if (field_0x274[fn_8022F340(field_0x234)] != 0) {
                                getGameView_802D6A9C()->setSwitchBit_802D8050(field_0x274[fn_8022F340(field_0x234)], true, 0xFF);
                            }
                        }
                    }
                    break;
                }
            }
            fn_80230498(field_0x234, getPlayerPos_8023E724(field_0x234), 0x3F, 0xFF, -0x80, -0x80, true);
            break;
        case 2:
            fn_8022FEB8(&near, mLayer_0x4, mSerial, false, true);
            if (fn_80388CAC(field_0x234, mSerial)) {
                healPlayerGroup_80236554(fn_8022F340(field_0x234), getPlayerMaxLife_80237810(fn_8022F340(field_0x234)), false);
                s32 layer = mLayer_0x4;
                if (layer >= 8) {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'PEFF', layer, &mPos, (field_0x234 & 3) | 4, -1, -1);
                }
                startSound_801F0E34(0x40090, 0);
                mState = 3;
            }
            break;
        case 3:
            if (getPlayerMaxLife_80237810(fn_8022F340(field_0x234)) == lbl_80529DEC.field_0xd8[fn_8022F340(field_0x234)] &&
                field_0x25c == 0 && fn_8023061C(field_0x234, false) == 1)
            {
                field_0x25c = 0x1E;
                field_0x29c = 0;
                mState = 6;
            }
            if (getPlayerMaxLife_80237810(fn_8022F340(field_0x234)) == lbl_80529DEC.field_0xd8[fn_8022F340(field_0x234)] &&
                field_0x25c > 0)
            {
                if (field_0x25c % 10 == 0) {
                    fn_80230EB8(fn_8022F340(field_0x234), 9);
                }
                field_0x25c--;
            }
            break;
        case 4:
            if (field_0x240-- == 0) {
                field_0x248 = 1;
            }
            break;
        case 5:
            if (getActorManager_801F856C()->findActorBySerial_801F666C(field_0x270) == NULL) {
                if (mSpawnParam_0x90 == 2) {
                    field_0x240 = 0xB4;
                    mState = 4;
                } else {
                    field_0x29c = 0;
                    mState = 6;
                }
            }
            break;
        case 6: {
            for (s32 i = 0; i < 4; i++) {
                if (i == field_0x234) {
                    if (!fn_8023B0F4(i, &far, mLayer_0x4, false, 500.0f)) {
                        mState = 0;
                        break;
                    }
                } else {
                    if (fn_8023B0F4(i, &far, mLayer_0x4, false, 500.0f) && !fn_8022F478(i) &&
                        (!arePlayersInSameGroup_8022E438(i, field_0x234) || fn_8022CAF8(field_0x234) == 4))
                    {
                        mState = 0;
                        break;
                    }
                }
            }
            break;
        }
        case 7:
            if (field_0x264 >= 0x3C && mSpawnParam_0x90 == 3) {
                field_0x284.set(*getPlayerPos_8023E724(field_0x234));
                field_0x284.y += 4.0f;
                field_0x284.z = 16.0f;
                field_0x290.x = 0.0f;
                field_0x290.y = -2.0f;
                field_0x290.z = 0.0f;
                field_0x264 = 0;
                mState = 8;
            } else {
                field_0x264++;
            }
            break;
        case 8: {
            s32 anim = 0x331;
            if (field_0x264 >= 8) {
                if (field_0x264 < 9) {
                    anim = 0x332;
                } else if (field_0x264 < 10) {
                    anim = 0x333;
                } else if (field_0x264 < 0x12) {
                    anim = 0x334;
                } else {
                    anim = 0x3F;
                }
            }
            fn_80230498(field_0x234, getPlayerPos_8023E724(field_0x234), anim, 0xFF, -0x80, -0x80, true);
            if (field_0x264 >= 10) {
                field_0x284 += field_0x290;
                if (field_0x284.z <= 0.0f) {
                    field_0x284.z = 0.0f;
                    field_0x264 = 0;
                    s32 layer = mLayer_0x4;
                    if (layer < 8) {
                        spawnEmitter_8013CC50(
                            layer, &field_0x284, 0x3D0, 0, 0, NULL, NULL, true, 1.0f);
                    } else {
                        getActorManager_801F856C()->spawnActor_801F9484(
                            'PEFF', layer, &field_0x284, 0x2C, -1, -1);
                    }
                    startSound_801F0E34(0x30022, 0);
                    field_0x284.set(mPos);
                    field_0x284.x = getPlayerPos_8023E724(field_0x234)->x;
                    field_0x290.x = 0.0f;
                    field_0x290.y = 2.0f;
                    field_0x290.z = 2.0f;
                    mState = 9;
                    break;
                }
                field_0x290.z -= 0.1f;
            }
            field_0x264++;
            break;
        }
        case 9:
            if (field_0x264 >= 0x78) {
                field_0x264 = 0;
                if (field_0x29c != 0xB) {
                    s32 layer = mLayer_0x4;
                    if (layer < 8) {
                        spawnEmitter_8013CC50(
                            layer, &field_0x284, 0x3D0, 0, 0, NULL, NULL, true, 1.0f);
                    } else {
                        getActorManager_801F856C()->spawnActor_801F9484(
                            'PEFF', layer, &field_0x284, 0x2C, -1, -1);
                    }
                    mState = 10;
                } else {
                    field_0x264 = 3;
                    field_0x268 = 7;
                    field_0x26c = 3;
                    mState = 10;
                }
            } else {
                field_0x264++;
            }
            break;
        case 10: {
            field_0x284 += field_0x290;
            JGeometry::TVec3f pos = *getPlayerPos_8023E724(field_0x234);
            if (pos.y - field_0x284.y < 8.0f) {
                field_0x264 = 0;
                mState = 11;
            } else if (field_0x29c != 0xB) {
                field_0x290.z -= 0.2f;
            } else if (field_0x264 > 0) {
                if (field_0x26c <= 0) {
                    field_0x26c = field_0x264;
                    field_0x268--;
                    if (field_0x268 <= 0) {
                        field_0x268 = 7;
                        field_0x264--;
                        if (field_0x264 <= 0) {
                            field_0x26c = 0;
                            break;
                        }
                    }
                }
                field_0x26c--;
            }
            break;
        }
        case 11: {
            JGeometry::TVec3f pos;
            pos.set(*getPlayerPos_8023E724(field_0x234));
            s32 idx;
            if (field_0x264 < 10) {
                idx = 0;
            } else if (field_0x264 < 14) {
                idx = 1;
                field_0x284.x = pos.x + 4.0f;
                field_0x284.y = pos.y - 16.0f;
                field_0x284.z = pos.z;
                if (field_0x264 == 10) {
                    fn_80230F44(fn_8022F340(field_0x234), 0x1000002);
                }
            } else {
                idx = 2;
                field_0x284.x = pos.x + 6.0f;
                field_0x284.y = pos.y - 19.0f;
                field_0x284.z = pos.z;
                if (field_0x2a0[0] == NULL) {
                    field_0x2a0[0] = spawnEmitter_8013CC50(
                        mLayer_0x4, &field_0x284, 0x562, 0, 1, NULL, NULL, true, 0.7f);
                }
                if (field_0x2a0[0] == NULL) {
                    field_0x2a0[1] = spawnEmitter_8013CC50(
                        mLayer_0x4, &field_0x284, 0x563, 0, 1, NULL, NULL, true, 0.6f);
                }
                if (field_0x264 >= 100) {
                    if (fn_8038BD08(field_0x234, field_0x260, mLayer_0x4, mSpawnPos, 0x2022) == 1) {
                        mState = 12;
                        if (field_0x29c == 10) {
                            fn_80236ED8(fn_8022F340(field_0x234), 2);
                        } else if (field_0x29c == 11) {
                            fn_80236DB0(fn_8022F340(field_0x234));
                        } else {
                            fn_802370FC(fn_8022F340(field_0x234), 2);
                        }
                        break;
                    }
                    field_0x264 = 100;
                }
            }
            fn_80230498(field_0x234, &pos, idx + 0xE9, 0xFF, -0x80, -0x80, true);
            field_0x264++;
            break;
        }
        case 12:
            if (fn_80388CAC(field_0x234, mSerial) == 1) {
                if (field_0x2a0[0] != NULL) {
                    field_0x2a0[0]->becomeInvalidEmitter();
                    field_0x2a0[0] = NULL;
                }
                if (field_0x2a0[1] != NULL) {
                    field_0x2a0[1]->becomeInvalidEmitter();
                    field_0x2a0[1] = NULL;
                }
                if (fn_8023061C(field_0x234, false) == 1) {
                    if (gGameRoot_8053AAF8->field_0x7f == 0) {
                        markDead_801F3668();
                    } else {
                        mState = 13;
                        startSound_801F0E34(0x40090, 0);
                        healPlayerGroup_80236554(fn_8022F340(field_0x234), getPlayerMaxLife_80237810(fn_8022F340(field_0x234)), false);
                        s32 layer = mLayer_0x4;
                        if (layer >= 8) {
                            getActorManager_801F856C()->spawnActor_801F9484(
                                'PEFF', layer, &mPos, (field_0x234 & 3) | 4, -1, -1);
                        }
                    }
                }
            }
            break;
        case 13:
            if (getPlayerLife_80237774(fn_8022F340(field_0x234)) <= lbl_80529DEC.field_0xd8[fn_8022F340(field_0x234)] &&
                field_0x25c == 0)
            {
                markDead_801F3668();
            }
            if (getPlayerLife_80237774(fn_8022F340(field_0x234)) <= lbl_80529DEC.field_0xd8[fn_8022F340(field_0x234)] &&
                field_0x25c > 0)
            {
                if (field_0x25c % 10 == 0) {
                    fn_80230EB8(fn_8022F340(field_0x234), 9);
                }
                field_0x25c--;
            }
            break;
        }
        if (mSpawnParam_0x90 != 3) {
            field_0x23c++;
            if (field_0x23c > lbl_804755E0[field_0x244]) {
                field_0x244++;
                if (field_0x244 >= 8) {
                    field_0x244 = 0;
                    field_0x23c = 0;
                }
            }
        }
    }
    if (mSpawnParam_0x90 != 3 && gGameRoot_8053AAF8->getPlayerCount_80138A30() > 1) {
        u8 active[4];
        active[0] = 0;
        active[1] = 0;
        active[2] = 0;
        active[3] = 0;
        for (s32 i = 0; i < 4; i++) {
            if (getPlayerLayer_8023DE58(i) == mLayer_0x4 && !fn_8022F478(i) && !isPlayerDead_80236B50(i)) {
                active[fn_8022F340(i)] = 1;
            }
        }
        for (s32 i = 0; i < gGameRoot_8053AAF8->getPlayerCount_80138A30(); i++) {
            if (active[i] == 0 && field_0x24c[i] == 1) {
                fn_803037D4(i, 0x12);
                field_0x24c[i] = 0;
            }
        }
    }
}

// FIXME(shotgun): 99.55 %, reloc-name; behaviour clean.
void GreatFairy_804AF338::draw() {
    if (field_0x248 != 1 && (field_0x240 & 2) && mSpawnParam_0x90 != 3) {
        drawSprite_801F06F0("enemy_both2_sob.bin", field_0x244 + 0x61, NULL, 7, 1.0f, 1.0f);
        showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, field_0x244 + 0x61, false, 2, -1, 0, 0, 0);
    }
    if (mState == 8 || mState == 10 || mState == 11 || mState == 12) {
        if (field_0x26c == 0 && mSpawnParam_0x90 == 3) {
            SpriteParams_801F5AE0 params;
            JGeometry::TVec3f savedPos = mPos;
            s32 frame = lbl_80475600[field_0x29c - 1];
            s32 flags = 7;
            params.field_0x1d = 0;
            params.field_0x1c = 0;
            mPos = field_0x284;
            if (mState == 10 || mState == 11 || mState == 12) {
                frame = lbl_80475600[field_0x29c + 10];
            }
            if (mState == 11 || mState == 12) {
                params.field_0x1d = 1;
                params.field_0x1c = 1;
                flags = -1;
            }
            drawSprite_801F06F0("enemy_both2_sob.bin", frame, &params, flags, 1.0f, 1.0f);
            showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, frame, false, 2, -1, 1, 0, 0);
            mPos = savedPos;
        }
    }
}

static inline s32 getFlagBit(const s32* flags, s32 player) {
    return flags[fn_8022F340(player)];
}

void GreatFairy_804AF338::fn_8038DE74() {
    if (getFlagBit(field_0x274, field_0x234) != 0 &&
        getGameView_802D6A9C()->isScreenFlagSet_802D800C(getFlagBit(field_0x274, field_0x234)) == 1) {
        field_0x29c = 0xC;
    } else {
        switch (fn_802373EC(fn_8022F340(field_0x234))) {
        case 10:
            field_0x29c = 1;
            break;
        case 6:
            field_0x29c = 2;
            break;
        case 5:
            field_0x29c = 3;
            break;
        case 3:
            field_0x29c = 4;
            break;
        case 4:
            field_0x29c = 5;
            break;
        case 8:
            field_0x29c = 6;
            break;
        case 1:
            field_0x29c = 7;
            break;
        case 7:
            field_0x29c = 8;
            break;
        case 9:
            field_0x29c = 9;
            break;
        case 0:
        case 2:
        default:
            field_0x29c = 0xC;
            break;
        }
    }
    if (field_0x29c != 0xC && fn_8023725C(fn_8022F340(field_0x234)) >= 2) {
        field_0x29c = 0xC;
    }
    if (field_0x29c == 0xC) {
        field_0x260 = 0x267;
        fn_8038470C(field_0x234, field_0x260);
        mState = 2;
    } else if (fn_80388CAC(field_0x234, mSerial) == 1) {
        field_0x270 = getActorManager_801F856C()->spawnActor_801F9484(
            ACTOR_BFRY, mLayer_0x4, &mPos, ((field_0x29c << 4) & 0xF0) | 3 | ((field_0x234 << 8) & 0x300), -1, -1);
        if (field_0x270 >= 0) {
            mState = 5;
            if (getFlagBit(field_0x274, field_0x234) != 0) {
                getGameView_802D6A9C()->setSwitchBit_802D8050(getFlagBit(field_0x274, field_0x234), true, 0xFF);
            }
        } else {
            field_0x29c = 0;
        }
    }
}
