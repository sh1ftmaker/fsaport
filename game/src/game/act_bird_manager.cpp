#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/act/act_bird_manager.h"
#include "game/act/act_bird_formation_8020B354.h"
#include "game/ActorManager.h"
#include "game/GameView.h"
#include "JSystem/JMath/JMath.h"

// Moves mPos 1.2 along x toward mTarget (forward) or mStart; returns whether it arrived.
static inline bool stepLine(BirdManager_804A5C88::Path* path, bool forward) {
    bool done = false;
    if (forward) {
        path->mPos.x += 1.2f;
        if (path->mPos.x >= path->mTarget.x) {
            path->mPos.x = path->mTarget.x;
            done = true;
        }
    } else {
        path->mPos.x -= 1.2f;
        if (path->mPos.x <= path->mStart.x) {
            path->mPos.x = path->mStart.x;
            done = true;
        }
    }
    return done;
}

// Advances mAngle by mAngleStep and places mPos on the radius-125 arc around mTarget
// (atTarget) or mStart; returns whether the angle reached the end of the arc.
static inline bool stepArc(BirdManager_804A5C88::Path* path, bool atTarget) {
    bool done = false;
    path->mAngle += path->mAngleStep;
    if (atTarget) {
        if (path->mAngle <= 0) {
            if (path->mAngleStep > 0) {
                path->mAngle = -0x8000;
            } else {
                path->mAngle = 0;
            }
            done = true;
        }
    } else {
        if (path->mAngle >= 0) {
            if (path->mAngleStep > 0) {
                path->mAngle = 0;
            } else {
                path->mAngle = -0x8000;
            }
            done = true;
        }
    }
    f32 sinVal = JMath::sincosTable_.sinShort(path->mAngle) * 125.0f;
    f32 cosVal = JMath::sincosTable_.cosShort(path->mAngle) * 125.0f;
    if (atTarget) {
        path->mPos.x = path->mTarget.x;
        path->mPos.y = path->mTarget.y;
    } else {
        path->mPos.x = path->mStart.x;
        path->mPos.y = path->mStart.y;
    }
    path->mPos.x += sinVal;
    path->mPos.y += cosVal;
    return done;
}

// FIXME: 97.84 %, scheduling+regalloc; behaviour clean.
// (scheduling+regalloc). In the inlined stepArc the 125.0f literal is loaded
// after the sincosTable address (ours before) and the multiply takes its operands the
// other way round (k * sin written either way).
// The direction arguments are materialised (cntlzw / extrwi. for == 0, neg / or / srwi. for
// != 0), hence `? true : false` (RULES_CARD byte-test shape); a plain compare folds into
// cmpwi. The old position is a TVec2f copy: two f32 locals let MWCC reuse mPos.x where the
// original reloads it.
void BirdManager_804A5C88::Path::fn_80209640(JGeometry::TVec2f* delta) {
    JGeometry::TVec2f old(mPos);

    switch (mState) {
    case 0:
        if (stepLine(this, mDirection == 0 ? true : false)) {
            mState = 1;
        }
        break;
    case 1:
        if (stepArc(this, mDirection == 0 ? true : false)) {
            mState = 2;
        }
        break;
    case 2:
        if (stepLine(this, mDirection != 0 ? true : false)) {
            mState = 3;
        }
        break;
    case 3:
        if (stepArc(this, mDirection != 0 ? true : false)) {
            mState = 4;
        }
        break;
    case 4:
        if (mDirection == 0) {
            mPos.x += 1.2f;
        } else {
            mPos.x -= 1.2f;
        }
        break;
    }

    if (delta != NULL) {
        delta->set(mPos);
        delta->x -= old.x;
        delta->y -= old.y;
    }
}

BirdManager_804A5C88::BirdManager_804A5C88() {
    field_0x340 = 0;
    field_0x344 = 0;
    field_0x348 = 0;
    mPath.mState = 0;
    mPath.mPos.x = 0.0f;
    mPath.mPos.y = 0.0f;
    mPath.mDirection = 0;
    mPath.mAngle = -0x8000;
    mPath.mAngleStep = 0;
    mPath.mStart.x = 0.0f;
    mPath.mStart.y = 0.0f;
    mPath.mTarget.x = 0.0f;
    mPath.mTarget.y = 0.0f;
}

// FIXME: 99.47 %, regalloc-only; behaviour clean.
// (regalloc-only). Minor register differences: dir in r30 vs r29, centerX/centerY in f3/f2 vs f2/f4, and loop counters r28/r29 swapped.
void BirdManager_804A5C88::init() {
    EnemyBase_804A4808::init();
    field_0x2ac = mSpawnParam_0x90 & 3;
    if (field_0x2ac >= 2 || field_0x2ac < 0) {
        field_0x2ac = randInt_801CD664(2);
    }
    field_0x340 = (mSpawnParam_0x90 >> 4) & 3;
    if (field_0x340 >= 2 || field_0x340 < 0) {
        field_0x340 = randInt_801CD664(2);
    }
    field_0x344 = (mSpawnParam_0x90 >> 8) & 1;
    field_0x348 = (u32)mSpawnParam_0x90 >> 27;

    mBox.set(1.0f, 1.0f, -1.0f, -1.0f);
    mBox2.set(mBox);
    mMapBox_0x80.set(1.0f, 1.0f, -1.0f, -1.0f);
    field_0xb0 = 0;
    mDir_0x98 = 2;
    mFlags |= ENEMY_FLAG_NO_ATTACK | ENEMY_FLAG_NO_HURT | ENEMY_FLAG_KEEP_LIFE;
    mLife = 1000;
    mLifeBackup = mLife;
    field_0x194 = 1;
    mState = 0;
    mStateTimer = 0;
    field_0x250 = randInt_801CD664(0x4B1) + 600;
    mSpeed = 0.0f;
    mTargetSpeed = 0.0f;

    s32 dir = (field_0x340 != 0);
    GameView_804A9950* view = getGameView_802D6A9C();
    f32 centerY = (view->mVisibleBox.i.y + view->mVisibleBox.f.y) * 0.5f;
    f32 centerX = (view->mVisibleBox.i.x + view->mVisibleBox.f.x) * 0.5f;
    mPath.mState = 0;
    switch (dir) {
    case 0:
        mPath.mPos.x = view->mVisibleBox.i.x - 48.0f;
        break;
    case 1:
        mPath.mPos.x = 48.0f + view->mVisibleBox.f.x;
        break;
    }
    mPath.mPos.y = centerY - 125.0f;
    mPath.mDirection = dir;
    mPath.mAngle = -0x8000;
    mPath.mAngleStep = 100;
    if (dir == 0) {
        mPath.mAngleStep = -mPath.mAngleStep;
    }
    mPath.mStart.x = centerX - 100.0f;
    mPath.mStart.y = centerY;
    mPath.mTarget.x = centerX + 100.0f;
    mPath.mTarget.y = centerY;

    JGeometry::TVec2f spawnPos2D;
    mPath.getPos(&spawnPos2D);
    mSpawnPos.x = spawnPos2D.x;
    if (field_0x344 == 0) {
        mSpawnPos.y = spawnPos2D.y;
    }
    mSpawnPos.z = 24.0f;

    f32 stepX = 0.0f;
    f32 stepY = 0.0f;
    JGeometry::TVec3f spawnPos = mSpawnPos;
    switch (field_0x2ac) {
    case 0:
        stepX = 16.0f;
        spawnPos.x -= 24.0f;
        break;
    case 1:
        stepY = 16.0f;
        spawnPos.y -= 24.0f;
        break;
    }

    for (s32 i = 0; i < 4; i++) {
        mSlots[i].mSerial = getActorManager_801F856C()->spawnActor_801F9484(
            'FOBR', mLayer_0x4, &spawnPos, mSerial, -1, -1);
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mSlots[i].mSerial);
        if (actor != NULL) {
            actor->field_0x94 = i;
        }
        spawnPos.x += stepX;
        spawnPos.y += stepY;
    }
}

void BirdManager_804A5C88::onFirstUpdate_0x54() {
    mPos.x = mSpawnPos.x;
    mPos.y = mSpawnPos.y;
    mPos.z = mSpawnPos.z;
}

void BirdManager_804A5C88::vfunc_0xA4() {
    if (mState != 2) {
        mStateTimer = 0;
        mState = 2;
    }
    for (s32 i = 0; i < 4; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mSlots[i].mSerial);
        if (actor != NULL) {
            static_cast<BirdFormation_804A5E98*>(actor)->fn_8020B434();
        }
    }
}

void BirdManager_804A5C88::vfunc_0xA8() {
    if (mState != 3) {
        mStateTimer = 0;
        mState = 3;
    }
    for (s32 i = 0; i < 4; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mSlots[i].mSerial);
        if (actor != NULL) {
            static_cast<BirdFormation_804A5E98*>(actor)->fn_8020B3E4(
                mSlots[i].mPlayer, mSlots[i].field_0xc, &mSlots[i].mTarget, mSlots[i].field_0x1c);
        }
    }
}

// State 0 starts the four slot actors, state 1 removes them all once the manager is 96 units outside
// the visible box. An inline of its own: only then do the loops' registers come out as in the
// original (docs/REGALLOC.md, rule 4).
static inline void updateByState(BirdManager_804A5C88* self) {
    switch (self->mState) {
    case 0:
        if ((self->field_0x348 == 0 && self->mStateTimer >= self->field_0x250) || self->isScreenFlagSet_0x34()) {
            for (s32 i = 0; i < 4; i++) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(self->mSlots[i].mSerial);
                if (actor != NULL) {
                    static_cast<BirdFormation_804A5E98*>(actor)->fn_8020B500();
                }
            }
            self->mStateTimer = 0;
            self->mState = 1;
        }
        break;
    case 1: {
        s32 layer = self->mLayer_0x4;
        GameView_804A9950* view = getGameView_802D6A9C();
        const JGeometry::TBox2f* box;
        if (layer < 8) {
            box = &view->mVisibleBox;
        } else {
            box = &view->field_0xdc;
        }
        if (self->mPos.x < box->i.x - 96.0f || self->mPos.x > box->f.x + 96.0f) {
            self->mAlive = false;
            for (s32 i = 0; i < 4; i++) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(self->mSlots[i].mSerial);
                if (actor != NULL) {
                    actor->mAlive = false;
                }
            }
        }
        break;
    }
    case 2:
    case 3:
        break;
    }
}

void BirdManager_804A5C88::updateState_0x90(u8 mapHit) {
    fn_80207A60();
    updateByState(this);
}

// mVel_0x3c handed on to the four slot actors. An inline of its own: its loop registers are
// handed out before calcVelocity_0x88's (docs/REGALLOC.md, rule 4).
static inline void copyField3cToSlotActors(BirdManager_804A5C88* self) {
    for (s32 i = 0; i < 4; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(self->mSlots[i].mSerial);
        if (actor != NULL) {
            actor->mVel_0x3c = self->mVel_0x3c;
        }
    }
}

void BirdManager_804A5C88::calcVelocity_0x88() {
    switch (mState) {
    case 0:
    case 2:
    case 3:
        mVel_0x3c.x = 0.0f;
        mVel_0x3c.y = 0.0f;
        mVel_0x3c.z = 0.0f;
        break;
    case 1:
        if (field_0x344 == 0) {
            JGeometry::TVec2f delta;
            mPath.fn_80209640(&delta);
            mVel_0x3c.set(delta.x, delta.y, 0.0f);
        } else if (field_0x340 == 0) {
            mVel_0x3c.set(1.2f, 0.0f, 0.0f);
        } else {
            mVel_0x3c.set(-1.2f, 0.0f, 0.0f);
        }
        copyField3cToSlotActors(this);
        break;
    }
}

BirdManager_804A5C88::~BirdManager_804A5C88() {}
