#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/Player.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/PlayerBit.h"
#include "game/ActorId.h"
#include "game/ViewLayer.h"
#include "game/VecEpsilon.h"
#include "game/BgRewrite.h"
#include "game/Effect.h"
#include "game/cls_804A71A8.h"
#include "game/tu_8030AE84.h"
#include "game/act/act_thief.h"
#include "game/act/act_cucco.h"
#include "game/BoxTest.h"
#include "game/Vec2Ops.h"

// File-scope tables in address order (.rodata). The per-pose tables are flat Vec[4 * n] indexed
// [pose + facing * n]: a 2-D Vec[4][n] indexed [facing][pose] changes the codegen (tried). Positions are offsets from mPos per
// facing (0x3DC: right, left, down, up) and animation pose (0x3FD).
// The held thing's position per facing and pose in the lift (state 0x11, fn_8026441C).
static const Vec lbl_8046B250[4 * 4] = {
    {16.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 8.0f}, {0.0f, 0.0f, 16.0f}, {16.0f, 0.0f, -1.0f},
    {-16.0f, 0.0f, 0.0f}, {-12.0f, 0.0f, 8.0f}, {0.0f, 0.0f, 16.0f}, {-16.0f, 0.0f, -1.0f},
    {0.0f, 16.0f, 0.0f}, {0.0f, 12.0f, 8.0f}, {0.0f, 0.0f, 16.0f}, {0.0f, 16.0f, -1.0f},
    {0.0f, -16.0f, 0.0f}, {0.0f, -12.0f, 8.0f}, {0.0f, 0.0f, 16.0f}, {0.0f, -16.0f, -1.0f},
};

// ... in state 0x15 (fn_80263860).
static const Vec lbl_8046B310[4 * 5] = {
    {-6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {16.0f, 0.0f, 8.0f}, {16.0f, 0.0f, 2.0f}, {16.0f, 0.0f, 4.0f},
    {6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {-16.0f, 0.0f, 8.0f}, {-16.0f, 0.0f, 2.0f}, {-16.0f, 0.0f, 4.0f},
    {0.0f, -6.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 16.0f, 8.0f}, {0.0f, 16.0f, 2.0f}, {0.0f, 16.0f, 4.0f},
    {0.0f, 6.0f, 11.0f}, {0.0f, 0.0f, 11.0f}, {0.0f, -16.0f, 8.0f}, {0.0f, -16.0f, 2.0f}, {0.0f, -16.0f, 6.0f},
};

// ... in the throw wind-up, state 0x16 (fn_802636D4): poses 0-2.
static const Vec lbl_8046B400[4 * 3] = {
    {-6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 0.0f, 20.0f},
    {6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 0.0f, 20.0f},
    {0.0f, -6.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 0.0f, 20.0f},
    {0.0f, 6.0f, 11.0f}, {0.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 20.0f},
};

// ... in the throw, state 0x17 (fn_802635D8): poses 2-4.
static const Vec lbl_8046B490[4 * 5] = {
    {-6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {16.0f, 0.0f, 8.0f}, {16.0f, 0.0f, 2.0f}, {16.0f, 0.0f, 4.0f},
    {6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {-16.0f, 0.0f, 8.0f}, {-16.0f, 0.0f, 2.0f}, {-16.0f, 0.0f, 4.0f},
    {0.0f, -6.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 16.0f, 8.0f}, {0.0f, 16.0f, 2.0f}, {0.0f, 16.0f, 4.0f},
    {0.0f, 6.0f, 11.0f}, {0.0f, 0.0f, 11.0f}, {0.0f, -16.0f, 8.0f}, {0.0f, -16.0f, 2.0f}, {0.0f, -16.0f, 6.0f},
};

// ... in state 0x18 (fn_802632A8).
static const Vec lbl_8046B580[4 * 5] = {
    {-6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {7.0f, 0.0f, 12.0f}, {12.0f, 0.0f, 0.0f}, {16.0f, 0.0f, 0.0f},
    {6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {-7.0f, 0.0f, 12.0f}, {-12.0f, 0.0f, 0.0f}, {-16.0f, 0.0f, 0.0f},
    {0.0f, -6.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 7.0f, 12.0f}, {0.0f, 12.0f, 0.0f}, {0.0f, 16.0f, 0.0f},
    {0.0f, 6.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, -7.0f, 12.0f}, {0.0f, -12.0f, 0.0f}, {0.0f, -16.0f, 0.0f},
};

// ... in state 0x14 (fn_80262254).
static const Vec lbl_8046B670[4 * 5] = {
    {-6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {16.0f, 0.0f, 8.0f}, {16.0f, 0.0f, 2.0f}, {16.0f, 0.0f, 4.0f},
    {6.0f, 0.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {-16.0f, 0.0f, 8.0f}, {-16.0f, 0.0f, 2.0f}, {-16.0f, 0.0f, 4.0f},
    {0.0f, -6.0f, 15.0f}, {0.0f, 0.0f, 17.0f}, {0.0f, 16.0f, 8.0f}, {0.0f, 16.0f, 2.0f}, {0.0f, 16.0f, 4.0f},
    {0.0f, 6.0f, 11.0f}, {0.0f, 0.0f, 11.0f}, {0.0f, -16.0f, 8.0f}, {0.0f, -16.0f, 2.0f}, {0.0f, -16.0f, 6.0f},
};

// 4 along the facing (fn_80261750, state 0x19).
static const Vec lbl_8046B760[4] = {
    {4.0f, 0.0f, 0.0f}, {-4.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}, {0.0f, -4.0f, 0.0f},
};

// Carry walk pose per facing and step (updateCarryWalk_8026152C, 0x80262BAC, 0x80263C64).
static const u8 lbl_8046B790[4][8] = {
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 4, 5, 6},
    {0, 1, 2, 3, 0, 4, 5, 6},
};

// The held thing's offset per carry pose; only the first three floats of a row are read.
static const f32 lbl_8046B7B0[7][5] = {
    {0.0f, 0.0f, 18.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 20.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 19.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 17.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 20.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 19.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 17.0f, 0.0f, 0.0f},
};

// 15.999 along the facing (fn_80260C08).
static const Vec lbl_8046B83C[4] = {
    {15.999f, 0.0f, 0.0f}, {-15.999f, 0.0f, 0.0f}, {0.0f, 15.999f, 0.0f}, {0.0f, -15.999f, 0.0f},
};

// 15.999 along the facing (fn_802604CC).
static const Vec lbl_8046B86C[4] = {
    {15.999f, 0.0f, 0.0f}, {-15.999f, 0.0f, 0.0f}, {0.0f, 15.999f, 0.0f}, {0.0f, -15.999f, 0.0f},
};

// 9.999 along the facing, and 10 against it (fn_8026022C).
static const Vec lbl_8046B89C[4] = {
    {9.999f, 0.0f, 0.0f}, {-9.999f, 0.0f, 0.0f}, {0.0f, 9.999f, 0.0f}, {0.0f, -9.999f, 0.0f},
};

static const Vec lbl_8046B8CC[4] = {
    {-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {0.0f, -10.0f, 0.0f}, {0.0f, 10.0f, 0.0f},
};

// Unit vectors along the facing (fn_8025F698).
static const Vec lbl_8046B8FC[4] = {
    {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
};

static const Vec lbl_8046B92C[4] = {
    {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
};

// mLifting_0xd64 == 1 as a u8, materialised (subfic / cntlzw / extrwi.) where the callers
// inline it: `? true : false` keeps the byte (a plain `== 1` folds into cmplwi 1, as
// isGameMode in game/GameMode.h); isLifting_8025F684 returns it. A member inline in Player.h
// was measured: isLifting_8025F684 is then not emitted and fn_8023EFEC (inlines it) drops.
static inline u8 isField0xd64One(const Player_804A6F38& p) {
    return p.mLifting_0xd64 == 1 ? true : false;
}

// Point-in-box test (x / y only), as in player_8026FC20.
static inline bool contains(const JGeometry::TBox2f& box, const JGeometry::TVec3f& p) {
    return p.x >= box.i.x && p.y >= box.i.y && p.x < box.f.x && p.y < box.f.y;
}

void Player_804A6F38::fn_80264768() {
    field_0x458 = 0;
}

// Update of state 0x11, the lift: the held thing goes through the four lift positions
// (lbl_8046B250, `pose`) over 24 frames, then state 0x12; a living actor (field_0x1a4)
// is first shaken for 48 frames (pose 3) and lifted over frames 49-80. Nothing to lift
// (mHeldActorSerial_0xd68 < 0): poses 0-1, state 0 at frame 22; a lost actor ends the lift at once.
void Player_804A6F38::fn_8026441C() {
    s32 pose = 0;
    mStateTimer_0x3fc++;
    ActorBase_804A5770* actor = heldActor();
    if (isLiftingPlayer_8025F590()) {
        if (mStateTimer_0x3fc < 8) {
            mSubPose_0x3fd = 0;
            pose = 0;
        } else if (mStateTimer_0x3fc < 16) {
            mSubPose_0x3fd = 1;
            pose = 0;
        } else if (mStateTimer_0x3fc < 24) {
            mSubPose_0x3fd = 2;
            pose = 1;
        } else {
            mSubPose_0x3fd = 3;
            pose = 2;
            requestState_80251AEC(0x12);
        }
    } else if (mHeldActorSerial_0xd68 < 0) {
        pose = 0;
        if (mStateTimer_0x3fc >= 22) {
            mSubPose_0x3fd = 1;
            requestState_80251AEC(0);
        } else if (mStateTimer_0x3fc <= 8) {
            mSubPose_0x3fd = 0;
        } else {
            mSubPose_0x3fd = 1;
        }
    } else if (actor == 0) {
        requestState_80251AEC(0);
        mLifting_0xd64 = 0;
        mHeldActorId_0xd6c = 'NULL';
        mHeldActorSerial_0xd68 = -1;
    } else if (actor->field_0x1a4 != 0) {
        if (mStateTimer_0x3fc <= 8) {
            mSubPose_0x3fd = 0;
            pose = 3;
        } else if (mStateTimer_0x3fc <= 24) {
            mSubPose_0x3fd = 1;
            pose = 3;
        } else if (mStateTimer_0x3fc <= 32) {
            mSubPose_0x3fd = 0;
            pose = 3;
        } else if (mStateTimer_0x3fc < 48) {
            mSubPose_0x3fd = 1;
            pose = 3;
        } else if (mStateTimer_0x3fc == 48) {
            // Its own test in the original (bge, then bne on the same compare with 48).
            mSubPose_0x3fd = 0;
            pose = 0;
        } else if (mStateTimer_0x3fc <= 56) {
            mSubPose_0x3fd = 0;
            pose = 0;
        } else if (mStateTimer_0x3fc <= 64) {
            mSubPose_0x3fd = 1;
            pose = 0;
        } else if (mStateTimer_0x3fc <= 72) {
            mSubPose_0x3fd = 2;
            pose = 1;
        } else if (mStateTimer_0x3fc <= 80) {
            mSubPose_0x3fd = 3;
            pose = 2;
        } else {
            mSubPose_0x3fd = 3;
            pose = 2;
            requestState_80251AEC(0x12);
        }
    } else {
        if (mStateTimer_0x3fc < 8) {
            mSubPose_0x3fd = 0;
            pose = 0;
        } else if (mStateTimer_0x3fc < 16) {
            mSubPose_0x3fd = 1;
            pose = 0;
        } else if (mStateTimer_0x3fc < 24) {
            mSubPose_0x3fd = 2;
            pose = 1;
        } else {
            mSubPose_0x3fd = 3;
            pose = 2;
            requestState_80251AEC(0x12);
        }
    }
    if (pose >= 0) {
        const Vec& offset = lbl_8046B250[pose + mFacing_0x3dc * 4];
        mHeldPos_0x2f4.set(offset.x, offset.y, offset.z);
        mHeldPos_0x2f4 += mPos;
    }
    fn_8025EB18();
    fn_80268B3C();
}

void Player_804A6F38::fn_80264410() {
    field_0x458 = 0;
}

// Update of state 0x12, the carry (player_80285E58's dispatch): the stick step (getFollowTargetPos_8027FA08's z
// of 0 drops mPos.z to the floor unless 0x434); fn_8022C13C (unless fn_802416C4 is 7): a
// held player that isPlayerDead_80236B50 rejects, a bomb ('HBMB') or a small key ('KEY0') is put
// down (0x18), anything else thrown (0x16, also when fn_8024B24C); fn_8022C0A0 throws
// (0x16); fn_802466B4 starts 0x15 unless the held actor's field_0x1a4 is 2 or more. Then
// it walks with the held thing (moveByStick_802808E8 at 1.25 for a player, 1.5 for an actor with
// field_0x1a4 == 0, none otherwise) through the carry cycle updateCarryWalk_8026152C(false); a held
// player that stops being one (fn_8023EA28) or a lost actor ends the carry (state 0).
void Player_804A6F38::fn_80263C64() {
    JGeometry::TVec3f stick;
    getFollowTargetPos_8027FA08(&stick);
    if (0.0f == stick.z && 0.0f != mPos.z && !field_0x434) {
        mPos.z = 0.0f;
    }
    ActorBase_804A5770* actor = heldActor();
    if (fn_8022C13C(mPlayerNo_0x4)) {
        if (fn_802416C4() != 7) {
            if (!fn_8024B24C()
                && ((isLiftingPlayer_8025F590() && !isPlayerDead_80236B50(mHeldPlayer_0xd70)) || fn_8025F648() == ACTOR_HBMB
                    || fn_8025F648() == ACTOR_KEY0)) {
                requestState_80251AEC(0x18);
            } else {
                requestState_80251AEC(0x16);
            }
        }
    } else if (fn_8022C0A0(mPlayerNo_0x4)) {
        requestState_80251AEC(0x16);
    } else if (fn_802466B4() && (actor == 0 || actor->field_0x1a4 < 2)) {
        requestState_80251AEC(0x15);
    }
    if (isLiftingPlayer_8025F590()) {
        if (fn_8023EA28(mHeldPlayer_0xd70)) {
            moveByStick_802808E8(1.25f);
            updateFacing_8027EB04();
            updateCarryWalk_8026152C(false);
        } else {
            requestState_80251AEC(0);
            mHeldPlayer_0xd70 = -1;
        }
    } else if (actor != 0) {
        if (actor->field_0x1a4 == 0) {
            moveByStick_802808E8(1.5f);
            updateFacing_8027EB04();
            updateCarryWalk_8026152C(false);
        } else {
            updateFacing_8027EB04();
            updateCarryWalk_8026152C(false);
        }
    } else {
        requestState_80251AEC(0);
        mLifting_0xd64 = 0;
        mHeldActorSerial_0xd68 = -1;
        mHeldActorId_0xd6c = 'NULL';
    }
    fn_80268B3C();
}

void Player_804A6F38::fn_80263BA0() {
    field_0x458 = 0;
    field_0x459 = 0;
    if (isLiftingPlayer_8025F590()) {
        fn_8027221C(4);
    } else {
        ActorBase_804A5770* actor = heldActor();
        if (actor != 0) {
            fn_8027221C(actor->mParamAtcn_0x118);
        } else {
            fn_8027221C(0);
        }
    }
    playSound_8024F0E8(0x1000E);
}

// Update of state 0x15 (with the held thing): poses 0-4 over 22 frames, then state 0x12;
// at frame 11 a held thief ('TZOK') gets fn_80206168; at pose 2 the tile under the held
// thing (fn_802A3164) triggers its vfunc_0x48; when it can be put down (fn_8025EB18)
// the pose goes back to 1 and mPushVel_0x35c is turned against the facing as in 0x18.
void Player_804A6F38::fn_80263860() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc < 8) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 9) {
        mSubPose_0x3fd = 1;
    } else if (mStateTimer_0x3fc < 10) {
        field_0x459 = 1;
        mSubPose_0x3fd = 2;
    } else if (mStateTimer_0x3fc < 18) {
        mSubPose_0x3fd = 3;
        if (mStateTimer_0x3fc == 11 && mHeldActorId_0xd6c == ACTOR_TZOK) {
            ActorBase_804A5770* actor = heldActor();
            if (actor != 0) {
                static_cast<Thief_804A5A30*>(actor)->fn_80206168();
            }
        }
    } else if (mStateTimer_0x3fc < 22) {
        mSubPose_0x3fd = 4;
    } else {
        mSubPose_0x3fd = 2;
        requestState_80251AEC(0x12);
    }
    const Vec& offset = lbl_8046B310[mSubPose_0x3fd + mFacing_0x3dc * 5];
    mHeldPos_0x2f4.set(offset.x, offset.y, offset.z);
    mHeldPos_0x2f4 += mPos;
    if (mSubPose_0x3fd == 2) {
        JGeometry::TVec3f pos(mHeldPos_0x2f4);
        pos.z = 0.0f;
        if (gBgRewrite_8053AC98->fn_802A3164(&pos, mPlayerNo_0x4, mLayer_0x3b4)) {
            ActorBase_804A5770* actor = heldActor();
            if (actor != 0) {
                actor->vfunc_0x48();
            }
        }
    }
    if (fn_8025EB18()) {
        mSubPose_0x3fd = 1;
        field_0x459 = 0;
        const Vec& back = lbl_8046B310[mSubPose_0x3fd + mFacing_0x3dc * 5];
        mHeldPos_0x2f4.set(back.x, back.y, back.z);
        mHeldPos_0x2f4 += mPos;
        if (mPushVel_0x35c.length() < 1.0f) {
            const Vec& dir = lbl_8046B310[mFacing_0x3dc * 5 + 2];
            mPushVel_0x35c.set(dir.x, dir.y, dir.z);
            mPushVel_0x35c.z = 0.0f;
            mPushVel_0x35c.normalize();
            mPushVel_0x35c.scale(-1.0f);
        }
    }
    fn_80268B3C();
}

void Player_804A6F38::fn_80263854() {
    field_0x459 = 0;
}

void Player_804A6F38::fn_80263848() {
    field_0x458 = 0;
}

// Update of state 0x16 (the throw wind-up, provisional): poses 0 / 1 over 10 frames, then
// state 0x17; a thrown player gets a 'THCL' ("attack hitbox when player is thrown",
// docs/names/actors.tsv) at its position.
void Player_804A6F38::fn_802636D4() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc < 8) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 10) {
        mSubPose_0x3fd = 1;
    } else {
        mSubPose_0x3fd = 2;
        if (requestState_80251AEC(0x17) && isLiftingPlayer_8025F590()) {
            JGeometry::TVec3f* pos; // declared first: the original's register order (r30)
            u16 thrower = mPlayerNo_0x4;
            s32 thrown = mHeldPlayer_0xd70;
            pos = getPlayerPos_8023E724(thrown);
            getActorManager_801F856C()->spawnActor_801F9484(ACTOR_THCL, mLayer_0x3b4, pos, thrown, -1, thrower);
        }
    }
    const Vec& offset = lbl_8046B400[mSubPose_0x3fd + mFacing_0x3dc * 3];
    mHeldPos_0x2f4.set(offset.x, offset.y, offset.z);
    mHeldPos_0x2f4 += mPos;
    fn_8025EB18();
    fn_80268B3C();
}

void Player_804A6F38::fn_802636D0() {}

void Player_804A6F38::fn_802636C4() {
    field_0x458 = 0;
}

// Update of state 0x17 (the throw, provisional): poses 2 / 3 / 4 over 14 frames, then
// state 0; the held position follows lbl_8046B490.
void Player_804A6F38::fn_802635D8() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc < 2) {
        mSubPose_0x3fd = 2;
    } else if (mStateTimer_0x3fc < 10) {
        mSubPose_0x3fd = 3;
    } else if (mStateTimer_0x3fc < 14) {
        mSubPose_0x3fd = 4;
    } else {
        mSubPose_0x3fd = 2;
        requestState_80251AEC(0);
    }
    const Vec& offset = lbl_8046B490[mSubPose_0x3fd + mFacing_0x3dc * 5];
    mHeldPos_0x2f4.set(offset.x, offset.y, offset.z);
    mHeldPos_0x2f4 += mPos;
    fn_8025EB18();
    fn_80268B3C();
}

void Player_804A6F38::fn_802635D4() {}

void Player_804A6F38::fn_802635C8() {
    field_0x458 = 0;
}

// Update of state 0x18, the put-down: poses 0-2 over 11 frames, then 3 (4 for a held
// player); at frame 12 state 0, letting go in place (an actor gets speed 0); the held
// thing follows lbl_8046B580, and when it can be put down (fn_8025EB18) and mPushVel_0x35c
// is shorter than 1 it becomes a unit vector against the facing (pose 3's offset, flat).
void Player_804A6F38::fn_802632A8() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc < 8) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 10) {
        mSubPose_0x3fd = 1;
    } else if (mStateTimer_0x3fc < 11) {
        mSubPose_0x3fd = 2;
    } else if (mStateTimer_0x3fc < 12) {
        if (isLiftingPlayer_8025F590()) {
            mSubPose_0x3fd = 4;
        } else {
            mSubPose_0x3fd = 3;
        }
    } else {
        mStateTimer_0x3fc = 12;
        if (requestState_80251AEC(0)) {
            if (isLiftingPlayer_8025F590()) {
                mSubPose_0x3fd = 4;
            } else {
                mSubPose_0x3fd = 3;
                ActorBase_804A5770* actor = heldActor();
                if (actor != 0) {
                    JGeometry::TVec3f speed(0.0f, 0.0f, 0.0f);
                    actor->fn_801F3708(&speed);
                }
            }
            mHeldPlayer_0xd70 = -1;
            mHeldActorSerial_0xd68 = -1;
            mLifting_0xd64 = 0;
            mHeldActorId_0xd6c = 'NULL';
            field_0xd74 = 3;
        }
    }
    const Vec& offset = lbl_8046B580[mSubPose_0x3fd + mFacing_0x3dc * 5];
    mHeldPos_0x2f4.set(offset.x, offset.y, offset.z);
    mHeldPos_0x2f4 += mPos;
    if (fn_8025EB18() && mPushVel_0x35c.length() < 1.0f) {
        const Vec& dir = lbl_8046B580[mFacing_0x3dc * 5 + 3];
        mPushVel_0x35c.set(dir.x, dir.y, dir.z);
        mPushVel_0x35c.z = 0.0f;
        mPushVel_0x35c.normalize();
        mPushVel_0x35c.scale(-1.0f);
    }
    fn_80268B3C();
}

void Player_804A6F38::fn_802632A4() {}

// FIXME: 99.78 %, regalloc-only; behaviour clean.
// Registers only: the view-box test keeps pos.x in f2 and pos.y in f1 (ours
// f1 / f2); with the byte compared directly (cmplwi, not the original's cmpwi) they match.
// Enter of state 0x13 (player_80285E58's dispatch; update fn_80262BAC, the cucco glide): with
// a held cucco ('NIWA') whose field_0x2a1 is 1, probe 16-steps along mVel_0x344.x inside the view box for the second tile 0x10-0x17; found,
// mGlideTarget_0x324 becomes the point one step further on (fn_80262BAC glides toward its x);
// else the player is put at mGlideTarget_0x324.y. Faces along mVel_0x344.x.
void Player_804A6F38::fn_80263078() {
    fn_8028414C();
    if (mHeldActorId_0xd6c == ACTOR_NIWA && !field_0x441) {
        Cucco_804AB8C0* cucco = static_cast<Cucco_804AB8C0*>(
            heldActor());
        // (s32) on the u8 byte: the original compares it with cmpwi here (plain u8 gives cmplwi).
        if (cucco != 0 && (s32)cucco->field_0x2a1 == 1) {
            JGeometry::TVec2f pos;
            pos.x = mPos.x;
            pos.y = mPos.y;
            int left = 2;
            do {
                if (mVel_0x344.x > 0.0f) {
                    pos.x += 16.0f;
                } else {
                    pos.x -= 16.0f;
                }
                if (!containsPoint(*layerBox(mLayer_0x3b4), pos)) {
                    break;
                }
                u16 tile = getTileCollisionCodeAlt_802265FC((const JGeometry::TVec3f*)&pos, mLayer_0x3b4);
                if (tile >= 0x10 && tile <= 0x17) {
                    left--;
                }
            } while (left > 0);
            if (left == 0) {
                if (mVel_0x344.x > 0.0f) {
                    pos.x += 16.0f;
                } else {
                    pos.x -= 16.0f;
                }
                mGlideTarget_0x324.x = pos.x;
                mGlideTarget_0x324.y = pos.y;
                mVel_0x344.z = 2.0f;
            } else {
                mVel_0x344.z = 2.0f;
                mPos.z = mGlideTarget_0x324.y - mPos.y;
                mPos.y = mGlideTarget_0x324.y;
            }
        } else {
            mVel_0x344.z = 2.0f;
            mPos.z = mGlideTarget_0x324.y - mPos.y;
            mPos.y = mGlideTarget_0x324.y;
        }
        if (mVel_0x344.x > 0.0f) {
            mFacing_0x3dc = 0;
            mMoveDir_0x3e8 = 0;
        } else if (mVel_0x344.x < 0.0f) {
            mFacing_0x3dc = 1;
            mMoveDir_0x3e8 = 1;
        }
    }
}

// FIXME: 99.23 %, missing/extra; behaviour near.
// Two lfs missing: the original reloads mPos.x for the 0.5 steps after
// computing mGlideTarget_0x324.x - mPos.x; ours reuses the register.
// Update of state 0x13 (the fall while carrying, mVel_0x344 the speed): a cucco ('NIWA') is
// carried gliding: the player moves 0.5 a frame toward mGlideTarget_0x324.x, bobbing with the
// distance, else falls and lands back in 0x12; anything else falls at 0.25 (0.17 down to
// -6 while 0x43F / 0x441, floating at 8 below mGlideTarget_0x324.y), with the carry walk cycle
// at lbl_8046B790 / lbl_8046B7B0 (updateCarryWalk_8026152C's) and sounds 0x1000D / 0x2003E when 0x440.
void Player_804A6F38::fn_80262BAC() {
    if (mHeldActorId_0xd6c == ACTOR_NIWA && !field_0x441) {
        f32 dx = mGlideTarget_0x324.x - mPos.x;
        if (dx >= 0.5f) {
            mPos.x += 0.5f;
        } else if (dx <= -0.5f) {
            mPos.x -= 0.5f;
        } else if (0.0f != dx) {
            mPos.x = mGlideTarget_0x324.x;
        }
        if (0.0f != dx) {
            mVel_0x344.z -= 0.25f;
            if (mVel_0x344.z <= 0.0f) {
                mVel_0x344.z = 0.0f;
            }
            mPos.z += mVel_0x344.z;
            mPos.z += 0.1f * JMath::sincosTable_.sinShort((s16)(4096.0f * dx));
        } else {
            mVel_0x344.z -= 0.25f;
            if (mVel_0x344.z <= -4.0f) {
                mVel_0x344.z = -4.0f;
            }
            mPos.z += mVel_0x344.z;
            if (mPos.z <= 0.0f) {
                mPos.z = 0.0f;
                if (requestState_80251AEC(0x12)) {
                    field_0xce8 = false;
                }
            }
        }
        mHeldPos_0x2f4.set(0.0f, 0.0f, 18.0f);
        mHeldPos_0x2f4 += mPos;
    } else {
        if (mPos.z >= 0.0f) {
            mPos += mVel_0x344;
        }
        if (field_0x43f || field_0x441) {
            if (mVel_0x344.z >= -6.0f) {
                mVel_0x344.z -= 0.17f;
                if (mVel_0x344.z < -6.0f) {
                    mVel_0x344.z = -6.0f;
                }
            }
        } else {
            mVel_0x344.z -= 0.25f;
        }
        if (mPos.z <= 0.0f) {
            if ((field_0x43f || field_0x441) && mPos.y < mGlideTarget_0x324.y) {
                mPos.z = 8.0f;
            } else {
                mPos.z = 0.0f;
                mStateLocked_0x124d = 0;
                requestState_80251AEC(0x12);
            }
        }
        updateCarryWalk_8026152C(false);
        if (field_0x440 && mVel_0x344.z < 0.0f) {
            playSound_8024F0E8(0x1000D);
            playSound_8024F0E8(0x2003E);
        }
    }
    if (mVel_0x344.z < -1.0f) {
        mWalkFrame_0x3f9 = 1;
    } else {
        mWalkFrame_0x3f9 = 0;
    }
}

void Player_804A6F38::fn_80262B8C() {
    fn_80283D2C();
}

void Player_804A6F38::fn_80262B6C() {
    fn_80259660();
}

// FIXME: 99.64 %, scheduling+regalloc; behaviour clean.
// scheduling+regalloc, as fn_80258BE8 (player_80256C74): the no-pad branch
// computes &mVel_0x344 (addi r3) before the x / y speed test in the original.
// Update of state 0x14, the jump while holding something (enter fn_80262234 runs the
// jump's exit fn_80258BBC, exit fn_80262B6C its enter fn_80259660): the steering, gravity
// and landing of the jump fn_80258BE8 (player_80256C74; same structure), with the held
// thing at lbl_8046B670 once 0x3FB is set (fn_80245938), let go at frame 8. Landing
// (z <= 0 after 2 frames, or on a 2D layer over a tile other than 2 below) returns to
// 0x12 while lifting, else 0; the held thing sits 18 up while falling.
void Player_804A6F38::fn_80262254() {
    if (fn_8022C6AC(mPlayerNo_0x4) > 0.0f) {
        if (0.0f == mVel_0x344.x) {
            mVel_0x344.x = 0.8f * fn_8022C82C(mPlayerNo_0x4);
        } else if (mVel_0x344.x > 0.0f) {
            if (mVel_0x344.z >= 0.0f) {
                mVel_0x344.x += 0.3f * fn_8022C82C(mPlayerNo_0x4);
            } else {
                mVel_0x344.x += 0.2f * fn_8022C82C(mPlayerNo_0x4);
            }
            if (mVel_0x344.x <= 0.001f || field_0xbf0->field_0x48[0]) {
                mVel_0x344.x = 0.001f;
            }
        } else if (mVel_0x344.x < 0.0f) {
            if (mVel_0x344.z >= 0.0f) {
                mVel_0x344.x += 0.3f * fn_8022C82C(mPlayerNo_0x4);
            } else {
                mVel_0x344.x += 0.2f * fn_8022C82C(mPlayerNo_0x4);
            }
            if (mVel_0x344.x >= -0.001f || field_0xbf0->field_0x48[1]) {
                mVel_0x344.x = -0.001f;
            }
        }
        if (0.0f == mVel_0x344.y) {
            mVel_0x344.y = 0.8f * -fn_8022C76C(mPlayerNo_0x4);
        } else if (mVel_0x344.y > 0.0f) {
            if (mVel_0x344.z >= 0.0f) {
                mVel_0x344.y -= 0.3f * fn_8022C76C(mPlayerNo_0x4);
            } else {
                mVel_0x344.y -= 0.2f * fn_8022C76C(mPlayerNo_0x4);
            }
            if (mVel_0x344.y <= 0.001f || field_0xbf0->field_0x48[2]) {
                mVel_0x344.y = 0.001f;
            }
        } else if (mVel_0x344.y < 0.0f) {
            if (mVel_0x344.z >= 0.0f) {
                mVel_0x344.y -= 0.3f * fn_8022C76C(mPlayerNo_0x4);
            } else {
                mVel_0x344.y -= 0.2f * fn_8022C76C(mPlayerNo_0x4);
            }
            if (mVel_0x344.y >= -0.001f || field_0xbf0->field_0x48[3]) {
                mVel_0x344.y = -0.001f;
            }
        }
        f32 z = mVel_0x344.z;
        mVel_0x344.z = 0.0f;
        f32 length = mVel_0x344.length();
        f32 speed = length;
        if (length >= 1.6f) {
            speed = 1.6f;
        }
        mVel_0x344.normalize();
        mVel_0x344.scale(speed);
        mVel_0x344.z = z;
    } else if (!(mVel_0x344.x * mVel_0x344.x + mVel_0x344.y * mVel_0x344.y <= JGeometry::TUtilf::epsilon())) {
        f32 z = mVel_0x344.z;
        mVel_0x344.z = 0.0f;
        f32 speed = mVel_0x344.length();
        if (fn_8022C274(mPlayerNo_0x4)) {
            if (z >= 0.0f) {
                speed += 0.3f;
                if (speed >= 1.6f) {
                    speed = 1.6f;
                }
            } else {
                speed += 0.2f;
                if (speed >= 1.6f) {
                    speed = 1.6f;
                }
            }
        } else {
            speed -= 0.1f;
            if (speed <= 0.001f) {
                speed = 0.001f;
            }
        }
        mVel_0x344.normalize();
        mVel_0x344.scale(speed);
        mVel_0x344.z = z;
    }

    if (!mVel_0x344.isZero() || mPos.z > 0.0f) {
        if (mAnimTimer_0x3fa >= 1) {
            mVel_0x344.z -= 0.25f;
            if (mVel_0x344.z < -4.0f) {
                mVel_0x344.z = -4.0f;
            }
            if (viewLayerFlag(mLayer_0x3b4)) {
                mPos.x += mVel_0x344.x;
                mPos.y -= mVel_0x344.z;
                mPos.z = 0.5f;
                if (mVel_0x344.z < 0.0f && field_0xbf0->field_0x48[2]) {
                    mKeptVel_0x368.set(mVel_0x344.x, mVel_0x344.y, 0.0f);
                    mVel_0x344.set(0.0f, 0.0f, 0.0f);
                    mPos.z = 0.0f;
                }
            } else {
                mPos += mVel_0x344;
                if (mVel_0x344.z < 0.0f && mPos.z <= 0.0f) {
                    mPos.z = 0.0f;
                    mKeptVel_0x368.set(mVel_0x344.x, mVel_0x344.y, 0.0f);
                    mVel_0x344.set(0.0f, 0.0f, 0.0f);
                }
            }
        }
    }

    if (fn_80245938()) {
        mPose_0x3fb = 1;
    }
    if (mPose_0x3fb == 1) {
        mStateTimer_0x3fc++;
        if (mStateTimer_0x3fc < 8) {
            mSubPose_0x3fd = 0;
        } else if (mStateTimer_0x3fc < 9) {
            fn_8025F698(false);
            mSubPose_0x3fd = 1;
        } else if (mStateTimer_0x3fc < 11) {
            mSubPose_0x3fd = 2;
        } else if (mStateTimer_0x3fc < 21) {
            mSubPose_0x3fd = 3;
        } else if (mStateTimer_0x3fc < 25) {
            mSubPose_0x3fd = 4;
        } else {
            mSubPose_0x3fd = 2;
        }
        const Vec& offset = lbl_8046B670[mSubPose_0x3fd + mFacing_0x3dc * 5];
        mHeldPos_0x2f4.set(offset.x, offset.y, offset.z);
        mHeldPos_0x2f4 += mPos;
        fn_80268B3C();
    }

    if (!field_0xb79 && mPos.z > 0.0f && fn_802415CC() >= 2 && fn_8022C310(mPlayerNo_0x4)) {
        mAnimTimer_0x3fa = 0;
        fn_80259778();
        field_0xb79 = 1;
    }
    if (mAnimTimer_0x3fa >= 2 && mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        if (mLifting_0xd64 == 1) {
            requestState_80251AEC(0x12);
        } else {
            requestState_80251AEC(0);
        }
    }
    if (mAnimTimer_0x3fa >= 8 && viewLayerFlag(mLayer_0x3b4) && mVel_0x344.z < 0.0f && mPos.z <= 1.0f) {
        JGeometry::TVec3f below;
        fn_8024B298(2, &below);
        below.y += 2.0f;
        if (getTileCollisionCode_8022461C(&below, mLayer_0x3b4) != 2) {
            if (mLifting_0xd64 == 1) {
                requestState_80251AEC(0x12);
            } else {
                requestState_80251AEC(0);
            }
        }
    }
    mAnimTimer_0x3fa++;
    fn_802685D4();
    mHeldPos_0x2f4.set(0.0f, 0.0f, 18.0f);
    mHeldPos_0x2f4 += mPos;
    if (mVel_0x344.z < -2.0f) {
        mWalkFrame_0x3f9 = 1;
    } else {
        mWalkFrame_0x3f9 = 0;
    }
}

void Player_804A6F38::fn_80262234() {
    fn_80258BBC();
}

void Player_804A6F38::fn_80262188() {
    field_0x458 = 0;
    if (mPrevState_0x1248 != 0x33 && field_0xcfe == 0) {
        playSound_8024F0E8(0x10007);
    }
    if (mHeldPlayer_0xd70 >= 0 && gGameRoot_8053AAF8->field_0x7e != 0 && !isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4)
        && mLeader_0xd60 < 0) {
        mPadPlayer_0x11ec = fn_8022F340(mHeldPlayer_0xd70);
        mLeader_0xd60 = fn_8022F340(mHeldPlayer_0xd70);
    }
}

// FIXME: 99.65 %, missing/extra; behaviour near.
// One extra lwz: the original passes the mHeldPlayer_0xd70 the inlined
// fn_8025F418 loaded straight to fn_8023EFEC; ours reloads it.
// Update of state 0x19. Grabbing a player: follow it (its position moved to this
// layer's view corner, raised 2; effect 0x28F once at height 10), let go when it
// stops lifting; grabbing an actor: stay at field_0x300. Otherwise the player flies
// along field_0xd78 (falling 0.25 a frame; held inside the view less 16) until it
// lands (state 0 / 1) or fn_802466B4 (state 7), bouncing off walls at half speed.
// The pose cycles over 36 frames.
void Player_804A6F38::fn_80261750() {
    if (isGrabbing_8025F448()) {
        if (fn_8025F418()) {
            if (!fn_8023EFEC(mHeldPlayer_0xd70)) {
                if (mPos.z <= 4.0f) {
                    field_0xd78.set(0.0f, 0.0f, 0.0f);
                } else {
                    const Vec& speed = lbl_8046B760[mFacing_0x3dc];
                    field_0xd78.set(speed.x, speed.y, speed.z);
                }
                playSound_8024F0E8(0x20014);
                mHeldPlayer_0xd70 = -1;
                field_0xd58 = 5;
            } else {
                if ((fn_8022D75C(mHeldPlayer_0xd70) && !fn_80234ECC(mHeldPlayer_0xd70)) || mLayer_0x3b4 >= 8
                    || getPlayerLayer_8023DE58(mHeldPlayer_0xd70) != 8) {
                    mLayer_0x3b4 = getPlayerLayer_8023DE58(mHeldPlayer_0xd70);
                }
                mPos = *fn_8023E7B0(mHeldPlayer_0xd70);
                JGeometry::TBox2f* box = layerBox(getPlayerLayer_8023DE58(mHeldPlayer_0xd70));
                f32 x = box->i.x;
                f32 y = box->i.y;
                box = layerBox(mLayer_0x3b4);
                JGeometry::TVec2f offset(box->i.x - x, box->i.y - y);
                mPos.x += offset.x;
                mPos.y += offset.y;
                mPos.y += 2.0f;
                mPos.z += 2.0f;
                if (field_0x121c == 0 && !isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4) && !fn_80234BC0(mHeldPlayer_0xd70)
                    && mLeader_0xd60 < 0 && mPos.z >= 10.0f) {
                    spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x28F, 1, 1, NULL, NULL, true, 1.0f);
                    field_0x121c = 1;
                }
                if (fn_8022D75C(mHeldPlayer_0xd70) && !fn_80234ECC(mHeldPlayer_0xd70)) {
                    if (!fn_80241E28(mLayer_0x3b4, &mPos)) {
                        if (field_0x3b8 < 8) {
                            mLastTvPos_0xcb0 = mLastPos_0x14;
                        }
                        mDepartPos_0xcc8 = mLastPos_0x14;
                        mDepartLayer_0xce0 = field_0x3b8;
                        mDestPos_0xcd4 = mPos;
                        mDestLayer_0xce4 = mLayer_0x3b4;
                        if (viewLayerFlag(mLayer_0x3b4) && fn_802486A0() != 5 && fn_802486A0() != 4) {
                            setGroupMode_80248004(5, false);
                        }
                    }
                    if (!fn_8022BC38(mHeldPlayer_0xd70, fn_8023E190(mHeldPlayer_0xd70), fn_8023E2A0(mHeldPlayer_0xd70))) {
                        mDestPos_0xcd4 = mPos;
                        mRespawnPos_0xcbc = mDestPos_0xcd4;
                        field_0xd1c = mRespawnPos_0xcbc;
                        mDestPos_0xcd4.z = 0.0f;
                        mRespawnPos_0xcbc.z = 0.0f;
                        field_0xd1c.z = 0.0f;
                    }
                    field_0x1216 = false;
                    resetPosHistory_8024BAEC();
                    mViewBox_0xbec->fn_8030BD3C(mPlayerNo_0x4);
                }
                mMoveDir_0x3e8 = mFacing_0x3dc = getPlayerFacing_80230CFC(mHeldPlayer_0xd70);
                if (!fn_8022D75C(mHeldPlayer_0xd70)) {
                    if (!fn_80243194(mHeldPlayer_0xd70) && fn_8022C5EC(mPlayerNo_0x4, 0xF) != 0) {
                        mAnimTimer_0x3fa++;
                        mStateTimer_0x3fc += 6;
                    } else {
                        mStateTimer_0x3fc++;
                    }
                    if (mAnimTimer_0x3fa > 16) {
                        fn_80231194(mHeldPlayer_0xd70);
                    }
                }
            }
            if (mLayer_0x3b4 < 8 && mHeldPlayer_0xd70 >= 0 && getPlayerLayer_8023DE58(mHeldPlayer_0xd70) > 8) {
                fn_80230C6C(mHeldPlayer_0xd70);
            }
        } else {
            ActorBase_804A5770* actor = heldActor();
            if (actor != 0) {
                // Per-field copies: the original moves floats (lfs / stfs), not words.
                mPos.x = field_0x300.x;
                mPos.y = field_0x300.y;
                mPos.z = field_0x300.z;
                if (fn_8022C5EC(mPlayerNo_0x4, 0xF) != 0) {
                    mStateTimer_0x3fc += 6;
                } else {
                    mStateTimer_0x3fc++;
                }
            } else {
                mHeldActorSerial_0xd68 = -1;
                field_0xd78.set(0.0f, 0.0f, 0.0f);
            }
        }
        field_0xbf0->fn_8026E464();
        field_0xbf0->field_0xa0 = 0;
    } else {
        fn_8027221C(4);
        bool outside = false;
        mPos += field_0xd78;
        field_0xd78.z -= 0.25;
        JGeometry::TBox2f box;
        box = *layerBox(mLayer_0x3b4);
        // The view box inset by 16 (the original subtracts the -16 from i, adds it to f).
        box.i.x -= -16.0f;
        box.i.y -= -16.0f;
        box.f.x += -16.0f;
        box.f.y += -16.0f;
        if (viewLayerFlag(mLayer_0x3b4) && !contains(box, mPos)) {
            outside = true;
            mPos.z = 0.0f;
        }
        if (mPos.z <= 0.0f) {
            if (viewLayerFlag(mLayer_0x3b4)) {
                if (outside || fn_802518AC(true)) {
                    mPos.z = 0.0f;
                } else {
                    mPos.y += 1.0f - mPos.z;
                    mPos.z = 1.0f;
                }
            }
            if (mPos.z <= 0.0f) {
                mKeptVel_0x368.set(field_0xd78.x, field_0xd78.y, 0.0f);
                field_0xd78.set(0.0f, 0.0f, 0.0f);
                mPos.z = 0.0f;
                if (gGameRoot_8053AAF8->field_0x7e != 0 && !isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4) && mLeader_0xd60 < 0) {
                    requestState_80251AEC(1);
                } else {
                    requestState_80251AEC(0);
                }
            }
        } else if (fn_802466B4() && requestState_80251AEC(7)) {
            mVel_0x344.x = field_0xd78.x;
            mVel_0x344.y = field_0xd78.y;
            mVel_0x344.z = field_0xd78.z;
            mKeptVel_0x368.set(field_0xd78.x, field_0xd78.y, 0.0f);
            field_0xd78.set(0.0f, 0.0f, 0.0f);
        }
        fn_802685D4();
        if (viewLayerFlag(mLayer_0x3b4)) {
            if (field_0xbf0->pushedAlongX()) {
                field_0xd78.x = -field_0xd78.x * 0.5f;
            }
        } else if (field_0xbf0->anyFlag48()) {
            field_0xd78.x = -field_0xd78.x * 0.5f;
            field_0xd78.y = -field_0xd78.y * 0.5f;
        }
    }
    if (mStateTimer_0x3fc >= 36) {
        mStateTimer_0x3fc = 0;
    }
    if (mStateTimer_0x3fc < 6) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 12) {
        mSubPose_0x3fd = 1;
    } else if (mStateTimer_0x3fc < 18) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 24) {
        mSubPose_0x3fd = 2;
    } else if (mStateTimer_0x3fc < 30) {
        mSubPose_0x3fd = 3;
    } else {
        mSubPose_0x3fd = 2;
    }
}

void Player_804A6F38::fn_8026173C() {
    field_0x121c = 0;
    field_0xd84 = 20;
}

// The carry walk cycle (mWalkAnimCounter_0x3f8 over 24 frames, 48 when slow) and the held thing's
// position: pose lbl_8046B790[facing][step], offset lbl_8046B7B0[pose] from mPos.
void Player_804A6F38::updateCarryWalk_8026152C(bool slow) {
    if (epsilonEquals(mPos, mLastPos_0x14, JGeometry::TUtilf::epsilon()) && mKeptVel_0x368.isZero()) {
        mWalkAnimCounter_0x3f8 = 0;
    } else {
        mWalkAnimCounter_0x3f8++;
        if (slow) {
            if (mWalkAnimCounter_0x3f8 >= 48) {
                mWalkAnimCounter_0x3f8 = 0;
            }
        } else if (mWalkAnimCounter_0x3f8 >= 24) {
            mWalkAnimCounter_0x3f8 = 0;
        }
    }
    if (slow) {
        mWalkFrame_0x3f9 = lbl_8046B790[mFacing_0x3dc][mWalkAnimCounter_0x3f8 / 6];
    } else {
        mWalkFrame_0x3f9 = lbl_8046B790[mFacing_0x3dc][mWalkAnimCounter_0x3f8 / 3];
    }
    const f32* offset = lbl_8046B7B0[mWalkFrame_0x3f9];
    mHeldPos_0x2f4.set(offset[0], offset[1], offset[2]);
    mHeldPos_0x2f4 += mPos;
}

void Player_804A6F38::fn_80261480() {
    if (field_0xc9c <= 0 && field_0xd74 == 0 && !(hasLeader_80240808() && fn_802486A0() == 5)) {
        if (fn_80260C08()) {
            field_0xd74 = 3;
            field_0x1174 = 1;
        } else if (fn_802604CC()) {
            field_0xd74 = 3;
            field_0x1174 = 1;
        } else {
            fn_8026022C();
        }
    }
}

void Player_804A6F38::fn_80261468() {
    if (field_0xd74 > 0) {
        field_0xd74--;
    }
}

// FIXME: 97.18 %, scheduling+regalloc; behaviour drift.
// scheduling+regalloc: the facing-table vector loads (as in fn_8026022C) and
// the side probes' argument loads (player / layer before the x / y stores in the original).
// Pick up what lies on the tile in front (16 along the facing): a wall tile the
// background rewriter takes (fn_8029E888), else the tile's object, spawned and
// lifted at once (state 0x11): grass, stone, a disguised stone, sign, rock, pot or
// coconut by the tile kind fn_802A02A8 returns.
bool Player_804A6F38::fn_80260C08() {
    if (fn_8022B1F8()) {
        return false;
    }
    if (!canRequestState_80251BFC(0x11)) {
        return false;
    }
    const Vec& d = lbl_8046B83C[mFacing_0x3dc];
    JGeometry::TVec3f pos;
    pos.x = d.x;
    pos.y = d.y;
    pos.z = d.z;
    pos += mPos;
    pos.z = 0.0f;
    if (fn_8022C1D8(mPlayerNo_0x4) || fn_8022C0A0(mPlayerNo_0x4)) {
        if (viewLayerFlag(mLayer_0x3b4)) {
            if (mFacing_0x3dc == 3 || mFacing_0x3dc == 1 || mFacing_0x3dc == 0) {
                if (gBgRewrite_8053AC98->fn_8029E888(&pos, mPlayerNo_0x4, mLayer_0x3b4) == 1) {
                    return true;
                }
            }
        } else if (mFacing_0x3dc == 3) {
            if (fn_80247330() < 0.0f) {
                JGeometry::TVec3f left(pos.x - 16.0f, pos.y, pos.z);
                if (gBgRewrite_8053AC98->fn_8029E888(&left, mPlayerNo_0x4, mLayer_0x3b4) == 1) {
                    return true;
                }
            } else if (fn_80247330() > 0.0f) {
                JGeometry::TVec3f right(16.0f + pos.x, pos.y, pos.z);
                if (gBgRewrite_8053AC98->fn_8029E888(&right, mPlayerNo_0x4, mLayer_0x3b4) == 1) {
                    return true;
                }
            }
            if (gBgRewrite_8053AC98->fn_8029E888(&pos, mPlayerNo_0x4, mLayer_0x3b4) == 1) {
                return true;
            }
        } else if (mFacing_0x3dc == 1 || mFacing_0x3dc == 0) {
            if (fn_8024712C() > 0.0f) {
                JGeometry::TVec3f down(pos.x, pos.y - 16.0f, pos.z);
                if (gBgRewrite_8053AC98->fn_8029E888(&down, mPlayerNo_0x4, mLayer_0x3b4) == 1) {
                    return true;
                }
            }
        }
    }
    if (fn_8022C1D8(mPlayerNo_0x4) && gBgRewrite_8053AC98->fn_802A01F0(&pos, mPlayerNo_0x4, mLayer_0x3b4)) {
        return true;
    }
    if (fn_8022C0A0(mPlayerNo_0x4)) {
        s32 kind = gBgRewrite_8053AC98->fn_802A02A8(&pos, mPlayerNo_0x4, mLayer_0x3b4);
        s32 cell = getCellIndexVec3_80226AE0(&pos, mLayer_0x3b4);
        switch (kind) {
        case 0:
        case 1:
        case 2:
        case 3: {
            s32 serial;
            if (kind == 3) {
                if (!getGameView_802D6A9C()->fn_802D6AC4(cell)) {
                    serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_GRAS, mLayer_0x3b4, &pos, 3, -1, mPlayerNo_0x4);
                    getGameView_802D6A9C()->fn_802D6B24(cell, true);
                } else {
                    serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_GRAS, mLayer_0x3b4, &pos, 4, -1, mPlayerNo_0x4);
                }
            } else {
                serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_GRAS, mLayer_0x3b4, &pos, kind, -1, mPlayerNo_0x4);
            }
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_GRAS;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
            }
            return true;
        }
        case 5: {
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_STON, mLayer_0x3b4, &pos, 0, -1, mPlayerNo_0x4);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_STON;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
            }
            return true;
        }
        case 6: {
            s32 player = mPlayerNo_0x4;
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_SSTN, mLayer_0x3b4, &pos, player << 4, -1, player);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_SSTN;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
            }
            return true;
        }
        case 7: {
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_BORD, mLayer_0x3b4, &pos, 0, -1, mPlayerNo_0x4);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_BORD;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
            }
            return true;
        }
        case 8: {
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_ROCK, mLayer_0x3b4, &pos, 0, -1, mPlayerNo_0x4);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_ROCK;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
                actor->mDrawEnabled = false;
            }
            return true;
        }
        case 12: {
            // A disguised stone, held as a rock.
            s32 player = mPlayerNo_0x4;
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_SSTN, mLayer_0x3b4, &pos, (player << 4) + 1, -1, player);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_ROCK;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
                actor->mDrawEnabled = false;
            }
            return true;
        }
        case 39: {
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_YASI, mLayer_0x3b4, &pos, 0, -1, mPlayerNo_0x4);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_YASI;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
            }
            return true;
        }
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35: {
            s32 index;
            s32 variant = 0;
            if (kind == 34) {
                variant = 1;
            } else if (kind == 35) {
                variant = 2;
            }
            index = kind - 24;
            if (kind >= 29) {
                index = kind - 29;
            }
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_TUBO, mLayer_0x3b4, &pos, (variant << 7) | index, -1, mPlayerNo_0x4);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && requestState_80251AEC(0x11)) {
                mHeldActorId_0xd6c = ACTOR_TUBO;
                mHeldPos_0x2f4 = pos;
                mLifting_0xd64 = 1;
                mHeldActorSerial_0xd68 = serial;
                actor->fn_801F3744(mPlayerNo_0x4);
            }
            return true;
        }
        }
    }
    return false;
}

// FIXME: 94.64 %, missing/extra; behaviour clean.
// FIXME: missing/extra (16 fewer): TVec2f operator+ temporaries are elided into pos in ours
// but kept on the stack in the original (sp+0x38, sp+0x28, sp+0x20).
// Lift another player standing in front (16 along the facing, state 0x11, 'PLAY'): it
// must be present and free, and both its spot seen from this layer and the midpoint of
// the two (on both layers) must not be blocked (fn_802260DC / fn_802684A4 unless tile
// 0x20); in a group or across layers the other player's fn_80231230 / fn_802312CC.
bool Player_804A6F38::fn_802604CC() {
    if (fn_8022B1F8()) {
        return false;
    }
    if (fn_8022AF10()) {
        return false;
    }
    if (fn_802486A0() != 4 && hasLeader_80240808()) {
        return false;
    }
    if (gGameRoot_8053AAF8->field_0x7e == 0 && fn_802486A0() != 4) {
        return false;
    }
    if (fn_8022C0A0(mPlayerNo_0x4)) {
        const Vec& a = lbl_8046B86C[mFacing_0x3dc];
        JGeometry::TVec3f ahead(a.x, a.y, a.z);
        ahead += mPos;
        ahead.z = 0.0f;
        for (int i = 0; i < 4; i++) {
            if (i == mPlayerNo_0x4 || !isPlayerActive_8023F264(i) || fn_8023EFEC(i) || fn_8023EA28(i) || fn_80233DA4(i)
                || fn_80230188(i) || fn_80236AB4(i) || fn_802354E4(i) || fn_8022D894(i)) {
                continue;
            }
            if (mLayer_0x3b4 == 8 && getPlayerLayer_8023DE58(i) < 8 && fn_8022CAF8(i) != 4 && hasPlayerLeader_8022F514(i)) {
                continue;
            }
            if (fn_802486A0() != 4 && arePlayersInSameGroup_8022E438(mPlayerNo_0x4, i) && fn_80246F28() > 0.0f) {
                continue;
            }
            // This player's position on the other player's layer.
            JGeometry::TBox2f* box = layerBox(getPlayerLayer_8023DE58(i));
            JGeometry::TBox2f* myBox = layerBox(mLayer_0x3b4);
            JGeometry::TVec2f pos;
            pos = xy(mPos) - myBox->i + box->i;
            if ((fn_802260DC(&pos, getPlayerLayer_8023DE58(i), true)
                 || fn_802684A4((const JGeometry::TVec3f*)&pos, getPlayerLayer_8023DE58(i)))
                && getTileCollisionCodeAlt_802265FC((const JGeometry::TVec3f*)&pos, getPlayerLayer_8023DE58(i)) != 0x20) {
                continue;
            }
            // The midpoint of the two players, relative to the view corners.
            box = layerBox(getPlayerLayer_8023DE58(i));
            JGeometry::TVec3f* other = getPlayerPos_8023E724(i);
            myBox = layerBox(mLayer_0x3b4);
            JGeometry::TVec2f mid = xy(mPos) - myBox->i + xy(*other) - box->i;
            mid.scale(0.5f);
            box = layerBox(getPlayerLayer_8023DE58(i));
            pos = mid + box->i;
            if ((fn_802260DC(&pos, getPlayerLayer_8023DE58(i), true)
                 || fn_802684A4((const JGeometry::TVec3f*)&pos, getPlayerLayer_8023DE58(i)))
                && getTileCollisionCodeAlt_802265FC((const JGeometry::TVec3f*)&pos, getPlayerLayer_8023DE58(i)) != 0x20) {
                continue;
            }
            if (getLayer() != getPlayerLayer_8023DE58(i)) {
                myBox = layerBox(mLayer_0x3b4);
                pos = mid + myBox->i;
                if ((fn_802260DC(&pos, mLayer_0x3b4, true) || fn_802684A4((const JGeometry::TVec3f*)&pos, mLayer_0x3b4))
                    && getTileCollisionCodeAlt_802265FC((const JGeometry::TVec3f*)&pos, getPlayerLayer_8023DE58(i)) != 0x20) {
                    continue;
                }
            }
            JGeometry::TBox2f at(ahead.x, ahead.y, ahead.x, ahead.y);
            if (!fn_80239C9C(i, &at, mLayer_0x3b4)) {
                continue;
            }
            if (isPlayerBitSet(gGameRoot_8053AAF8, i) || arePlayersInSameGroup_8022E438(mPlayerNo_0x4, i)
                || (gGameRoot_8053AAF8->field_0x7e != 0 && fn_8022CAF8(i) == 4)
                || (mLayer_0x3b4 == 8 && getPlayerLayer_8023DE58(i) < 8)) {
                if ((isPlayerDead_80236B50(i) || canRequestState_80251BFC(0x19)) && requestState_80251AEC(0x11)) {
                    if (isPlayerDead_80236B50(i)) {
                        fn_80231230(i, mPlayerNo_0x4);
                    } else {
                        fn_802312CC(i, mPlayerNo_0x4);
                    }
                    mHeldPos_0x2f4 = ahead;
                    mLifting_0xd64 = 1;
                    mHeldPlayer_0xd70 = i;
                    mHeldActorId_0xd6c = 'PLAY';
                    playSound_8024F0E8(0x20022);
                    fn_802702A4();
                    return true;
                }
            } else {
                if (isPlayerDead_80236B50(i)) {
                    if (requestState_80251AEC(0x11)) {
                        fn_80231230(i, mPlayerNo_0x4);
                        mHeldPos_0x2f4 = ahead;
                        mLifting_0xd64 = 1;
                        mHeldPlayer_0xd70 = i;
                        mHeldActorId_0xd6c = 'PLAY';
                        playSound_8024F0E8(0x20022);
                        fn_802702A4();
                    }
                } else {
                    fn_802312CC(i, mPlayerNo_0x4);
                }
                return true;
            }
        }
    }
    return false;
}

// FIXME: 98.48 %, scheduling+regalloc; behaviour clean.
// scheduling+regalloc: the two facing-table vectors are loaded x, y, z and
// stored interleaved with the mPos add in the original (ours loads z first); the argument
// loads of fn_80272B4C's copy differ in order.
// The lift / push start (from fn_80261480): with a wall behind (fn_802265DC / fn_802684A4
// at 10 against the facing) or fn_8024B24C, lift (0x11, nothing held yet) when
// fn_80246F28 is 0, else 0x4F; with the tile 9.999 ahead blocked, push (0x1A). In state
// 0x4F (fn_8022C004) push through fn_8025190C / fn_802517F8 or fn_8025197C.
void Player_804A6F38::fn_8026022C() {
    const Vec& a = lbl_8046B89C[mFacing_0x3dc];
    JGeometry::TVec3f ahead;
    ahead.x = a.x;
    ahead.y = a.y;
    ahead.z = a.z;
    ahead += mPos;
    ahead.z = 0.0f;
    const Vec& b = lbl_8046B8CC[mFacing_0x3dc];
    JGeometry::TVec3f behind;
    behind.x = b.x;
    behind.y = b.y;
    behind.z = b.z;
    behind += mPos;
    if (field_0x1174 == 0 && mLifting_0xd64 == 0 && field_0xd74 == 0) {
        if (fn_8022BF68(mPlayerNo_0x4)) {
            if (!fn_802265DC(&behind, mLayer_0x3b4, true) && !fn_802684A4(&behind, mLayer_0x3b4) && !fn_8024B24C()
                // fn_80272B4C gets a copy of ahead (the original copies it to sp 0x8 first).
            && (fn_802265DC(&ahead, mLayer_0x3b4, true) || fn_80272B4C(&JGeometry::TVec3f(ahead)))) {
                requestState_80251AEC(0x1A);
            } else if (fn_80246F28() == 0.0f) {
                if (!fn_8022B1F8() && requestState_80251AEC(0x11)) {
                    mLifting_0xd64 = 1;
                    mHeldActorSerial_0xd68 = -1;
                    mHeldActorId_0xd6c = 'NULL';
                }
            } else if (!fn_8022B244() || fn_802416C4() == 5) {
                requestState_80251AEC(0x4F);
            }
        } else if (mState_0x1240 == 0x4F && fn_8022C004(mPlayerNo_0x4) && !fn_8024B24C()) {
            if (viewLayerFlag(mLayer_0x3b4)) {
                if (fn_8025190C(true) || fn_802517F8(false)) {
                    requestState_80251AEC(0x1A);
                }
            } else if (fn_8025197C(true)) {
                requestState_80251AEC(0x1A);
            }
        }
    }
}

// Grab another player: lift it (state 0x19) when it can't take part in a pair
// ('TSNO', "two-sword motion", docs/names/actors.tsv), else pair up with it.
bool Player_804A6F38::fn_802600A0(s32 player) {
    if (isDeadState_80270A8C()) {
        return false;
    }
    if (!isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4) && !arePlayersInSameGroup_8022E438(mPlayerNo_0x4, player)
        && (gGameRoot_8053AAF8->field_0x7e == 0 || fn_802486A0() != 4)
        && (getPlayerLayer_8023DE58(player) != 8 || mLayer_0x3b4 >= 8)) {
        mLeader_0xd60 = fn_8022F340(player);
        mPadPlayer_0x11ec = fn_8022F340(player);
        getActorManager_801F856C()->spawnActor_801F9484(ACTOR_TSNO, mLayer_0x3b4, &mPos, (player & 3) | (mPlayerNo_0x4 << 2), -1, player);
        if (player > mPlayerNo_0x4) {
            fn_80233F78(getLeader_80240644());
        }
    } else if (requestState_80251AEC(0x19)) {
        mHeldPlayer_0xd70 = player;
        if (fn_802486A0() != 4) {
            for (int i = 0; i < 4; i++) {
                if (fn_80243194(i) && i != mPlayerNo_0x4) {
                    fn_80230C6C(i);
                }
            }
        }
    } else {
        return false;
    }
    endFormActor_80248FF0();
    return true;
}

bool Player_804A6F38::fn_80260048(s32 arg) {
    if (!isDeadState_80270A8C()) {
        return false;
    }
    mHeldPlayer_0xd70 = arg;
    endFormActor_80248FF0();
    return true;
}

bool Player_804A6F38::fn_8026000C() {
    fn_8025F698(false);
    return requestState_80251AEC(0);
}

bool Player_804A6F38::fn_8025FF70() {
    if (!isLiftingPlayer_8025F590()) {
        return false;
    }
    requestState_80251AEC(0);
    mLifting_0xd64 = 0;
    mHeldPlayer_0xd70 = -1;
    mHeldActorId_0xd6c = 'NULL';
    return true;
}

bool Player_804A6F38::fn_8025FEDC() {
    if (!isGrabbing_8025F448()) {
        return false;
    }
    requestState_80251AEC(0);
    mHeldPlayer_0xd70 = -1;
    mHeldActorSerial_0xd68 = -1;
    mPos.z = 0.0f;
    return true;
}

// Let go of the held player: take its position, moved from its layer's view corner
// to this player's, and remember pos as the drop point.
bool Player_804A6F38::fn_8025FDD0(const JGeometry::TVec3f* pos) {
    mPos = *fn_8023E7B0(mHeldPlayer_0xd70);
    JGeometry::TBox2f* box = layerBox(getPlayerLayer_8023DE58(mHeldPlayer_0xd70));
    f32 x = box->i.x;
    f32 y = box->i.y;
    box = layerBox(mLayer_0x3b4);
    f32 x2 = box->i.x;
    f32 y2 = box->i.y;
    mPos.x += x2 - x;
    mPos.y += y2 - y;
    mHeldPlayer_0xd70 = -1;
    field_0xd58 = 5;
    field_0xd78 = *pos;
    return true;
}

// The state is compared as the member each time: read once into a local, MWCC folds
// 0x11-0x16 into one range where the original tests 0x11 on its own (RULES_CARD).
// Whether the requested state makes the player let go of what it holds (true unless the
// state is one of the lift / carry / throw states 0x11-0x18, 0x1B-0x1D, a screen change
// 0x20-0x25 while holding something other than a player, 0x1E / 0x1F holding a small
// key, or 0x4C / 0x4D while holding an actor). fn_8025F698 inlines it.
bool Player_804A6F38::fn_8025FCC8() {
    if (mRequestedState_0x1244 == 0x11 || mRequestedState_0x1244 == 0x12 || mRequestedState_0x1244 == 0x13 || mRequestedState_0x1244 == 0x14
        || mRequestedState_0x1244 == 0x15 || mRequestedState_0x1244 == 0x16 || mRequestedState_0x1244 == 0x18 || mRequestedState_0x1244 == 0x1B
        || mRequestedState_0x1244 == 0x1C || mRequestedState_0x1244 == 0x1D
        || (isField0xd64One(*this) && !isLiftingPlayer_8025F590()
            && (mRequestedState_0x1244 == 0x20 || mRequestedState_0x1244 == 0x21 || mRequestedState_0x1244 == 0x22
                || mRequestedState_0x1244 == 0x23 || mRequestedState_0x1244 == 0x24 || mRequestedState_0x1244 == 0x25))
        || (mHeldActorId_0xd6c == ACTOR_KEY0 && (mRequestedState_0x1244 == 0x1E || mRequestedState_0x1244 == 0x1F))
        || (mLifting_0xd64 == 1 && !isLiftingPlayer_8025F590() && (mRequestedState_0x1244 == 0x4C || mRequestedState_0x1244 == 0x4D))) {
        return false;
    }
    return true;
}

// FIXME: 99.85 %, regalloc-only; behaviour clean.
// regalloc: the actor branch's scale factor is in f29 in the original, f31 in ours.
// Let go of what the player holds (from the commit of every state change, with
// checkState, and from fn_8026000C): a held player is thrown 16 a step along the facing
// (snapped to the tile centre across when two of three steps are walls, else at 3.3
// along the facing; nothing when the state is 0x1E / 0x32 or the player has no life),
// an actor gets a speed along the facing by kind (crystal ball / Bombchu 2.5, grass 4,
// dead 6, else 4); then the thrown player's group partners holding no living actor
// leave their states too.
void Player_804A6F38::fn_8025F698(bool checkState) {
    if (mLifting_0xd64 != 0 && (!checkState || fn_8025FCC8())) {
        bool held = true;
        if (mHeldActorId_0xd6c == 'NULL') {
            held = false;
        }
        if (isLiftingPlayer_8025F590()) {
            JGeometry::TVec3f speed;
            JGeometry::TVec3f step;
            const Vec& dir = lbl_8046B8FC[mFacing_0x3dc];
            step.x = dir.x;
            step.y = dir.y;
            step.z = dir.z;
            step.scale(16.0f);
            if (mRequestedState_0x1244 == 0x1E || mRequestedState_0x1244 == 0x32 || getLife_8024189C() <= 0) {
                speed.set(0.0f, 0.0f, 0.0f);
            } else {
                JGeometry::TVec3f pos(*getPlayerPos_8023E724(mHeldPlayer_0xd70));
                int walls = 0;
                for (int i = 0; i < 3; i++) {
                    pos += step;
                    u16 tile = getTileCollisionCode_8022461C(&pos, getPlayerLayer_8023DE58(mHeldPlayer_0xd70));
                    if (tile == 3 || tile == 0xE || tile == 0x3A) {
                        walls++;
                    }
                }
                const Vec& unit = lbl_8046B8FC[mFacing_0x3dc];
                speed.x = unit.x;
                speed.y = unit.y;
                speed.z = unit.z;
                if (walls >= 2) {
                    if (mFacing_0x3dc == 0 || mFacing_0x3dc == 1) {
                        pos.x = (s32)pos.x / 16 * 16 + 8;
                    } else {
                        pos.y = (s32)pos.y / 16 * 16 + 8;
                    }
                    JGeometry::TVec3f diff = pos - *getPlayerPos_8023E724(mHeldPlayer_0xd70);
                    speed.set(diff);
                    speed.scale(1.0f / 14.0f);
                } else {
                    speed.scale(3.3f);
                }
            }
            fn_80230D80(mHeldPlayer_0xd70, &speed);
            mHeldPlayer_0xd70 = -1;
            playSound_8024F0E8(0x20014);
        } else {
            ActorBase_804A5770* actor = heldActor();
            if (actor != 0) {
                JGeometry::TVec3f speed;
                const Vec& dir = lbl_8046B92C[mFacing_0x3dc];
                speed.x = dir.x;
                speed.y = dir.y;
                speed.z = dir.z;
                if (actor->mActorId == ACTOR_CRTL || actor->mActorId == ACTOR_BMTY) {
                    speed.scale(2.5f);
                } else if (actor->mActorId == ACTOR_GRAS) {
                    speed.scale(4.0f);
                } else if (actor->field_0x1a4 == 0) {
                    speed.scale(6.0f);
                } else {
                    speed.scale(4.0f);
                }
                actor->fn_801F3708(&speed);
                playSound_8024F0E8(0x20014);
            }
            mHeldActorSerial_0xd68 = -1;
        }
        mLifting_0xd64 = 0;
        mHeldActorId_0xd6c = 'NULL';
        field_0xd74 = 3;
        if (held == true && fn_802486A0() != 4 && isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4)) {
            for (int i = 0; i < 4; i++) {
                if (arePlayersInSameGroup_8022E438(mPlayerNo_0x4, i) && i != mPlayerNo_0x4) {
                    ActorBase_804A5770* other = getActorManager_801F856C()->findActorBySerial_801F666C(fn_8023EF50(i));
                    if (other != 0 && other->field_0x1a4 <= 0) {
                        fn_80230C6C(i);
                    }
                }
            }
        }
    }
}

// Lifting something (mLifting_0xd64 == 1).
bool Player_804A6F38::isLifting_8025F684() {
    return isField0xd64One(*this);
}

s32 Player_804A6F38::fn_8025F668() {
    if (mLifting_0xd64 == 1) {
        return mHeldActorSerial_0xd68;
    }
    return -1;
}

s32 Player_804A6F38::fn_8025F648() {
    if (mLifting_0xd64 == 1) {
        return mHeldActorId_0xd6c;
    }
    return 'NULL';
}

void Player_804A6F38::fn_8025F5CC() {
    if (mLifting_0xd64 != 0) {
        s32 serial = mHeldActorSerial_0xd68;
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0) {
                actor->mAlive = false;
                mHeldActorId_0xd6c = 'NULL';
                mLifting_0xd64 = 0;
                mHeldActorSerial_0xd68 = -1;
            }
        }
    }
}

// Holding a player: mLifting_0xd64 == 1 with a player index in mHeldPlayer_0xd70 (inlined by most
// functions of the unit).
bool Player_804A6F38::isLiftingPlayer_8025F590() {
    if (isField0xd64One(*this) == 1 && mHeldPlayer_0xd70 >= 0 && mHeldPlayer_0xd70 < 4) {
        return true;
    }
    return false;
}

s32 Player_804A6F38::fn_8025F55C() {
    if (isField0xd64One(*this) == 1 && mHeldPlayer_0xd70 >= 0 && mHeldPlayer_0xd70 < 4) {
        return mHeldPlayer_0xd70;
    }
    return -1;
}

s32 Player_804A6F38::fn_8025F534() {
    if (mLifting_0xd64 == 1 || mHeldPlayer_0xd70 < 0 || mHeldPlayer_0xd70 >= 4) {
        return -1;
    }
    return mHeldPlayer_0xd70;
}

bool Player_804A6F38::fn_8025F500() {
    if (isField0xd64One(*this) == 1 && mHeldActorSerial_0xd68 >= 0) {
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_8025F484() const {
    if (isField0xd64One(*this) == 1) {
        s32 serial = mHeldActorSerial_0xd68;
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != 0 && actor->field_0x1a4 > 0) {
                return true;
            }
            return false;
        }
    }
    return false;
}

// Holding a player (mHeldPlayer_0xd70) or an actor (mHeldActorSerial_0xd68) without lifting it
// (mLifting_0xd64 == 0), i.e. grabbing it. fn_8025EE48, fn_8025EFFC and fn_80261750 inline it.
bool Player_804A6F38::isGrabbing_8025F448() {
    if ((mHeldPlayer_0xd70 >= 0 && mHeldPlayer_0xd70 < 4 || mHeldActorSerial_0xd68 >= 0) && mLifting_0xd64 == 0) {
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_8025F418() {
    if (mHeldPlayer_0xd70 >= 0 && mHeldPlayer_0xd70 < 4 && mLifting_0xd64 == 0) {
        return true;
    }
    return false;
}

// Whether the player may pick something up (not down, holding, grabbing, busy (0xCE8 /
// 0x454), in the air, or a follower in mode 5 without a pending 0xD88). fn_8025EFFC
// inlines it.
bool Player_804A6F38::fn_8025F2CC() {
    if (isDeadState_80270A8C()) {
        return false;
    }
    if (isLifting_8025F684()) {
        return false;
    }
    if (isGrabbing_8025F448()) {
        return false;
    }
    if (fn_8024BBB0()) {
        return false;
    }
    if (fn_80241230()) {
        return false;
    }
    if (fn_802412A4()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (mPos.z > 0.0f) {
        return false;
    }
    if (field_0xd88 < 0 && hasLeader_80240808() && fn_802486A0() == 5) {
        return false;
    }
    return true;
}

// FIXME: 98.89 %, scheduling; behaviour clean.
// scheduling: the original sets up r3 (mr r3, r29) for requestState_80251AEC before
// the li / beq choosing 0x12 / 0x11; ours after (`lift ? 0x11 : 0x12` gives neg / or / srawi).
// Lift the actor `serial` (state 0x11, or 0x12 with lift false). With grabOnly, a
// player already holding or grabbing only claims it through fn_8025EF68.
bool Player_804A6F38::fn_8025EFFC(s32 serial, bool lift, bool grabOnly) {
    if (grabOnly) {
        if (isLifting_8025F684() || isGrabbing_8025F448() || fn_80241230() || fn_802412A4()) {
            fn_8025EF68(serial);
            return true;
        }
    } else if (!fn_8025F2CC()) {
        return false;
    }
    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
    if (actor != 0) {
        s32 state = 0x12;
        if (lift) {
            state = 0x11;
        }
        if (requestState_80251AEC(state)) {
            field_0xd88 = -1;
            mHeldActorId_0xd6c = actor->mActorId;
            mHeldPos_0x2f4 = actor->mPos;
            mLifting_0xd64 = 1;
            mHeldActorSerial_0xd68 = serial;
            actor->fn_801F3744(mPlayerNo_0x4);
            field_0xd74 = 3;
            field_0x1174 = 1;
            playSound_8024F0E8(0x20022);
            return true;
        }
    }
    return false;
}

bool Player_804A6F38::fn_8025EF68(s32 arg) {
    if (field_0xd88 == arg) {
        return true;
    }
    if (field_0xd88 >= 0) {
        return false;
    }
    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(arg);
    if (actor != 0) {
        if (actor->fn_801F36D4(mPlayerNo_0x4)) {
            field_0xd88 = arg;
            return true;
        }
        return false;
    }
    return false;
}

// Grab the actor `serial` (state 0x19).
bool Player_804A6F38::fn_8025EE48(s32 serial) {
    if (isDeadState_80270A8C()) {
        return false;
    }
    if (isLifting_8025F684()) {
        return false;
    }
    if (isGrabbing_8025F448()) {
        return false;
    }
    if (fn_8024BBB0()) {
        return false;
    }
    if (fn_80241230()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (mHeldActorSerial_0xd68 < 0 && requestState_80251AEC(0x19)) {
        mHeldActorSerial_0xd68 = serial;
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_8025EE34(s32 arg) {
    return mHeldActorSerial_0xd68 == arg;
}

void Player_804A6F38::fn_8025EE0C(s32 a, const JGeometry::TVec3f* b) {
    if (mHeldActorSerial_0xd68 == a) {
        field_0x300.set(b->x, b->y, b->z);
    }
}

bool Player_804A6F38::fn_8025EDD4(s32 a, const JGeometry::TVec3f* pos) {
    if (mHeldActorSerial_0xd68 == a) {
        mHeldActorSerial_0xd68 = -1;
        field_0xd78 = *pos;
        return true;
    }
    return false;
}

// Whether the held thing can be put down at mHeldPos_0x2f4: a player held across from
// a GBA layer (8) is checked on its own layer, with the point moved between the two
// view corners; else the tile there, and for an actor its isBlocked_0x4C. On success the
// drop point takes the player's x / y.
bool Player_804A6F38::fn_8025EB18() {
    if (isLiftingPlayer_8025F590() && mLayer_0x3b4 == 8 && getPlayerLayer_8023DE58(mHeldPlayer_0xd70) < 8) {
        JGeometry::TVec3f pos(mHeldPos_0x2f4);
        JGeometry::TBox2f* box = layerBox(getPlayerLayer_8023DE58(mHeldPlayer_0xd70));
        f32 x = box->i.x;
        f32 y = box->i.y;
        box = layerBox(mLayer_0x3b4);
        f32 x2 = box->i.x;
        f32 y2 = box->i.y;
        pos.x += x - x2;
        pos.y += y - y2;
        u16 tile = getTileCollisionCode_8022461C(&pos, getPlayerLayer_8023DE58(mHeldPlayer_0xd70));
        if ((fn_802265DC(&pos, getPlayerLayer_8023DE58(mHeldPlayer_0xd70), true) && tile != 0xE && tile != 0x39 && tile != 0xD)
            || fn_802684A4(&pos, getPlayerLayer_8023DE58(mHeldPlayer_0xd70))) {
            mHeldPos_0x2f4.x = mPos.x;
            mHeldPos_0x2f4.y = mPos.y;
            return true;
        }
    } else {
        u16 tile = getTileCollisionCode_8022461C(&mHeldPos_0x2f4, mLayer_0x3b4);
        if (isLiftingPlayer_8025F590()) {
            if ((fn_802265DC(&mHeldPos_0x2f4, mLayer_0x3b4, true) && tile != 0xE && tile != 0x39 && tile != 0xD)
                || fn_802684A4(&mHeldPos_0x2f4, mLayer_0x3b4)) {
                mHeldPos_0x2f4.x = mPos.x;
                mHeldPos_0x2f4.y = mPos.y;
                return true;
            }
        } else {
            ActorBase_804A5770* actor = heldActor();
            if (actor != 0 && actor->isBlocked_0x4C(mLayer_0x3b4, &mHeldPos_0x2f4)) {
                mHeldPos_0x2f4.x = mPos.x;
                mHeldPos_0x2f4.y = mPos.y;
                return true;
            }
        }
    }
    return false;
}
