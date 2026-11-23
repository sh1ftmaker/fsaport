#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/cls_8049E350.h"

// Animations of mAnim244 and mAnim27c: (cel, duration) pairs ended by a cel < 0.
static const s16 lbl_80464EA0[6] = {0, 20, 1, 15, -1, -1};
static const s16 lbl_80464EAC[6] = {2, 20, 3, 15, -1, -1};
static const s16 lbl_80464EB8[6] = {4, 20, 5, 15, -1, -1};
static const s16 lbl_80464EC4[6] = {6, 20, 7, 15, -1, -1};
static const s16 lbl_80464ED0[6] = {0, 4, 2, 10, -1, -1};
static const s16 lbl_80464EDC[6] = {0, 4, 3, 10, -1, -1};

static const s16* lbl_8049E330[4] = {lbl_80464EA0, lbl_80464EAC, lbl_80464EB8, lbl_80464EC4};
static const s16* lbl_8049E340[4] = {lbl_80464ED0, lbl_80464EDC, lbl_80464ED0, lbl_80464EDC};


void EscortedWomanArrivalPoint_8049E350::init() {
    ActorBase_804A5770::init();
    field_0x238 = mSpawnParam_0x90 & 1;
    field_0x240 = (mSpawnParam_0x90 >> 22) & 0x1F;
    mBox.set(-8.0f, -8.0f, 8.0f, 8.0f);
    mBox2.set(mBox);
    mMapBox_0x80.set(mBox);
    mDir_0x98 = 2;
    mAnim244.setKeyTables_801D2608(lbl_8049E330, 4);
    mAnim244.startAnim_801D22DC(2, 2, false);
    mAnim244.setFrame_801D1F14(0.0f);
    mAnim27c.setKeyTables_801D2608(lbl_8049E340, 4);
    mAnim27c.startAnim_801D22DC(0, 0, false);
    mAnim27c.setFrame_801D1F14(0.0f);
    mDrawEnabled = false;
    if (getScreenFlagBit_0x3C() != 0 && !isScreenFlagSet_0x34()) {
        return;
    }
    field_0x234 = 1;
    switch (field_0x234) {
    case 0:
        field_0x230 = 0;
        break;
    case 1:
        setScreenFlag_0x30(0xFF, true);
        field_0x230 = 1;
        mDrawEnabled = true;
        field_0x23c = -1;
        break;
    case 2: {
        s32 bit = field_0x240;
        if (bit > 0) {
            getGameView_802D6A9C()->setSwitchBit_802D8050(bit, true, 0xFF);
        }
        break;
    }
    }
}

#include "game/Message.h"
#include "game/Player.h"

inline void EscortedWomanArrivalPoint_8049E350::enterState() {
    switch (field_0x234) {
    case 0:
        field_0x230 = 0;
        break;
    case 1:
        setScreenFlag_0x30(0xFF, true);
        field_0x230 = 1;
        mDrawEnabled = true;
        field_0x23c = -1;
        break;
    case 2: {
        s32 bit = field_0x240;
        if (bit > 0) {
            getGameView_802D6A9C()->setSwitchBit_802D8050(bit, true, 0xFF);
        }
        break;
    }
    }
}

// 0x80125EFC.
// FIXME: 93.08 %, missing/extra; behaviour clean.
void EscortedWomanArrivalPoint_8049E350::update() {
    ActorBase_804A5770::update();
    JGeometry::TBox2f box(mBox);
    box.addPos(mPos.x, mPos.y);
    switch (field_0x234) {
    case 0:
        if (getScreenFlagBit_0x3C() > 0 && isScreenFlagSet_0x34()) {
            if (field_0x238 == 0) {
                mDir_0x98 = 0;
            }
            field_0x234 = 1;
            enterState();
        }
        break;
    case 1: {
        u16 msg;
        switch (field_0x238) {
        case 1:
            msg = 0x4C0;
            break;
        case 0:
        default:
            msg = 0x4BF;
            break;
        }
        JGeometry::TBox2f talkBox(mBox);
        talkBox.addPos(mPos.x, mPos.y);
        fn_8022FEB8(&talkBox, mLayer_0x4, mSerial, true, false);
        s32 player = fn_8022FD8C(mSerial);
        if (player < 0) {
            player = -1;
        } else if (!fn_802300EC(player)) {
            player = -1;
        } else if (talkTo(player, msg)) {
        } else {
            player = -1;
        }
        if (player >= 0) {
            field_0x23c = player;
            field_0x234 = 2;
            enterState();
        }
        break;
    }
    case 2:
        fn_8022FF8C(field_0x23c, &box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x23c, mSerial)) {
            field_0x234 = 1;
            enterState();
        }
        break;
    }
    if (field_0x234 != 0) {
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
    }
    if (field_0x230 != 0) {
        fn_80239914(&box, mLayer_0x4, mSerial);
    }
    s32 anim = mDir_0x98;
    if (anim >= 0) {
        mDir_0x98 = anim;
        if (anim != mAnim244.mAnim) {
            mAnim244.startAnim_801D22DC(anim, 2, false);
            mAnim244.setFrame_801D1F14(0.0f);
        }
    }
    if (field_0x231) {
        mAnim244.mFrameCtrl.setRate(3.0f);
    } else {
        mAnim244.mFrameCtrl.setRate(1.0f);
    }
    mAnim244.update_801D1C18();
    mAnim27c.update_801D1C18();
    gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
}

void EscortedWomanArrivalPoint_8049E350::draw() {
    ActorBase_804A5770::draw();
    SpriteParams_801F5AE0 params;
    s32 baseFrame;
    switch (field_0x238) {
    case 1:
        baseFrame = 0x2D2;
        break;
    case 0:
    default:
        baseFrame = 0x2E6;
        break;
    }
    drawSpriteScaled_801F02BC("enemy_gc_sob.bin", baseFrame + mAnim244.mCel,
                &params, 0x407, 0, false, 1.0f, 1.0f);
    if (field_0x231) {
        JGeometry::TVec3f pos(mPos);
        pos.y += -20.0f;
        if (field_0x238 == 1) {
            pos.x += 10.0f;
        } else {
            pos.x += -10.0f;
        }
        s32 frame = mAnim27c.mCel + 0x2A3;
        if (mLayer_0x4 < 8) {
            void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
            gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, frame, NULL, 1.0f, 1.0f);
        }
    }
}

void EscortedWomanArrivalPoint_8049E350::fn_80125C58(bool flag) {
    field_0x231 = flag;
    if (field_0x231) {
        startSignAnim();
    }
}

void EscortedWomanArrivalPoint_8049E350::fn_80125BF4() {
    JGeometry::TVec3f pos(mPos);
    pos.y += -24.0f;
    spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x33A, 0, 0, NULL, NULL, true, 1.0f);
}

// From here down: plain functions that update and fn_80125C58 inline and whose own copies the
// linker dropped. The original's .sdata2 has their literals (0.0f, then 64.0f) ahead of
// fn_80125BF4's, in the order of these functions from the bottom of the file up, so they were
// generated before it (tools/pool.py).
// Opens message `msg` for `player`, 64 units above the actor.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 5-line definition): split out only where the .sdata2 order needs it.
bool EscortedWomanArrivalPoint_8049E350::talkTo(s32 player, u16 msg) {
    JGeometry::TVec3f pos(mPos);
    pos.y -= 64.0f;
    return openMessage_8038AD24(player, msg, mSerial, mLayer_0x4, pos, 0x28);
}

// Starts mAnim27c's animation for the actor's kind (field_0x238) at frame 0.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 12-line definition): split out only where the .sdata2 order needs it.
void EscortedWomanArrivalPoint_8049E350::startSignAnim() {
    switch (field_0x238) {
    case 1:
        mAnim27c.startAnim_801D22DC(0, 0, false);
        break;
    case 0:
    default:
        mAnim27c.startAnim_801D22DC(1, 0, false);
        break;
    }
    mAnim27c.setFrame_801D1F14(0.0f);
}

EscortedWomanArrivalPoint_8049E350::~EscortedWomanArrivalPoint_8049E350() {}
