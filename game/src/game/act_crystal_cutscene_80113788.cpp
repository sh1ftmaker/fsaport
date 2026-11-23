#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/Effect.h"
#include "game/GameView.h"
#include "game/ActorManager.h"
#include "game/Camera.h"
#include "game/cls_804AE9B0.h"
#include "game/EnemyBase.h"
#include "game/Map.h"
#include "game/Message.h"
#include "game/Player.h"
#include "game/StageFlag.h"
#include "game/cls_8049D83C.h"
#include "game/GameRootSub20.h"
#include "game/RotMatrix.h"
#include "game/SpriteParams.h"
#include "game/ActorId.h"
#include "game/cls_804A8638.h"
#include "game/act/act_environment_control_2.h"
#include "JSystem/JUtility/JUTVideo.h"

// Sprite animation frame lists of mAnim ({frame, duration} pairs, -1 terminated).
static const s16 lbl_8053C660[4] = {0, 9, -1, -1};
static const s16 lbl_8053C668[4] = {0, 9, -1, -1};
static const s16 lbl_804647C8[14] = {0, 1, 1, 1, 2, 1, 3, 1, 4, 1, 5, 1, -1, -1};
static const s16 lbl_8053C670[4] = {8, 20, -1, -1};
static const s16 lbl_804647E4[14] = {0, 9, 1, 9, 2, 9, 3, 9, 4, 9, 5, 9, -1, -1};
static const s16 lbl_80464800[8] = {11, 10, 10, 30, 8, 10, -1, -1};
static const s16 lbl_80464810[8] = {0, 9, 1, 9, 2, 9, -1, -1};

static const s16* lbl_8049D820[7] = {
    lbl_8053C660, lbl_8053C668, lbl_804647C8, lbl_8053C670, lbl_804647E4, lbl_80464800, lbl_80464810,
};

static const char* lbl_80539F00 = "kk_black.bti";

static const s32 lbl_80464830[8] = {
    0x199, 0x1E7, 0x17F, 0x201, 0x1CD, 0x1B3, 0x21B, 0x21B,
};

// Message ids of vfunc_0x60's case 5, per field_0x238.
static const s32 lbl_80464850[7] = {
    0x14C, 0x173, 0x20E, 0x21A, 0x29B, 0x2B1, 0x2CD,
};

static const s32 lbl_8046486C[18] = {
    0, 0, 0, 0x96, 0x1FE, 0x32A, 0x3F2, 0x51E, 0x582,
    0x5B4, 0x6E0, 0x6E0, 0x7EE, 0x852, 0x852, 0x8AC, 0x8D4, 0x974,
};

// Frame tables of fn_8011395C (indexed by field_0x240): where the counter restarts, where the
// stage ends, and the crystal slot of each field_0x238.
static const s32 lbl_804648B4[3] = {0, 0xC9, 0x191};
static const s32 lbl_804648C0[3] = {0xC8, 0x190, 0x258};
static const s32 lbl_804648CC[7] = {1, 4, 0, 5, 3, 2, 6};

static const u16 lbl_804648E8[5] = {0x105, 0x106, 0x107, 0x108, 0x109};
static const s32 lbl_804648F4[5] = {2, 0, 0, 0, 0};

// Effect ids of vfunc_0x60: the seven of case 6, then one table per emitter indexed by field_0x238.
static const u16 lbl_80464908[7] = {0x2AB, 0x2AC, 0x2AD, 0x2C4, 0x2C5, 0x2C7, 0x2C6};
static const u16 lbl_80464918[7] = {0x2C8, 0x2CD, 0x2CA, 0x2C9, 0x2CC, 0x2CB, 0x2CE};
static const u16 lbl_80464928[7] = {0x30B, 0x310, 0x30D, 0x30C, 0x30F, 0x30E, 0x311};
static const u16 lbl_80464938[7] = {0x2AF, 0x2BE, 0x2B5, 0x2B2, 0x2BB, 0x2B8, 0x2C1};
static const u16 lbl_80464948[7] = {0x2B0, 0x2BF, 0x2B6, 0x2B3, 0x2BC, 0x2B9, 0x2C2};
static const u16 lbl_80464958[7] = {0x2B1, 0x2C0, 0x2B7, 0x2B4, 0x2BD, 0x2BA, 0x2C3};

// 0x80115590.
CrystalCutscene_8049D83C::CrystalCutscene_8049D83C()
    : field_0x230(1), field_0x231(1), field_0x232(0), field_0x233(0), field_0x234(1), field_0x238(0),
      field_0x23c(0), field_0x240(0), field_0x244(0), field_0x248(0), field_0x24c(0), field_0x250(0.0f),
      field_0x254(0.0f), field_0x258(0.0f), field_0x25c(0.0f), field_0x260(NULL) {
    field_0x29c = NULL;
    field_0x2a0 = NULL;
    field_0x2a4 = NULL;
    field_0x2e4 = NULL;
    for (s32 i = 0; i < 7; i++) {
        field_0x2a8[i] = NULL;
        if (i < 2) {
            field_0x2e8[i] = NULL;
        }
        if (i < 3) {
            field_0x2c4[i] = NULL;
        }
        if (i < 5) {
            field_0x2d0[i] = NULL;
        }
    }
    gAudioMgr_8053AF80->stopBgm_80458F9C(0x3C);
}

// An inline of its own: its loop registers are handed out before the destructor's
// (docs/REGALLOC.md, rule 4).
static inline void releaseEmitterArrays(CrystalCutscene_8049D83C* self) {
    for (s32 i = 0; i < 7; i++) {
        releaseEmitter_801EE434(self->field_0x2a8[i]);
        self->field_0x2a8[i] = NULL;
        if (i < 2) {
            releaseEmitter_801EE434(self->field_0x2e8[i]);
            self->field_0x2e8[i] = NULL;
        }
        if (i < 3) {
            releaseEmitter_801EE434(self->field_0x2c4[i]);
            self->field_0x2c4[i] = NULL;
        }
        if (i < 5) {
            releaseEmitter_801EE434(self->field_0x2d0[i]);
            self->field_0x2d0[i] = NULL;
        }
    }
}

// 0x80115430.
CrystalCutscene_8049D83C::~CrystalCutscene_8049D83C() {
    if (field_0x2a0 != NULL) {
        releaseEmitter_801EE434(field_0x2a0);
        field_0x2a0 = NULL;
    }
    if (field_0x29c != NULL) {
        releaseEmitter_801EE434(field_0x29c);
        field_0x29c = NULL;
    }
    releaseEmitterArrays(this);
    if (field_0x2a4 != NULL) {
        releaseEmitter_801EE434(field_0x2a4);
        field_0x2a4 = NULL;
    }
    if (field_0x2e4 != NULL) {
        releaseEmitter_801EE434(field_0x2e4);
        field_0x2e4 = NULL;
    }
}

// 0x80115248.
// FIXME(shotgun): 96.72 %, scheduling; behaviour clean.
void CrystalCutscene_8049D83C::init() {
    DemoActorBase_8049D140::init();
    field_0x238 = mSpawnParam_0x90 & 0xFF;
    field_0x244 = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x260 = gGameRoot_8053AAF8->field_0xa4->getResource('????', lbl_80539F00);
    mAnim.setKeyTables_801D2608(lbl_8049D820, 7);
    switch (field_0x238) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        mAnim.startAnim_801D22DC(4, 2, false);
        break;
    case 6:
    default:
        mAnim.startAnim_801D22DC(6, 2, false);
        break;
    }
    mAnim.setFrame_801D1F14(0.0f);
    mPos.y -= 32.0f;
    spawnEmitters();
    getGameView_802D6A9C()->fn_802D7440(0);
}

// 0x80114C98.
// FIXME(shotgun): 97.58 %, missing/extra; behaviour drift.
// (case 5: the original keeps the flag test and the input test in a bool temporary)
void CrystalCutscene_8049D83C::update() {
    DemoActorBase_8049D140::update();
    updateCameraFocus();
    switch (field_0x23c) {
    case 0:
        gAudioMgr_8053AF80->startBgm_80458FF0(0x100001B, false, 0);
        vfunc_0x60(1);
        break;
    case 3:
        for (s32 i = 0; i < 4; i++) {
            s32 level = fn_8023BCD4(i) - 2;
            if (level <= 0x1E) {
                level = 0;
            }
            JGeometry::TVec3f playerPos = *getPlayerPos_8023E724(i);
            fn_80230498(i, &playerPos, fn_8023CBC0(i), level, -0x80, -0x80, true);
        }
        if (fn_8011921C()->field_0x8 == 0x1B8) {
            gAudioMgr_8053AF80->stopBgm_80458F9C(0x3C);
        }
        if (fn_8016A3AC()->field_0x1ef == 0 && field_0x2e4 == NULL) {
            field_0x2e4 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x54C, 0, 0, NULL, NULL, true, 1.0f);
        }
        break;
    case 4: {
        moveToward_801CD950(&field_0x25c, 0.02f, 0.0004f);
        moveToward_801CD950(&field_0x254, 0.15f, 0.01f);
        mPos.y -= field_0x254;
        f32 limit = mSpawnPos.y - 48.0f;
        if (mPos.y <= limit) {
            mPos.y = limit;
        }
        field_0x248 += 2;
        if (field_0x248 >= 0xE1) {
            field_0x248 = 0xE1;
        }
        break;
    }
    case 5:
        if (isStageFlagSet(field_0x238, 3)) {
            gGameRoot_8053AAF8->field_0x85 = 2;
        }
        field_0x248 = 0xE1;
        if (fn_8011921C()->field_0x1 == 0) {
            moveToward_801CD950(&field_0x25c, 0.0f, -0.0002f);
        }
        fn_8022FE80(0, mLayer_0x4, mSerial, true, false);
        if (stepMessageWindow_803865B8(0)) {
            fn_80388EEC(0);
            fn_8011921C()->fn_801183BC();
            break;
        }
        if (isStageFlagSet(field_0x238, 3) && lbl_8053AD30->fn_80309C4C(0x1000, -1) != 0) {
            fn_80387494(0);
            fn_8011921C()->fn_801183BC();
        }
        break;
    case 6:
        switch (fn_8011921C()->field_0x8) {
        case 0x43D:
            startSound_801F0E34(0x30082, 0);
            break;
        case 0x46A:
            startSound_801F0E34(0x30082, 0);
            startSound_801F0E34(0x30083, 0);
            break;
        }
        break;
    case 11:
        moveToward_801CD950(&mPos.y, mSpawnPos.y, 0.3f);
        break;
    case 13:
        fn_8022FE80(0, mLayer_0x4, mSerial, true, false);
        if (stepMessageWindow_803865B8(0)) {
            fn_80388EEC(0);
            fn_8011921C()->fn_801183BC();
        }
        break;
    case 16:
        for (s32 i = 0; i < 2; i++) {
            if (field_0x2e8[i] != NULL) {
                setEmitterPos_8013CB44(field_0x2e8[i], &mPos, 0, 0, true);
                field_0x2e8[i]->setGlobalEnvColor(0x2D, 0x78, 0xFF);
            }
        }
        break;
    case 17:
        break;
    }
    if (field_0x233 != 0) {
        field_0x258 += field_0x25c;
    }
    mAnim.update_801D1C18();
    if (field_0x230 != 0) {
        fn_8011395C();
    }
}

// 0x80114C1C.
void CrystalCutscene_8049D83C::draw() {
    DemoActorBase_8049D140::draw();
    fn_80113790();
    switch (field_0x23c) {
    case 0x10:
        fn_80113B54(0x1D6);
        break;
    default:
        if (field_0x232 != 0) {
            fn_80113B54(lbl_80464830[field_0x238]);
        }
        break;
    }
}

// 0x80114860.
void CrystalCutscene_8049D83C::vfunc_0x54() {
    cls_801192AC* sub = fn_8011921C();
    s32 t = sub->field_0x8;
    switch (field_0x238) {
    case 0:
        if (t == lbl_8046486C[2]) {
            vfunc_0x60(2);
        } else if (t == lbl_8046486C[3]) {
            vfunc_0x60(3);
        } else if (t == lbl_8046486C[4]) {
            vfunc_0x60(4);
        } else if (t == lbl_8046486C[5]) {
            vfunc_0x60(5);
        } else if (t == lbl_8046486C[6]) {
            vfunc_0x60(6);
        } else if (t == lbl_8046486C[7]) {
            vfunc_0x60(7);
        } else if (t == lbl_8046486C[9]) {
            vfunc_0x60(9);
        } else if (t == lbl_8046486C[11]) {
            vfunc_0x60(11);
        } else if (t == lbl_8046486C[12]) {
            vfunc_0x60(12);
        } else if (t == lbl_8046486C[13]) {
            vfunc_0x60(13);
        } else if (t == lbl_8046486C[14]) {
            vfunc_0x60(14);
        } else if (t == lbl_8046486C[15]) {
            vfunc_0x60(15);
        } else if (t == lbl_8046486C[16]) {
            vfunc_0x60(16);
        } else if (t == lbl_8046486C[17]) {
            vfunc_0x60(17);
        }
        break;
    default:
        if (t == lbl_8046486C[2]) {
            vfunc_0x60(2);
        } else if (t == lbl_8046486C[3]) {
            vfunc_0x60(3);
        } else if (t == lbl_8046486C[4]) {
            vfunc_0x60(4);
        } else if (t == lbl_8046486C[5]) {
            vfunc_0x60(5);
        } else if (t == lbl_8046486C[6]) {
            vfunc_0x60(6);
        } else if (t == lbl_8046486C[7]) {
            vfunc_0x60(7);
        } else if (t == lbl_8046486C[9]) {
            vfunc_0x60(9);
        } else if (t == lbl_8046486C[10]) {
            vfunc_0x60(10);
        }
        break;
    }
}

// 0x80113EC4.
// Still differs: `this` and the .rodata base take each other's register (r31 / r29), the loop
// counters rotate with them, and case 6 zeroes field_0x233 from the loop counter's register.
// FIXME(shotgun): 97.93 %, scheduling+regalloc; behaviour drift.
void CrystalCutscene_8049D83C::vfunc_0x60(s32 arg) {
    switch (arg) {
    case 2:
        field_0x234 = 0;
        for (s32 i = 0; i < 5; i++) {
            field_0x2d0[i]->becomeInvalidEmitter();
            field_0x2d0[i] = NULL;
        }
        fn_8011921C()->setField_0x2(1);
        for (s32 i = 0; i < 4; i++) {
            JGeometry::TVec3f playerPos = *getPlayerPos_8023E724(i);
            fn_80230498(i, &playerPos, fn_8023CBC0(i), 0xFF, 0, 0, true);
        }
        break;
    case 3: {
        field_0x234 = 1;
        field_0x240 = 1;
        field_0x24c = lbl_804648B4[1];
        field_0x2c4[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x2AE, 0, 2, NULL, NULL, true, 0.5f);
        field_0x2c4[1] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_80464938[field_0x238], 0, 2, NULL, NULL, true, 0.5f);
        field_0x2c4[2] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_80464948[field_0x238], 0, 2, NULL, NULL, true, 0.5f);
        field_0x2a4 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_80464958[field_0x238], 0, 2, NULL, NULL, true, 1.0f);
        s32 swevSerial = getActorManager_801F856C()->findSerialByActorId_801F7A08(ACTOR_SWEV);
        ActorBase_804A5770* swev = getActorManager_801F856C()->findActorBySerial_801F666C(swevSerial);
        if (swev != NULL) {
            static_cast<EnvironmentSwitchControl_804A8638*>(swev)->fn_802B3A3C();
        }
        s32 sev2Serial = getActorManager_801F856C()->findSerialByActorId_801F7A08(ACTOR_SEV2);
        ActorBase_804A5770* sev2 = getActorManager_801F856C()->findActorBySerial_801F666C(sev2Serial);
        if (sev2 != NULL) {
            static_cast<EnvironmentControl2_804A9CD0*>(sev2)->fn_802E43E0();
        }
        startSound_801F0E34(0x400D5, 0);
        break;
    }
    case 4:
        switch (field_0x238) {
        case 6:
            gAudioMgr_8053AF80->startBgm_80458FF0(0x100000F, false, 0);
            break;
        default:
            gAudioMgr_8053AF80->startBgm_80458FF0(0x100001C, false, 0);
            break;
        }
        field_0x233 = 1;
        break;
    case 5: {
        JGeometry::TVec3f pos;
        pos.x = (s32)JUTVideo::getManager()->getRenderMode()->fbWidth / 2.0f;
        pos.y = (s32)JUTVideo::getManager()->getRenderMode()->efbHeight / 2.0f;
        pos.z = 0.0f;
        pos.y += -16.0f;
        fn_8022FE80(0, mLayer_0x4, mSerial, true, false);
        if (fn_8038BD08(0, lbl_80464850[field_0x238], mLayer_0x4, pos, 0x62)) {
            fn_8011921C()->fn_80118454();
        }
        break;
    }
    case 6:
        field_0x233 = 0;
        spawnSealEmitters();
        startSound_801F0E34(0x30082, 0);
        fn_8011921C()->setField_0x2(0);
        break;
    case 7:
        field_0x230 = 0;
        field_0x232 = 1;
        break;
    case 8:
        if (field_0x2e4 != NULL) {
            field_0x2e4->becomeInvalidEmitter();
            field_0x2e4 = NULL;
        }
        break;
    case 9: {
        fn_80119DD0()->startFade_80119998(0xB4, 0, 0, 0, 1, 0, 0);
        field_0x248 = 1;
        positionPlayers();
        if (field_0x238 == 0) {
            fn_8011921C()->setField_0x2(1);
        } else {
            field_0x232 = 0;
        }
        if (field_0x2a0 != NULL) {
            releaseEmitter_801EE434(field_0x2a0);
            field_0x2a0 = NULL;
        }
        if (field_0x29c != NULL) {
            releaseEmitter_801EE434(field_0x29c);
            field_0x29c = NULL;
        }
        for (s32 i = 0; i < 7; i++) {
            releaseEmitter_801EE434(field_0x2a8[i]);
            field_0x2a8[i] = NULL;
            if (i < 2) {
                releaseEmitter_801EE434(field_0x2e8[i]);
                field_0x2e8[i] = NULL;
            }
            if (i < 3) {
                releaseEmitter_801EE434(field_0x2c4[i]);
                field_0x2c4[i] = NULL;
            }
            if (i < 5) {
                releaseEmitter_801EE434(field_0x2d0[i]);
                field_0x2d0[i] = NULL;
            }
        }
        if (field_0x2a4 != NULL) {
            releaseEmitter_801EE434(field_0x2a4);
            field_0x2a4 = NULL;
        }
        if (field_0x2e4 != NULL) {
            releaseEmitter_801EE434(field_0x2e4);
            field_0x2e4 = NULL;
        }
        s32 crystal = field_0x238;
        if (crystal != 0 && crystal < 6) {
            pool_8053C678(crystal);
        }
        break;
    }
    case 10: {
        lbl_8053AD70->fn_8037AD00(field_0x238);
        field_0x248 = 0;
        markDead_801F3668();
        s32 bit = field_0x244;
        if (bit > 0) {
            getGameView_802D6A9C()->setSwitchBit_802D8050(bit, true, 0xFF);
        }
        break;
    }
    case 11:
        field_0x232 = 1;
        field_0x254 = 0.0f;
        mAnim.startAnim_801D22DC(4, 2, false);
        mAnim.setFrame_801D1F14(0.0f);
        break;
    case 12:
        mAnim.startAnim_801D22DC(5, 0, false);
        mAnim.setFrame_801D1F14(0.0f);
        break;
    case 13:
        fn_8022FE80(0, mLayer_0x4, mSerial, true, false);
        fn_8022FD8C(mSerial);
        fn_8038BD08(0, 0x14B, mLayer_0x4, JGeometry::TVec3f(0.0f, -16.0f, 0.0f), 0x4022);
        fn_8011921C()->fn_80118454();
        break;
    case 15:
        mDrawEnabled = false;
        mPos.z = 20.0f;
        spawnBurstEmitters();
        startSound_801F0E34(0x400D9, 0);
        break;
    case 16:
        mDrawEnabled = true;
        field_0x2e8[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x32C, 0, 0, NULL, NULL, true, 1.0f);
        field_0x2e8[1] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x183, 0, 0, NULL, NULL, true, 1.0f);
        mAnim.startAnim_801D22DC(2, 2, false);
        mAnim.setFrame_801D1F14(0.0f);
        break;
    case 17: {
        fn_8011921C()->setField_0x2(0);
        lbl_8053AD70->fn_8037AD00(field_0x238);
        field_0x248 = 0;
        for (s32 i = 0; i < 2; i++) {
            if (field_0x2e8[i] != NULL) {
                field_0x2e8[i]->becomeInvalidEmitter();
                field_0x2e8[i] = NULL;
            }
        }
        markDead_801F3668();
        s32 bit = field_0x244;
        if (bit > 0) {
            getGameView_802D6A9C()->setSwitchBit_802D8050(bit, true, 0xFF);
            s32 crystal = field_0x238;
            if (crystal < 6) {
                pool_8053C678(crystal);
            }
        }
        break;
    }
    }
    field_0x23c = arg;
}

// 0x80113E98.
s32 CrystalCutscene_8049D83C::vfunc_0x5C() {
    switch (field_0x23c) {
    case 0xA:
    case 0x11:
        return 1;
    default:
        return 0;
    }
}

// Not auto-inlined: draw calls fn_80113790 out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// Darkens the screen: draws the image field_0x260 scaled by 96 with an alpha of field_0x248.
// 0x80113790.
// FIXME(shotgun): 91.17 %, missing/extra; behaviour clean.
static inline void drawSob_80113B54(s32 layer, const JGeometry::TVec3f* pos, const char* name, s32 frame, SpriteParams_801F5AE0* params) {
    if (layer >= 8) {
        return;
    }
    void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', name);
    gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pos, res, frame, params, 1.0f, 1.0f);
}

// FIXME: 96.91 %, missing/extra; behaviour clean.
void CrystalCutscene_8049D83C::fn_80113B54(s32 arg) {
    SpriteParams_801F5AE0 params;
    SpriteParams_801F5AE0 params2;
    JGeometry::TVec3f pos(mPos);
    pos.y = mSpawnPos.y;
    if (arg == 0x1D6) {
        drawShadow_801EF830(mLayer_0x4, &pos, &params2, 7, "enemy_both2_sob.bin", arg + mAnim.mCel, NULL);
        drawSob_80113B54(mLayer_0x4, &mPos, "enemy_both2_sob.bin", arg + mAnim.mCel, &params);
    } else {
        drawShadow_801EF830(mLayer_0x4, &pos, &params2, 7, "enemy_gc_sob.bin", arg + mAnim.mCel, NULL);
        drawSob_80113B54(mLayer_0x4, &mPos, "enemy_gc_sob.bin", arg + mAnim.mCel, &params);
    }
}

// FIXME(shotgun): 99.68 %, same-text; behaviour clean.
void CrystalCutscene_8049D83C::fn_8011395C() {
    Mtx mtx;
    PSMTXTrans(mtx, mPos.x, mPos.y, mPos.z);
    JGeometry::TVec3f axisX(1.0f, 0.0f, 0.0f);
    makeAxisRotation_8010F668(mtx, &axisX, 0.08726646f);
    Mtx rot;
    rot[0][0] = rot[1][0] = rot[2][0] = rot[0][1] = rot[1][1] = rot[2][1] = rot[0][2] = rot[1][2] =
        rot[2][2] = rot[0][3] = rot[1][3] = rot[2][3] = 0.0f;
    JGeometry::TVec3f axisZ(0.0f, 0.0f, 1.0f);
    makeAxisRotation_8010F668(rot, &axisZ, field_0x258);
    PSMTXConcat(mtx, rot, mtx);
    if (field_0x234 != 0) {
        field_0x24c++;
    }
    switch (field_0x240) {
    case 0:
    case 2:
        if (field_0x24c >= lbl_804648C0[field_0x240]) {
            field_0x24c = lbl_804648B4[field_0x240];
        }
        break;
    case 1:
        if (field_0x24c >= lbl_804648C0[field_0x240]) {
            field_0x240 = 2;
        }
        break;
    }
    s32 index = lbl_804648CC[field_0x238];
    s32 frame = field_0x24c;
    GameRootSub20_801902C0::Crystal& crystal = fn_801902C0()->mCrystals[index];
    if (crystal.mModel != NULL && crystal.mActive == 0) {
        crystal.mActive = 1;
        crystal.mPos.x = 0.5f;
        crystal.mPos.y = 0.5f;
        crystal.mPos.z = 0.5f;
        JMath::gekko_ps_copy12(crystal.mMtx, mtx);
        crystal.field_0x48 = -1.0f;
        crystal.field_0x4c = frame;
        crystal.field_0x54 = 0;
        crystal.field_0x58 = 2;
    }
}

void CrystalCutscene_8049D83C::fn_80113790() {
    if (field_0x248 > 0) {
        SpriteParams_801F5AE0 params;
        Mtx mtx;
        PSMTXTrans(mtx, 0.0f, 0.0f, 0.0f);
        mtx[0][0] = 96.0f;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = 1.0f;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = 96.0f;
        params.field_0x14.set(0, 0, 0, field_0x248);
        params.mMtx = mtx;
        params.field_0x1d = -1;
        params.field_0x2d = 1;
        JGeometry::TVec3f pos = mPos;
        pos.z = 0.0f;
        void* res = field_0x260;
        if (mLayer_0x4 < 8) {
            gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&pos, res, &params);
        }
    }
}
#pragma auto_inline reset

// 0x80113788.
s32 CrystalCutscene_8049D83C::vfunc_0x58() {
    return field_0x23c;
}

// Plain function init inlines (the linker dropped its own copy): its literals 80.0f and 0.0f
// are pooled before those of fn_80113790.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 12-line definition): split out only where the .sdata2 order needs it.
void CrystalCutscene_8049D83C::spawnEmitters() {
    JGeometry::TVec3f pos[5] = {
        mPos,
        JGeometry::TVec3f(mSpawnPos.x - 80.0f, mSpawnPos.y - 48.0f, 0.0f),
        JGeometry::TVec3f(mSpawnPos.x + 80.0f, mSpawnPos.y - 48.0f, 0.0f),
        JGeometry::TVec3f(mSpawnPos.x - 80.0f, mSpawnPos.y + 48.0f, 0.0f),
        JGeometry::TVec3f(mSpawnPos.x + 80.0f, mSpawnPos.y + 48.0f, 0.0f),
    };
    for (s32 i = 0; i < 5; i++) {
        field_0x2d0[i] = spawnEmitter_8013CC50(mLayer_0x4, &pos[i], lbl_804648E8[i], 0, lbl_804648F4[i], NULL, NULL, true, 1.0f);
    }
}

// Plain functions inlined by their callers (the linker dropped their out-of-line copies);
// their order below spawnEmitters sets the .sdata2 float-literal pool order.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 7-line definition): split out only where the .sdata2 order needs it.
void CrystalCutscene_8049D83C::spawnSealEmitters() {
    for (s32 i = 0; i < 7; i++) {
        field_0x2a8[i] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_80464908[i], 0, 2, NULL, NULL, true, 0.5f);
    }
    field_0x29c = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_80464918[field_0x238], 0, 2, NULL, NULL, true, 0.5f);
    field_0x2a0 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_80464928[field_0x238], 0, 2, NULL, NULL, true, 1.0f);
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 4-line definition): split out only where the .sdata2 order needs it.
void CrystalCutscene_8049D83C::spawnBurstEmitters() {
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x20, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x21, 0, 0, NULL, NULL, true, 1.0f);
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 12-line definition): split out only where the .sdata2 order needs it.
void CrystalCutscene_8049D83C::positionPlayers() {
    JGeometry::TVec3f pos = mSpawnPos;
    pos.y += 48.0f;
    pos.x -= 36.0f;
    for (s32 i = 0; i < 4; i++) {
        if (i > 0) {
            pos.x += 24.0f;
        }
        fn_80230498(i, &pos, 0x3F, 0xFF, -0x80, -0x80, true);
        setPlayerFacing_80230BF4(i, 3);
    }
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 44-line definition): split out only where the .sdata2 order needs it.
void CrystalCutscene_8049D83C::updateCameraFocus() {
    switch (field_0x23c) {
    case 0:
    case 1:
    case 10:
    case 17:
        break;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16: {
        JGeometry::TVec3f focus = mPos;
        focus.y += 32.0f;
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&focus);
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mShowWholeArea_0x7f5 == 0) {
            camera->mFocusOverride_0x7f4 = 1;
            camera->mFocusMode_0x7f8 = 4;
            camera->mFocusPos_0x7fc.x = focus.x;
            camera->mFocusPos_0x7fc.y = focus.y;
            camera->mFocusPos_0x7fc.z = focus.z;
            camera->mOverrideBlend_0x7f6 = 0;
        }
        break;
    }
    default: {
        JGeometry::TVec3f focus = mPos;
        focus.y += 16.0f + (mSpawnPos.y - mPos.y - 32.0f);
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&focus);
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mShowWholeArea_0x7f5 == 0) {
            camera->mFocusOverride_0x7f4 = 1;
            camera->mFocusMode_0x7f8 = 4;
            camera->mFocusPos_0x7fc.x = focus.x;
            camera->mFocusPos_0x7fc.y = focus.y;
            camera->mFocusPos_0x7fc.z = focus.z;
            camera->mOverrideBlend_0x7f6 = 0;
        }
        break;
    }
    }
}

// R2 pool helper (inlined at 2 call sites); was spawnCrystal.
void CrystalCutscene_8049D83C::pool_8053C678(s32 crystal) {
    JGeometry::TVec3f spawnPos = mSpawnPos;
    spawnPos.z = 20.0f;
    getActorManager_801F856C()->spawnActor_801F9484('CRNV', mLayer_0x4, &spawnPos, crystal, -1, -1);
}
