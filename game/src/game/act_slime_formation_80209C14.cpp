#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/GameMath.h"
#include "JSystem/JMath/JMath.h"
#include "game/act/act_slime_manager.h"
#include "game/act/tu_80207548.h"
#include "game/act/act_slime_formation_80209C14.h"

SlimeFormation_804A5D60::SlimeFormation_804A5D60() : mTargetSerial(-1), field_0x2ac(0), field_0x2b0(1), field_0x2b1(0) {
    mVel.x = 0.0f;
    mVel.y = 0.0f;
    mVel.z = 0.0f;
    field_0x2c0.set(0.0f, 0.0f);
    field_0x2c8.set(0.0f, 0.0f);
}

// The animation tables: pairs of frame and duration, ended by -1.
static const s16 lbl_8046A2E8[] = { 0, 6, 1, 6, 2, 20, 3, 8, -1, -1 };
static const s16 lbl_8053F948[] = { 4, 6, -1, -1 };
static const s16 lbl_8046A2FC[] = { 5, 6, 6, 6, -1, -1 };
static const s16 lbl_8053F950[] = { 4, 6, -1, -1 };
static const s16 lbl_8046A308[] = { 7, 4, 8, 6, 7, 4, -1, -1 };
static const s16 lbl_8046A318[] = { 4, 6, 7, 4, 8, 6, 7, 4, -1, -1 };
static const s16 lbl_8046A32C[] = { 5, 11, 6, 11, -1, -1 };
static const s16 lbl_8053F958[] = { 4, 50, -1, -1 };
static const s16 lbl_8046A338[] = { 3, 2, 2, 20, 3, 8, -1, -1 };
static const s16 lbl_8046A348[] = { 3, 8, 2, 20, -1, -1 };

static const s16* lbl_804A5D38[10] = {
    lbl_8046A2E8, lbl_8053F948, lbl_8046A2FC, lbl_8053F950,
    lbl_8046A308, lbl_8046A318, lbl_8046A32C, lbl_8053F958,
    lbl_8046A338, lbl_8046A348,
};

// FIXME(shotgun): 98.51 %, scheduling+regalloc; behaviour clean.
void SlimeFormation_804A5D60::init() {
    EnemyBase_804A4808::init();
    mBox.i.set(1.0f, 1.0f);
    mBox.f.set(-1.0f, -1.0f);
    mBox2.set(mBox);
    mMapBox_0x80.i.set(1.0f, 1.0f);
    mMapBox_0x80.f.set(-1.0f, -1.0f);
    field_0xb0 = 0x50303;
    mDir_0x98 = 2;
    mFlags |= ENEMY_FLAG_NO_HURT;
    field_0x194 = 1;
    mTargetSerial = mSpawnParam_0x90;
    SlimeManager_804A5BD8* manager = static_cast<SlimeManager_804A5BD8*>(getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial));
    mAnim.setKeyTables_801D2608(lbl_804A5D38, 10);
    switch (manager->field_0x340) {
    case 0:
        mState = 6;
        mAnim.startAnim_801D22DC(5, 2, false);
        mBox.i.set(-8.0f, -8.0f);
        mBox.f.set(8.0f, 8.0f);
        mBox2.set(mBox);
        break;
    case 1:
    case 2:
        mState = 0;
        mAnim.startAnim_801D22DC(0, 0, false);
        startSound_801F0E34(0x50037, 0);
        break;
    case 3: {
        mState = 3;
        mAnim.startAnim_801D22DC(7, 0, false);
        startSound_801F0E34(0x50037, 0);
        s32 player = findNearestPlayer_801D05F8(&mPos, 0.0f);
        s16 angle;
        if (player >= 0) {
            angle = getAngleToPlayer_801D0280(player) - 0x8000;
        } else {
            angle = randInt_801CD664(0x10000);
        }
        mVel.set(JMASSin(angle) * 1.5f, JMASCos(angle) * 1.5f, 0.5f * (50.0f * (0.3f * -lbl_8053EFB8)));
        mVel_0x3c.set(mVel);
        break;
    }
    }
    mStateTimer = 0;
    field_0x250 = -1;
    mSpeed = 0.0f;
    mTargetSpeed = 0.0f;
}

void SlimeFormation_804A5D60::onFirstUpdate_0x54() {
    JGeometry::TVec2f offset(mSpawnPos.x - mPos.x, mSpawnPos.y - mPos.y);
    field_0x2c0 = offset;
}

// FIXME(shotgun): 99.83 %, regalloc-only; behaviour drift.
// 0x8020AA78: enters `state`, starting its animation and
// velocity. While bit 0x2000 is set only 8, 11 and 12 are taken.
void SlimeFormation_804A5D60::setState_8020AA78(s32 state) {
    if (mFlags & 0x2000) {
        switch (state) {
        case 8:
        case 11:
        case 12:
            break;
        default:
            return;
        }
    }
    switch (state) {
    case 1:
        mAnim.startAnim_801D22DC(1, 0, false);
        mVel.x = 0.0f;
        mVel.y = 0.0f;
        mVel.z = 0.0f;
        break;
    case 2:
        mAnim.startAnim_801D22DC(4, 0, false);
        break;
    case 4:
        mAnim.startAnim_801D22DC(8, 0, false);
        break;
    case 5:
        mAnim.startAnim_801D22DC(1, 0, false);
        mVel.set(0.083333336f * field_0x2c0.x, 0.083333336f * field_0x2c0.y,
                 0.5f * (12.0f * -lbl_8053EFB8));
        break;
    case 6:
        mAnim.startAnimIfChanged_801D22A0(5, 2);
        break;
    case 7:
        mFlags &= ~0x4000;
        mDir_0x98 = field_0x2ac;
        mAnim.startAnim_801D22DC(1, 0, false);
        mVel.x = 0.0f;
        mVel.y = 0.0f;
        switch (mDir_0x98) {
        case 0:
            mVel.x = 1.4f;
            mVel.y = 0.0f;
            break;
        case 1:
            mVel.x = -1.4f;
            mVel.y = 0.0f;
            break;
        case 2:
            mVel.x = 0.0f;
            mVel.y = 1.4f;
            break;
        case 3:
            mVel.x = 0.0f;
            mVel.y = -1.4f;
            break;
        }
        mVel.z = 0.5f * (12.0f * -lbl_8053EFB8);
        field_0x2b1 = field_0x2b0;
        if (field_0x2b1 < 1) {
            field_0x2b1 = 1;
        }
        break;
    case 8: {
        mFlags &= ~0x2000;
        mAnim.startAnim_801D22DC(1, 0, false);
        s32 player = findNearestPlayer_801D05F8(&mPos, 0.0f);
        s16 angle;
        if (player >= 0) {
            angle = getAngleToPlayer_801D0280(player) - 0x8000;
        } else {
            angle = randInt_801CD664(0x10000);
        }
        mVel.set(JMASSin(angle) * 4.0f, JMASCos(angle) * 4.0f, 0.5f * (12.0f * -lbl_8053EFB8));
        field_0x2b1 = 3;
        break;
    }
    case 9:
        mFlags &= ~0x8000;
        mAnim.startAnim_801D22DC(1, 0, false);
        mVel.set(0.045454547f * (field_0x2c8.x - mPos.x), 0.045454547f * (field_0x2c8.y - mPos.y),
                 0.5f * (22.0f * -lbl_8053EFB8));
        break;
    case 10:
        mAnim.startAnim_801D22DC(4, 0, false);
        break;
    case 11:
        mAnim.startAnim_801D22DC(9, 0, false);
        startSound_801F0E34(0x50038, 0);
        mBox.i.set(1.0f, 1.0f);
        mBox.f.set(-1.0f, -1.0f);
        mBox2.set(mBox);
        break;
    case 12:
        break;
    }
    mStateTimer = 0;
    mState = state;
}

// The animation steps of the box-shrinking state machine; updateState_0x90 is its per-frame step.
// FIXME(shotgun): 99.42 %, missing/extra; behaviour drift.
void SlimeFormation_804A5D60::updateState_0x90(u8 mapHit) {
    if (mFlags & 0x2000) {
        switch (mState) {
        case 8:
        case 11:
        case 12:
            break;
        default:
            switch (mAnim.mAnim) {
            case 2:
            case 6:
                break;
            default:
                setState_8020AA78(11);
                break;
            }
            break;
        }
    }
    switch (mState) {
    case 0:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            mBox.i.set(-8.0f, -8.0f);
            mBox.f.set(8.0f, 8.0f);
            mBox2.set(mBox);
            setState_8020AA78(1);
        }
        break;
    case 1:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            switch (mAnim.mAnim) {
            case 1:
                mAnim.startAnim_801D22DC(2, 0, false);
                break;
            case 2:
                mAnim.startAnim_801D22DC(3, 0, false);
                break;
            case 3:
                setState_8020AA78(2);
                break;
            }
        }
        break;
    case 2:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            ActorBase_804A5770* manager = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
            if (manager != NULL) {
                static_cast<SlimeManager_804A5BD8*>(manager)->fn_80207ECC();
            }
            setState_8020AA78(7);
        }
        break;
    case 3:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            setState_8020AA78(4);
        }
        break;
    case 4:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            setState_8020AA78(5);
        }
        break;
    case 5:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            switch (mAnim.mAnim) {
            case 1:
                mAnim.startAnim_801D22DC(2, 0, false);
                break;
            case 2:
                mAnim.startAnim_801D22DC(3, 0, false);
                break;
            case 3: {
                mBox.i.set(-8.0f, -8.0f);
                mBox.f.set(8.0f, 8.0f);
                mBox2.set(mBox);
                ActorBase_804A5770* manager = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
                if (manager != NULL) {
                    static_cast<SlimeManager_804A5BD8*>(manager)->fn_80207ECC();
                }
                setState_8020AA78(7);
                break;
            }
            }
        }
        break;
    case 6:
        if (mAnim.mAnim == 5 && mAnim.isLoopEnd_801D217C()) {
            if (mFlags & 0x4000) {
                setState_8020AA78(7);
            } else if (mFlags & 0x8000) {
                setState_8020AA78(9);
            }
        }
        break;
    case 7:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            switch (mAnim.mAnim) {
            case 1:
                mAnim.startAnim_801D22DC(2, 0, false);
                startSound_801F0E34(0x50036, 0);
                break;
            case 2:
                mAnim.startAnim_801D22DC(3, 0, false);
                break;
            case 3:
                mAnim.startAnim_801D22DC(4, 0, false);
                break;
            case 4:
                field_0x2b1--;
                if (field_0x2b1 == 0) {
                    ActorBase_804A5770* manager = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
                    if (manager != NULL) {
                        if (static_cast<SlimeManager_804A5BD8*>(manager)->fn_80207E58()) {
                            setState_8020AA78(11);
                        } else {
                            setState_8020AA78(6);
                        }
                    } else {
                        setState_8020AA78(11);
                    }
                } else {
                    mAnim.startAnim_801D22DC(1, 0, false);
                }
                break;
            }
        }
        break;
    case 8:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            switch (mAnim.mAnim) {
            case 1:
                mAnim.startAnim_801D22DC(2, 0, false);
                startSound_801F0E34(0x50036, 0);
                break;
            case 2:
                mAnim.startAnim_801D22DC(3, 0, false);
                break;
            case 3:
                mAnim.startAnim_801D22DC(4, 0, false);
                break;
            case 4:
                field_0x2b1--;
                if (field_0x2b1 == 0) {
                    setState_8020AA78(11);
                } else {
                    mAnim.startAnim_801D22DC(1, 0, false);
                }
                break;
            }
        }
        break;
    case 9:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            switch (mAnim.mAnim) {
            case 1:
                mAnim.startAnim_801D22DC(6, 0, false);
                startSound_801F0E34(0x50039, 0);
                break;
            case 6:
                mAnim.startAnim_801D22DC(3, 0, false);
                break;
            case 3:
                setState_8020AA78(10);
                break;
            }
        }
        break;
    case 10:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            ActorBase_804A5770* manager = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
            if (manager != NULL) {
                static_cast<SlimeManager_804A5BD8*>(manager)->fn_80207E24();
            }
            setState_8020AA78(6);
        }
        break;
    case 11:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            setState_8020AA78(12);
        }
        break;
    case 12:
        if (mStateTimer >= 12) {
            ActorBase_804A5770* manager = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
            if (manager != NULL) {
                manager->mAlive = false;
            }
            mAlive = false;
        } else {
            f32 left = (f32)(12 - mStateTimer);
            if (left < FLT_EPSILON) {
                field_0x54.x = 0.0f;
            } else {
                field_0x54.x = (1.0f / left) * (0.0f - field_0x54.x) + field_0x54.x;
            }
            field_0x54.y = field_0x54.x;
            field_0x54.z = field_0x54.x;
        }
        break;
    }
}

void SlimeFormation_804A5D60::calcVelocity_0x88() {
    switch (mState) {
    case 3:
        mVel_0x3c.z += 0.3f * lbl_8053EFB8;
        break;
    case 1:
    case 5:
    case 7:
    case 8:
    case 9:
        switch (mAnim.mAnim) {
        case 2:
        case 6:
            mVel_0x3c.x = mVel.x;
            mVel_0x3c.y = mVel.y;
            break;
        default:
            mVel_0x3c.x = 0.0f;
            mVel_0x3c.y = 0.0f;
            break;
        }
        mVel_0x3c.z += lbl_8053EFB8;
        break;
    default:
        mVel_0x3c.x = 0.0f;
        mVel_0x3c.y = 0.0f;
        applyGravity_0x84();
        break;
    }
}

bool SlimeFormation_804A5D60::vfunc_0x58() {
    if (mBox2.f.x - mBox2.i.x <= 0.0f || mBox2.f.y - mBox2.i.y <= 0.0f) {
        return false;
    }
    s32 player;
    s32 id = -1;
    bool arrow = false;
    bool blocked = false;
    JGeometry::TBox2f box;
    JGeometry::TVec2f probe;
    fn_801F317C(&box);
    player = fn_8023AE34(&box, mLayer_0x4, &probe);
    if (player >= 0) {
        arrow = fn_801CC820(player, NULL);
    } else {
        player = fn_8023AA18(&box, mLayer_0x4, &probe);
        if (player >= 0) {
            blocked = true;
        } else {
            player = fn_8023AC74(&box, mLayer_0x4, &probe);
            if (player >= 0) {
                fn_801CC538(player, &probe, NULL, &id);
            } else {
                player = fn_8023AD54(&box, mLayer_0x4, &probe);
                if (player >= 0) {
                    fn_801CC640(player, &id, NULL);
                }
            }
        }
    }
    bool hit = false;
    if (player >= 0) {
        if (blocked) {
            if (field_0x1dc.startTimer_801EAC20(player)) {
                hit = true;
            }
        } else if (arrow || (id >= 0 && field_0x1dc.fn_801EAC54(id))) {
            hit = true;
        }
        if (hit) {
            JGeometry::TVec3f at(probe.x, probe.y, 0.1f);
            fn_801CB538(mLayer_0x4, &at, 0, 0);
        }
    }
    return hit;
}

u8 SlimeFormation_804A5D60::checkHitByActor_0x60() {
    checkActorHit_801F2DB4(mActorId, 'NULL', NULL);
    return 0;
}

u8 SlimeFormation_804A5D60::checkHitByPlayer_0x5C() {
    if (mBox.f.x - mBox.i.x > 0.0f && mBox.f.y - mBox.i.y > 0.0f) {
        s32 flag;
        s32 player = findOverlappingPlayer_801F285C(NULL, &flag, true, -1);
        if (player >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
            if (actor != NULL) {
                static_cast<cls_804A5B28*>(actor)->fn_80207830(field_0x94, player, fn_8023B6CC(player), getPlayerPos_8023E724(player), flag);
            }
        }
    }
    return 0;
}

void SlimeFormation_804A5D60::onHitReactionEnd_0x74() {
    mFlags |= ENEMY_FLAG_NO_HURT;
}

void SlimeFormation_804A5D60::die_0x24() {
    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mTargetSerial);
    if (actor != NULL) {
        static_cast<cls_804A5B28*>(actor)->fn_802075D8(field_0x94);
    }
    ActorBase_804A5770::die_0x24();
}

// FIXME: 95.97 %, missing/extra; behaviour near.
void SlimeFormation_804A5D60::draw() {
    if (field_0x54.isZero()) {
        return;
    }

    SpriteParams_801F5AE0 params;
    params.field_0x1c = 0;
    params.field_0x1d = 0;

    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    mtx[0][0] = field_0x54.x;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = field_0x54.y;
    mtx[1][2] = 0.0f;
    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = field_0x54.z;
    params.mMtx = mtx;

    s32 frame = mAnim.mCel + 0x2C9;
    if (!(mFlags & ENEMY_FLAG_NO_HURT)) {
        drawSprite_801F06F0("enemy_gc_sob.bin", frame, &params, 7, 1.0f, 1.0f);
    } else {
        if (mSpriteParams.mMtx == NULL) {
            drawShadow_801EF830(mLayer_0x4, &mPos, &params, 7, "enemy_gc_sob.bin", frame, NULL);
        }
        if (mLayer_0x4 < 8) {
            void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
            gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&mPos, res, frame, &params, 1.0f, 1.0f);
        }
    }
}

void SlimeFormation_804A5D60::fn_80209D44() {
    mFlags |= 0x2000;
}

void SlimeFormation_804A5D60::fn_80209D20(s32 dir, u8 steps) {
    if (!checkFlag(0x2000)) {
        mFlags |= 0x4000;
        field_0x2ac = dir;
        field_0x2b0 = steps;
    }
}

void SlimeFormation_804A5D60::fn_80209CF4(const JGeometry::TVec2f* target) {
    if (!checkFlag(0x2000)) {
        mFlags |= 0x8000;
        field_0x2c8 = *target;
    }
}

void SlimeFormation_804A5D60::fn_80209CA4(s32 player, s32 attack, const JGeometry::TVec3f* from, s32 type) {
    if (takeHit_801F37AC(attack, from, 0, type, player)) {
        mFlags &= ~ENEMY_FLAG_NO_HURT;
    }
}

SlimeFormation_804A5D60::~SlimeFormation_804A5D60() {}
