#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/GameMath.h"
#include "game/Player.h"
#include "game/act/act_terrorpin.h"


// FIXME: die_0x24, 0x44, 0x68, 0x6C, 0x7C, 0x94 and init are instruction-identical;
// only their float literals are @NNN, not the original lbl_ (.sdata2 order; the
// pool is complete only once checkHitByPlayer_0x5C and updateState_0x90 are 100 %).

// The two animations' keys (cel pairs for 10 frames each, then -1) and the
// table the init hands to SpriteAnim::setKeyTables_801D2608 (names unknown).
static const s16 lbl_80469180[6] = {0, 10, 1, 10, -1, -1};
static const s16 lbl_8046918C[6] = {2, 10, 3, 10, -1, -1};
static const s16* lbl_8053A120[2] = {lbl_80469180, lbl_8046918C};

// The scale part of m (its translation column is the caller's). The draw
// loads the three scales z first, as arguments of an inline call (cf. player_80264998.cpp).
static inline void setScaleMtx(Mtx m, f32 x, f32 y, f32 z) {
    m[0][0] = x;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = y;
    m[1][2] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = z;
}

static inline void approach(f32& cur, f32 target, f32 remaining) {
    if (remaining < FLT_EPSILON) {
        cur = target;
    } else {
        cur += (1.0f / remaining) * (target - cur);
    }
}

static inline s32 firstHitPlayer(u8 hits) {
    s32 player = -1;
    for (s32 i = 0; i < 4; i++) {
        if (hits & (1 << i)) {
            player = i;
            break;
        }
    }
    return player;
}

inline bool Terrorpin_804A3B10::isVulnerable() const {
    bool vulnerable = true;
    bool active = false;
    switch (mState) {
    case 1:
    case 2:
    case 3:
    case 5:
        active = true;
        break;
    }
    if (active) {
        vulnerable = false;
        if (mStateTimer < 10) {
            switch (mState) {
            case 5:
            case 1:
                vulnerable = true;
                break;
            }
        }
    }
    return vulnerable;
}

inline bool Terrorpin_804A3B10::hasLanded() const {
    bool landed = false;
    switch (mState) {
    case 1:
    case 4:
        if ((mStateTimer > 1 && mPos.z <= 0.0f && mVel_0x3c.z == 0.0f) || mStateTimer >= 120) {
            landed = true;
        }
        break;
    }
    return landed;
}

Terrorpin_804A3B10::Terrorpin_804A3B10() {
    field_0x2a8 = 0;
}

void Terrorpin_804A3B10::init() {
    EnemyBase_804A4808::init();
    // mSpawnParam_0x90 bit 0 enables the scaled box; bits 1..3 pick field_0x2a8.
    if (mSpawnParam_0x90 & 1) {
        mFlags |= 0x2000;
        u32 sel = (mSpawnParam_0x90 >> 1) & 7;
        if (sel >= 1 && sel <= 4) {
            mFlags |= 0x4000;
            field_0x2a8 = sel - 1;
        }
    }
    if (mFlags & 0x2000) {
        f32 scale = 1.4f;
        field_0x54.x = scale;
        field_0x54.y = scale;
        field_0x54.z = scale;
        mBox.i.x *= scale;
        mBox.i.y *= scale;
        mBox.f.x *= scale;
        mBox.f.y *= scale;
        mMapBox_0x80.i.x = mBox.i.x;
        mMapBox_0x80.i.y = mBox.i.y;
        mMapBox_0x80.f.x = mBox.f.x;
        mMapBox_0x80.f.y = mBox.f.y;
    } else {
        mLife = 1;
    }
    mLifeBackup = mLife;
    field_0xb0 = 0x4130B;
    mState = 0;
    mStateTimer = 0;
    field_0x250 = randInt_801CD664(16) + 30;
    mSpeed = 0.35f;
    mTargetSpeed = 0.35f;
    mAnim.setKeyTables_801D2608(lbl_8053A120, 2);
    mAnim.startAnim_801D22DC(0, 2, false);
    mAnim.setRandomFrame_801D1D60();
}

void Terrorpin_804A3B10::setState_801B91A8(s32 state) {
    switch (state) {
    case 0:
        field_0x250 = randInt_801CD664(16) + 30;
        mTargetSpeed = 0.35f;
        if (mState != 4) {
            mDir_0x98 = fn_801CF6B8();
        }
        break;
    case 1:
    case 4:
        if (state == 1) {
            if (mFlags & 0x2000) {
                startSound_801F0E34(0x50016, 0);
            } else {
                startSound_801F0E34(0x50017, 0);
            }
        } else {
            if (mFlags & 0x2000) {
                startSound_801F0E34(0x50021, 0);
            } else {
                startSound_801F0E34(0x50022, 0);
            }
        }
        field_0x250 = -1;
        mDir_0x98 = oppositeDir_801CD354(mDir_0x98);
        mTargetSpeed = 0.6f;
        mSpeed = 0.6f;
        if (mFlags & 0x2000) {
            mVel_0x3c.z = 2.0f;
        } else {
            mVel_0x3c.z = 3.0f;
        }
        break;
    case 2:
        field_0x250 = randInt_801CD664(101) + 400;
        mTargetSpeed = 0.0f;
        break;
    case 3:
        field_0x250 = randInt_801CD664(11) + 30;
        mTargetSpeed = 0.0f;
        break;
    case 5:
        field_0x250 = randInt_801CD664(1) + 540;
        mDir_0x98 = oppositeDir_801CD354(mDir_0x98);
        mTargetSpeed = 2.5f;
        mSpeed = 2.5f;
        mVel_0x3c.z = 2.0f;
        break;
    }
    mStateTimer = 0;
    mState = state;
}

// FIXME: 98.89 %, missing/extra; behaviour near.
// try.py class missing/extra: the original copies the u8 mPlayersHit into its own register (one clrlwi r5, r0, 24) before the unrolled first-hit-player loop, we test the lbz result directly; the registers of mPlayersHit, the loop shift counts and the -1 initial player (li r30) differ accordingly.
void Terrorpin_804A3B10::updateState_0x90(u8 mapHit) {
    // Declared in this order: MWCC gives the later aggregate the lower stack
    // slot (normal at sp+0x20, box at sp+0x10, out at sp+0x8).
    JGeometry::TVec3f normal;
    if (mPos.z <= 0.0f) {
        s32 player;
        bool found = false;
        JGeometry::TBox2f box(mPos.x - 40.0f, mPos.y - 40.0f, 40.0f + mPos.x, 40.0f + mPos.y);
        player = -1;
        JGeometry::TVec2f out;
        if (mFlags & 0x4000) {
            if (fn_8023AAF8(field_0x2a8, &box, mLayer_0x4, &out)) {
                player = field_0x2a8;
            }
        } else {
            player = fn_8023AA18(&box, mLayer_0x4, &out);
        }
        if (player >= 0) {
            f32 dx = out.x - mPos.x;
            f32 dy = out.y - mPos.y;
            if (square(dx) + square(dy) <= 1600.0f) {
                found = true;
            }
        }
        if (found && mPos.z <= 0.0f) {
            switch (mState) {
            case 0:
                setState_801B91A8(1);
                break;
            case 2:
            case 3:
            case 5:
                setState_801B91A8(4);
                break;
            }
        }
    }
    switch (mState) {
    case 0:
        if (mapHit) {
            mDir_0x98 = pickNewDir_801CF720(mDir_0x98);
            field_0x250 += 10;
        } else if (mStateTimer >= field_0x250) {
            setState_801B91A8(0);
        }
        break;
    case 1:
        if (hasLanded()) {
            mVel_0x3c.z = 0.0f;
            mPos.z = 0.0f;
            setState_801B91A8(2);
        }
        break;
    case 2:
        if (mStateTimer >= field_0x250) {
            setState_801B91A8(3);
        }
        break;
    case 3:
        if (mStateTimer >= field_0x250) {
            setState_801B91A8(4);
        }
        break;
    case 4:
        if (hasLanded()) {
            mVel_0x3c.z = 0.0f;
            mPos.z = 0.0f;
            setState_801B91A8(0);
        }
        break;
    case 5:
        if (mapHit) {
            fn_801CC350(&mMapHitDir, &mVel_0x3c);
            startSound_801F0E34(0x4003C, 0);
        } else if (mPlayersHit != 0) {
            s32 player = firstHitPlayer(mPlayersHit);
            normal.set(mPos.x - getPlayerPos_8023E724(player)->x,
                       mPos.y - getPlayerPos_8023E724(player)->y, 0.0f);
            fn_801CC350(&normal, &mVel_0x3c);
            startSound_801F0E34(0x4003C, 0);
        }
        if (mStateTimer >= field_0x250) {
            mVel_0x3c.z = 0.0f;
            mPos.z = 0.0f;
            mSpeed = 0.0f;
            setState_801B91A8(2);
        }
        break;
    }
    switch (mState) {
    case 1:
        mFlags |= ENEMY_FLAG_BOUNCE;
        break;
    default:
        mFlags &= ~ENEMY_FLAG_BOUNCE;
        break;
    }
    if (isVulnerable()) {
        field_0xb0 = 0x4130B;
    } else {
        field_0xb0 = 0x41101;
    }
}

// 0x2A0 is mAnim.mFrameCtrl.mRate (J3DFrameCtrl _0C at 0x270 + 0x24 + 0xC).
void Terrorpin_804A3B10::updateAnim_0x94() {
    // The bool and the 0/1 argument are separate locals: the original keeps
    // both (li r3 / li r4) and tests the bool with clrlwi. to build the flag.
    s32 flag = 0;
    bool active = false;
    switch (mState) {
    case 1:
    case 2:
    case 3:
    case 5:
        active = true;
        break;
    }
    if (active) {
        flag = 1;
    }
    mAnim.startAnimIfChanged_801D22A0(flag, 2);
    f32 rate = 1.0f;
    if (mState == 3) {
        rate = 2.0f;
    }
    mAnim.mFrameCtrl.setRate(rate);
    EnemyBase_804A4808::updateAnim_0x94();
}

void Terrorpin_804A3B10::calcVelocity_0x88() {
    if (mState == 5) {
        mVel_0x3c.z += lbl_8053EFB8;
        s32 rem = field_0x250 - mStateTimer;
        if (rem > 0 && rem < 30) {
            approach(mVel_0x3c.x, 0.0f, rem);
            approach(mVel_0x3c.y, 0.0f, rem);
        } else {
            JGeometry::TVec3f vel(mVel_0x3c.x, mVel_0x3c.y, 0.0f);
            if (vel.squared() < 1.8f * 1.8f) {
                if (vel.isZero()) {
                    mVel_0x3c.x = 0.0f;
                    mVel_0x3c.y = 1.8f;
                } else {
                    vel.normalize();
                    vel.scale(1.8f);
                    mVel_0x3c.x = vel.x;
                    mVel_0x3c.y = vel.y;
                }
            }
        }
    } else {
        EnemyBase_804A4808::calcVelocity_0x88();
    }
}

void Terrorpin_804A3B10::vfunc_0x44() {
    if (field_0xd4 == 1) {
        bool active = false;
        if (mFlags & 0x4000) {
            if (field_0x2a8 == lbl_8053A100) {
                active = true;
            }
        } else {
            active = true;
        }
        if (active && mPos.z <= 0.0f) {
            switch (mState) {
            case 0:
                setState_801B91A8(1);
                break;
            case 2:
            case 3:
            case 5:
                setState_801B91A8(4);
                break;
            }
        }
        field_0xd4 = 0;
    }
}

void Terrorpin_804A3B10::onBounce_0x7C() {
    if (__fabsf(mVel_0x3c.z) >= 0.5f) {
        if (mFlags & 0x2000) {
            startSound_801F0E34(0x50014, 0);
        } else {
            startSound_801F0E34(0x50015, 0);
        }
    }
}

u8 Terrorpin_804A3B10::checkHitByActor_0x60() {
    u8 hit = 0;
    if (isVulnerable()) {
        checkActorHit_801F2DB4(mActorId, 'NULL', NULL);
    } else {
        hit = checkActorHit_801F2DB4('NULL', 'NULL', NULL);
    }
    return hit;
}

// While mState is 5, spawns two 'RUPY' actors at mPos and sets each one's
// mVel_0x3c: x is 1.5f for the first and its negation for the second,
// y and z are fixed.
void Terrorpin_804A3B10::onHitByPlayer_0x6C() {
    if (mState == 5) {
        f32 speed = 1.5f;
        for (s32 i = 0; i < 2; i++) {
            s32 serial = getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &mPos, 2);
            if (serial >= 0) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (actor != NULL) {
                    f32 x = speed;
                    if (i != 0) {
                        x = -speed;
                    }
                    actor->mVel_0x3c = JGeometry::TVec3f(x, 0.0f, 4.5f);
                }
            }
        }
    }
}

u8 Terrorpin_804A3B10::checkHitByPlayer_0x5C() {
    u8 hit = 0;
    if (isVulnerable()) {
        if (mFlags & 0x2000) {
            fn_801CEBD0();
        } else if (0 == mState) {
            s32 player = findOverlappingPlayer_801F285C(NULL, NULL, true, -1);
            if (player >= 0) {
                setState_801B91A8(5);
                JGeometry::TVec2f dir;
                dir.y = mPos.y - getPlayerPos_8023E724(player)->y;
                dir.x = mPos.x - getPlayerPos_8023E724(player)->x;
                if (dir.squared() <= JGeometry::TUtilf::epsilon()) {
                    mVel_0x3c.x = 0.0f;
                    mVel_0x3c.y = mSpeed;
                } else {
                    dir.normalize();
                    dir.scale(mSpeed);
                    mVel_0x3c.x = dir.x;
                    mVel_0x3c.y = dir.y;
                }
                startSound_801F0E34(0x4003B, 0);
            }
        }
    } else {
        hit = checkPlayerAttacks_801F2FAC();
    }
    return hit;
}

u8 Terrorpin_804A3B10::attackPlayers_0x68() {
    u8 hit = 0;
    if (mBox.f.x - mBox.i.x > 0.0f && mBox.f.y - mBox.i.y > 0.0f) {
        bool inState = false;
        // Case 1 has its own body: the original keeps it apart from 2 and 3.
        switch (mState) {
        case 1:
            inState = true;
            break;
        case 2:
        case 3:
            inState = true;
            break;
        }
        if (inState) {
            JGeometry::TBox2f box;
            getBoxAtPos_801F31C4(&box);
            fn_802397B4(&box, mLayer_0x4, mSerial);
        } else {
            hit = damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 1);
            if (hit != 0 && mState == 5) {
                for (s32 i = 0; i < 4; i++) {
                    if (hit & (1 << i)) {
                        dropPlayerForceGems_80236074(i, 0x64, 0);
                    }
                }
            }
        }
    }
    return hit;
}

void Terrorpin_804A3B10::die_0x24() {
    // The original reads mLastAttacker_0x10c here, not mState (0x248) as the other vfuncs do.
    if ((mFlags & 0x2000) && (mFlags & 0x4000) && mLastAttacker_0x10c >= 0 && mLastAttacker_0x10c < 4 && mLastAttacker_0x10c == field_0x2a8) {
        f32 speed = 1.5f;
        for (s32 i = 0; i < 2; i++) {
            s32 serial = getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &mPos, 2);
            if (serial >= 0) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (actor != NULL) {
                    f32 x = speed;
                    if (i != 0) {
                        x = -speed;
                    }
                    actor->mVel_0x3c = JGeometry::TVec3f(x, 0.0f, 4.5f);
                }
            }
        }
    }
    ActorBase_804A5770::die_0x24();
}

void Terrorpin_804A3B10::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    if (mFlags & 0x2000) {
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;
        setScaleMtx(mtx, field_0x54.x, field_0x54.y, field_0x54.z);
        params.mMtx = mtx;
    }
    if (mFlags & 0x4000) {
        params.field_0x0[8] = field_0x2a8 + 2;
    }
    drawSprite_801F06F0("enemy_gc_sob.bin", mAnim.mCel + 0xBD, &params, 0, 1.0f, 1.0f);
}

Terrorpin_804A3B10::~Terrorpin_804A3B10() {}
