#include "game/GameAudio.h"
#include "game/GameView.h"
#include "game/act_leever_80182914.h"
#include "game/act_light_environment_control_802EF76C.h"
#include "game/GameGlobals.h"
#include "game/Camera.h"
#include "game/ActorManager.h"
#include "game/Player.h"
#include "game/cls_804AE9B0.h"
#include "game/act/act_vaati_time_up.h"

// FIXME: 99.51 %, operands; behaviour clean.
// mPos.y's sum takes f4/f5 where the original uses f5/f6, the
// u32-to-float conversions differ in one li, and the epilogue reloads r0 and r31 in
// the other order.
void VaatiTimeUp_804ACCC0::init() {
    cls_801EBA14 rec;

    mViewBox = getGameView_802D6A9C()->mVisibleBox;
    mPos.x = 64.0f + mViewBox.f.x;
    mPos.y = (mViewBox.i.y + mViewBox.f.y) * 0.5f + 96.0f;
    mPos.z = 100.0f;
    rec.field_0x0 = 1;
    rec.field_0x4 = 1;
    rec.field_0x8 = 100.0f;
    rec.field_0xc = 90.0f;
    rec.field_0x10 = 20.0f;
    field_0x230.field_0xc = rec;
    field_0x230.fn_801EB710(0, &mPos);

    field_0x2c8 = 0;
    field_0x2bc = 0;
    field_0x2c0 = 0;
    field_0x2f4 = 0.0f;
    field_0x290 = mViewBox.f.x + (mViewBox.i.x - mViewBox.f.x) * 0.5f;
    field_0x294 = mViewBox.f.y + (mViewBox.i.y - mViewBox.f.y) * 0.5f;
    field_0x294 += 96.0f;
    field_0x2a4 = 0.0f;
    field_0x2a0 = 0.0f;
    field_0x298 = mViewBox.f.x - mViewBox.i.x - 96.0f;
    field_0x29c = mViewBox.f.y - mViewBox.i.y - 96.0f;
    field_0x2c4 = 0;
    field_0x2ce = 0;
    field_0x2cc = 0;
    field_0x2d0 = 0;
    field_0x2f8 = 1;

    field_0x2d4 = getShadowColor_8018330C().toUInt32();
    field_0x2d8 = (f32)(field_0x2d4 >> 24);
    field_0x2dc = (f32)((field_0x2d4 >> 16) & 0xFF);
    field_0x2e0 = (f32)((field_0x2d4 >> 8) & 0xFF);
    field_0x2e4 = (f32)(field_0x2d4 & 0xFF);
    field_0x2e8 = -(0.4f * field_0x2d8) / 60.0f;
    field_0x2ec = -(0.4f * field_0x2dc) / 60.0f;
    field_0x2f0 = -(0.4f * field_0x2e0) / 60.0f;
    mVel_0x3c.x = -2.0f;
    field_0x2b8 = 1;
}

// FIXME: 96.07 %, missing/extra; behaviour drift.
// FIXME(shotgun): the original's copy of pickTarget keeps the compare of the squared distance
// with 0.0f (fcmpo, cror, no branch) although its result is unused; ours drops it.
void VaatiTimeUp_804ACCC0::update() {
    if (field_0x2b8 != 0) {
        cls_804A03A8* sub = gGameRoot2_8053AB10->field_0x30;
        if (sub->field_0x284 == 0) {
            sub->field_0x284 = 1;
            sub->field_0x28c = 0.05f;
        }
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mFocusOverride_0x7f4 == 0) {
            camera->mShowWholeArea_0x7f5 = 1;
        }
    }

    switch (field_0x2b8) {
    case 0:
        gGameRoot_8053AAF8->fn_80134FC4(1);
        lbl_8053AD70->fn_8037AE00();
        gGameRoot_8053AAF8->fn_80134FBC(1);
        field_0x2c0 = 60;
        field_0x2b8 = 1;
        break;
    case 1:
        mPos += mVel_0x3c;
        if (field_0x2c0 <= 0) {
            startSound_801F0E34(0x4003F, 0);
            field_0x230.field_0x58 = 255.0f;
            mVel_0x3c.z = 0.0f;
            mVel_0x3c.y = 0.0f;
            mVel_0x3c.x = 0.0f;
            field_0x2c0 = 0;
            field_0x2b8 = 2;
            field_0x2d8 = 0.6f * (f32)(field_0x2d4 >> 24);
            field_0x2dc = 0.6f * (f32)((field_0x2d4 >> 16) & 0xFF);
            field_0x2e0 = 0.6f * (f32)((field_0x2d4 >> 8) & 0xFF);
        } else {
            field_0x2d8 += field_0x2e8;
            field_0x2dc += field_0x2ec;
            field_0x2e0 += field_0x2f0;
        }
        field_0x2c0--;
        break;
    case 2:
        if (field_0x2c0 <= 0) {
            field_0x2c0 = 300;
            field_0x2b8 = 3;
        } else {
            field_0x2c0--;
        }
        field_0x2bc--;
        if (field_0x2bc < 0) {
            field_0x2bc = 10;
            field_0x2c8++;
            field_0x2c8 &= 3;
        }
        break;
    case 3:
        steer();
        field_0x2c4--;
        mPos += mVel_0x3c;
        field_0x230.fn_801EB418(&mPos);
        if (field_0x2c0 <= 0) {
            if (mViewBox.i.x - 192.0f > mPos.x) {
                field_0x2b8 = 4;
            }
        } else if (field_0x2c0 <= 40 && field_0x2f8 == 1) {
            startSound_801F0E34(0x40041, 0);
            field_0x2f8 = 0;
        }
        field_0x2c0--;
        startSound_801F0E34(0x40051, 0);
        field_0x2bc--;
        if (field_0x2bc < 0) {
            field_0x2bc = 10;
            field_0x2c8++;
            field_0x2c8 &= 3;
        }
        break;
    case 4:
        gGameRoot_8053AAF8->fn_80134FC4(0);
        lbl_8053AD70->fn_8037AE30();
        for (s32 i = 0; i < 4; i++) {
            hitPlayerGroup_80238548(i, 40, (const JGeometry::TVec2f*)&mPos, 18);
        }
        field_0x230.field_0x5c = 1;
        field_0x230.field_0x58 = 0.0f;
        gGameRoot_8053AAF8->fn_80134FBC(0);
        markDead_801F3668();
        break;
    }

    JUtility::TColor color = ((s32)field_0x2d8 << 24) | ((s32)field_0x2dc << 16) | ((s32)field_0x2e0 << 8) | (s32)field_0x2e4;
    fn_8013A884(&color);
}

// Frame offsets of the "enemy_gc_sob.bin" sprite, indexed by field_0x2c8.
static const s32 lbl_80472288[4] = {0, 1, 0, 2};

void VaatiTimeUp_804ACCC0::draw() {
    SpriteParams_801F5AE0 params;
    params.field_0x2d = 1;
    params.field_0x1c = 1;
    params.field_0x1d = 1;
    params.field_0x30 = 8;
    drawSprite_801F06F0("enemy_gc_sob.bin", 0x466 + lbl_80472288[field_0x2c8], &params, 7, 1.0f, 1.0f);
}

// The two functions below are plain functions that update inlines (state 3) and whose own copies
// the linker dropped: the original's .sdata2 starts with 192.0f, 96.0f, 0.5f, 0.0f (pickTarget)
// and 10430.378f, 6.0f, 0.1f, 0.2f (steer), ahead of draw's 1.0f, so they were generated before
// draw (tools/pool.py).

// Every 6 frames (or once past the target with the timer up) takes the angle to the target, then
// turns towards it and eases the velocity to speed 6 in that direction.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 12-line definition): split out only where the .sdata2 order needs it.
void VaatiTimeUp_804ACCC0::steer() {
    if (field_0x2c4 <= 0 || (field_0x2c0 <= 0 && mPos.x >= field_0x290 && mPos.y >= field_0x294)) {
        pickTarget();
        field_0x2d0 = (u16)(10430.378f * JMath::atanTable_.atan2_(field_0x290 - mPos.x, field_0x294 - mPos.y));
        field_0x2c4 = 6;
    }
    addCalcAngle2_801F714C(&field_0x2cc, field_0x2d0, 20, 0x1000);
    field_0x2a0 = 6.0f * JMath::sincosTable_.sinShort(field_0x2cc);
    field_0x2a4 = 6.0f * JMath::sincosTable_.cosShort(field_0x2cc);
    addCalc2_801F71A4(&mVel_0x3c.x, field_0x2a0, 0.1f, 0.2f);
    addCalc2_801F71A4(&mVel_0x3c.y, field_0x2a4, 0.1f, 0.2f);
}

// With the timer up the target becomes the point 192 left of the view box; else tells whether the
// target is reached (steer does not use the result; the 0.0f is this compare's).
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 10-line definition): split out only where the .sdata2 order needs it.
bool VaatiTimeUp_804ACCC0::pickTarget() {
    if (field_0x2c0 <= 0) {
        field_0x290 = mViewBox.i.x - 192.0f;
        field_0x294 = (mViewBox.i.y + mViewBox.f.y) * 0.5f + 96.0f;
        return false;
    }
    f32 dx = field_0x290 - mPos.x;
    f32 dy = field_0x294 - mPos.y;
    return dx * dx + dy * dy <= 0.0f;
}

VaatiTimeUp_804ACCC0::~VaatiTimeUp_804ACCC0() {}
