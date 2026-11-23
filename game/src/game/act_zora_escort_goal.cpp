#include <float.h>
#include "game/GameAudio.h"
#include "game/act/act_zora_escort_goal.h"
#include "game/ActorManager.h"
#include "game/Camera.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "JSystem/JMath/JMath.h"
#include "game/Message.h"
#include "game/Player.h"
#include "game/zora_escort_goal_list_80127E20.h"

// The escort's animation keys: (cel, frames) s16 pairs, ended by a cel of -1.
static const s16 lbl_80464F10[10] = {2, 10, 1, 7, 3, 10, 1, 7, -1, -1};
static const s16 lbl_80464F24[10] = {6, 10, 4, 7, 5, 10, 4, 7, -1, -1};
static const s16 lbl_80464F38[10] = {8, 10, 7, 7, 9, 10, 7, 7, -1, -1};
static const s16 lbl_80464F4C[10] = {11, 10, 10, 7, 12, 10, 10, 7, -1, -1};
static const s16 lbl_80464F60[10] = {2, 10, 1, 7, 3, 10, 1, 7, -1, -1};
static const s16 lbl_80464F74[10] = {6, 10, 4, 7, 5, 10, 4, 7, -1, -1};
static const s16 lbl_8053CA98[4] = {7, 7, -1, -1};
static const s16 lbl_80464F88[10] = {11, 10, 10, 7, 12, 10, 10, 7, -1, -1};
static const s16 lbl_80464F9C[8] = {0, 10, 1, 10, 2, 10, -1, -1};

// The key tables of mAnim, in address order.
static const s16* lbl_8049E400[9] = {
    lbl_80464F10,
    lbl_80464F24,
    lbl_80464F38,
    lbl_80464F4C,
    lbl_80464F60,
    lbl_80464F74,
    lbl_8053CA98,
    lbl_80464F88,
    lbl_80464F9C,
};

static inline void approach(f32& cur, f32 target, f32 remaining) {
    if (remaining < FLT_EPSILON) {
        cur = target;
    } else {
        cur += (target - cur) * (1.0f / remaining);
    }
}

static inline void approach(s32& cur, s32 target, f32 remaining) {
    if (remaining < FLT_EPSILON) {
        cur = target;
    } else {
        cur = (s32)((f32)(target - cur) * (1.0f / remaining) + (f32)cur);
    }
}

ZoraEscortGoal_8049E424::ZoraEscortGoal_8049E424()
    : field_0x230(0), field_0x234(0), field_0x238(0), field_0x23c(-1), field_0x240(0),
      field_0x244(0), field_0x248(0), field_0x24c(0), field_0x250(0), field_0x254(0xFF),
      field_0x258(0), field_0x259(0), field_0x25c(0.0f), field_0x260(1.0f), field_0x264(0.0f),
      field_0x268(0.0f), field_0x26c(0.0f), field_0x270(0.0f, 0.0f, 0.0f), mAnim27c(),
      field_0x37c(NULL), field_0x380(NULL) {}

ZoraEscortGoal_8049E424::~ZoraEscortGoal_8049E424() {
    releaseEmitter_801EE434(field_0x37c);
    releaseEmitter_801EE434(field_0x380);
    field_0x37c = NULL;
    field_0x380 = NULL;
}

void ZoraEscortGoal_8049E424::init() {
    ActorBase_804A5770::init();
    if (mSpawnParam_0x90 & 1) {
        field_0x258 = 1;
    }
    field_0x234 = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x238 = (mSpawnParam_0x90 >> 8) & 0xFF;
    mBox.set(-10.0f, -8.0f, 10.0f, 8.0f);
    mBox2.set(mBox);
    mMapBox_0x80.set(mBox);
    mAnim27c.setKeyTables_801D2608(lbl_8049E400, 9);
    mAnim27c.startAnim_801D22DC(6, 2, false);
    mAnim27c.setFrame_801D1F14(0.0f);
    mRail.clear();
    mDrawEnabled = false;
    getZoraEscortGoalList_801283E8()->fn_80128398(this);
    if (field_0x234 > 0) {
        if (!isScreenFlagSet_0x34() && getGameView_802D6A9C()->isScreenFlagSet_802D800C(field_0x234)) {
            setState_80126A4C(1);
        }
    }
}

// FIXME: 98.09 %, missing/extra; behaviour drift.
// ours has 1 fewer instruction (563 vs 564). The original loads the angle of the rail walk with lha + clrlwi (ours lhz), loads r5/r6 for openMessage_8038AD24 before the by-value position copy is stored, and orders a few box float stores differently.
void ZoraEscortGoal_8049E424::update() {
    ActorBase_804A5770::update();
    field_0x260 = 1.0f;
    JGeometry::TBox2f box(mBox);
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;

    switch (field_0x230) {
    case 0:
        if (field_0x234 > 0) {
            if (isScreenFlagSet_0x34()) {
                setState_80126A4C(1);
                setState_80126A4C(11);
            } else if (getGameView_802D6A9C()->isScreenFlagSet_802D800C(field_0x234)) {
                setState_80126A4C(1);
            }
        }
        break;
    case 1:
        if (!getZoraEscortGoalList_801283E8()->fn_801280F8()) {
            if (getZoraEscortGoalList_801283E8()->fn_801282E0()) {
                setState_80126A4C(12);
            } else {
                s32 player = pool_8053CAB4(0x2B7);
                if (player >= 0) {
                    field_0x23c = player;
                    setState_80126A4C(2);
                }
            }
        }
        break;
    case 2:
        fn_8022FF8C(field_0x23c, &box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x23c, mSerial)) {
            setState_80126A4C(1);
        }
        break;
    case 3:
        fn_8022FF8C(field_0x23c, &box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x23c, mSerial)) {
            setState_80126A4C(4);
        }
        break;
    case 5: {
        if (walkRail()) {
            setState_80126A4C(6);
        }
        break;
    }
    case 6:
        field_0x240++;
        if (field_0x240 >= 30) {
            setState_80126A4C(7);
        }
        break;
    case 7: {
        field_0x244++;
        field_0x254 = (60 - field_0x244) * 255 / 60;
        if (field_0x244 >= 60) {
            setState_80126A4C(8);
        }
        break;
    }
    case 8: {
        pool_8053CAA8();
        if (!getZoraEscortGoalList_801283E8()->fn_801280F8()) {
            s32 player = pool_8053CAB4(0x3FB);
            if (player >= 0) {
                field_0x23c = player;
                setState_80126A4C(9);
            }
        }
        break;
    }
    case 9: {
        pool_8053CAA8();
        fn_8022FF8C(field_0x23c, &box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x23c, mSerial)) {
            getZoraEscortGoalList_801283E8()->fn_801281A4();
        }
        break;
    }
    case 10:
        field_0x24c++;
        field_0x254 -= 10;
        if (field_0x24c >= 30) {
            setState_80126A4C(16);
        }
        break;
    case 11:
        setState_80126A4C(8);
        break;
    case 12:
        field_0x250++;
        if (field_0x250 >= 60) {
            setState_80126A4C(13);
        }
        break;
    case 13:
        field_0x250--;
        approach(field_0x254, 0xFF, (f32)field_0x250);
        approach(mPos.x, field_0x270.x, (f32)field_0x250);
        if (field_0x250 <= 0) {
            setState_80126A4C(14);
        }
        break;
    case 14:
        field_0x250++;
        if (field_0x250 >= 60) {
            setState_80126A4C(15);
        }
        break;
    case 15:
    case 16:
        break;
    }

    if (field_0x259) {
        fn_80239914(&box, mLayer_0x4, mSerial);
    }
    mAnim27c.mFrameCtrl.setRate(field_0x260);
    mAnim27c.update_801D1C18();
    gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
}

void ZoraEscortGoal_8049E424::draw() {
    ActorBase_804A5770::draw();
    SpriteParams_801F5AE0 params;
    if (field_0x230 == 0xD || field_0x230 == 0xE) {
        params.field_0x14.r = field_0x254;
        params.field_0x14.g = field_0x254;
        params.field_0x14.b = field_0x254;
    }
    drawSpriteScaled_801F02BC("enemy_both2_sob.bin", mAnim27c.mCel + 0x242, &params, 0x407, 0, false, 1.0f, 1.0f);
}

bool ZoraEscortGoal_8049E424::fn_80126EF8() const {
    if (field_0x230 == 1 || field_0x230 == 0xC) {
        return true;
    }
    return false;
}

// FIXME: 99.30 %, missing/extra; behaviour drift.
void ZoraEscortGoal_8049E424::setState_80126A4C(s32 arg) {
    field_0x230 = arg;
    switch (arg) {
    case 0:
        field_0x259 = 0;
        break;
    case 1:
        field_0x254 = 0xFF;
        field_0x259 = 1;
        mDrawEnabled = true;
        field_0x23c = -1;
        break;
    case 4:
        getZoraEscortGoalList_801283E8()->fn_8012824C();
        break;
    case 5:
        mRail.fn_800FE3EC(field_0x238, mPos, mLayer_0x4);
        break;
    case 6:
        mAnim27c.startAnim_801D22DC(6, 2, false);
        break;
    case 7:
        field_0x240 = 0;
        break;
    case 8: {
        field_0x254 = 0xFF;
        field_0x244 = 0;
        field_0x264 = mPos.x;
        field_0x268 = mPos.y;
        field_0x26c = mPos.z;
        mAnim27c.startAnim_801D22DC(8, 2, false);
        mBox.set(-20.0f, -8.0f, 20.0f, 8.0f);
        mBox2.set(mBox);
        mMapBox_0x80.set(mBox);
        if (field_0x258) {
            spawnEmitters();
        }
        getGameView_802D6A9C()->setSwitchBit_802D8050(getScreenFlagBit_0x3C(), true, 0xFF);
        getZoraEscortGoalList_801283E8()->fn_80128228();
        break;
    }
    case 10:
        for (s32 i = 0; i < 4; i++) {
            if (gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) {
                healPlayerGroup_80236554(i, getPlayerMaxLife_80237810(i), false);
                fn_80237524(i, 0x180);
            }
        }
        startSound_801F0E34(0x40090, 0);
        break;
    case 11: {
        mRail.fn_800FE3EC(field_0x238, mPos, mLayer_0x4);
        JGeometry::TVec3f step(0.0f, 0.0f, 0.0f);
        field_0x260 = 1.0f;
        u8 count = mRail.fn_800FD894(-1, NULL, 0) - 1;
        step = mRail.fn_800FDC70(mPos, 1, count);
        mVel_0x3c.set(step);
        mPos += mVel_0x3c;
        field_0x264 = mPos.x;
        field_0x268 = mPos.y;
        field_0x26c = mPos.z;
        break;
    }
    case 12:
        mAnim27c.startAnim_801D22DC(6, 2, false);
        field_0x250 = 0;
        break;
    case 13:
        mAnim27c.startAnim_801D22DC(6, 2, false);
        field_0x250 = 120;
        field_0x254 = 0;
        field_0x270 = *getZoraEscortGoalList_801283E8()->fn_80127F90();
        if (field_0x258) {
            startSound_801F0E34(0x400EE, 0);
        }
        break;
    case 14:
        field_0x250 = 0;
        break;
    case 15:
        if (getActorManager_801F856C()->findSerialByActorId_801F7A08('SPFR') < 0) {
            u32 arg = getScreenFlagBit_0x3C() << 27;
            getActorManager_801F856C()->spawnActor_801F9484('SPFR', mLayer_0x4, &field_0x270, arg, -1, -1);
            getZoraEscortGoalList_801283E8()->fn_80128074();
        }
        break;
    case 16:
        if (field_0x37c != NULL) {
            field_0x37c->becomeInvalidEmitter();
        }
        if (field_0x380 != NULL) {
            field_0x380->becomeInvalidEmitter();
        }
        markDead_801F3668();
        break;
    }
}

// From here down: plain functions that update and setState_80126A4C inline and whose own copies the
// linker dropped. The original's .sdata2 has their literals ahead of setState_80126A4C's, in the order
// of these functions from the bottom of the file up, so they were generated before it
// (tools/pool.py).
// Shared by update's states 1 and 8, which differ only in the message: the player who
// started the talk, or -1.
// R2 pool helper (inlined at 2 call sites); was startTalk.
s32 ZoraEscortGoal_8049E424::pool_8053CAB4(u32 msg) {
    JGeometry::TBox2f box(mBox);
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
    s32 player = fn_8022FD8C(mSerial);
    if (player < 0) {
        return -1;
    }
    if (!fn_802300EC(player)) {
        return -1;
    }
    JGeometry::TVec3f msgPos(mPos);
    msgPos.y -= 64.0f;
    if (openMessage_8038AD24(player, msg, mSerial, mLayer_0x4, msgPos, 0x28)) {
        return player;
    }
    return -1;
}

// State 5: a step along the rail at speed 0.5, facing the way it goes. True at the rail's end
// (or when field_0x238 gives no rail).
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 25-line definition): split out only where the .sdata2 order needs it.
bool ZoraEscortGoal_8049E424::walkRail() {
    bool flag;
    if (field_0x238 <= 0) {
        flag = true;
    } else {
        JGeometry::TVec3f step(0.0f, 0.0f, 0.0f);
        field_0x260 = 1.0f;
        u16 angle = 0;
        s32 layer = mLayer_0x4;
        JGeometry::TVec3f pos(mPos);
        if (mRail.stepAlongRail_800FDEA0(&pos, 0.5f, &step, &layer, &angle, 0x200) == 3) {
            flag = true;
        } else {
            mVel_0x3c.set(step);
            mPos += mVel_0x3c;
            s32 anim = angleToDir_801F681C(angle);
            if (anim != mAnim27c.mAnim) {
                mAnim27c.startAnim_801D22DC(anim, 2, false);
                mAnim27c.setFrame_801D1F14(0.0f);
            }
            flag = false;
        }
    }
    return flag;
}

// The bobbing of states 8 and 9: z follows a sine of field_0x248, and the emitters follow mPos.
// R2 pool helper (inlined at 2 call sites); was hover.
void ZoraEscortGoal_8049E424::pool_8053CAA8() {
    JGeometry::TVec3f pos(0.0f, 0.0f, 0.0f);
    field_0x248 += 600;
    if (field_0x248 >= 0x10000) {
        field_0x248 = 0;
    }
    pos.z = 4.0f * JMath::sincosTable_.sinShort(field_0x248) + 4.0f;
    mPos.z = field_0x26c + pos.z;
    if (field_0x258) {
        pos = mPos;
        pos.x += 8.0f;
        setEmitterPos_8013CB44(field_0x37c, &pos, 0, -1, true);
        setEmitterPos_8013CB44(field_0x380, &pos, 0, -1, true);
    }
}

// The two emitters that follow the goal (hover moves them), 8 units right of mPos.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 6-line definition): split out only where the .sdata2 order needs it.
void ZoraEscortGoal_8049E424::spawnEmitters() {
    JGeometry::TVec3f pos(mPos);
    pos.x += 8.0f;
    field_0x37c = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x183, 0, -1, 0, 0, true, 1.0f);
    field_0x380 = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x184, 0, -1, 0, 0, true, 1.0f);
}
