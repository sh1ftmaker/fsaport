#include "game/GameAudio.h"
#include "game/ActorManager.h"
#include "game/BgRewrite.h"
#include "game/EmitterDelete.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/act/act_leaf_effect_npc_generator.h"
#include "game/act/act_shadow_link_thwomp_minor_802233A4.h"

ShadowLinkThwompMinor_804A6D00::ShadowLinkThwompMinor_804A6D00() {
    field_0x2b8 = NULL;
    field_0x2bc = NULL;
    field_0x2c0 = NULL;
}

ShadowLinkThwompMinor_804A6D00::~ShadowLinkThwompMinor_804A6D00() {
    deleteEmitter(&field_0x2b8);
    fn_801CDC8C(0x4000E);
    releaseEmitter_801EE434(field_0x2bc);
    releaseEmitter_801EE434(field_0x2c0);
    field_0x2bc = NULL;
    field_0x2c0 = NULL;
}

void ShadowLinkThwompMinor_804A6D00::init() {
    ShadowLinkBase_804A6A00::init();
    mDir_0x98 = mSpawnParam_0x90 & 3;
    mFlags &= ~0x100;
    mState = 4;
    mStateTimer = 0;
    field_0x250 = -1;
    mSpeed = 0.0f;
    mTargetSpeed = 0.0f;
    mAnim.startAnim_801D22DC(2, 0, false);
}

// Not auto-inlined: updateState_0x90 calls it with a plain bl in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// FIXME(shotgun): 94.95 %, missing/extra; behaviour near.
void ShadowLinkThwompMinor_804A6D00::fn_80223AD8(s32 state) {
    switch (state) {
    case 5:
        mTargetSpeed = 2.5f;
        mAnim.startAnimIfChanged_801D22A0(mDir_0x98 + 0x14, 2);
        field_0x2bc = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x116, 0, 0, NULL, NULL, true, 1.0f);
        field_0x2c0 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x115, 0, 0, NULL, NULL, true, 1.0f);
        break;
    case 6: {
        mTargetSpeed = 0.0f;
        mVel_0x3c.x = -mVel_0x3c.x;
        mVel_0x3c.y = -mVel_0x3c.y;
        mVel_0x3c.z = 0.0f;
        mVel_0x3c.normalize();
        mVel_0x3c.scale(1.2f);
        mVel_0x3c.z = 4.0f;
        mAnim.startAnim_801D22DC(mDir_0x98 + 0xC, 0, false);
        if (!isUpperLayer_801CD2F8(mLayer_0x4)) {
            s32 serial = getActorManager_801F856C()->fn_801F7884('LFGN', mLayer_0x4);
            if (serial >= 0) {
                ActorBase_804A5770* gen = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (gen != NULL) {
                    ((LeafEffectNPCGenerator_804AE288*)gen)->fn_8036D858(mPos);
                }
            }
        }
        startSound_801F0E34(0x400EB, 0);
        JGeometry::TVec3f off(0.0f, 0.0f, 0.0f);
        if (mDir_0x98 < 2 && mDir_0x98 >= 0) {
            off.set(0.0f, -8.0f, 0.0f);
            getGameView_802D6A9C()->fn_802D79E8(mLayer_0x4, 0.0f, 4.0f, false);
        } else {
            getGameView_802D6A9C()->fn_802D79E8(mLayer_0x4, 4.0f, 0.0f, false);
        }
        gGameRoot_8053AAF8->startRumble_80134DE4(1, 0);
        JGeometry::TVec3f n(-mVel_0x3c.y, -mVel_0x3c.x, 0.0f);
        n.normalize();
        JGeometry::TVec3f at = mPos + off;
        if (mLayer_0x4 < 8) {
            gGameRoot2_8053AB10->field_0x20->startHeightGridEffect_802F601C(&at, 3, &n, 1.0f, 1.0f);
        }
        break;
    }
    case 7:
    case 8:
        mSpeed = 0.0f;
        mTargetSpeed = 0.0f;
        mVel_0x3c.x = 0.0f;
        mVel_0x3c.y = 0.0f;
        mAnim.startAnimIfChanged_801D22A0(mDir_0x98, 0);
        if (state == 7) {
            field_0x2b8 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x565, 0, 1, NULL, NULL, true, 0.5f);
            startSound_801F0E34(0x4000E, 0);
        }
        break;
    }
    mStateTimer = 0;
    mState = state;
}
#pragma auto_inline reset

// Shifts the rectangle by (dx, dy). The original adds the zero component too.
static inline void moveBox(f32& x0, f32& y0, f32& x1, f32& y1, f32 dx, f32 dy) {
    x0 += dx;
    y0 += dy;
    x1 += dx;
    y1 += dy;
}

// The rectangle (in front of the actor while it charges in state 5) as (x0, y0, x1, y1);
// empty (1, 1, -1, -1) in the other states.
static inline void attackBox(ShadowLinkThwompMinor_804A6D00* a, f32& x0, f32& y0, f32& x1, f32& y1) {
    x0 = 1.0f;
    x1 = -1.0f;
    y0 = x0;
    y1 = x1;
    if (a->mState == 5) {
        x0 = -8.0f;
        x1 = 8.0f;
        y0 = x0;
        y1 = x1;
        switch (a->mDir_0x98) {
        case 0:
            moveBox(x0, y0, x1, y1, 16.0f, 0.0f);
            break;
        case 1:
            moveBox(x0, y0, x1, y1, -16.0f, 0.0f);
            break;
        case 2:
            moveBox(x0, y0, x1, y1, 0.0f, 16.0f);
            break;
        case 3:
            moveBox(x0, y0, x1, y1, 0.0f, -16.0f);
            break;
        }
    }
}

// Not auto-inlined: updateState_0x90 calls it with a plain bl in the original.
#pragma auto_inline off
// FIXME(shotgun): 82.92 %, scheduling+regalloc; behaviour clean.
void ShadowLinkThwompMinor_804A6D00::fn_802238CC() {
    f32 x0, y0, x1, y1;
    attackBox(this, x0, y0, x1, y1);
    if (x1 - x0 > 0.0f && y1 - y0 > 0.0f) {
        x0 += mPos.x;
        y0 += mPos.y;
        x1 += mPos.x;
        y1 += mPos.y;
        JGeometry::TVec3f p(x0, y0, 0.0f);
        gBgRewrite_8053AC98->fn_802A20F0(&p, -1, mLayer_0x4, 0, 0);
        JGeometry::TVec3f q(x1, y0, 0.0f);
        gBgRewrite_8053AC98->fn_802A20F0(&q, -1, mLayer_0x4, 0, 0);
        JGeometry::TVec3f r(x1, y1, 0.0f);
        gBgRewrite_8053AC98->fn_802A20F0(&r, -1, mLayer_0x4, 0, 0);
        JGeometry::TVec3f t(x0, y1, 0.0f);
        gBgRewrite_8053AC98->fn_802A20F0(&t, -1, mLayer_0x4, 0, 0);
    }
}
#pragma auto_inline reset

void ShadowLinkThwompMinor_804A6D00::updateState_0x90(u8 mapHit) {
    switch (mState) {
    case 0:
        fn_80221408();
        break;
    case 1:
        fn_80221334();
        break;
    case 2:
        fn_802212AC();
        break;
    case 3:
        if (mStateTimer >= 0x78) {
            fn_80223AD8(5);
        }
        break;
    case 4:
        fn_802216E8(0x78);
        if (mPos.z <= 0.0f) {
            if (mFlags & CLS_804A6A00_FLAG_0x2000) {
                fn_80223AD8(5);
            } else {
                mVel_0x3c.z = lbl_8053FE70;
            }
        }
        break;
    case 5: {
        u32 sound = 0x400E7;
        if (getTileCollisionCode_8022461C(&mPos, mLayer_0x4) == 0x39) {
            sound = 0x400E8;
        } else if (fn_80224B88(&mPos, mLayer_0x4)) {
            sound = 0x400E9;
        } else if (fn_80224E88(&mPos, mLayer_0x4)) {
            sound = 0x400EA;
        }
        startSound_801F0E34(sound, 0);
        fn_802238CC();
        if (mStateTimer > 1 && mapHit) {
            releaseEmitter_801EE434(field_0x2bc);
            releaseEmitter_801EE434(field_0x2c0);
            field_0x2bc = NULL;
            field_0x2c0 = NULL;
            fn_80223AD8(6);
        }
        break;
    }
    case 6:
        if (mStateTimer > 1 && mPos.z <= 0.0f) {
            fn_80223AD8(7);
        }
        break;
    case 7:
        if (mStateTimer >= 0x7B) {
            deleteEmitter(&field_0x2b8);
            fn_801CDC8C(0x4000E);
            fn_80223AD8(8);
        } else if (mStateTimer % 6 == 0) {
            mDir_0x98 = fn_801CD30C(mDir_0x98);
            mAnim.startAnim_801D22DC(mDir_0x98, 0, false);
        }
        break;
    case 8:
        if (mStateTimer >= 0x1E) {
            fn_8022149C(1);
        }
        break;
    }
}

void ShadowLinkThwompMinor_804A6D00::updateBehavior_0x9C(u8 mapHit) {
    ShadowLinkBase_804A6A00::updateBehavior_0x9C(mapHit);
    setEmitterPos_8013CB44(field_0x2bc, &mPos, 0, 0, true);
    setEmitterPos_8013CB44(field_0x2c0, &mPos, 0, 0, true);
}

void ShadowLinkThwompMinor_804A6D00::calcVelocity_0x88() {
    switch (mState) {
    case 2:
        fn_80221250();
        break;
    case 6:
        mVel_0x3c.z += lbl_8053EFB8;
        break;
    default:
        EnemyBase_804A4808::calcVelocity_0x88();
        break;
    }
}

void ShadowLinkThwompMinor_804A6D00::onHitByPlayer_0x6C() {
    ShadowLinkBase_804A6A00::onHitByPlayer_0x6C();
    deleteEmitter(&field_0x2b8);
    fn_801CDC8C(0x4000E);
    releaseEmitter_801EE434(field_0x2bc);
    releaseEmitter_801EE434(field_0x2c0);
    field_0x2bc = NULL;
    field_0x2c0 = NULL;
}

// FIXME(shotgun): 97.95 %, scheduling+regalloc; behaviour clean.
// (the li r3, 0 of the return sits before the state test)
u8 ShadowLinkThwompMinor_804A6D00::attackPlayers_0x68() {
    f32 x0, y0, x1, y1;
    attackBox(this, x0, y0, x1, y1);
    if (x1 - x0 > 0.0f && y1 - y0 > 0.0f) {
        f32 s0 = mBox.i.x;
        f32 s1 = mBox.i.y;
        f32 s2 = mBox.f.x;
        f32 s3 = mBox.f.y;
        mBox.i.x = x0;
        mBox.i.y = y0;
        mBox.f.x = x1;
        mBox.f.y = y1;
        damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 1);
        mBox.i.x = s0;
        mBox.i.y = s1;
        mBox.f.x = s2;
        mBox.f.y = s3;
    }
    return 0;
}
