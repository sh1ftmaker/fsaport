#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/Effect.h"
#include "game/GameView.h"
#include "game/tu_8030AE84.h"
#include "game/ViewLayer.h"
#include "game/BgRewrite.h"
#include "game/Camera.h"
#include "game/Map.h"
#include "JSystem/JParticle/JPAMath.h"
#include "game/cls_804A71A8.h"

// The emitters' y rotation per facing (0x3DC: right, left, down, up), read
// by fn_80290FC0.
static const s32 lbl_8046C228[4] = {0x4000, 0xC000, 0x0000, 0x8000};

// The spin while rising or dropping (updates of 0x2A, 0x2B, 0x22, 0x1F:
// fn_80291B8C, fn_80291910, fn_80293398, fn_80293900): the facing 0x3DC/0x3E8
// turns a quarter every four counts of 0x3FC (down, left, up, right).
inline void Player_804A6F38::spinFacing() {
    s32 turn = (mStateTimer_0x3fc >> 2) & 3;
    if (turn == 0) {
        mFacing_0x3dc = 2;
        mMoveDir_0x3e8 = 2;
    } else if (turn == 1) {
        mFacing_0x3dc = 1;
        mMoveDir_0x3e8 = 1;
    } else if (turn == 2) {
        mFacing_0x3dc = 3;
        mMoveDir_0x3e8 = 3;
    } else {
        mFacing_0x3dc = 0;
        mMoveDir_0x3e8 = 0;
    }
}

// The walk cycle (fn_80292198, fn_802924AC, stepTransitionAnim): 0x3F8
// counts 0-23, the frame 0x3F9 is a quarter of it.
inline void Player_804A6F38::stepWalkCycle() {
    mWalkAnimCounter_0x3f8++;
    if (mWalkAnimCounter_0x3f8 >= 24) {
        mWalkAnimCounter_0x3f8 = 0;
    }
    mWalkFrame_0x3f9 = mWalkAnimCounter_0x3f8 / 6;
}

// Shared by the updates of states 0x2A and 0x22 (fn_80291B8C, fn_80293398):
// the player rises 4 a frame; above 128 the position on the TV is kept
// (0xCB0), and once `state` is granted it lands at 0xCD4 on layer 0xCE4.
inline void Player_804A6F38::riseToTarget(s32 state) {
    mPos.z += 4.0f;
    if (mPos.z > 128.0f) {
        if (mLayer_0x3b4 < 8) {
            mLastTvPos_0xcb0 = mPos;
        }
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(state)) {
            fn_80241454(0);
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mPos.z = 0.0f;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    }
}

// The camera follows a point 150 above the player while it is on the TV
// (fn_802929FC, fn_80292ECC, fn_80293398, fn_802935A4).
inline void Player_804A6F38::followCamera() {
    if (mLayer_0x3b4 < 8) {
        JGeometry::TVec3f pos = mPos;
        pos.y -= 150.0f;
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&pos);
    }
}

// The state after an arrival while carrying (fn_80292820, fn_802935A4): 0x33
// without life, 0x3E with 0xCF0, 0x12 while holding an actor, else 0.
inline bool Player_804A6F38::requestArrival() {
    if (getLife_8024189C() <= 0) {
        return requestState_80251AEC(0x33);
    }
    if (field_0xcf0 == 1) {
        return requestState_80251AEC(0x3E);
    }
    if (isLifting_8025F684()) {
        return requestState_80251AEC(0x12);
    }
    return requestState_80251AEC(0);
}

// Effects 0x15B and 0x15C at the player (fn_80291E44, fn_802925E4).
inline void Player_804A6F38::spawnEffect15B() {
    spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x15B, 0, 1, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x15C, -1, 0, NULL, NULL, true, 1.0f);
}

// The life that decides between landing and state 0x33: the leader's
// (getPlayerLife_80237774 of getLeader_80240644) when hasLeader_80240808 holds, else the player's own.
// fn_80293900 inlines it, and the unwritten fn_80290FC0 twice.
inline s32 Player_804A6F38::life() {
    if (hasLeader_80240808()) {
        return getPlayerLife_80237774(getLeader_80240644());
    }
    return getLife_8024189C();
}

// Gravity for the drops onto the floor below (fn_802953A0, fn_8029513C):
// the fall speed 0x34C grows by 0.25 up to 4, then the player moves by 0x344.
inline void Player_804A6F38::fallStep() {
    mVel_0x344.z -= 0.25f;
    if (mVel_0x344.z < -4.0f) {
        mVel_0x344.z = -4.0f;
    }
    mPos += mVel_0x344;
}

// Effect 0x17D 32 above and 16 over the player (fn_80291910, fn_802935A4,
// fn_80293774).
inline void Player_804A6F38::spawnEffect17D() {
    JGeometry::TVec3f pos;
    pos.set(mPos);
    pos.y += 32.0f;
    pos.z += 16.0f;
    spawnEmitter_8013CC50(mLayer_0x3b4, &pos, 0x17D, 1, 0, NULL, NULL, true, 1.0f);
}

// The animation while walking in or out through an entrance (fn_80293FE4,
// fn_802948C0): the walk cycle with 0xCEB, a 19-count cycle of frames 0-2
// with 0xCEC, the carry step (updateCarryWalk_8026152C) while holding an actor, else
// fn_8027EE84.
inline void Player_804A6F38::stepTransitionAnim() {
    if (field_0xceb == 1) {
        stepWalkCycle();
    } else if (field_0xcec == 1) {
        mWalkAnimCounter_0x3f8++;
        if (mWalkAnimCounter_0x3f8 >= 19) {
            mWalkAnimCounter_0x3f8 = 0;
        }
        if (mWalkAnimCounter_0x3f8 < 7) {
            mWalkFrame_0x3f9 = 0;
        } else if (mWalkAnimCounter_0x3f8 < 12) {
            mWalkFrame_0x3f9 = 1;
        } else {
            mWalkFrame_0x3f9 = 2;
        }
    } else if (isLifting_8025F684()) {
        updateCarryWalk_8026152C(false);
    } else {
        fn_8027EE84();
    }
}

// The held actor's position: mPos raised 16 (raiseHeldActor and the updates
// fn_80292820, fn_802929FC, fn_80292ECC, fn_80293398, fn_80293B30; enter fn_80293E8C).
inline void Player_804A6F38::setHeldPos() {
    mHeldPos_0x2f4.set(mPos);
    mHeldPos_0x2f4.z += 16.0f;
}

// Shared by the enters of states 0x25 and 0x23 (fn_80292944, fn_802932BC),
// which inline the same block: 0x2F4 = mPos raised 16, and while the player
// holds an actor (isLifting_8025F684, serial mHeldActorSerial_0xd68) that actor is warped there
// on the player's layer. The GameView fetch's result is unused in both.
inline void Player_804A6F38::raiseHeldActor() {
    setHeldPos();
    if (isLifting_8025F684()) {
        s32 serial = mHeldActorSerial_0xd68;
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            getGameView_802D6A9C();
            if (actor != NULL) {
                actor->fn_801F349C(mLayer_0x3b4, &mHeldPos_0x2f4);
            }
        }
    }
}

void Player_804A6F38::fn_80295584() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mSubPose_0x3fd = 1;
    mFacing_0x3dc = 2;
    mMoveDir_0x3e8 = 2;
}

void Player_804A6F38::fn_802954DC() {
    if (mStateTimer_0x3fc >= 60) {
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x2D)) {
            if (mLayer_0x3b4 < 8) {
                mLastTvPos_0xcb0 = mPos;
            }
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mPos.z = 128.0f;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    } else {
        mStateTimer_0x3fc++;
    }
}

void Player_804A6F38::fn_802954B8() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mVel_0x344.x = 0.0f;
    mVel_0x344.y = 0.0f;
    mVel_0x344.z = 0.0f;
}

void Player_804A6F38::fn_802953A0() {
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
        return;
    }
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    if (mStateTimer_0x3fc < 60) {
        return;
    }
    fallStep();
    if (mPos.z < 0.0f) {
        mPos.z = 0.0f;
        mStateLocked_0x124d = 0;
        bool requested;
        if (getLife_8024189C() <= 0) {
            requested = requestState_80251AEC(0x33);
        } else {
            requested = requestState_80251AEC(0);
        }
        if (requested) {
            fn_803037D4(mPlayerNo_0x4, 0xD);
        }
    }
}

void Player_804A6F38::fn_80295334() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    if (mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(false);
    }
    mFacing_0x3dc = 2;
    mMoveDir_0x3e8 = 2;
    mPos.z = 0.0f;
}

void Player_804A6F38::fn_80295268() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc >= 10) {
        mPos.z += 4.0f;
        if (mPos.z > 128.0f) {
            if (mLayer_0x3b4 < 8) {
                mLastTvPos_0xcb0 = mPos;
            }
            mStateLocked_0x124d = 0;
            if (requestState_80251AEC(0x2F)) {
                fn_80241454(0);
                mLayer_0x3b4 = mDestLayer_0xce4;
                mPos = mDestPos_0xcd4;
                mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
            }
        }
    }
}

void Player_804A6F38::fn_80295240() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mVel_0x344.x = 0.0f;
    mVel_0x344.y = -0.5f;
    mVel_0x344.z = 0.0f;
}

void Player_804A6F38::fn_8029513C() {
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
        return;
    }
    fn_80241454(1);
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    fallStep();
    if (mPos.z < 0.0f) {
        mPos.z = 0.0f;
        mStateLocked_0x124d = 0;
        if (getLife_8024189C() <= 0) {
            requestState_80251AEC(0x33);
        } else {
            requestState_80251AEC(0);
        }
    }
}

void Player_804A6F38::fn_80294ADC() {
    if (mPrevState_0x1248 == 0x4A || mPrevState_0x1248 == 0x4B) {
        field_0xceb = 1;
        field_0xcec = 0;
    } else if (mPrevState_0x1248 == 0xB) {
        field_0xceb = 0;
        field_0xcec = 1;
    } else {
        field_0xceb = 0;
        field_0xcec = 0;
    }
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mPos.z = 0.0f;
    mWalkStart_0x398.set(mPos);
    mWalkStart_0x398.z = 0.0f;
    mWalkTarget_0x3a4.set(mPos);
    mWalkTarget_0x3a4.z = 0.0f;
    if (mTransitionKind_0xcf4 == 0x15) {
        if (viewLayerFlag(mLayer_0x3b4)) {
            if (mLayer_0x3b4 < 8) {
                mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -1.25f, 0.0f);
                mFacing_0x3dc = 3;
                mMoveDir_0x3e8 = 3;
                playSound_8024F0E8(0x20018);
            } else {
                mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 1.25f, 0.0f);
                mFacing_0x3dc = 2;
                mMoveDir_0x3e8 = 2;
                playSound_8024F0E8(0x2001A);
            }
        } else if (mLayer_0x3b4 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
            mFacing_0x3dc = 3;
            mMoveDir_0x3e8 = 3;
            playSound_8024F0E8(0x20018);
        } else if (mDestLayer_0xce4 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
            playSound_8024F0E8(0x2001A);
        } else {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
            mFacing_0x3dc = 3;
            mMoveDir_0x3e8 = 3;
            playSound_8024F0E8(0x20018);
        }
    } else if (mTransitionKind_0xcf4 == 0x23) {
        if (viewLayerFlag(mLayer_0x3b4)) {
            if (mLayer_0x3b4 < 8) {
                mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 1.25f, 0.0f);
                mFacing_0x3dc = 2;
                mMoveDir_0x3e8 = 2;
                playSound_8024F0E8(0x2001A);
            } else {
                mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -1.25f, 0.0f);
                mFacing_0x3dc = 3;
                mMoveDir_0x3e8 = 3;
                playSound_8024F0E8(0x20018);
            }
        } else if (mLayer_0x3b4 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
            playSound_8024F0E8(0x2001A);
        } else if (mDestLayer_0xce4 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
            mFacing_0x3dc = 3;
            mMoveDir_0x3e8 = 3;
            playSound_8024F0E8(0x20018);
        } else {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
            playSound_8024F0E8(0x2001A);
        }
    } else if (mTransitionKind_0xcf4 == 0x17) {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(20.0f, 0.0f, 0.0f);
        mFacing_0x3dc = 0;
        mMoveDir_0x3e8 = 0;
        if (field_0xceb == 0) {
            playSound_8024F0E8(0x2001A);
        }
    } else if (mTransitionKind_0xcf4 == 0x18) {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(-20.0f, 0.0f, 0.0f);
        mFacing_0x3dc = 1;
        mMoveDir_0x3e8 = 1;
        if (field_0xceb == 0) {
            playSound_8024F0E8(0x2001A);
        }
    } else if (mTransitionKind_0xcf4 == 1 || mTransitionKind_0xcf4 == 2) {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
        mFacing_0x3dc = 2;
        mMoveDir_0x3e8 = 2;
        if (field_0xceb == 0) {
            playSound_8024F0E8(0x2001A);
        }
    } else {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
        mFacing_0x3dc = 3;
        mMoveDir_0x3e8 = 3;
        if (field_0xceb == 0) {
            playSound_8024F0E8(0x20018);
        }
    }
    if (mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(false);
    }
    mAlpha_0xca0 = 0xFF;
}

void Player_804A6F38::fn_802948C0() {
    fn_8027E5BC(0.5f);
    mStateTimer_0x3fc++;
    if ((mWalkTarget_0x3a4 - mPos).isZero() || mStateTimer_0x3fc >= 50) {
        if (mLayer_0x3b4 < 8) {
            mLastTvPos_0xcb0 = mPos;
        }
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x1D)) {
            fn_80241454(0);
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    } else if (mTransitionKind_0xcf4 == 2) {
        fn_80265D68();
    }
    if (mTransitionKind_0xcf4 == 2) {
        fn_80268B3C();
    }
    stepTransitionAnim();
}

void Player_804A6F38::fn_80294374() {
    mWalkStart_0x398.set(mPos);
    mWalkStart_0x398.z = 0.0f;
    mWalkTarget_0x3a4.set(mPos);
    mWalkTarget_0x3a4.z = 0.0f;
    mStateLocked_0x124d = 1;
    if (mTransitionKind_0xcf4 == 0x15) {
        if (viewLayerFlag(mLayer_0x3b4)) {
            if (mLayer_0x3b4 < 8) {
                mWalkStart_0x398 += JGeometry::TVec3f(0.0f, -1.25f, 0.0f);
                mPos.y -= 1.25f;
                mFacing_0x3dc = 2;
                mMoveDir_0x3e8 = 2;
            } else {
                mWalkStart_0x398 += JGeometry::TVec3f(0.0f, 1.25f, 0.0f);
                mPos.y += 1.25f;
                mFacing_0x3dc = 3;
                mMoveDir_0x3e8 = 3;
            }
        } else if (mLayer_0x3b4 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
        } else if (mDepartLayer_0xce0 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
            mFacing_0x3dc = 3;
            mMoveDir_0x3e8 = 3;
        } else {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
        }
    } else if (mTransitionKind_0xcf4 == 0x23) {
        if (viewLayerFlag(mLayer_0x3b4)) {
            if (mLayer_0x3b4 < 8) {
                mWalkStart_0x398 += JGeometry::TVec3f(0.0f, 1.25f, 0.0f);
                mPos.y += 1.25f;
                mFacing_0x3dc = 3;
                mMoveDir_0x3e8 = 3;
            } else {
                mWalkStart_0x398 += JGeometry::TVec3f(0.0f, -1.25f, 0.0f);
                mPos.y -= 1.25f;
                mFacing_0x3dc = 2;
                mMoveDir_0x3e8 = 2;
            }
        } else if (mLayer_0x3b4 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
            mFacing_0x3dc = 3;
            mMoveDir_0x3e8 = 3;
        } else if (mDepartLayer_0xce0 < 8) {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
        } else {
            mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
            mFacing_0x3dc = 3;
            mMoveDir_0x3e8 = 3;
        }
    } else if (mTransitionKind_0xcf4 == 0 || mTransitionKind_0xcf4 == 3) {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, -20.0f, 0.0f);
        mFacing_0x3dc = 3;
        mMoveDir_0x3e8 = 3;
    } else if (mTransitionKind_0xcf4 == 0x17) {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(20.0f, 0.0f, 0.0f);
        mFacing_0x3dc = 0;
        mMoveDir_0x3e8 = 0;
    } else if (mTransitionKind_0xcf4 == 0x18) {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(-20.0f, 0.0f, 0.0f);
        mFacing_0x3dc = 1;
        mMoveDir_0x3e8 = 1;
    } else {
        mWalkTarget_0x3a4 += JGeometry::TVec3f(0.0f, 20.0f, 0.0f);
        mFacing_0x3dc = 2;
        mMoveDir_0x3e8 = 2;
    }
    field_0x458 = 0;
    if (mTransitionKind_0xcf4 == 1) {
        gBgRewrite_8053AC98->fn_8029E508(&mPos, mPlayerNo_0x4, mLayer_0x3b4);
    }
    if (mTransitionKind_0xcf4 == 3) {
        mAlpha_0xca0 = 0;
    } else {
        mAlpha_0xca0 = 0xFF;
    }
}

void Player_804A6F38::fn_80293FE4() {
    if (mStateTimer_0x3fc < 30) {
        mStateTimer_0x3fc++;
        return;
    }
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    if (mStateTimer_0x3fc == 30) {
        if (mWalkTarget_0x3a4.y > mWalkStart_0x398.y) {
            if (mLayer_0x3b4 < 8) {
                if (field_0xceb == 0) {
                    playSound_8024F0E8(0x2001B);
                }
                playSound_8024F0E8(0x20021);
            } else {
                fn_803037D4(mPlayerNo_0x4, 0xD);
            }
        } else {
            if (mLayer_0x3b4 < 8) {
                if (field_0xceb == 0) {
                    playSound_8024F0E8(0x20019);
                }
                playSound_8024F0E8(0x20021);
            } else {
                fn_803037D4(mPlayerNo_0x4, 0xD);
            }
        }
    }
    if (mTransitionKind_0xcf4 == 3) {
        fn_80265D8C();
    }
    mStateTimer_0x3fc++;
    fn_80241454(1);
    fn_8027E5BC(0.5f);
    if ((mWalkTarget_0x3a4 - mPos).isZero() || mStateTimer_0x3fc >= 80) {
        mStateLocked_0x124d = 0;
        if (getLife_8024189C() <= 0) {
            requestState_80251AEC(0x33);
        } else if (field_0xceb == 1) {
            if (viewLayerFlag(mLayer_0x3b4)) {
                requestState_80251AEC(0x4B);
            } else {
                requestState_80251AEC(0x4A);
            }
        } else if (field_0xcec == 1) {
            requestState_80251AEC(0xB);
        } else if (isLifting_8025F684()) {
            requestState_80251AEC(0x12);
        } else {
            requestState_80251AEC(0);
        }
    }
    if (!((mTransitionKind_0xcf4 == 0x15 || mTransitionKind_0xcf4 == 0x23) && viewLayerFlag(mLayer_0x3b4))) {
        fn_80268B3C();
    }
    stepTransitionAnim();
}

void Player_804A6F38::fn_80293E8C() {
    mStateLocked_0x124d = 1;
    mPos.z = 0.0f;
    field_0x458 = 0;
    playSound_8024F0E8(0x10003);
    playSound_8024F0E8(0x2002D);
    s32 layer = mLayer_0x3b4;
    if (layer > 8 && mDestLayer_0xce4 > 8 && viewLayerFlag(layer) && viewLayerFlag(mDestLayer_0xce4) &&
        mDestLayer_0xce4 != mLayer_0x3b4)
    {
        mStateTimer_0x3fc = 58;
    }
    setHeldPos();
    if (mTransitionKind_0xcf4 == 0x24 && mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(false);
    }
    if (fn_802486A0() != 4) {
        if (fn_802486A0() != 5) {
            setGroupMode_80248004(5, true);
        }
        resetPosHistory_8024BAEC();
    }
}

void Player_804A6F38::fn_80293B30() {
    if (mStateTimer_0x3fc >= 58) {
        mStateTimer_0x3fc = 58;
        mSubPose_0x3fd = 0;
        mPose_0x3fb = 1;
        fn_80241454(0);
        if (!fn_80301CD0(mPlayerNo_0x4)) {
            if (fn_80240670()) {
                if (fn_8022D75C(fn_8024064C())) {
                    return;
                }
                if (mTransitionKind_0xcf4 == 0x12) {
                    mDestPos_0xcd4 = *getPlayerPos_8023E724(fn_8024064C());
                    mDestLayer_0xce4 = getPlayerLayer_8023DE58(fn_8024064C());
                }
            }
            if (mLayer_0x3b4 < 8) {
                mLastTvPos_0xcb0 = mPos;
            }
            mStateLocked_0x124d = 0;
            if (requestState_80251AEC(0x1F)) {
                if (fn_80241E28(mDestLayer_0xce4, &mDestPos_0xcd4)) {
                    if (fn_802486A0() == 5 && hasLeader_80240808()) {
                        mDestPos_0xcd4.set(*getPlayerPos_8023E724(getLeader_80240644()));
                    }
                    u16 tile = getTileCollisionCode_8022461C(&mDestPos_0xcd4, mDestLayer_0xce4);
                    if (fn_802265DC(&mDestPos_0xcd4, mDestLayer_0xce4, true) || tile == 3 || tile == 0xE ||
                        fn_802684A4(&mDestPos_0xcd4, mDestLayer_0xce4))
                    {
                        if (mDestLayer_0xce4 < 8) {
                            mDestPos_0xcd4.set(mReturnPos_0x2c4);
                        } else {
                            mDestPos_0xcd4.set(mRespawnPos_0xcbc);
                        }
                    }
                }
                mLayer_0x3b4 = mDestLayer_0xce4;
                mPos = mDestPos_0xcd4;
                // hitOwnGroup_80272160 reads only x and y of the position.
                if (fn_802DEABC() && mLayer_0x3b4 <= 8 && (mTransitionKind_0xcf4 == 9 || mTransitionKind_0xcf4 == 0x12)) {
                    if (!fn_80240670()) {
                        hitOwnGroup_80272160(2, (const JGeometry::TVec2f*)&mPos, 0x11);
                    }
                } else if (mLayer_0x3b4 > 8 && (mTransitionKind_0xcf4 == 9 || mTransitionKind_0xcf4 == 0x12) && !fn_80240670()) {
                    hitOwnGroup_80272160(2, (const JGeometry::TVec2f*)&mPos, 0x11);
                }
                if (mTransitionKind_0xcf4 == 9) {
                    mPos.z = 0.0f;
                } else if (mTransitionKind_0xcf4 == 0x24) {
                    mPos.z = 128.0f;
                } else if (viewLayerFlag(mLayer_0x3b4)) {
                    mPos.z = 0.0f;
                } else if (mDestLayer_0xce4 >= 8) {
                    mPos.z = 128.0f;
                } else {
                    mPos.z = 0.0f;
                }
                mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
            }
        }
    }
    fn_8027EDDC();
    setHeldPos();
}

void Player_804A6F38::fn_80293B1C() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
}

void Player_804A6F38::fn_80293900() {
    if (mStateTimer_0x3fc < 60) {
        if (fn_80240670()) {
            mStateTimer_0x3fc = 60;
        } else {
            mStateTimer_0x3fc++;
        }
    }
    if (!fn_80240670()) {
        if (mLayer_0x3b4 != 0 && mDestLayer_0xce4 != mDepartLayer_0xce0 && fn_80303750(mPlayerNo_0x4)) {
            return;
        }
        if (mStateTimer_0x3fc < 60) {
            return;
        }
    }
    if (mStateTimer_0x3fc == 60) {
        if (mLayer_0x3b4 < 8) {
            playSound_8024F0E8(0x20021);
        } else {
            fn_803037D4(mPlayerNo_0x4, 0xD);
        }
    }
    mStateTimer_0x3fc++;
    fn_80241454(1);
    mPos.z -= 4.0f;
    if (mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        mStateLocked_0x124d = 0;
        if (life() <= 0) {
            requestState_80251AEC(0x33);
        } else if (isLifting_8025F684()) {
            if (requestState_80251AEC(0x12) && mTransitionKind_0xcf4 == 9) {
                field_0x464 = 180;
            }
        } else if (requestState_80251AEC(0) && mTransitionKind_0xcf4 == 9) {
            field_0x464 = 180;
        }
    }
    spinFacing();
}

void Player_804A6F38::fn_802938B8() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mSubPose_0x3fd = 1;
    if (mTransitionKind_0xcf4 != 0x1D) {
        playSound_8024F0E8(0x2000C);
    }
}

void Player_804A6F38::fn_80293774() {
    if (mStateTimer_0x3fc == 60) {
        if (mTransitionKind_0xcf4 != 0x1D) {
            if (mLayer_0x3b4 < 8) {
                spawnEffect17D();
            } else if (mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
                fn_8024EFA0(false);
            }
        }
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc >= 118) {
        if (mLayer_0x3b4 < 8) {
            mLastTvPos_0xcb0 = mPos;
        }
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x21)) {
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    }
    mStateTimer_0x3fc++;
}

void Player_804A6F38::fn_80293760() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
}

void Player_804A6F38::fn_802935A4() {
    followCamera();
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
    }
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    if (mStateTimer_0x3fc < 60) {
        return;
    }
    mStateLocked_0x124d = 0;
    if (requestArrival()) {
        if (mLayer_0x3b4 < 8) {
            spawnEffect17D();
            playSound_8024F0E8(0x20020);
            playSound_8024F0E8(0x20021);
        } else {
            fn_803037D4(mPlayerNo_0x4, 0xD);
        }
    }
}

void Player_804A6F38::fn_80293534() {
    mStateLocked_0x124d = 1;
    mPos.z = 0.0f;
    field_0x458 = 0;
    playSound_8024F0E8(0x2002E);
    if (mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(false);
    }
}

void Player_804A6F38::fn_80293398() {
    followCamera();
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc >= 60 || mTransitionKind_0xcf4 == 0x1F) {
        riseToTarget(0x23);
    }
    spinFacing();
    setHeldPos();
}

void Player_804A6F38::fn_802932BC() {
    mStateLocked_0x124d = 1;
    mPos.z = 0.0f;
    field_0x458 = 0;
    mVel_0x344.set(0.0f, -0.5f, 4.0f);
    raiseHeldActor();
    if (mTransitionKind_0xcf4 == 0x1F) {
        mStateTimer_0x3fc = 50;
    }
}

void Player_804A6F38::fn_80292ECC() {
    followCamera();
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
    }
    if (mDestLayer_0xce4 == mDepartLayer_0xce0 || fn_80240670() || !fn_80303750(mPlayerNo_0x4)) {
        if (mStateTimer_0x3fc >= 60) {
            if (mStateTimer_0x3fc == 60) {
                if (mDepartLayer_0xce0 >= 8) {
                    spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x545, 1, 1, NULL, NULL, true, 1.0f);
                    switch (mPlayerNo_0x4) {
                    case 0:
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x140, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x141, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x142, 1, 1, NULL, NULL, true, 1.0f);
                        break;
                    case 1:
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x147, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x148, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x149, 1, 1, NULL, NULL, true, 1.0f);
                        break;
                    case 2:
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x13D, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x13E, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x13F, 1, 1, NULL, NULL, true, 1.0f);
                        break;
                    case 3:
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x144, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x145, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x146, 1, 1, NULL, NULL, true, 1.0f);
                        break;
                    }
                    spawnEmitter_8013CC50(mDestLayer_0xce4, &mDestPos_0xcd4, 0x143, 1, 1, NULL, NULL, true, 1.0f);
                    playSound_8024F0E8(0x20020);
                }
                playSound_8024F0E8(0x2002F);
                playSound_8024F0E8(0x20021);
            }
            mStateTimer_0x3fc++;
            if (mStateTimer_0x3fc >= 171) {
                mPos.z = 0.0f;
                mStateLocked_0x124d = 0;
                if (isLifting_8025F684()) {
                    requestState_80251AEC(0x12);
                } else {
                    requestState_80251AEC(0);
                }
                mFacing_0x3dc = 2;
                mMoveDir_0x3e8 = 2;
                fn_80241454(1);
            }
        }
    }
    setHeldPos();
}

void Player_804A6F38::fn_80292E90() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mSubPose_0x3fd = 1;
    playSound_8024F0E8(0x2000C);
}

void Player_804A6F38::fn_802929FC() {
    followCamera();
    if (mStateTimer_0x3fc == 60) {
        if (mTransitionKind_0xcf4 != 0x1E && mTransitionKind_0xcf4 != 0x1F) {
            spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x546, 1, 1, NULL, NULL, true, 1.0f);
            switch (mPlayerNo_0x4) {
            case 0:
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x14E, 1, 1, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x14F, 1, 1, NULL, NULL, true, 1.0f);
                if (fn_80241544() >= 2) {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x151, 1, 1, NULL, NULL, true, 1.0f);
                } else {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x150, 1, 1, NULL, NULL, true, 1.0f);
                }
                break;
            case 1:
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x157, 1, 1, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x158, 1, 1, NULL, NULL, true, 1.0f);
                if (fn_80241544() >= 2) {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x15A, 1, 1, NULL, NULL, true, 1.0f);
                } else {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x159, 1, 1, NULL, NULL, true, 1.0f);
                }
                break;
            case 2:
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x14A, 1, 1, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x14B, 1, 1, NULL, NULL, true, 1.0f);
                if (fn_80241544() >= 2) {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x14D, 1, 1, NULL, NULL, true, 1.0f);
                } else {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x14C, 1, 1, NULL, NULL, true, 1.0f);
                }
                break;
            case 3:
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x153, 1, 1, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x154, 1, 1, NULL, NULL, true, 1.0f);
                if (fn_80241544() >= 2) {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x156, 1, 1, NULL, NULL, true, 1.0f);
                } else {
                    spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x155, 1, 1, NULL, NULL, true, 1.0f);
                }
                break;
            }
            spawnEmitter_8013CC50(mDepartLayer_0xce0, &mDepartPos_0xcc8, 0x152, 1, 1, NULL, NULL, true, 1.0f);
        }
        mSubPose_0x3fd = 0;
        fn_80241454(0);
    } else if (mStateTimer_0x3fc >= 118) {
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x25)) {
            if (mLayer_0x3b4 < 8) {
                mLastTvPos_0xcb0 = mPos;
            }
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    }
    mStateTimer_0x3fc++;
    setHeldPos();
}

void Player_804A6F38::fn_80292944() {
    fn_80241454(1);
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    raiseHeldActor();
}

void Player_804A6F38::fn_80292820() {
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
    }
    if (mDestLayer_0xce4 == mDepartLayer_0xce0 || fn_80240670() || !fn_80303750(mPlayerNo_0x4)) {
        if (mStateTimer_0x3fc >= 60) {
            mStateLocked_0x124d = 0;
            if (requestArrival()) {
                fn_803037D4(mPlayerNo_0x4, 0xD);
            }
        }
    }
    setHeldPos();
}

void Player_804A6F38::fn_802927B0() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mPose_0x3fb = 0;
    playSound_8024F0E8(0x20060);
    spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x547, -1, 0, NULL, NULL, true, 1.0f);
}

void Player_804A6F38::fn_802925E4() {
    if (mWalkAnimCounter_0x3f8 < 8) {
        mWalkFrame_0x3f9 = 0;
    } else if (mWalkAnimCounter_0x3f8 < 14) {
        mWalkFrame_0x3f9 = 1;
    } else if (mWalkAnimCounter_0x3f8 < 20) {
        mWalkFrame_0x3f9 = 2;
    } else if (mWalkAnimCounter_0x3f8 < 26) {
        mWalkFrame_0x3f9 = 3;
    } else if (mWalkAnimCounter_0x3f8 < 30) {
        mWalkFrame_0x3f9 = 4;
    } else if (mWalkAnimCounter_0x3f8 < 34) {
        mWalkFrame_0x3f9 = 5;
    } else if (mWalkAnimCounter_0x3f8 < 38) {
        mWalkFrame_0x3f9 = 6;
    } else if (mWalkAnimCounter_0x3f8 < 42) {
        mWalkFrame_0x3f9 = 7;
    } else if (mWalkAnimCounter_0x3f8 < 46) {
        mWalkFrame_0x3f9 = 8;
    } else if (mWalkAnimCounter_0x3f8 < 48) {
        mWalkFrame_0x3f9 = 9;
    } else if (mWalkAnimCounter_0x3f8 < 50) {
        mWalkFrame_0x3f9 = 10;
    } else {
        mWalkFrame_0x3f9 = 11;
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x27)) {
            spawnEffect15B();
            if (mLayer_0x3b4 < 8) {
                mLastTvPos_0xcb0 = mPos;
            }
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    }
    mWalkAnimCounter_0x3f8++;
}

void Player_804A6F38::fn_802925C4() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mFacing_0x3dc = 2;
    mMoveDir_0x3e8 = 2;
}

void Player_804A6F38::fn_802924AC() {
    stepWalkCycle();
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
    }
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    if (mStateTimer_0x3fc < 60) {
        return;
    }
    if (mStateTimer_0x3fc >= 92) {
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x4B) && mDestLayer_0xce4 >= 8) {
            fn_803037D4(mPlayerNo_0x4, 0xD);
        }
    } else if (mStateTimer_0x3fc < 92) { // the original tests it again (a dead bge)
        mPos.y += 1.0;
        mStateTimer_0x3fc++;
    }
}

void Player_804A6F38::fn_80292454() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mFacing_0x3dc = 3;
    mMoveDir_0x3e8 = 3;
    if (mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(0);
    }
}

void Player_804A6F38::fn_802922EC() {
    mWalkAnimCounter_0x3f8++;
    if (mWalkAnimCounter_0x3f8 < 7) {
        mWalkFrame_0x3f9 = 4;
    } else if (mWalkAnimCounter_0x3f8 < 13) {
        mWalkFrame_0x3f9 = 5;
    } else if (mWalkAnimCounter_0x3f8 < 19) {
        mWalkFrame_0x3f9 = 6;
    } else if (mWalkAnimCounter_0x3f8 < 34) {
        mWalkFrame_0x3f9 = 7;
    } else if (mWalkAnimCounter_0x3f8 < 40) {
        mWalkFrame_0x3f9 = 4;
    } else if (mWalkAnimCounter_0x3f8 < 46) {
        mWalkFrame_0x3f9 = 5;
    } else if (mWalkAnimCounter_0x3f8 < 52) {
        mWalkFrame_0x3f9 = 6;
    } else if (mWalkAnimCounter_0x3f8 < 67) {
        mWalkFrame_0x3f9 = 7;
    }
    if (mWalkAnimCounter_0x3f8 < 67) {
        mPos.y -= 0.5;
    } else {
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(0x29)) {
            fn_80241454(0);
            if (mLayer_0x3b4 < 8) {
                mLastTvPos_0xcb0 = mPos;
            }
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        }
    }
}

void Player_804A6F38::fn_802922D0() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mPose_0x3fb = 1;
    mWalkFrame_0x3f9 = 0;
}

void Player_804A6F38::fn_80292198() {
    stepWalkCycle();
    if (mStateTimer_0x3fc == 2) {
        fn_80241454(1);
    }
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
    }
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    if (mStateTimer_0x3fc < 60) {
        return;
    }
    if (mPose_0x3fb == 0) {
        if (field_0x400 != 0) {
            field_0x400--;
        } else {
            mStateLocked_0x124d = 0;
            if (requestState_80251AEC(0x4A) && mDestLayer_0xce4 >= 8) {
                fn_803037D4(mPlayerNo_0x4, 0xD);
            }
        }
    } else {
        mPose_0x3fb = 0;
        field_0x400 = 8;
    }
}

void Player_804A6F38::fn_80292124() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mFacing_0x3dc = 3;
    mMoveDir_0x3e8 = 3;
    if (!fn_80240670() && mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(0);
    }
}

void Player_804A6F38::fn_80291F10() {
    mWalkAnimCounter_0x3f8++;
    if (mWalkAnimCounter_0x3f8 == 4 || mWalkAnimCounter_0x3f8 == 8) {
        s32 index = mPlayerNo_0x4;
        getActorManager_801F856C()->spawnActor_801F9484('PEFF', mLayer_0x3b4, &mPos, (index & 3) | 0xC, -1, index);
    }
    if (mWalkAnimCounter_0x3f8 <= 4) {
        mWalkFrame_0x3f9 = 0;
    } else if (mWalkAnimCounter_0x3f8 < 19) {
        mWalkFrame_0x3f9 = 1;
    } else if (mWalkAnimCounter_0x3f8 < 23) {
        mWalkFrame_0x3f9 = 2;
    } else if (mWalkAnimCounter_0x3f8 < 27) {
        mWalkFrame_0x3f9 = 3;
    } else if (mWalkAnimCounter_0x3f8 < 31) {
        mWalkFrame_0x3f9 = 4;
    } else if (mWalkAnimCounter_0x3f8 < 35) {
        mWalkFrame_0x3f9 = 5;
    } else if (mWalkAnimCounter_0x3f8 < 39) {
        mWalkFrame_0x3f9 = 2;
    } else if (mWalkAnimCounter_0x3f8 < 43) {
        mWalkFrame_0x3f9 = 3;
    } else if (mWalkAnimCounter_0x3f8 < 47) {
        mWalkFrame_0x3f9 = 4;
    } else if (mWalkAnimCounter_0x3f8 < 51) {
        mWalkFrame_0x3f9 = 5;
    } else if (mWalkAnimCounter_0x3f8 < 55) {
        mWalkFrame_0x3f9 = 2;
    } else if (mWalkAnimCounter_0x3f8 < 59) {
        mWalkFrame_0x3f9 = 3;
    } else if (mWalkAnimCounter_0x3f8 < 63) {
        mWalkFrame_0x3f9 = 4;
    } else if (mWalkAnimCounter_0x3f8 < 67) {
        mWalkFrame_0x3f9 = 5;
    }
    if (mWalkAnimCounter_0x3f8 < 67) {
        mPos.y -= 0.1;
    } else {
        mWalkAnimCounter_0x3f8 = 67;
        if (!fn_80301CD0(mPlayerNo_0x4)) {
            mStateLocked_0x124d = 0;
            if (requestState_80251AEC(0x31)) {
                mLayer_0x3b4 = mDestLayer_0xce4;
                mPos = mDestPos_0xcd4;
                mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
            }
        }
    }
}

void Player_804A6F38::fn_80291E44() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mWalkFrame_0x3f9 = 0;
    mFacing_0x3dc = 2;
    mMoveDir_0x3e8 = 2;
    if (mDestLayer_0xce4 < 8) {
        spawnEffect15B();
        playSound_8024F0E8(0x20041);
        playSound_8024F0E8(0x1000F);
        field_0x430 = 0x5C;
    }
}

void Player_804A6F38::fn_80291D28() {
    if (fn_80301CD0(mPlayerNo_0x4)) {
        return;
    }
    if (mDestLayer_0xce4 < 8) {
        if (field_0x430 > 0) {
            field_0x430--;
        }
        mPose_0x3fb = 3;
        if (field_0x430 <= 90) {
            if (field_0x430 == 90) {
                fn_80241454(1);
            } else if (field_0x430 > 84) {
                mWalkFrame_0x3f9 = (90 - field_0x430) / 2;
            } else {
                mWalkFrame_0x3f9 = ((84 - field_0x430) / 6 & 3) + 3;
            }
        }
        if (field_0x430 == 0) {
            mStateLocked_0x124d = 0;
            field_0x44c = 5;
            requestState_80251AEC(0x4A);
        }
    } else if (mWalkAnimCounter_0x3f8 < 60) {
        mWalkAnimCounter_0x3f8++;
    } else {
        mStateLocked_0x124d = 0;
        requestState_80251AEC(0);
    }
}

void Player_804A6F38::fn_80291CB8() {
    mStateLocked_0x124d = 1;
    mPos.z = 0.0f;
    field_0x458 = 0;
    playSound_8024F0E8(0x2002E);
    if (mLayer_0x3b4 >= 8 && mDestLayer_0xce4 < 8) {
        fn_8024EFA0(false);
    }
}

void Player_804A6F38::fn_80291B8C() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc >= 60) {
        riseToTarget(0x2B);
    }
    spinFacing();
}

void Player_804A6F38::fn_80291B64() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    mVel_0x344.x = 0.0f;
    mVel_0x344.y = 0.0f;
    mVel_0x344.z = 4.0f;
}

void Player_804A6F38::fn_80291910() {
    if (mStateTimer_0x3fc < 60) {
        mStateTimer_0x3fc++;
    }
    if (mDestLayer_0xce4 != mDepartLayer_0xce0 && !fn_80240670() && fn_80303750(mPlayerNo_0x4)) {
        return;
    }
    if (mStateTimer_0x3fc < 60) {
        return;
    }
    if (mStateTimer_0x3fc == 60) {
        if (mDepartLayer_0xce0 == 8) {
            spawnEffect17D();
            playSound_8024F0E8(0x20020);
        }
        playSound_8024F0E8(0x2002F);
        playSound_8024F0E8(0x20021);
        fn_80241454(1);
    }
    mPos += mVel_0x344;
    mVel_0x344.z -= 0.25f;
    if (mVel_0x344.z < -4.0f) {
        mVel_0x344.z = -4.0f;
    }
    if (mVel_0x344.z < 0.0f && mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        mStateLocked_0x124d = 0;
        bool requested;
        if (getLife_8024189C() <= 0) {
            requested = requestState_80251AEC(0x33);
        } else if (field_0xcf0 == 1) {
            requested = requestState_80251AEC(0x3E);
        } else {
            requested = requestState_80251AEC(0);
        }
        if (requested == true) {
            mFacing_0x3dc = 2;
            mMoveDir_0x3e8 = 2;
        }
    } else {
        mStateTimer_0x3fc++;
        spinFacing();
    }
}

void Player_804A6F38::fn_8029185C() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    playSound_8024F0E8(0x10003);
    if (mTransitionKind_0xcf4 == 0xD) {
        playSound_8024F0E8(0x20067);
        mStateTimer_0x3fc = 40;
    } else if (mTransitionKind_0xcf4 == 0x26) {
        mStateTimer_0x3fc = 58;
    }
    if (fn_802486A0() != 4 && fn_802486A0() != 5) {
        setGroupMode_80248004(5, true);
    }
    resetPosHistory_8024BAEC();
}

// FIXME: 99.62 %. Scheduling only: the original's lwz r4, 0x135c is scheduled at
// +0x7B0 (we emit it at +0x7B8), and the li r5, 0 / li r3, 0 pair at +0x840 is in the opposite order.
void Player_804A6F38::fn_80290FC0() {
    if (mStateTimer_0x3fc == 40) {
        if (mTransitionKind_0xcf4 == 0xC && fn_802DE9A0()) {
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x2AA, 0, 1, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x2A9, -1, 0, NULL, NULL, true, 1.0f);
        } else if (mTransitionKind_0xcf4 == 0xD) {
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x45F, 0, 1, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x45E, -1, 0, NULL, NULL, true, 1.0f);
            switch (mPlayerNo_0x4) {
            case 0:
                spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x16B, 0, 1, NULL, NULL, true, 1.0f);
                break;
            case 1:
                spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x16D, 0, 1, NULL, NULL, true, 1.0f);
                break;
            case 2:
                spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x16A, 0, 1, NULL, NULL, true, 1.0f);
                break;
            case 3:
                spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x16C, 0, 1, NULL, NULL, true, 1.0f);
                break;
            }
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x16E, 0, 1, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x16F, 0, 1, NULL, NULL, true, 1.0f);
        }
    }
    if (mTransitionKind_0xcf4 == 0xD) {
        if (mStateTimer_0x3fc < 130) {
            mStateTimer_0x3fc++;
            return;
        }
        if (!fn_80240670()) {
            hitOwnGroup_80272160(2, (const JGeometry::TVec2f*)&mPos, 0x11);
        }
        s32 hp = life();
        s32 state = 0;
        if (hp <= 0) {
            state = 0x33;
        }
        mStateLocked_0x124d = 0;
        if (requestState_80251AEC(state)) {
            mLayer_0x3b4 = mDestLayer_0xce4;
            mPos = mDestPos_0xcd4;
            mPos.z = 0.0f;
        }
        return;
    }
    if (mStateTimer_0x3fc >= 58) {
        if (mPose_0x3fb == 0) {
            playSound_8024F0E8(0x1000A);
            if (mTransitionKind_0xcf4 == 0xC && fn_802DE9A0()) {
                playSound_8024F0E8(0x20031);
            }
            mPose_0x3fb = 1;
            if (mTransitionKind_0xcf4 == 0xC && fn_802DE9A0() && field_0x135c == NULL) {
                switch (mFacing_0x3dc) {
                case 0:
                    field_0x135c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x248, 0, 0, NULL, NULL, true, 1.0f);
                    break;
                case 1:
                    field_0x135c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x247, 0, 0, NULL, NULL, true, 1.0f);
                    break;
                case 2:
                    field_0x135c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x246, 0, -1, NULL, NULL, true, 1.0f);
                    break;
                case 3:
                    field_0x135c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x249, 0, 0, NULL, NULL, true, 1.0f);
                    break;
                }
            }
            if (mTransitionKind_0xcf4 == 0xC && fn_802DE9A0() && field_0x132c == NULL) {
                switch (mFacing_0x3dc) {
                case 0:
                    field_0x132c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x244, 0, 0, NULL, &field_0x1330, true, 1.0f);
                    break;
                case 1:
                    field_0x132c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x243, 0, 0, NULL, &field_0x1330, true, 1.0f);
                    break;
                case 2:
                    field_0x132c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x242, 0, -1, NULL, &field_0x1330, true, 1.0f);
                    break;
                case 3:
                    field_0x132c = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x245, 0, 0, NULL, &field_0x1330, true, 1.0f);
                    break;
                }
            }
            JGeometry::TVec3f velocity = (mDestPos_0xcd4 - mPos) * (1.0f / 64.0f);
            mVel_0x344.set(velocity.x, velocity.y, 8.0f);
        }
        if (mStateTimer_0x3fc < 62) {
            mSubPose_0x3fd = 0;
        } else if (mStateTimer_0x3fc < 66) {
            mSubPose_0x3fd = 1;
        } else if (mStateTimer_0x3fc < 70) {
            mSubPose_0x3fd = 2;
        } else if (mStateTimer_0x3fc < 74) {
            mSubPose_0x3fd = 3;
        }
        mStateTimer_0x3fc++;
        mPos += mVel_0x344;
        mVel_0x344.z -= 0.25f;
        if (mVel_0x344.z < 0.0f && mPos.z <= 0.0f) {
            mPos.z = 0.0f;
            if (!fn_80240670() && (mTransitionKind_0xcf4 == 0xC || mTransitionKind_0xcf4 == 0xD || mTransitionKind_0xcf4 == 0x26)) {
                hitOwnGroup_80272160(2, (const JGeometry::TVec2f*)&mPos, 0x11);
            } else {
                hitOwnGroup_80272160(0, (const JGeometry::TVec2f*)&mPos, 0x11);
            }
            s32 hp = life();
        s32 state = 0;
        if (hp <= 0) {
            state = 0x33;
        }
            mStateLocked_0x124d = 0;
            if (requestState_80251AEC(state)) {
                mLayer_0x3b4 = mDestLayer_0xce4;
                mPos = mDestPos_0xcd4;
                mPos.z = 0.0f;
            }
        }
        if (field_0x135c != NULL) {
            switch (mFacing_0x3dc) {
            case 0:
            case 1:
            case 3:
                setEmitterPos_8013CB44(field_0x135c, &mPos, 0, 0, true);
                break;
            case 2:
                setEmitterPos_8013CB44(field_0x135c, &mPos, 0, -1, true);
                break;
            }
            JPAGetXYZRotateMtx(0, lbl_8046C228[mFacing_0x3dc], 0, field_0x135c->mGlobalRot);
        }
        if (field_0x132c != NULL) {
            field_0x1344.set(mPos);
            if (mFacing_0x3dc == 2) {
                field_0x1344.y -= 3.0f;
            } else if (mFacing_0x3dc == 3) {
                field_0x1344.y += 3.0f;
            }
            field_0x1330.field_0x4.set(field_0x1344);
            JPAGetXYZRotateMtx(0, lbl_8046C228[mFacing_0x3dc], 0, field_0x132c->mGlobalRot);
        }
    } else {
        fn_8027EDDC();
    }
}

void Player_804A6F38::fn_80290F20() {
    if (field_0x132c != NULL) {
        field_0x132c->becomeInvalidEmitter();
        field_0x132c = NULL;
    }
    if (field_0x135c != NULL) {
        field_0x135c->becomeInvalidEmitter();
        field_0x135c = NULL;
    }
    if (fn_802486A0() != 4 && hasLeader_80240808()) {
        fn_80238244(mPlayerNo_0x4);
    }
}

void Player_804A6F38::fn_80290E90() {
    if (!fn_80240670()) {
        fn_80301D54(mPlayerNo_0x4);
    }
    field_0x400 = 0;
    if (gGameRoot_8053AAF8->field_0x7e == 0 && !fn_80240670()) {
        if (mDestLayer_0xce4 < 8) {
            gAudioMgr_8053AF80->setMusicSlow_8045815C(false);
        } else if (mDestLayer_0xce4 == 8) {
            gAudioMgr_8053AF80->setMusicSlow_8045815C(true);
        }
    }
}

void Player_804A6F38::fn_80290D58() {
    if (!fn_80241E28(mDepartLayer_0xce0, &mDepartPos_0xcc8)) {
        field_0xd1c = mRespawnPos_0xcbc = mPos;
    }
    if (mTransitionKind_0xcf4 == 0x14 && mLayer_0x3b4 >= 8) {
        field_0xbd8 = 600;
        field_0xbdc = 1;
    }
    if (fn_80240D64()) {
        mInputLockTimer_0x11f0 = 5;
    }
    field_0xce8 = false;
    field_0xcf8 = mTransitionKind_0xcf4;
    mTransitionKind_0xcf4 = 0x29;
    field_0x468 = 60;
    field_0xced = 0;
    fn_80241454(1);
    field_0x1216 = false;
    resetPosHistory_8024BAEC();
    mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
    if (fn_802486A0() != 4 && fn_802486A0() != 5) {
        setGroupMode_80248004(5, true);
    }
    fn_80248604();
    field_0xceb = 0;
    field_0x400 = 0;
    field_0xcec = 0;
}

// FIXME: 99.56. Order and stack slots only: the three offset vectors passed to
// fn_80252BAC sit at 0x2C / 0x20 / 0x14 (original 0x14 / 0x2C / 0x20, as if the
// kind-4 group's were declared last), and for kinds 0x17 / 0x18 the 0.0f is loaded
// before from->y, mPos.y (stores x, z, y in the original).
// Starts the transition `kind` from the current place to `to` on `layer` (see
// docs/PLAYER.md, player_8028FBA0): refuses while a transition runs (0xCE8), the
// player is down or held, or its GBA link is busy; requests the kind's leave
// state, records the destination 0xCD4 / 0xCE4 and the departure 0xCC8 / 0xCE0,
// and tells the GBA links (fn_803020C8) which way the player moves. Kind 0x21
// from a GBA layer is immediate: back to layer 0 at `to`, arrived at once
// (fn_80290E90, fn_80290D58); kind 0x22 (same layer) only moves the player.
bool Player_804A6F38::startTransition_80290070(s32 layer, const JGeometry::TVec3f* from, const JGeometry::TVec3f* to,
                                  s32 kind) {
    if (kind == 0xC && (fn_802DE93C() || mLayer_0x3b4 >= 8)) {
        kind = 0x26;
    }
    JGeometry::TVec3f dest(*to);
    if (kind == 0x24) {
        if (field_0xce8 == true && !fn_80240DAC()) {
            return false;
        }
    } else if (field_0xce8 == true) {
        return false;
    }
    if (field_0xcee == 1) {
        return false;
    }
    if (field_0x1216 == true) {
        return false;
    }
    if (field_0x1238 > 0) {
        return false;
    }
    if (kind == 0x21 && mLayer_0x3b4 >= 8) {
        mDepartLayer_0xce0 = mLayer_0x3b4;
        mDepartPos_0xcc8 = mPos;
        mLayer_0x3b4 = 0;
        mDestLayer_0xce4 = 0;
        mPos = *to;
        mRespawnPos_0xcbc = mDestPos_0xcd4 = mPos;
        mPos.z = 0.0f;
        mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
        fn_80290E90();
        fn_80290D58();
        return true;
    }
    if (isDeadState_80270A8C()) {
        return false;
    }
    if (getLife_8024189C() <= 0) {
        return false;
    }
    if (isGrabbing_8025F448()) {
        return false;
    }
    if (fn_8022C8EC(fn_80240628()) && layer >= 8 && fn_80303750(fn_80240628())) {
        return false;
    }
    field_0xcf1 = 0;
    if (kind == 0x22) {
        if (layer != mLayer_0x3b4) {
            return false;
        }
        u16 tile = getTileCollisionCode_8022461C(to, layer);
        if (fn_802265DC(to, layer, true) || tile == 3 || tile == 0xE || fn_802684A4(to, layer)) {
            if (layer < 8) {
                mPos.set(mReturnPos_0x2c4);
            } else {
                mPos.set(mRespawnPos_0xcbc);
            }
        } else {
            mPos.set(*to);
        }
        mPrevPos_0x2c.set(mPos);
        field_0x464 = 60;
        return true;
    }
    if (kind == 4 || kind == 7 || kind == 8 || kind == 9 || kind == 0x20 || kind == 0x24) {
        if (!hasLeader_80240808() && field_0x1216 == true) {
            return false;
        }
        if (hasLeader_80240808() && field_0x1238 > 0) {
            return false;
        }
        if (requestState_80251AEC(0x1E)) {
            // `*from - mPos`: libs Vec.h's TVec3::operator- is not const, so its body
            // (copy, subtract, copy out; the original's two stack vectors) is spelled out.
            JGeometry::TVec3f offset = JGeometry::TVec3f(*from) -= mPos;
            fn_80252BAC(&offset);
            if (fn_80240670()) {
                layer = mLayer_0x3b4;
                kind = 9;
                dest.set(field_0xd1c);
            }
        } else {
            return false;
        }
    } else if (kind == 0xC || kind == 0xD || kind == 0x26) {
        if ((!hasLeader_80240808() && field_0x1216 == true) ||
            (hasLeader_80240808() && !fn_8022D75C(getLeader_80240644()) && field_0x1238 > 0))
        {
            return false;
        }
        if (layer != mLayer_0x3b4) {
            return false;
        }
        if (!requestState_80251AEC(0x32)) {
            return false;
        }
    } else if (kind == 5) {
        if (requestState_80251AEC(0x20)) {
            field_0xcf1 = field_0xcf0;
            u16 tile = getTileCollisionCode_8022461C(&dest, layer);
            if (tile == 0 || tile == 2 || tile == 3 || tile == 0x21) {
                field_0xcf0 = 0;
            } else {
                field_0xcf0 = 1;
            }
        } else {
            return false;
        }
    } else if (kind == 6 || kind == 0xB || kind == 0x14 || kind == 0x1F) {
        if (layer >= 8) {
            if (requestState_80251AEC(0x24)) {
                mPos = *from;
            } else {
                return false;
            }
        } else {
            if (requestState_80251AEC(0x22)) {
                mPos = *from;
            } else {
                return false;
            }
        }
    } else if (kind == 0xE || kind == 0x25) {
        if (requestState_80251AEC(0x26)) {
            mPos = *from;
        } else {
            return false;
        }
    } else if (kind == 0xF) {
        if (!requestState_80251AEC(0x28)) {
            return false;
        }
    } else if (kind == 0x27) {
        if (!requestState_80251AEC(0x30)) {
            return false;
        }
    } else if (kind == 0xA) {
        if (mLayer_0x3b4 == 8) {
            if (requestState_80251AEC(0x2A)) {
                field_0xcf1 = field_0xcf0;
                u16 tile = getTileCollisionCode_8022461C(&dest, layer);
                if (tile == 0 || tile == 2 || tile == 0x21) {
                    field_0xcf0 = 0;
                } else {
                    field_0xcf0 = 1;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else if (kind == 0x11) {
        if (!fn_80240818()) {
            return false;
        }
    } else if (kind == 0x13 || kind == 0x16) {
        if (!(mState_0x1240 == 0x4D && mRequestedState_0x1244 == 0x4D) && !requestState_80251AEC(0x4D)) {
            return false;
        }
    } else if (kind == 0x17 || kind == 0x18) {
        if (requestState_80251AEC(0x1C)) {
            JGeometry::TVec3f offset(0.0f, from->y - mPos.y, 0.0f);
            fn_80252BAC(&offset);
        } else {
            return false;
        }
    } else if (kind == 0x1D) {
        if (requestState_80251AEC(0x2C)) {
            mPos = *from;
        } else {
            return false;
        }
    } else if (kind == 0x1E) {
        if (requestState_80251AEC(0x2E)) {
            mPos = *from;
        } else {
            return false;
        }
    } else {
        if (requestState_80251AEC(0x1C)) {
            JGeometry::TVec3f offset(from->x - mPos.x, 0.0f, 0.0f);
            fn_80252BAC(&offset);
        } else {
            return false;
        }
    }
    if (kind == 0x12) {
        mDestPos_0xcd4 = *getPlayerPos_8023E724(getLeader_80240644());
        mDestLayer_0xce4 = getPlayerLayer_8023DE58(getLeader_80240644());
    } else {
        mDestPos_0xcd4 = dest;
        mDestLayer_0xce4 = layer;
    }
    mTransitionKind_0xcf4 = kind;
    mDepartPos_0xcc8 = mPos;
    mDepartLayer_0xce0 = mLayer_0x3b4;
    field_0xce8 = true;
    if (mDestLayer_0xce4 != 8) {
        field_0xcf2 = 0;
    }
    if (!hasLeader_80240808()) {
        fn_8022D36C(mPlayerNo_0x4);
    }
    if (isLiftingPlayer_8025F590() && mLayer_0x3b4 == 8 && getPlayerLayer_8023DE58(mHeldPlayer_0xd70) < 8) {
        fn_8023105C(fn_8025F55C());
        fn_8025FF70();
    }
    if (mDestLayer_0xce4 >= 8 && mDestLayer_0xce4 != mLayer_0x3b4) {
        fn_803049F0(mPlayerNo_0x4, mDestLayer_0xce4);
        fn_80304B00(mPlayerNo_0x4, mDestLayer_0xce4);
        if (isLiftingPlayer_8025F590() && !fn_8025FCC8() && !fn_8022F478(mHeldPlayer_0xd70)) {
            fn_803049F0(mHeldPlayer_0xd70, mDestLayer_0xce4);
            fn_80304B00(mHeldPlayer_0xd70, mDestLayer_0xce4);
        }
    }
    s32 mode = -1;
    if (!fn_80240670() && !(mTransitionKind_0xcf4 == 0xC || mTransitionKind_0xcf4 == 0xD || mTransitionKind_0xcf4 == 0x26 ||
                            mTransitionKind_0xcf4 == 0x22))
    {
        s32 cur;
        if ((mTransitionKind_0xcf4 == 4 || mTransitionKind_0xcf4 == 7 || mTransitionKind_0xcf4 == 8 || mTransitionKind_0xcf4 == 9 ||
             mTransitionKind_0xcf4 == 0x20 || mTransitionKind_0xcf4 == 0x24) &&
            (cur = mLayer_0x3b4) > 8 && mDestLayer_0xce4 > 8 && viewLayerFlag(cur) &&
            viewLayerFlag(mDestLayer_0xce4) && mDestLayer_0xce4 != mLayer_0x3b4)
        {
            mode = 0;
        } else if (mTransitionKind_0xcf4 == 0x1E) {
            mode = 2;
        } else if (mTransitionKind_0xcf4 == 0xF) {
            mode = 2;
        } else if (mTransitionKind_0xcf4 == 0x27) {
            mode = 1;
        } else if ((mTransitionKind_0xcf4 == 6 || mTransitionKind_0xcf4 == 0xB) && mDepartLayer_0xce0 > 8) {
            mode = 2;
        } else if (mTransitionKind_0xcf4 == 0x13 || mTransitionKind_0xcf4 == 0x16) {
            if (mDestLayer_0xce4 < 8) {
                mode = 0;
            }
        } else {
            mode = 1;
        }
    }
    if (mode >= 0) {
        fn_803020C8(mPlayerNo_0x4, mDepartLayer_0xce0, mDestLayer_0xce4, mode);
    }
    if (isLiftingPlayer_8025F590() && !fn_8022F478(mHeldPlayer_0xd70)) {
        if (!fn_8025FCC8() && mode >= 0) {
            fn_803020C8(mHeldPlayer_0xd70, mDepartLayer_0xce0, mDestLayer_0xce4, mode);
        }
        if (mDepartLayer_0xce0 >= 8 && mDestLayer_0xce4 < 8) {
            fn_80230E1C(mHeldPlayer_0xd70, false);
        }
    }
    field_0xbf0->field_0xbc = 0;
    field_0x1216 = false;
    if (mRequestedState_0x1244 != 0x1C) {
        field_0x48c = 0;
        field_0xbc0 = 0;
    }
    if (mTransitionKind_0xcf4 == 0x1F) {
        setGroupMode_80248004(5, true);
    }
    endFormActor_80248FF0();
    return true;
}

bool Player_804A6F38::fn_8028FC2C() {
    if (field_0xd28 < 8) {
        return startTransition_80290070(0, &mPos, &mLastTvPos_0xcb0, 0x27);
    }
    s32 stage = getGameView_802D6A9C()->mStage_0x4;
    s32 level = getGameView_802D6A9C()->mLevel_0x8;
    s32 col = getGameView_802D6A9C()->mScreenX_0x14;
    GameView_804A9950* view = getGameView_802D6A9C();
    s32 screen = getGameView_802D6A9C()->field_0x60[view->mScreenY_0x18][col];
    s32 floor = mLayer_0x3b4 - 7;
    if (stage == 0 && level == 0) {
        JGeometry::TVec3f pos(1960.0f, 1000.0f, 0.0f);
        return startTransition_80290070(0, &mPos, &pos, 0x27);
    }
    if (stage == 1 && level == 0 && screen == 3) {
        JGeometry::TVec3f pos(1750.0f, 1080.0f, 0.0f);
        return startTransition_80290070(0, &mPos, &pos, 0x27);
    }
    if (stage == 1 && level == 0 && screen == 5 && floor == 5) {
        JGeometry::TVec3f pos(56.0f, 326.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 1 && level == 0 && screen == 5 && floor == 6) {
        JGeometry::TVec3f pos(120.0f, 88.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 1 && level == 0 && screen == 5 && floor == 7) {
        JGeometry::TVec3f pos(40.0f, 284.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 1 && level == 0 && screen == 6) {
        JGeometry::TVec3f pos(184.0f, 214.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 1 && level == 3) {
        JGeometry::TVec3f pos(218.0f, 72.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 2 && level == 0) {
        JGeometry::TVec3f pos(234.0f, 72.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 3 && level == 0) {
        JGeometry::TVec3f pos(202.0f, 88.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 5 && level == 0) {
        return startTransition_80290070(0, &mPos, &mLastTvPos_0xcb0, 0x27);
    }
    if (stage == 6 && level == 0 && screen == 3) {
        JGeometry::TVec3f pos(256.0f, 20.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 6 && level == 0 && screen == 4) {
        JGeometry::TVec3f pos(256.0f, 20.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    if (stage == 6 && level == 0 && screen == 5) {
        JGeometry::TVec3f pos(472.0f, 44.0f, 0.0f);
        return startTransition_80290070(field_0xd28, &mPos, &pos, 0x27);
    }
    return false;
}

bool Player_804A6F38::fn_8028FBA0() {
    JGeometry::TVec3f pos;
    pos.x = mLastTvPos_0xcb0.x;
    pos.y = mLastTvPos_0xcb0.y;
    pos.z = mLastTvPos_0xcb0.z;
    if (field_0xcf2 == 0) {
        pos.y = mLastTvPos_0xcb0.y - 16.0f;
    }
    if (startTransition_80290070(0, &mPos, &pos, 5)) {
        field_0xbdc = 0;
        return true;
    }
    return false;
}
