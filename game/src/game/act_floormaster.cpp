#include <float.h>

#include "game/GameAudio.h"
#include "game/act/act_floormaster.h"
#include "game/ActorManager.h"
#include "game/Camera.h"
#include "game/Effect.h"
#include "game/EmitterDelete.h"
#include "game/GameGlobals.h"
#include "game/Map.h"
#include "game/Player.h"

bool approachValue_801CD8E4(f32* value, f32 target, f32 factor, f32 threshold);
bool fn_801CD1D0(s32 player);
bool fn_802316C8(s32 player, s32 layer, const JGeometry::TVec3f* pos);

static const s16 lbl_8046A3D8[] = { 0, 5, 1, 5, 2, 5, -1, -1 };
static const s16 lbl_8046A3E8[] = { 3, 5, 4, 10, -1, -1 };
static const s16 lbl_8046A3F4[] = { 2, 10, 5, 5, -1, -1 };
static const s16 lbl_8053FA20[] = { 3, 40, -1, -1 };
static const s16 lbl_8046A400[] = { 6, 5, 7, 5, 8, 5, -1, -1 };
static const s16 lbl_8046A410[] = { 2, 10, 5, 5, -1, -1 };
static const s16 lbl_8046A41C[] = { 3, 5, 6, 5, 7, 5, 8, 5, -1, -1 };

static const s16* lbl_804A5FE8[] = {
    lbl_8046A3D8,
    lbl_8046A3E8,
    lbl_8046A3F4,
    lbl_8053FA20,
    lbl_8046A400,
    lbl_8046A410,
    lbl_8046A41C,
};

// The hit reaction shared by vfunc_0x58/0x5C/0x60: a hit in the attack states starts state 4.
// Fades the emitter from white to (200, 0, 255) as field_0x2b4 goes 0 -> 1.
inline f32 lerp(f32 from, f32 to, f32 t) {
    return from + t * (to - from);
}

inline void approachLinear(f32* value, f32 target, f32 remaining) {
    if (remaining < FLT_EPSILON) {
        *value = target;
    } else {
        *value += (1.0f / remaining) * (target - *value);
    }
}

inline void Floormaster_804A6004::setEmitterColor() {
    u8 prm = lerp(255.0f, 0.0f, field_0x2b4);
    mEmitter2b0->setGlobalPrmColor(prm, prm, prm);
    mEmitter2b0->setGlobalEnvColor(lerp(255.0f, 200.0f, field_0x2b4),
                                   lerp(255.0f, 0.0f, field_0x2b4),
                                   lerp(255.0f, 255.0f, field_0x2b4));
}

inline void Floormaster_804A6004::onHit() {
    // The flag form matches; calling setState_8020ED5C(4) straight from the cases drops the callers to 92-97 %.
    bool change = false;
    switch (mState) {
    case 3:
    case 5:
    case 6:
    case 8:
        change = true;
        break;
    }
    if (change) {
        setState_8020ED5C(4);
    }
}

Floormaster_804A6004::Floormaster_804A6004() {
    field_0x2a8 = 0.0f;
    field_0x2ac = 0.0f;
    mEmitter2b0 = NULL;
    field_0x2b4 = 0.0f;
    field_0x2b8 = -1;
    field_0x2bc = 0;
    field_0x2c0 = -1;
}

Floormaster_804A6004::~Floormaster_804A6004() {
    releaseEmitter_801EE434(mEmitter2b0);
}

void Floormaster_804A6004::init() {
    EnemyBase_804A4808::init();
    if (mSpawnParam_0x90 & 1) {
        mFlags |= 0x2000;
        f32 y = 8.0f + (f32)(((mSpawnParam_0x90 >> 8) & 0x3F) * 8);
        f32 x = 8.0f + (f32)(((mSpawnParam_0x90 >> 16) & 0x3F) * 8);
        field_0x2a8 = x;
        field_0x2ac = y;
    }
    mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
    // Field-wise copy throughout: `mBox2 = mBox` drops init/setState_8020ED5C/updateAnim_0x94 to 72-93 %.
    mBox2.i.x = mBox.i.x;
    mBox2.i.y = mBox.i.y;
    mBox2.f.x = mBox.f.x;
    mBox2.f.y = mBox.f.y;
    mMapBox_0x80.set(1.0f, 1.0f, -1.0f, -1.0f);
    field_0xb0 = 0x50303;
    mDir_0x98 = 2;
    mFlags |= ENEMY_FLAG_NO_HURT | ENEMY_FLAG_NO_ATTACK | ENEMY_FLAG_KEEP_LIFE;
    mLife = 1000;
    mLifeBackup = mLife;
    field_0x194 = 1;
    mState = 0;
    mStateTimer = 0;
    field_0x250 = randInt_801CD664(61) + 60;
    mSpeed = 0.0f;
    mTargetSpeed = 0.0f;
    mAnim.setKeyTables_801D2608(lbl_804A5FE8, 7);
    mAnim.startAnim_801D22DC(0, 0, false);
}

// Case 1: the three env-colour conversions use other stack slots and f-registers than the original
// (0x18/0x20/0x10 for r/g/b), and the player-x check swaps f2/f3.
// FIXME: 99.18 %, scheduling+regalloc; behaviour drift.
void Floormaster_804A6004::setState_8020ED5C(s32 state) {
    switch (state) {
    case 0:
        field_0x2b4 = 0.0f;
        field_0x2b8 = -1;
        field_0x2bc = 0;
        mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        field_0x250 = randInt_801CD664(61) + 60;
        mPos.x = mSpawnPos.x;
        mPos.y = mSpawnPos.y;
        mPos.z = mSpawnPos.z;
        mSpeed = 0.0f;
        mTargetSpeed = 0.0f;
        break;

    case 1: {
        field_0x2b4 = 0.0f;
        mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 1.0f;
        mEmitter2b0 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1AF, 0, 0, NULL, NULL, true, 1.0f);
        if (mEmitter2b0 != NULL) {
            setEmitterColor();
        }
        break;
    }

    case 2:
        mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(0, 0, false);
        break;

    case 3:
        mBox.set(-8.0f, -16.0f, 8.0f, 8.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(1, 0, false);
        break;

    case 4:
        if (field_0x2b8 >= 0) {
            if (fn_8022DCD8(field_0x2b8)) {
                if (fn_8022DD74(field_0x2b8)) {
                    mFlags &= ~0x4000;
                    field_0x2c0 = -1;
                } else {
                    mFlags |= 0x4000;
                    field_0x2c0 = field_0x2b8;
                }
            } else if (fn_80230188(field_0x2b8)) {
                fn_80230224(field_0x2b8, 0);
            }
        }
        mBox.set(-8.0f, -16.0f, 8.0f, 8.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        break;

    case 5:
        mBox.set(-8.0f, -16.0f, 8.0f, 8.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(2, 0, false);
        break;

    case 6:
        mBox.set(-8.0f, -16.0f, 8.0f, 8.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(3, 0, false);
        break;

    case 7:
        mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(4, 0, false);
        break;

    case 8:
        mBox.set(-8.0f, -16.0f, 8.0f, 8.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(5, 0, false);
        break;

    case 9:
        mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        mAnim.startAnim_801D22DC(6, 0, false);
        break;

    case 10:
        field_0x2b8 = -1;
        mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
        mBox2.i.x = mBox.i.x;
        mBox2.i.y = mBox.i.y;
        mBox2.f.x = mBox.f.x;
        mBox2.f.y = mBox.f.y;
        mTargetSpeed = 0.0f;
        break;
    }

    mStateTimer = 0;
    mState = state;
    if (field_0x2b8 >= 0) {
        switch (mState) {
        case 2:
        case 3:
            const JGeometry::TVec3f* pPos = getPlayerPos_8023E724(field_0x2b8);
            if (mPos.x < pPos->x - 4.0f) {
                mFlags &= ~0x8000;
            } else if (pPos->x + 4.0f < mPos.x) {
                mFlags |= 0x8000;
            }
            break;
        }
    }
}

inline void Floormaster_804A6004::updateGrabbedPlayer_8020EB08() {
    JGeometry::TVec3f pPos = *getPlayerPos_8023E724(field_0x2b8);
    JGeometry::TVec3f targetPos = pPos;
    JGeometry::TVec3f dest;
    if (mState == 6) {
        dest.set(mPos.x, mPos.y, 16.0f);
    } else {
        dest.set(mPos.x, mPos.y, 0.0f);
    }
    approachValue_801CD8E4(&targetPos.x, dest.x, 0.2f, 0.0f);
    approachValue_801CD8E4(&targetPos.y, dest.y, 0.2f, 0.0f);
    approachValue_801CD8E4(&targetPos.z, dest.z, 0.2f, 0.0f);

    if (mState == 5) {
        fn_80230498(field_0x2b8, &targetPos, (u16)fn_8023CBC0(field_0x2b8), 0xFF, -0x80, -0x80, true);
    } else {
        JGeometry::TVec3f diff = targetPos - pPos;
        fn_8023DFF4(field_0x2b8, &diff);
    }
}

// Not auto-inlined: updateState_0x90 calls fn_8020EB08 out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
void Floormaster_804A6004::fn_8020EB08() {
    setEmitterPos_8013CB44(mEmitter2b0, &mPos, 0, 0, true);
    switch (mState) {
    case 1:
    case 2:
    case 3:
        if (field_0x2b8 >= 0) {
            field_0x2bc++;
        }
        s32 player = findNearestPlayer_801D05F8(&mPos, 0.0f);
        if (player != field_0x2b8) {
            if (field_0x2b8 < 0) {
                field_0x2b8 = player;
                field_0x2bc = 0;
            } else if (player < 0) {
                field_0x2b8 = -1;
                field_0x2bc = 0;
            } else if (field_0x2bc >= 45) {
                field_0x2b8 = player;
                field_0x2bc = 0;
            }
        }
        break;
    }

    // Flag form matches; moving the body into the cases drops this function to 97 %.
    bool inRange = false;
    switch (mState) {
    case 5:
    case 6:
    case 7:
        inRange = true;
        break;
    }
    if (inRange) {
        updateGrabbedPlayer_8020EB08();
    }

    if (mFlags & 0x4000) {
        if (fn_8022DD74(field_0x2c0)) {
            mFlags &= ~0x4000;
            field_0x2c0 = -1;
        }
    }
}
#pragma auto_inline reset

// case 3 sets the flag with `mr r30, r4` where ours has `li r30, 1`;
// case 7 stores x/y from pos in the other order and swaps pos/vec stack slots;
// setEmitterColor() env-color stfd conversion order differs in cases 1 and 10.
// FIXME: 98.13 %, different; behaviour drift.
void Floormaster_804A6004::updateState_0x90(u8 mapHit) {
    fn_8020EB08();
    switch (mState) {
    case 0:
        if (!(mFlags & 0x4000) && mStateTimer >= field_0x250) {
            field_0x2bc = 0;
            field_0x2b8 = findNearestPlayer_801D05F8(&mPos, 48.0f);
            if (field_0x2b8 >= 0) {
                setState_8020ED5C(1);
            }
        }
        break;

    case 1:
        if (mStateTimer >= 60) {
            if (field_0x2b8 >= 0) {
                setState_8020ED5C(2);
            } else {
                setState_8020ED5C(10);
            }
        } else {
            approachLinear(&field_0x2b4, 1.0f, 60 - mStateTimer);
            if (mEmitter2b0 != NULL) {
                setEmitterColor();
            }
            startSound_801F0E34(0x50089, 0);
        }
        break;

    case 2:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            if (field_0x2b8 >= 0) {
                setState_8020ED5C(3);
            } else {
                setState_8020ED5C(8);
            }
        }
        break;

    case 3:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            startSound_801F0E34(0x50088, 0);
            if (field_0x2b8 >= 0) {
                bool flag = false;
                if (mState == 3 && field_0x2b8 >= 0 && !fn_80230188(field_0x2b8) &&
                    fn_802300EC(field_0x2b8) && !fn_8022DCD8(field_0x2b8) &&
                    !fn_801CD1D0(field_0x2b8)) {
                    mBox.set(-10.0f, -16.0f, 10.0f, 10.0f);
                    if (getPlayersInBox_801F2740(-1) & (1 << field_0x2b8)) {
                        flag = true;
                    }
                    mBox.set(-8.0f, -16.0f, 8.0f, 8.0f);
                    mBox2.i.x = mBox.i.x;
                    mBox2.i.y = mBox.i.y;
                    mBox2.f.x = mBox.f.x;
                    mBox2.f.y = mBox.f.y;
                }
                if (flag) {
                    if (fn_80230AD4(field_0x2b8)) {
                        setState_8020ED5C(5);
                    } else {
                        setState_8020ED5C(8);
                    }
                    break;
                }
            }
            setState_8020ED5C(8);
        }
        break;

    case 4:
        if (mStateTimer >= 120) {
            field_0x2bc = 0;
            field_0x2b8 = findNearestPlayer_801D05F8(&mPos, 48.0f);
            if (field_0x2b8 >= 0) {
                setState_8020ED5C(3);
            } else {
                setState_8020ED5C(9);
            }
        }
        break;

    case 5:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            fn_80230224(field_0x2b8, 0);
            if (fn_8022DE10(field_0x2b8, 2)) {
                setState_8020ED5C(6);
            } else {
                setState_8020ED5C(9);
            }
        }
        break;

    case 6:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            setState_8020ED5C(7);
        }
        break;

    case 7:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            JGeometry::TVec3f pos;
            if (!(mFlags & 0x2000)) {
                fn_801CD150(&mPos, &pos);
                field_0x2a8 = pos.x;
                field_0x2ac = pos.y;
            }
            JGeometry::TVec3f temp(field_0x2a8, field_0x2ac, 0.0f);
            JGeometry::TVec3f vec;
            vec = temp;
            fn_802316C8(field_0x2b8, 8, &vec);
            if (fn_8022DD74(field_0x2b8)) {
                mFlags &= ~0x4000;
                field_0x2c0 = -1;
            } else {
                mFlags |= 0x4000;
                field_0x2c0 = field_0x2b8;
            }
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1B0, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x548, 0, -1, NULL, NULL, true, 1.0f);
            setState_8020ED5C(10);
        }
        break;

    case 8:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            setState_8020ED5C(9);
        }
        break;

    case 9:
        if (mStateTimer > 1 && mAnim.isStopped_801D21F0()) {
            setState_8020ED5C(10);
        }
        break;

    case 10:
        if (mStateTimer >= 60) {
            deleteEmitter(&mEmitter2b0);
            setState_8020ED5C(0);
        } else {
            approachLinear(&field_0x2b4, 0.0f, 60 - mStateTimer);
            if (mEmitter2b0 != NULL) {
                setEmitterColor();
            }
        }
        break;
    }
}

void Floormaster_804A6004::updateAnim_0x94() {
    if (mState == 4) return;
    EnemyBase_804A4808::updateAnim_0x94();
    if (!(mBox.f.x - mBox.i.x > 0.0f)) return;
    if (!(mBox.f.y - mBox.i.y > 0.0f)) return;
    switch (mAnim.mCel) {
    case 2:
        mBox.set(-7.0f, -12.0f, 16.0f, 8.0f);
        break;
    case 3:
        mBox.set(-12.0f, -22.0f, 2.0f, 8.0f);
        break;
    case 4:
        mBox.set(-12.0f, -20.0f, 1.0f, 8.0f);
        break;
    case 5:
        mBox.set(-4.0f, -16.0f, 16.0f, 8.0f);
        break;
    }
    if (mFlags & 0x8000) {
        f32 maxX = mBox.f.x;
        f32 minX = mBox.i.x;
        mBox.i.x = -maxX;
        mBox.f.x = -minX;
    }
    mBox2.i.x = mBox.i.x;
    mBox2.i.y = mBox.i.y;
    mBox2.f.x = mBox.f.x;
    mBox2.f.y = mBox.f.y;
}

void Floormaster_804A6004::calcVelocity_0x88() {
    mVel_0x3c.set(0.0f, 0.0f, 0.0f);
    switch (mState) {
    case 1:
        if (field_0x2b8 >= 0) {
            JGeometry::TVec2f vec;
            fn_801D00B4(field_0x2b8, &vec, 1.0f);
            mVel_0x3c.x = vec.x;
            mVel_0x3c.y = vec.y;
        }
        break;
    }
}

bool Floormaster_804A6004::vfunc_0x58() {
    if (mBox2.f.x - mBox2.i.x <= 0.0f || mBox2.f.y - mBox2.i.y <= 0.0f) {
        return false;
    }
    JGeometry::TVec3f vel54;
    JGeometry::TVec3f vel48;
    JGeometry::TBox2f box;
    JGeometry::TVec2f hitPos;
    s32 hit;
    s32 actorSerial = -1;
    bool b1 = false;
    bool b2 = false;
    vel48.set(0.0f, 0.0f, 0.0f);
    fn_801F317C(&box);

    hit = fn_8023AE34(&box, mLayer_0x4, &hitPos);
    if (hit >= 0) {
        b1 = fn_801CC820(hit, &vel48);
    } else {
        hit = fn_8023AA18(&box, mLayer_0x4, &hitPos);
        if (hit >= 0) {
            b2 = true;
        } else {
            hit = fn_8023AC74(&box, mLayer_0x4, &hitPos);
            if (hit >= 0) {
                fn_801CC538(hit, &hitPos, &vel54, &actorSerial);
            } else {
                hit = fn_8023AD54(&box, mLayer_0x4, &hitPos);
                if (hit >= 0) {
                    fn_801CC640(hit, &actorSerial, &vel48);
                }
            }
        }
    }

    bool hitOccurred = false;
    if (hit >= 0) {
        if (b1) {
            hitOccurred = true;
        } else if (b2) {
            if (field_0x1dc.startTimer_801EAC20(hit)) {
                hitOccurred = true;
            }
        } else if ((hit = actorSerial) >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(hit);
            if (actor && field_0x1dc.fn_801EAC54(actor->mSerial)) {
                hitOccurred = true;
            }
        }

        if (hitOccurred) {
            JGeometry::TVec3f effectPos(hitPos.x, hitPos.y, 0.0f);
            fn_801CB538(mLayer_0x4, &effectPos, 0, 0);
        }
        if (hitOccurred) {
            startSound_801F0E34(0x5007D, 0);
            onHit();
        }
    }
    return hitOccurred;
}

u8 Floormaster_804A6004::checkHitByActor_0x60() {
    u8 result = 0;
    if (mBox.f.x - mBox.i.x > 0.0f && mBox.f.y - mBox.i.y > 0.0f) {
        result = checkActorHit_801F2DB4(mActorId, 'NULL', NULL);
        if (result) {
            startSound_801F0E34(0x5007D, 0);
            onHit();
        }
    }
    return result;
}

u8 Floormaster_804A6004::checkHitByPlayer_0x5C() {
    u8 result = 0;
    if (mBox.f.x - mBox.i.x > 0.0f && mBox.f.y - mBox.i.y > 0.0f) {
        for (int i = 0; i < 4; i++) {
            s32 flag;
            JGeometry::TVec2f pos;
            if (i == findOverlappingPlayer_801F285C(&pos, &flag, true, i)) {
                fn_8023B6CC(i);
                if (field_0x1dc.startTimer_801EAC20(i)) {
                    JGeometry::TVec3f effectPos(pos.x, pos.y, 0.0f);
                    result = 1;
                    fn_801CB538(mLayer_0x4, &effectPos, 0, 0);
                }
            }
        }

        if (result) {
            startSound_801F0E34(0x5007D, 0);
            onHit();
        }
    }
    return result;
}

void Floormaster_804A6004::vfunc_0xA0() {
    if (mState != 0) {
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
    }
}

void Floormaster_804A6004::draw() {
    switch (mState) {
    default: {
        JGeometry::TVec3f pos = mPos;
        if (mState == 4 && mStateTimer >= 60) {
            if (mStateTimer % 4 < 2) {
                pos.x += 1.0f;
            } else {
                pos.x -= 1.0f;
            }
        }
        SpriteParams_801F5AE0 params;
        params.field_0x1c = 0;
        params.field_0x1d = 0;
        Mtx mtx;
        if (mFlags & 0x8000) {
            mtx[0][3] = 0.0f;
            mtx[1][3] = 0.0f;
            mtx[2][3] = 0.0f;
            mtx[0][0] = -1.0f;
            mtx[0][1] = 0.0f;
            mtx[0][2] = 0.0f;
            mtx[1][0] = 0.0f;
            mtx[1][1] = 1.0f;
            mtx[1][2] = 0.0f;
            mtx[2][0] = 0.0f;
            mtx[2][1] = 0.0f;
            mtx[2][2] = 1.0f;
            params.mMtx = mtx;
        }
        s32 frame = mAnim.mCel + 0x321;
        if (mLayer_0x4 < 8) {
            void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
            gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, frame, &params, 1.0f, 1.0f);
        }
        break;
    }
    case 0:
    case 1:
    case 10:
        break;
    }
}
