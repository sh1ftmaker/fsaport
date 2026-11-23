#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Camera.h"
#include "game/EnemyBase.h"
#include "game/Effect.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/Message.h"
#include "game/act/act_four_sword_demo.h"

static const s16 lbl_8053C738[] = {
    0, 1,
    -1, -1,
};

static const u16 lbl_8053C740[4] = {
    0x0477,
    0x0479,
    0x0476,
    0x0478,
};

static const s16 lbl_80464A28[] = {
    0x031A, 8,
    0x031B, 3,
    0x031C, 30,
    -1, -1,
};

static const s16 lbl_80464A38[] = {
    0x00DB, 30,
    0x00DC, 1,
    0x00DD, 1,
    0x00DE, 1,
    0x00DF, 1,
    0x00E0, 1,
    0x00E1, 1,
    0x00E2, 1,
    0x00E3, 1,
    0x00E4, 1,
    0x00E5, 1,
    0x00E6, 8,
    0x00E7, 1,
    0x00E8, 60,
    0x003E, 1,
    0x003E, 1,
    -1, -1,
};

// Values of the object's field_0x8 that select fn_80117074's index; vfunc_0x54
// never compares entries 0, 1, 6 and 10.
static const s32 lbl_80464A7C[13] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000096, 0x000000D2, 0x000000F0, 0x000000F0,
    0x000000FF, 0x000001DB, 0x0000032A, 0x00000348, 0x000003C0, 0x000003DE,
};

static const s32 lbl_80464AB0[4] = {
    -3, -1, 1, 3,
};

static const s16* lbl_8049DA00[3] = {
    lbl_8053C738,
    lbl_80464A28,
    lbl_80464A38,
};

// The visible box is passed by value: that is what makes the original copy it to
// the stack (lwz/stw) before reading it with lfs.
static inline void setCenter(JGeometry::TVec3f& pos, JGeometry::TBox2f box) {
    pos.x = box.f.x + 0.5f * (box.i.x - box.f.x);
    pos.y = box.f.y + 0.5f * (box.i.y - box.f.y);
    pos.z = 0.0f;
}

FourSwordCutscene_8049DA0C::FourSwordCutscene_8049DA0C()
    : field_0x230(0),
      field_0x231(0),
      field_0x234(0),
      field_0x238(-1),
      field_0x23c(0.0f),
      field_0x240(0.0f) {
    mEmitter2a8 = NULL;
    for (s32 i = 0; i < 4; i++) {
        mEmitters288[i] = NULL;
        mEmitters298[i] = NULL;
    }
}

FourSwordCutscene_8049DA0C::~FourSwordCutscene_8049DA0C() {
    for (s32 i = 0; i < 4; i++) {
        releaseEmitter_801EE434(mEmitters288[i]);
        mEmitters288[i] = NULL;
        releaseEmitter_801EE434(mEmitters298[i]);
        mEmitters298[i] = NULL;
        releaseEmitter_801EE434(mEmitter2a8);
        mEmitter2a8 = NULL;
    }
}

// FIXME: 99.65 %, regalloc-only; behaviour clean.
// (regalloc-only). Minor register allocation differences in visibleBox center computation.
void FourSwordCutscene_8049DA0C::init() {
    DemoActorBase_8049D140::init();
    mAnim250.setKeyTables_801D2608(lbl_8049DA00, 3);
    mAnim250.startAnim_801D22DC(0, 2, false);
    mAnim250.setFrame_801D1F14(0.0f);
    JGeometry::TVec3f pos;
    setCenter(pos, getGameView_802D6A9C()->mVisibleBox);
    mEmitter2a8 = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x33e, 0, 0, NULL, NULL, true, 1.0f);
}

// FIXME: 98.48 %, missing/extra; behaviour drift.
// Same length. Differences: fn_8038BD08's result is converted from a byte in the original
// (clrlwi / neg / or / srwi.; the shared declaration returns bool); the box corner adds store in another
// order; case 2's camera setup reloads mPos.x / mPos.y where the original reuses registers; the loop
// counters and base pointers of cases 5 and 8 sit in other saved registers.
void FourSwordCutscene_8049DA0C::update() {
    DemoActorBase_8049D140::update();
    JGeometry::TBox2f box(mBox);
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    fn_80116E8C();

    switch (field_0x234) {
    case 0:
        if (fn_8011921C()->field_0x0 == 0) {
            fn_80239914(&box, mLayer_0x4, mSerial);
            if (checkPlayers()) {
                if (field_0x231 != 0) {
                    break;
                }
                fn_8023077C();
                fn_8022FE80(0, mLayer_0x4, mSerial, 1, 0);
                fn_8022FD8C(mSerial);
                if (showMessage()) {
                    fn_80117074(1);
                }
                break;
            }
            field_0x231 = 0;
        }
        break;
    case 1:
        fn_8022FE80(0, mLayer_0x4, mSerial, 1, 0);
        if (stepMessageWindow_803865B8(0)) {
            switch (fn_8037EFB4(0)) {
            case 0:
                fn_80388CAC(0, mSerial);
                for (s32 i = 0; i < 4; i++) {
                    setPlayerFacing_80230BF4(i, 3);
                    fn_80230498(i, getPlayerPos_8023E724(i), 0x3f, 0xff, -128, -128, true);
                }
                fn_80230534(false);
                fn_8011921C()->fn_801186B0(2);
                break;
            case 1:
            default:
                fn_80388CAC(0, mSerial);
                fn_80230534(false);
                fn_80117074(0);
                field_0x230 = 1;
                field_0x231 = 1;
                break;
            }
        }
        break;
    case 2:
        if (fn_8011921C()->field_0x8 == 60) {
            mDrawEnabled = false;
            field_0x244.x = mPos.x;
            field_0x244.y = mPos.y;
            field_0x244.z = mPos.z;
            field_0x244.y += 32.0f;
            for (s32 i = 0; i < 4; i++) {
                setPlayerFacing_80230BF4(i, 2);
                fn_80230498(i, &field_0x244, 0x31a, 0xff, -128, -128, true);
            }
            mAnim250.startAnim_801D22DC(1, 0, false);
            mAnim250.setFrame_801D1F14(0.0f);
            mAnim250.mFrameCtrl.setRate(0.0f);
        }
        if (fn_8011921C()->field_0x8 > 60) {
            Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
            if (camera->mBlendBoxOn_0x840 == 0) {
                camera->mBlendBoxOn_0x840 = 1;
                camera->mBlendBox_0x844.i.x = camera->mBlendBox_0x844.f.x = mPos.x;
                camera->mBlendBox_0x844.i.y = camera->mBlendBox_0x844.f.y = mPos.y;
                camera->mBlendWeight_0x854 = 1.0f;
                camera->mMargin_0x1d4 = -1.0f;
                camera->mMinHalfWidth_0x1d8 = -1.0f;
            }
            gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
        }
        if (fn_8011921C()->field_0x8 >= 50) {
            fn_8011921C()->field_0x2 = 1;
        }
        break;
    case 5: {
        if (spreadPlayers()) {
            fn_80117074(6);
        }
        break;
    }
    case 8:
        for (s32 i = 0; i < 4; i++) {
            fn_80230498(i, getPlayerPos_8023E724(i), mAnim250.mCel, 0xff, -128, -128, true);
            if (mAnim250.mCel == 0xdc) {
                fn_80230FD0(i, 0x10001);
            }
        }
        if (fn_8011921C()->field_0x8 == lbl_80464A7C[9] - 30) {
            JGeometry::TVec3f pos;
            setCenter(pos, getGameView_802D6A9C()->mVisibleBox);
            spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x102, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x103, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x104, 0, 0, NULL, NULL, true, 1.0f);
            field_0x238 = 0;
        }
        break;
    case 9: {
        if (raisePlayers()) {
            fn_80117074(10);
        }
        break;
    }
    case 12:
        break;
    }

    if (field_0x238 >= 0) {
        field_0x238++;
        if (field_0x238 <= 0x474) {
            startSound_801F0E34(0x3007a, 0);
        }
    }
    mAnim250.update_801D1C18();
}

void FourSwordCutscene_8049DA0C::draw() {
    DemoActorBase_8049D140::draw();
    SpriteParams_801F5AE0 params;
    if (mSpriteParams.mMtx == NULL) {
        drawShadow_801EF830(mLayer_0x4, &mPos, &params, 1, "enemy_gc_sob.bin", 0x179, NULL);
    }
    JGeometry::TVec3f pos = mPos;
    pos.y += 6.0f;
    if (mLayer_0x4 < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
        gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, 0x179, NULL, 1.0f, 1.0f);
    }
}

void FourSwordCutscene_8049DA0C::vfunc_0x54() {
    if (fn_8011921C()->field_0x4 == 2) {
        s32 value = fn_8011921C()->field_0x8;
        if (value == lbl_80464A7C[2]) {
            fn_80117074(2);
        } else if (value == lbl_80464A7C[3]) {
            fn_80117074(3);
        } else if (value == lbl_80464A7C[4]) {
            fn_80117074(4);
        } else if (value == lbl_80464A7C[5]) {
            fn_80117074(5);
        } else if (value == lbl_80464A7C[7]) {
            fn_80117074(7);
        } else if (value == lbl_80464A7C[8]) {
            fn_80117074(8);
        } else if (value == lbl_80464A7C[9]) {
            fn_80117074(9);
        } else if (value == lbl_80464A7C[11]) {
            fn_80117074(11);
        } else if (value == lbl_80464A7C[12]) {
            fn_80117074(12);
        }
    }
}

s32 FourSwordCutscene_8049DA0C::vfunc_0x58() {
    return field_0x234;
}

s32 FourSwordCutscene_8049DA0C::vfunc_0x5C() {
    switch (field_0x234) {
    case 12:
        return 1;
    default:
        return 0;
    }
}

inline void FourSwordCutscene_8049DA0C::startState5() {
    JGeometry::TVec3f pos;
    s32 i;
    for (i = 0; i < 4; i++) {
        pos = *getPlayerPos_8023E724(i);
        mEmitters288[i] = spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_8053C740[i], 0, -1, NULL, NULL, true, 1.0f);
        mEmitters298[i] = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x475, 0, -1, NULL, NULL, true, 1.0f);
    }
    for (i = 0; i < 4; i++) {
        fn_80230FD0(i, 0x20048);
    }
    field_0x23c = 0.0f;
    fn_8011921C()->fn_80118454();
}

inline void FourSwordCutscene_8049DA0C::releaseEmitters() {
    for (s32 i = 0; i < 4; i++) {
        if (mEmitters288[i] != NULL) {
            mEmitters288[i]->becomeInvalidEmitter();
            mEmitters288[i] = NULL;
        }
        if (mEmitters298[i] != NULL) {
            mEmitters298[i]->becomeInvalidEmitter();
            mEmitters298[i] = NULL;
        }
    }
}

inline void FourSwordCutscene_8049DA0C::startState7() {
    JGeometry::TVec3f pos;
    s32 i;
    for (i = 0; i < 4; i++) {
        pos = *getPlayerPos_8023E724(i);
        pos.x += 6.0f;
        pos.y += -29.0f;
        spawnEmitter_8013CC50(mLayer_0x4, &pos, 0xcb, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &pos, 0xcc, 0, 0, NULL, NULL, true, 1.0f);
    }
    releaseEmitters();
}

inline void FourSwordCutscene_8049DA0C::startState8() {
    mAnim250.startAnim_801D22DC(2, 0, false);
    mAnim250.setFrame_801D1F14(0.0f);
    mAnim250.mFrameCtrl.setRate(1.0f);
    for (s32 i = 0; i < 4; i++) {
        fn_80230FD0(i, 0x20047);
    }
}

inline void FourSwordCutscene_8049DA0C::startState11() {
    spawnPlayerEffects();
    for (s32 i = 0; i < 4; i++) {
        setPlayerFacing_80230BF4(i, 3);
        fn_80230498(i, getPlayerPos_8023E724(i), 0x3f, 0xff, -128, -128, true);
        fn_80230FD0(i, 0x10015);
    }
}

void FourSwordCutscene_8049DA0C::fn_80117074(s32 index) {
    switch (index) {
    case 2:
        fn_80119DD0()->startFade_80119998(120, 0, 2, 0, 1, 0, 0);
        fn_80234810();
        fn_802382D0(0, false);
        gAudioMgr_8053AF80->startBgm_80458FF0(0x01000020, 1, 0);
        if (mEmitter2a8 != NULL) {
            mEmitter2a8->becomeInvalidEmitter();
            mEmitter2a8 = NULL;
        }
        break;
    case 4:
        mAnim250.mFrameCtrl.setRate(1.0f);
        break;
    case 5:
        startState5();
        break;
    case 6:
        fn_8011921C()->fn_801183BC();
        break;
    case 7:
        startState7();
        break;
    case 8:
        startState8();
        break;
    case 11:
        startState11();
        break;
    case 12:
        break;
    }
    field_0x234 = index;
}

// From here to fn_80116E8C: plain functions that fn_80117074 (the first) and update inline and
// whose own copies the linker dropped. The original's .sdata2 has their literals ahead of
// fn_80117074's, in the order of these functions from the bottom up (tools/pool.py).
// Entering state 11: effect 0x33a 16 above each player.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 11-line definition): split out only where the .sdata2 order needs it.
void FourSwordCutscene_8049DA0C::spawnPlayerEffects() {
    JGeometry::TVec3f pos = mPos;
    for (s32 i = 0; i < 4; i++) {
        const JGeometry::TVec3f* playerPos = getPlayerPos_8023E724(i);
        pos.x = playerPos->x;
        pos.y = playerPos->y;
        pos.z = playerPos->z;
        pos.y += -16.0f;
        spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x33a, 0, 0, NULL, NULL, true, 1.0f);
    }
}

// State 5: the players move apart sideways from field_0x244. True once the first is 24 away.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 16-line definition): split out only where the .sdata2 order needs it.
bool FourSwordCutscene_8049DA0C::spreadPlayers() {
    bool flag = false;
    for (s32 i = 0; i < 4; i++) {
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(i);
        moveToward_801CD950(&field_0x23c, 0.3f, 0.03f);
        moveToward_801CD950(&pos.x, field_0x244.x + 8.0f * (f32)lbl_80464AB0[i], field_0x23c * (f32)lbl_80464AB0[i]);
        setPlayerFacing_80230BF4(i, 2);
        fn_80230498(i, &pos, mAnim250.mCel, 0xff, -128, -128, true);
        mEmitters288[i]->setGlobalTranslation(pos);
        mEmitters298[i]->setGlobalTranslation(pos);
        if (i == 0 && field_0x244.x - pos.x >= 24.0f) {
            flag = true;
        }
    }
    return flag;
}

// State 9: the players rise to 80 above the actor. True once they are there.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 13-line definition): split out only where the .sdata2 order needs it.
bool FourSwordCutscene_8049DA0C::raisePlayers() {
    bool flag = false;
    for (s32 i = 0; i < 4; i++) {
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(i);
        pos.y += 1.1f;
        if (pos.y >= mPos.y + 80.0f) {
            pos.y = mPos.y + 80.0f;
            flag = true;
        }
        fn_802303FC(i, &pos, true);
    }
    return flag;
}

// State 0: whether all four players stand in front of the actor, facing it.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 22-line definition): split out only where the .sdata2 order needs it.
bool FourSwordCutscene_8049DA0C::checkPlayers() {
    for (s32 i = 0; i < 4; i++) {
        if (!fn_8023561C(i)) {
            return false;
        }
    }
    if (getPlayerFacing_80230CFC(0) != 3) {
        return false;
    }
    u16 angle;
    s32 dir;
    s32 player;
    f32 dist;
    findTargetPlayer_801EEC98(mSerial, &angle, &dir, &dist, &player, 0, 0);
    if (dir != 2) {
        return false;
    }
    if (dist <= 16.0f) {
        return true;
    }
    return false;
}

// State 0: opens the message (0x4b2 the first time, 0x4b3 after a "no").
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 7-line definition): split out only where the .sdata2 order needs it.
bool FourSwordCutscene_8049DA0C::showMessage() {
    u32 msg = 0x4b2;
    if (field_0x230 != 0) {
        msg = 0x4b3;
    }
    return fn_8038BD08(0, (u16)msg, mLayer_0x4, JGeometry::TVec3f(0.0f, -10.0f, 0.0f), 0x22);
}

void FourSwordCutscene_8049DA0C::fn_80116E8C() {
    if (field_0x234 >= 2 && field_0x234 <= 11) {
        if (fn_8011921C()->field_0x8 >= 50) {
            cls_804A03A8* obj = gGameRoot2_8053AB10->field_0x30;
            if (obj->field_0x284 == 0) {
                obj->field_0x284 = 1;
                obj->field_0x28c = 0.05f;
            }
        }
    }

    switch (field_0x234) {
    case 0:
    case 1:
    case 2:
    case 12:
        break;
    case 9: {
        JGeometry::TVec3f pos = mPos;
        pos.y += field_0x240;
        moveToward_801CD950(&field_0x240, mPos.y + 32.0f, 0.5f);
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mBlendBoxOn_0x840 == 0) {
            camera->mBlendBoxOn_0x840 = 1;
            camera->mBlendBox_0x844.i.x = camera->mBlendBox_0x844.f.x = pos.x;
            camera->mBlendBox_0x844.i.y = camera->mBlendBox_0x844.f.y = pos.y;
            camera->mBlendWeight_0x854 = 1.0f;
            camera->mMargin_0x1d4 = -1.0f;
            camera->mMinHalfWidth_0x1d8 = -1.0f;
        }
        break;
    }
    case 10:
    case 11: {
        JGeometry::TVec3f pos = mPos;
        pos.y += field_0x240;
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mBlendBoxOn_0x840 == 0) {
            camera->mBlendBoxOn_0x840 = 1;
            camera->mBlendBox_0x844.i.x = camera->mBlendBox_0x844.f.x = pos.x;
            camera->mBlendBox_0x844.i.y = camera->mBlendBox_0x844.f.y = pos.y;
            camera->mBlendWeight_0x854 = 1.0f;
            camera->mMargin_0x1d4 = -1.0f;
            camera->mMinHalfWidth_0x1d8 = -1.0f;
        }
        break;
    }
    default: {
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mBlendBoxOn_0x840 == 0) {
            camera->mBlendBoxOn_0x840 = 1;
            camera->mBlendBox_0x844.i.x = camera->mBlendBox_0x844.f.x = mPos.x;
            camera->mBlendBox_0x844.i.y = camera->mBlendBox_0x844.f.y = mPos.y;
            camera->mBlendWeight_0x854 = 1.0f;
            camera->mMargin_0x1d4 = -1.0f;
            camera->mMinHalfWidth_0x1d8 = -1.0f;
        }
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
        break;
    }
    }
}
