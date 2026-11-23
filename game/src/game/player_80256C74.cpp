#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/act/act_somaria_block.h"
#include "game/Map.h"
#include "game/cls_804A71A8.h"
#include "game/BgRewrite.h"
#include "game/Effect.h"
#include "game/ViewLayer.h"
#include "game/act/act_arrow.h"
#include "game/act/tu_801F9484.h"
#include "game/VecEpsilon.h"
#include "game/BoxTest.h"
#include "game/PlayerBit.h"
#include "JSystem/JParticle/JPAMath.h"

// One entry per facing (0x3DC: right, left, down, up): 8 to the side and 10
// below, read by the enter of state 0x39 (fn_8025E79C).
static const Vec lbl_8046AF00[4] = {
    {-8.0f, 0.0f, -10.0f}, {8.0f, 0.0f, -10.0f}, {0.0f, -8.0f, -10.0f}, {0.0f, 0.0f, -10.0f},
};

// State 0x39's update (fn_8025DACC) reads these three from one base: the run
// frame per two frames of the 16-frame cycle and facing, the eight points around
// the player it burns (BgRewrite fn_802A20F0), and where the actor field_0xbb4 is
// kept (34 ahead).
static const u8 lbl_8046AF30[4][8] = {
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 4, 5, 6},
    {0, 1, 2, 3, 0, 4, 5, 6},
};
static const Vec lbl_8046AF50[8] = {
    {7.999f, 0.0f, 0.0f}, {-7.999f, 0.0f, 0.0f}, {0.0f, 7.999f, 0.0f}, {0.0f, -7.999f, 0.0f},
    {6.0f, 6.0f, 0.0f},   {-6.0f, 6.0f, 0.0f},   {6.0f, -6.0f, 0.0f},  {-6.0f, -6.0f, 0.0f},
};
static const Vec lbl_8046AFB0[4] = {
    {34.0f, 0.0f, 0.0f}, {-34.0f, 0.0f, 0.0f}, {0.0f, 34.0f, 0.0f}, {0.0f, -34.0f, 0.0f},
};

// The recoil hop's speed per facing (0x3DC: right, left, down, up): 2 back
// against the facing and 4 up (enter of state 0x41, fn_8025D7E4).
static const Vec lbl_8046AFE0[4] = {
    {-2.0f, 0.0f, 4.0f}, {2.0f, 0.0f, 4.0f}, {0.0f, -2.0f, 4.0f}, {0.0f, 2.0f, 4.0f},
};

// The charge point per facing of the bow (state 0x3A, fn_8025C894) and the slingshot
// (state 0x3B, fn_8025BEB0); the slingshot's two side shots take their direction
// (argument bits 4-6) from the pairs lbl_8046B070.
static const Vec lbl_8046B010[4] = {
    {12.0f, 18.0f, 20.0f}, {-12.0f, 18.0f, 20.0f}, {2.0f, 28.0f, 20.0f}, {-2.0f, 2.0f, 20.0f},
};
static const Vec lbl_8046B040[4] = {
    {12.0f, 18.0f, 20.0f}, {-12.0f, 18.0f, 20.0f}, {2.0f, 28.0f, 20.0f}, {-2.0f, 2.0f, 20.0f},
};
static const s32 lbl_8046B070[4][2] = {
    {4, 6}, {5, 7}, {4, 5}, {6, 7},
};

// State 0x3C's update (fn_8025B3F0), off one base: the charge point per facing, the
// hit point per facing (uncharged, then charged), and the tile 16 in front.
static const Vec lbl_8046B090[4] = {
    {-16.0f, -2.0f, 16.0f}, {16.0f, -2.0f, 16.0f}, {0.0f, -6.0f, 16.0f}, {0.0f, -4.0f, 16.0f},
};
static const Vec lbl_8046B0C0[8] = {
    {20.0f, 7.0f, 0.0f}, {-20.0f, 7.0f, 0.0f}, {0.0f, 12.0f, 0.0f}, {0.0f, -8.0f, 0.0f},
    {24.0f, 7.0f, 0.0f}, {-24.0f, 7.0f, 0.0f}, {0.0f, 11.0f, 0.0f}, {0.0f, -6.0f, 0.0f},
};
static const Vec lbl_8046B120[4] = {
    {16.0f, 0.0f, 0.0f}, {-16.0f, 0.0f, 0.0f}, {0.0f, 16.0f, 0.0f}, {0.0f, -16.0f, 0.0f},
};

// State 0x44 (fn_8025A15C): the same walk frames and emitter rotations as state
// 0x45's below.
static const u8 lbl_8046B150[4][8] = {
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 4, 5, 6},
    {0, 1, 2, 3, 0, 4, 5, 6},
};
static const s32 lbl_8046B170[4] = {0xC000, 0x4000, 0x8000, 0x0000};
static const s32 lbl_8046B180[4] = {0xC000, 0x4000, 0x0000, 0x8000};

// State 0x45 (fn_80259830): the walk frame (0x3F9) for each eighth of the
// 28-frame cycle 0x3F8, per facing (0x3DC).
static const u8 lbl_8046B190[4][8] = {
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 0, 4, 5, 6},
    {0, 1, 2, 3, 0, 4, 5, 6},
};

// State 0x45: the y / z rotations of the emitters 0x12C4 / 0x12C8 per facing.
static const s32 lbl_8046B1B0[4] = {0xC000, 0x4000, 0x8000, 0x0000};
static const s32 lbl_8046B1C0[4] = {0xC000, 0x4000, 0x0000, 0x8000};

// Not read by any function of the unit (between state 0x45's tables and the dig
// effects; kept at its original place and size).
static const Vec lbl_8046B1D0[4] = {
    {-1.0f, 0.0f, -8.0f}, {1.0f, 0.0f, -8.0f}, {5.0f, 0.0f, -8.0f}, {-5.0f, 0.0f, -8.0f},
};

// The dig effect per result of fn_802A1844 (1-4) and facing (fn_80258890).
static const u16 lbl_8046B200[4][4] = {
    {0x137, 0x132, 0x12D, 0x13C},
    {0x135, 0x134, 0x133, 0x136},
    {0x13A, 0x139, 0x138, 0x13B},
    {0x130, 0x12F, 0x12E, 0x131},
};

// One step of 12.999 along each facing (0: +x, 1: -x, 2: +y, 3: -y): where item 8
// ('HBMB') is placed in front of the player (fn_8025785C).
static const Vec lbl_8046B220[4] = {
    {12.999f, 0.0f, 0.0f}, {-12.999f, 0.0f, 0.0f}, {0.0f, 12.999f, 0.0f}, {0.0f, -12.999f, 0.0f},
};

// Inlined by fn_80259660 and fn_80258BE8 (no out-of-line copy).
inline void Player_804A6F38::gatherOthers() {
    if (fn_802486A0() == 5 && !hasLeader_80240808()) {
        for (int i = 0; i < 4; i++) {
            if (fn_80243194(i) && i != mPlayerNo_0x4) {
                fn_8023E690(i, &mPos);
            }
        }
        resetPosHistory_8024BAEC();
    }
}

void Player_804A6F38::fn_8025E79C() {
    fn_8027221C(6);
    field_0x458 = 1;
    if (hasLeader_80240808() && fn_802486A0() != 4) {
        s32 facing = getPlayerFacing_80230CFC(getLeader_80240644());
        mFacing_0x3dc = facing;
        mMoveDir_0x3e8 = facing;
    } else if (fn_80246B10(2)) {
        mFacing_0x3dc = 0;
        mMoveDir_0x3e8 = 0;
    } else if (fn_80246B10(1)) {
        mFacing_0x3dc = 1;
        mMoveDir_0x3e8 = 1;
    } else if (fn_80246B10(8)) {
        mFacing_0x3dc = 3;
        mMoveDir_0x3e8 = 3;
    } else if (fn_80246B10(4)) {
        mFacing_0x3dc = 2;
        mMoveDir_0x3e8 = 2;
    }
    // The result is unused, as in the original (it computes it and drops it). The
    // entry is copied component by component (three lfs / stfs), not with TVec3f(Vec).
    const Vec& offset = lbl_8046AF00[mFacing_0x3dc];
    JGeometry::TVec3f pos;
    pos.x = offset.x;
    pos.y = offset.y;
    pos.z = offset.z;
    pos += mPos;
    field_0xba8 = 0;
    mAnimTimer_0x3fa = 0;
    field_0xbac = 0;
}

// FIXME: 98.90 %, missing/extra; behaviour clean.
// Structurally close (2 extra instructions): the burn loop's point is copied
// once more (operator+'s result to the stack before pos; the original builds pos in place,
// frame 0x100, ours 0x110) and field_0x4 / mLayer_0x3b4 are loaded after the sum; the
// actor's offset reads the table x before y.
// State 0x39 (item 1; enter fn_8025E79C, exit fn_8025D9FC): a dash. The ground under
// the player (on a view-flag layer the probe 2 above the hand point) picks the trail
// emitters: 0x3D4 (field_0x1368) on tile 0x39, else 0x116 (field_0x1360, callback
// field_0x1398) and on plain ground 0x115 (field_0x1364) unless fn_8026FC20 is 2. At
// level >= 2 off view-flag layers it follows a ledge run: a follower copies the
// leader's count (fn_8023A1A4) into field_0xbac and at count 4 jumps to the leader
// (field_0xba8); a leader with its player bit set and field_0xbf0->field_0xb8 looks 16 a
// step along the facing, inside the view box less 16, for the ledge tile of that facing
// (0x12, 0x13, 0x10, 0x11; then setGroupMode_80248004(5, true)). The first 10 frames wind up (sound
// by ground, walk frame per two frames) and end when the button is let go; then the
// player runs (fn_8025D0B0) with sounds by ground, burning the eight points around it
// (BgRewrite fn_802A20F0, lbl_8046AF50), until field_0xba8 counts down; a side flag
// along the facing or a player hit ahead (arePlayersInSameGroup_8022E438 / fn_802350A0) recoils (0x41).
// The actor field_0xbb4 is kept 34 ahead (lbl_8046AFB0).
void Player_804A6F38::fn_8025DACC() {
    setFacingBoxes_80276E5C();
    fn_802709C8();
    u16 tile;
    if (viewLayerFlag(mLayer_0x3b4)) {
        JGeometry::TVec3f probe;
        fn_8024B298(2, &probe);
        probe.y += 2.0f;
        tile = getTileCollisionCode_8022461C(&probe, mLayer_0x3b4);
    } else {
        tile = getTileCollisionCode_8022461C(&mPos, mLayer_0x3b4);
    }
    if (tile == 0x39) {
        if (field_0x1368 == NULL) {
            field_0x1368 = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x3d4, 0, -1, &field_0x13a8, NULL, true, 1.0f);
        } else {
            setEmitterPos_8013CB44(field_0x1368, &mPos, 0, -1, true);
        }
        if (field_0x1360 != NULL) {
            field_0x1360->becomeInvalidEmitter();
            field_0x1360 = NULL;
        }
        if (field_0x1364 != NULL) {
            field_0x1364->becomeInvalidEmitter();
            field_0x1364 = NULL;
        }
    } else if (tile == 2 || tile == 3) {
        if (field_0x1360 == NULL) {
            field_0x1360 = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x116, 0, -1, &field_0x1398, NULL, true, 1.0f);
        } else {
            setEmitterPos_8013CB44(field_0x1360, &mPos, 0, -1, true);
        }
        if (field_0x1364 != NULL) {
            field_0x1364->becomeInvalidEmitter();
            field_0x1364 = NULL;
        }
        if (field_0x1368 != NULL) {
            field_0x1368->becomeInvalidEmitter();
            field_0x1368 = NULL;
        }
    } else {
        if (field_0x1360 == NULL) {
            field_0x1360 = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x116, 0, -1, &field_0x1398, NULL, true, 1.0f);
        } else {
            setEmitterPos_8013CB44(field_0x1360, &mPos, 0, -1, true);
        }
        if (fn_8026FC20() == 2) {
            if (field_0x1364 != NULL) {
                field_0x1364->becomeInvalidEmitter();
                field_0x1364 = NULL;
            }
        } else if (field_0x1364 == NULL) {
            field_0x1364 = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x115, 0, -1, NULL, NULL, true, 1.0f);
        } else {
            setEmitterPos_8013CB44(field_0x1364, &mPos, 0, -1, true);
        }
        if (field_0x1368 != NULL) {
            field_0x1368->becomeInvalidEmitter();
            field_0x1368 = NULL;
        }
    }
    if (fn_802415CC() >= 2 && !viewLayerFlag(mLayer_0x3b4)) {
        if (hasLeader_80240808()) {
            s32 count = fn_8023A1A4(getLeader_80240644());
            if (count > 0) {
                if (field_0xbac == 0 && !field_0xce8) {
                    field_0xce8 = true;
                    field_0xbac = count;
                } else if (field_0xbac > 0) {
                    field_0xbac = count;
                }
            }
            if (fn_8026FC20() == 4 && field_0xba8 == 0) {
                field_0xba8 = 1;
                JGeometry::TVec3f* leader = getPlayerPos_8023E724(getLeader_80240644());
                mPos.x = leader->x;
                mPos.y = leader->y;
                mPos.z = leader->z;
            }
        } else if (isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4) && field_0xbf0->field_0xb8) {
            if (!field_0xce8 && field_0xbac == 0) {
                bool found = false;
                JGeometry::TVec3f pos = mPos;
                JGeometry::TBox2f box;
                box = *layerBox(mLayer_0x3b4);
                growBox(box, -16.0f);
                do {
                    switch (mFacing_0x3dc) {
                    case 0:
                        pos.x += 16.0f;
                        if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x12) {
                            found = true;
                        }
                        break;
                    case 1:
                        pos.x -= 16.0f;
                        if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x13) {
                            found = true;
                        }
                        break;
                    case 2:
                        pos.y += 16.0f;
                        if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x10) {
                            found = true;
                        }
                        break;
                    case 3:
                        pos.y -= 16.0f;
                        if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x11) {
                            found = true;
                        }
                        break;
                    }
                } while (!found && containsPoint(box, pos));
                if (found) {
                    field_0xce8 = true;
                    setGroupMode_80248004(5, true);
                    field_0xbac++;
                }
            } else if (field_0xbac == 2) {
                bool found = false;
                JGeometry::TVec3f pos = mPos;
                switch (mFacing_0x3dc) {
                case 0:
                    pos.x += 16.0f;
                    if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x12) {
                        found = true;
                    }
                    break;
                case 1:
                    pos.x -= 16.0f;
                    if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x13) {
                        found = true;
                    }
                    break;
                case 2:
                    pos.y += 16.0f;
                    if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x10) {
                        found = true;
                    }
                    break;
                case 3:
                    pos.y -= 16.0f;
                    if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x11) {
                        found = true;
                    }
                    break;
                }
                if (found) {
                    field_0xbac++;
                }
            }
        } else if (field_0xbac == 1) {
            field_0xbac++;
        } else if (field_0xbac == 3) {
            field_0xbac++;
            field_0xba8 = 1;
            field_0xce8 = false;
        }
    }
    resetPosHistory_8024BAEC();
    if (mAnimTimer_0x3fa < 10) {
        if (tile == 0x39 || tile == 0xd) {
            playSound_8024F0E8(0x2005a);
        } else if (fn_80224B88(&mPos, mLayer_0x3b4)) {
            playSound_8024F0E8(0x2005b);
        } else if (fn_80224E88(&mPos, mLayer_0x3b4)) {
            playSound_8024F0E8(0x2005e);
        } else {
            playSound_8024F0E8(0x20030);
        }
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 0;
        mWalkAnimCounter_0x3f8++;
        if (mWalkAnimCounter_0x3f8 >= 16) {
            mWalkAnimCounter_0x3f8 = 0;
        }
        mWalkFrame_0x3f9 = mWalkAnimCounter_0x3f8 / 2;
        if (!fn_8022C274(mPlayerNo_0x4)) {
            requestState_80251AEC(0);
        }
    } else {
        if (field_0xba8 <= 0) {
            if (field_0xbac == 0 && !fn_8022C274(mPlayerNo_0x4)) {
                field_0xba8 = 1;
            }
        } else {
            field_0xba8--;
            if (field_0xba8 == 0) {
                requestState_80251AEC(0);
            }
        }
        u16 under = getTileCollisionCode_8022461C(&mPos, mLayer_0x3b4);
        if (under == 0x39 || under == 0xd) {
            playSound_8024F0E8(0x20052);
        } else if (fn_80224B88(&mPos, mLayer_0x3b4)) {
            playSound_8024F0E8(0x20058);
        } else if (fn_80224E88(&mPos, mLayer_0x3b4)) {
            playSound_8024F0E8(0x2005d);
        } else {
            playSound_8024F0E8(0x20051);
        }
        mPose_0x3fb = 1;
        mWalkAnimCounter_0x3f8++;
        if (mWalkAnimCounter_0x3f8 >= 16) {
            mWalkAnimCounter_0x3f8 = 0;
        }
        mWalkFrame_0x3f9 = lbl_8046AF30[mFacing_0x3dc][mWalkAnimCounter_0x3f8 / 2];
        fn_8025D0B0();
        JGeometry::TVec3f dir = mPos - mLastPos_0x14;
        dir.normalize();
        for (int i = 0; i < 8; i++) {
            JGeometry::TVec3f offset(lbl_8046AF50[i].x, lbl_8046AF50[i].y, lbl_8046AF50[i].z);
            JGeometry::TVec3f pos = mPos + offset;
            gBgRewrite_8053AC98->fn_802A20F0(&pos, mPlayerNo_0x4, mLayer_0x3b4, 0, 1);
        }
    }
    fn_80268B3C();
    if (mAnimTimer_0x3fa >= 10 && (viewLayerFlag(mLayer_0x3b4) || !field_0xbf0->field_0xb8)) {
        if (mFacing_0x3dc == 0) {
            if (field_0xbf0->field_0x48[0]) {
                requestState_80251AEC(0x41);
            }
        } else if (mFacing_0x3dc == 1) {
            if (field_0xbf0->field_0x48[1]) {
                requestState_80251AEC(0x41);
            }
        } else if (mFacing_0x3dc == 2) {
            if (field_0xbf0->field_0x48[2]) {
                requestState_80251AEC(0x41);
            }
        } else if (mFacing_0x3dc == 3) {
            if (field_0xbf0->field_0x48[3]) {
                requestState_80251AEC(0x41);
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        if (arePlayersInSameGroup_8022E438(mPlayerNo_0x4, i) && fn_802350A0(i)) {
            requestState_80251AEC(0x41);
        }
    }
    if (hasLeader_80240808() && fn_802350A0(getLeader_80240644())) {
        requestState_80251AEC(0x41);
    }
    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(field_0xbb4);
    if (actor != NULL) {
        const Vec& ahead = lbl_8046AFB0[mFacing_0x3dc];
        JGeometry::TVec3f offset(ahead.x, ahead.y, ahead.z);
        offset += mPos;
        actor->mPos = offset;
    }
}

void Player_804A6F38::fn_8025D9FC() {
    if (field_0x1360 != NULL) {
        field_0x1360->becomeInvalidEmitter();
        field_0x1360 = NULL;
    }
    if (field_0x1368 != NULL) {
        field_0x1368->becomeInvalidEmitter();
        field_0x1368 = NULL;
    }
    if (field_0x1364 != NULL) {
        field_0x1364->becomeInvalidEmitter();
        field_0x1364 = NULL;
    }
    if (field_0xbac > 0 || fn_8026FC20() > 0) {
        field_0xce8 = false;
    }
    field_0xbac = 0;
    field_0xba8 = 0;
}

void Player_804A6F38::fn_8025D7E4() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
    const Vec& speed = lbl_8046AFE0[mFacing_0x3dc];
    mVel_0x344.set(speed.x, speed.y, speed.z);
    playSound_8024F0E8(0x20010);
    if (mVel_0x344.y != 0.0f) {
        getGameView_802D6A9C()->fn_802D79E8(mLayer_0x3b4, 4.0f, 0.0f, false);
    } else {
        getGameView_802D6A9C()->fn_802D79E8(mLayer_0x3b4, 0.0f, 4.0f, false);
    }
    gGameRoot_8053AAF8->startRumble_80134DE4(1, 0);
    JGeometry::TVec3f offset;
    if (mFacing_0x3dc == 0 || mFacing_0x3dc == 1) {
        offset.set(0.0f, -8.0f, 0.0f);
    } else {
        offset.zero();
    }
    JGeometry::TVec3f dir(-mVel_0x344.x, -mVel_0x344.y, 0.0f);
    dir.normalize();
    const JGeometry::TVec3f& pos = mPos + offset;
    if (mLayer_0x3b4 < 8) {
        gGameRoot2_8053AB10->field_0x20->startHeightGridEffect_802F601C(&pos, 3, &dir, 1.0f, 1.0f);
    }
    field_0xb9c = true;
}

void Player_804A6F38::fn_8025D6AC() {
    if (mStateTimer_0x3fc < 4) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 8) {
        mSubPose_0x3fd = 1;
    } else if (mStateTimer_0x3fc < 12) {
        mSubPose_0x3fd = 2;
    } else {
        mSubPose_0x3fd = 3;
    }
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc == 2) {
        field_0xb9c = false;
    }
    mPos += mVel_0x344;
    mVel_0x344.z -= 0.25f;
    if (mVel_0x344.z < -4.0f) {
        mVel_0x344.z = -4.0f;
    }
    if (mVel_0x344.z <= 0.0f && mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        mStateLocked_0x124d = 0;
        requestState_80251AEC(0);
    }
    fn_80268B3C();
    if (fn_8025197C(true)) {
        mVel_0x344.x = 0.0f;
        mVel_0x344.y = 0.0f;
    }
}

void Player_804A6F38::fn_8025D6A0() {
    field_0xb9c = false;
}

// The step of state 0x39's update (fn_8025DACC): with field_0xbac >= 1, 3 a frame along
// the facing; else along the facing until the pad (fn_8022C82C / fn_8022C76C of the
// player) pushes against it (field_0xba8 = 1), 2.1 diagonally while it pushes across,
// turning to that side after 8 such frames with the facing axis released (the turn
// from a side facing ends it instead while field_0xbf0->field_0xb4 is set, which also
// keeps y).
void Player_804A6F38::fn_8025D0B0() {
    f32 y = mPos.y;
    if (field_0xbac >= 1) {
        switch (mFacing_0x3dc) {
        case 0:
            mPos.x += 3.0f;
            break;
        case 1:
            mPos.x -= 3.0f;
            break;
        case 2:
            mPos.y += 3.0f;
            break;
        case 3:
            mPos.y -= 3.0f;
            break;
        }
        return;
    }
    switch (mFacing_0x3dc) {
    case 0:
        if (fn_8022C82C(mPlayerNo_0x4) < 0.0f) {
            field_0xba8 = 1;
        } else if (fn_8022C76C(mPlayerNo_0x4) > 0.0f) {
            mPos.x += 2.1f;
            mPos.y -= 2.1f;
            if (0.0f == fn_8022C82C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    if (field_0xbf0->field_0xb4) {
                        field_0xba8 = 1;
                    } else {
                        mFacing_0x3dc = 3;
                        mMoveDir_0x3e8 = 3;
                        mStateTimer_0x3fc = 0;
                    }
                }
            }
        } else if (fn_8022C76C(mPlayerNo_0x4) < 0.0f) {
            mPos.x += 2.1f;
            mPos.y += 2.1f;
            if (0.0f == fn_8022C82C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    if (field_0xbf0->field_0xb4) {
                        field_0xba8 = 1;
                    } else {
                        mFacing_0x3dc = 2;
                        mMoveDir_0x3e8 = 2;
                        mStateTimer_0x3fc = 0;
                    }
                }
            }
        } else {
            mPos.x += 3.0f;
            mStateTimer_0x3fc = 0;
        }
        break;
    case 1:
        if (fn_8022C82C(mPlayerNo_0x4) > 0.0f) {
            field_0xba8 = 1;
        } else if (fn_8022C76C(mPlayerNo_0x4) > 0.0f) {
            mPos.x -= 2.1f;
            mPos.y -= 2.1f;
            if (0.0f == fn_8022C82C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    if (field_0xbf0->field_0xb4) {
                        field_0xba8 = 1;
                    } else {
                        mFacing_0x3dc = 3;
                        mMoveDir_0x3e8 = 3;
                        mStateTimer_0x3fc = 0;
                    }
                }
            }
        } else if (fn_8022C76C(mPlayerNo_0x4) < 0.0f) {
            mPos.x -= 2.1f;
            mPos.y += 2.1f;
            if (0.0f == fn_8022C82C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    if (field_0xbf0->field_0xb4) {
                        field_0xba8 = 1;
                    } else {
                        mFacing_0x3dc = 2;
                        mMoveDir_0x3e8 = 2;
                        mStateTimer_0x3fc = 0;
                    }
                }
            }
        } else {
            mPos.x -= 3.0f;
            mStateTimer_0x3fc = 0;
        }
        break;
    case 2:
        if (fn_8022C76C(mPlayerNo_0x4) > 0.0f) {
            field_0xba8 = 1;
        } else if (fn_8022C82C(mPlayerNo_0x4) > 0.0f) {
            mPos.x += 2.1f;
            mPos.y += 2.1f;
            if (0.0f == fn_8022C76C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    mFacing_0x3dc = 0;
                    mMoveDir_0x3e8 = 0;
                    mStateTimer_0x3fc = 0;
                }
            }
        } else if (fn_8022C82C(mPlayerNo_0x4) < 0.0f) {
            mPos.x -= 2.1f;
            mPos.y += 2.1f;
            if (0.0f == fn_8022C76C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    mFacing_0x3dc = 1;
                    mMoveDir_0x3e8 = 1;
                    mStateTimer_0x3fc = 0;
                }
            }
        } else {
            mPos.y += 3.0f;
            mStateTimer_0x3fc = 0;
        }
        break;
    case 3:
        if (fn_8022C76C(mPlayerNo_0x4) < 0.0f) {
            field_0xba8 = 1;
        } else if (fn_8022C82C(mPlayerNo_0x4) > 0.0f) {
            mPos.x += 2.1f;
            mPos.y -= 2.1f;
            if (0.0f == fn_8022C76C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    mFacing_0x3dc = 0;
                    mMoveDir_0x3e8 = 0;
                    mStateTimer_0x3fc = 0;
                }
            }
        } else if (fn_8022C82C(mPlayerNo_0x4) < 0.0f) {
            mPos.x -= 2.1f;
            mPos.y -= 2.1f;
            if (0.0f == fn_8022C76C(mPlayerNo_0x4)) {
                mStateTimer_0x3fc++;
                if (mStateTimer_0x3fc > 8) {
                    mFacing_0x3dc = 1;
                    mMoveDir_0x3e8 = 1;
                    mStateTimer_0x3fc = 0;
                }
            }
        } else {
            mPos.y -= 3.0f;
            mStateTimer_0x3fc = 0;
        }
        break;
    }
    if (field_0xbf0->field_0xb4) {
        mPos.y = y;
    }
}

void Player_804A6F38::fn_8025D090() {
    field_0x458 = 0;
    mVel_0x344.x = 0.0f;
    mVel_0x344.y = 0.0f;
    mVel_0x344.z = 0.0f;
    field_0x40c = 0;
}

// State 0x3A (item 4, the bow; enter fn_8025D090, exit fn_8025C7BC): the slingshot's
// state 0x3B with arrows. The charge (lbl_8046B010 for the point) runs twice as fast at
// level >= 2 (fn_802415CC); letting go shoots an 'ARRW' into the first free slot of
// mArrowSerials_0xb00[3] (argument 0x100 uncharged; its layer in field_0xb40), a used slot
// passing on to the next only at level >= 2. A shot under fn_8022B160 starts the 60-frame
// cooldown field_0xb74, which also ends the state instead of drawing again.
void Player_804A6F38::fn_8025C894() {
    if (mAnimTimer_0x3fa < 3) {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 6) {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa == 6) {
        if (mStateTimer_0x3fc < 48) {
            if (fn_802415CC() >= 2) {
                mStateTimer_0x3fc += 2;
                if (mStateTimer_0x3fc >= 48) {
                    mStateTimer_0x3fc = 48;
                }
            } else {
                mStateTimer_0x3fc++;
            }
            if (mStateTimer_0x3fc == 48) {
                const Vec& point = lbl_8046B010[mFacing_0x3dc];
                mItemChargePos_0x12fc.set(point.x, point.y, point.z);
                mItemChargePos_0x12fc += mPos;
                if (mItemChargeEmitter_0x12cc == NULL) {
                    field_0x12d0.field_0x4.set(mItemChargePos_0x12fc);
                    mItemChargeEmitter_0x12cc = spawnEmitter_8013CC50(mLayer_0x3b4, &mItemChargePos_0x12fc, 0xcb, 0, -1, NULL, &field_0x12d0,
                                                         true, 1.0f);
                    if (mItemChargeEmitter_0x12cc != NULL) {
                        mItemChargeEmitter_0x12cc->setStatus(JPAEMIT_Immortal);
                    }
                }
                if (mItemChargeEmitter_0x12e4 == NULL) {
                    field_0x12e8.field_0x4.set(mItemChargePos_0x12fc);
                    mItemChargeEmitter_0x12e4 = spawnEmitter_8013CC50(mLayer_0x3b4, &mItemChargePos_0x12fc, 0xcc, 0, -1, NULL, &field_0x12e8,
                                                         true, 1.0f);
                    if (mItemChargeEmitter_0x12e4 != NULL) {
                        mItemChargeEmitter_0x12e4->setStatus(JPAEMIT_Immortal);
                    }
                }
                if (mChargeEffectSerial_0x12c0 < 0) {
                    mChargeEffectSerial_0x12c0 = getActorManager_801F856C()->spawnActor_801F9484(
                        ACTOR_PEFF, mLayer_0x3b4, &mItemChargePos_0x12fc, (mPlayerNo_0x4 & 3) | 8, -1, -1);
                }
                playSound_8024F0E8(0x2000d);
            }
        } else {
            const Vec& point = lbl_8046B010[mFacing_0x3dc];
            mItemChargePos_0x12fc.set(point.x, point.y, point.z);
            mItemChargePos_0x12fc += mPos;
            if (mItemChargeEmitter_0x12cc != NULL) {
                field_0x12d0.field_0x4.set(mItemChargePos_0x12fc);
            }
            if (mItemChargeEmitter_0x12e4 != NULL) {
                field_0x12e8.field_0x4.set(mItemChargePos_0x12fc);
            }
            s32 serial = mChargeEffectSerial_0x12c0;
            if (serial >= 0) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (actor != NULL) {
                    actor->mPos = mItemChargePos_0x12fc;
                }
            }
        }
        if (!mVel_0x344.isZero() || mPos.z > 0.0f) {
            mPos += mVel_0x344;
            mVel_0x344.z -= 0.25f;
            if (mPos.z <= 0.0f) {
                mPos.z = 0.0f;
                mKeptVel_0x368.set(mVel_0x344.x, mVel_0x344.y, 0.0f);
                mVel_0x344.set(0.0f, 0.0f, 0.0f);
                field_0x43d = 3;
            }
        } else if (field_0x43d != 0) {
            field_0x43d--;
        } else if (fn_8022C274(mPlayerNo_0x4)) {
            if (fn_80246B10(0x20)) {
                if (fn_80246F28() == 0.0f && field_0xbf0->field_0xb7) {
                    mVel_0x344.x = mKeptVel_0x368.x;
                    mVel_0x344.y = mKeptVel_0x368.y;
                } else {
                    mVel_0x344.x += 2.5f * fn_80247330();
                    mVel_0x344.y += 2.5f * -fn_8024712C();
                }
                mVel_0x344.z = 1.5f;
                playSound_8024F0E8(0x10002);
            } else {
                moveByStick_802808E8(1.125f);
            }
            if (epsilonEqualsXY(mPos, mLastPos_0x14, JGeometry::TUtilf::epsilon())) {
                mWalkAnimCounter_0x3f8 = 0;
            } else {
                mWalkAnimCounter_0x3f8++;
                if (mWalkAnimCounter_0x3f8 >= 14) {
                    mWalkAnimCounter_0x3f8 = 0;
                }
            }
            if (mWalkAnimCounter_0x3f8 < 4) {
                mWalkFrame_0x3f9 = 0;
            } else if (mWalkAnimCounter_0x3f8 < 8) {
                mWalkFrame_0x3f9 = 1;
            } else if (mWalkAnimCounter_0x3f8 < 10) {
                mWalkFrame_0x3f9 = 0;
            } else {
                mWalkFrame_0x3f9 = 2;
            }
        } else {
            mAnimTimer_0x3fa++;
            for (int i = 0; i < 3; i++) {
                if (mArrowSerials_0xb00[i] >= 0) {
                    // A used slot: the next one only at level >= 2.
                    if (fn_802415CC() >= 2) {
                        continue;
                    }
                    break;
                } else {
                    JGeometry::TVec3f pos = mPos;
                    switch (mFacing_0x3dc) {
                    case 0:
                        pos.y -= 4.0f;
                        break;
                    case 1:
                        pos.y -= 4.0f;
                        break;
                    case 2:
                        break;
                    case 3:
                        pos.x -= 2.0f;
                        break;
                    }
                    if (mStateTimer_0x3fc >= 48) {
                        s32 index = mPlayerNo_0x4;
                        mArrowSerials_0xb00[i] = getActorManager_801F856C()->spawnActor_801F9484(
                            ACTOR_ARRW, mLayer_0x3b4, &pos, ((mFacing_0x3dc & 3) << 4) | (index & 3), -1, index);
                    } else {
                        s32 index = mPlayerNo_0x4;
                        mArrowSerials_0xb00[i] = getActorManager_801F856C()->spawnActor_801F9484(
                            ACTOR_ARRW, mLayer_0x3b4, &pos, ((mFacing_0x3dc & 3) << 4) | 0x100 | (index & 3), -1, index);
                    }
                    if (mArrowSerials_0xb00[i] >= 0) {
                        field_0xb40[i] = mLayer_0x3b4;
                        playSound_8024F0E8(0x20004);
                        if (fn_8022B160()) {
                            field_0xb74 = 60;
                        } else if (hasLeader_80240808()) {
                            // Nothing: the original calls hasLeader_80240808 and ignores it.
                        }
                    }
                    mStateTimer_0x3fc = 0;
                    break;
                }
            }
        }
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 10) {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 2;
    } else {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 3;
    }
    if (mAnimTimer_0x3fa >= 14) {
        if (field_0xb74 == 0 && fn_8022C274(mPlayerNo_0x4)) {
            mAnimTimer_0x3fa = 0;
            mPose_0x3fb = 0;
        } else {
            requestState_80251AEC(0);
        }
    }
    fn_80268B3C();
    field_0x40c++;
    if (field_0x40c < 30) {
        field_0x40c++;
    } else {
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (!camera->mFocusOverride_0x7f4) {
            camera->mShowWholeArea_0x7f5 = 1;
        }
    }
}

void Player_804A6F38::fn_8025C7BC() {
    if (mItemChargeEmitter_0x12cc != NULL) {
        mItemChargeEmitter_0x12cc->clearStatus(JPAEMIT_Immortal);
        mItemChargeEmitter_0x12cc->becomeInvalidEmitter();
        mItemChargeEmitter_0x12cc = NULL;
    }
    if (mItemChargeEmitter_0x12e4 != NULL) {
        mItemChargeEmitter_0x12e4->clearStatus(JPAEMIT_Immortal);
        mItemChargeEmitter_0x12e4->becomeInvalidEmitter();
        mItemChargeEmitter_0x12e4 = NULL;
    }
    s32 serial = mChargeEffectSerial_0x12c0;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->mAlive = false;
        }
        mChargeEffectSerial_0x12c0 = -1;
    }
    field_0x40c = 0;
}

void Player_804A6F38::fn_8025C79C() {
    field_0x458 = 0;
    mVel_0x344.x = 0.0f;
    mVel_0x344.y = 0.0f;
    mVel_0x344.z = 0.0f;
    field_0x40c = 0;
}

// FIXME: 99.82 %, missing/extra; behaviour clean.
// Structurally close (1 extra instruction): the side shot's argument is
// built in r0 and moved to r30 (the original builds it in r30).
// State 0x3B (item 10, the slingshot; enter fn_8025C79C, exit fn_8025BDD8): after six
// frames of drawing, frame 6 holds while the button is held (fn_8022C274), charging
// mStateTimer_0x3fc to 48: a full charge lights the point mItemChargePos_0x12fc (lbl_8046B040 by facing;
// emitters 0xCB / 0xCC following it through the particle callbacks field_0x12d0 /
// field_0x12e8 below layer 8, else a 'PEFF' actor in mChargeEffectSerial_0x12c0). The player walks at
// 1.125 meanwhile (14-frame walk cycle) or hops on button 0x20 (2.5 along the stick, 1.5
// up, the last landing speed mKeptVel_0x368 on a side wall; 0.25 gravity, 3 frames' rest
// field_0x43d after landing). Letting go shoots a 'PCDM' (slingshot seed) into the first
// free slot of mSlingshotSerials_0xb0c[3] (argument 0x100 uncharged); charged at level >= 2 with all
// three slots free, two more go out on the side directions of lbl_8046B070.
void Player_804A6F38::fn_8025BEB0() {
    if (mAnimTimer_0x3fa < 3) {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 6) {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa == 6) {
        if (mStateTimer_0x3fc < 48) {
            mStateTimer_0x3fc++;
            if (mStateTimer_0x3fc == 48) {
                const Vec& point = lbl_8046B040[mFacing_0x3dc];
                mItemChargePos_0x12fc.set(point.x, point.y, point.z);
                mItemChargePos_0x12fc += mPos;
                if (mItemChargeEmitter_0x12cc == NULL && mLayer_0x3b4 < 8) {
                    field_0x12d0.field_0x4.set(mItemChargePos_0x12fc);
                    mItemChargeEmitter_0x12cc = spawnEmitter_8013CC50(mLayer_0x3b4, &mItemChargePos_0x12fc, 0xcb, 0, -1, NULL, &field_0x12d0,
                                                         true, 1.0f);
                    if (mItemChargeEmitter_0x12cc != NULL) {
                        mItemChargeEmitter_0x12cc->setStatus(JPAEMIT_Immortal);
                    }
                }
                if (mItemChargeEmitter_0x12e4 == NULL && mLayer_0x3b4 < 8) {
                    field_0x12e8.field_0x4.set(mItemChargePos_0x12fc);
                    mItemChargeEmitter_0x12e4 = spawnEmitter_8013CC50(mLayer_0x3b4, &mItemChargePos_0x12fc, 0xcc, 0, -1, NULL, &field_0x12e8,
                                                         true, 1.0f);
                    if (mItemChargeEmitter_0x12e4 != NULL) {
                        mItemChargeEmitter_0x12e4->setStatus(JPAEMIT_Immortal);
                    }
                }
                if (mChargeEffectSerial_0x12c0 < 0) {
                    mChargeEffectSerial_0x12c0 = getActorManager_801F856C()->spawnActor_801F9484(
                        ACTOR_PEFF, mLayer_0x3b4, &mItemChargePos_0x12fc, (mPlayerNo_0x4 & 3) | 8, -1, -1);
                }
                playSound_8024F0E8(0x2000d);
            }
        } else {
            const Vec& point = lbl_8046B040[mFacing_0x3dc];
            mItemChargePos_0x12fc.set(point.x, point.y, point.z);
            mItemChargePos_0x12fc += mPos;
            if (mItemChargeEmitter_0x12cc != NULL) {
                field_0x12d0.field_0x4.set(mItemChargePos_0x12fc);
            }
            if (mItemChargeEmitter_0x12e4 != NULL) {
                field_0x12e8.field_0x4.set(mItemChargePos_0x12fc);
            }
            s32 serial = mChargeEffectSerial_0x12c0;
            if (serial >= 0) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (actor != NULL) {
                    actor->mPos = mItemChargePos_0x12fc;
                } else {
                    mChargeEffectSerial_0x12c0 = -1;
                }
            }
        }
        if (!mVel_0x344.isZero() || mPos.z > 0.0f) {
            mPos += mVel_0x344;
            mVel_0x344.z -= 0.25f;
            if (mPos.z <= 0.0f) {
                mPos.z = 0.0f;
                mKeptVel_0x368.set(mVel_0x344.x, mVel_0x344.y, 0.0f);
                mVel_0x344.set(0.0f, 0.0f, 0.0f);
                field_0x43d = 3;
            }
        } else if (field_0x43d != 0) {
            field_0x43d--;
        } else if (fn_8022C274(mPlayerNo_0x4)) {
            if (fn_80246B10(0x20)) {
                if (fn_80246F28() == 0.0f && field_0xbf0->field_0xb7) {
                    mVel_0x344.x = mKeptVel_0x368.x;
                    mVel_0x344.y = mKeptVel_0x368.y;
                } else {
                    mVel_0x344.x += 2.5f * fn_80247330();
                    mVel_0x344.y += 2.5f * -fn_8024712C();
                }
                mVel_0x344.z = 1.5f;
                playSound_8024F0E8(0x10002);
            } else {
                moveByStick_802808E8(1.125f);
            }
            if (epsilonEqualsXY(mPos, mLastPos_0x14, JGeometry::TUtilf::epsilon())) {
                mWalkAnimCounter_0x3f8 = 0;
            } else {
                mWalkAnimCounter_0x3f8++;
                if (mWalkAnimCounter_0x3f8 >= 14) {
                    mWalkAnimCounter_0x3f8 = 0;
                }
            }
            if (mWalkAnimCounter_0x3f8 < 4) {
                mWalkFrame_0x3f9 = 0;
            } else if (mWalkAnimCounter_0x3f8 < 8) {
                mWalkFrame_0x3f9 = 1;
            } else if (mWalkAnimCounter_0x3f8 < 10) {
                mWalkFrame_0x3f9 = 0;
            } else {
                mWalkFrame_0x3f9 = 2;
            }
        } else {
            mAnimTimer_0x3fa++;
            bool spread = true;
            if (fn_802415CC() >= 2 && mStateTimer_0x3fc >= 48) {
                for (int i = 0; i < 3; i++) {
                    if (mSlingshotSerials_0xb0c[i] >= 0) {
                        spread = false;
                        break;
                    }
                }
            } else {
                spread = false;
            }
            for (int i = 0; i < 3; i++) {
                if (mSlingshotSerials_0xb0c[i] < 0) {
                    JGeometry::TVec3f pos = mPos;
                    switch (mFacing_0x3dc) {
                    case 0:
                        pos.y -= 4.0f;
                        break;
                    case 1:
                        pos.y -= 4.0f;
                        break;
                    case 2:
                        break;
                    case 3:
                        pos.x -= 2.0f;
                        break;
                    }
                    if (mStateTimer_0x3fc >= 48) {
                        s32 index = mPlayerNo_0x4;
                        mSlingshotSerials_0xb0c[i] = getActorManager_801F856C()->spawnActor_801F9484(
                            ACTOR_PCDM, mLayer_0x3b4, &pos, ((mFacing_0x3dc & 3) << 4) | (index & 3), -1, index);
                    } else {
                        s32 index = mPlayerNo_0x4;
                        mSlingshotSerials_0xb0c[i] = getActorManager_801F856C()->spawnActor_801F9484(
                            ACTOR_PCDM, mLayer_0x3b4, &pos, ((mFacing_0x3dc & 3) << 4) | 0x100 | (index & 3), -1, index);
                    }
                    if (mSlingshotSerials_0xb0c[i] >= 0) {
                        field_0xb4c[i] = mLayer_0x3b4;
                        playSound_8024F0E8(0x20004);
                    }
                    if (spread == true) {
                        if (mSlingshotSerials_0xb0c[1] < 0) {
                            s32 index = mPlayerNo_0x4;
                            u32 arg = ((lbl_8046B070[mFacing_0x3dc][0] & 7) << 4) | (index & 3);
                            mSlingshotSerials_0xb0c[1] = getActorManager_801F856C()->spawnActor_801F9484(
                                ACTOR_PCDM, mLayer_0x3b4, &pos, arg, -1, index);
                            if (mSlingshotSerials_0xb0c[1] >= 0) {
                                field_0xb4c[1] = mLayer_0x3b4;
                            }
                        }
                        if (mSlingshotSerials_0xb0c[2] < 0) {
                            s32 index = mPlayerNo_0x4;
                            mSlingshotSerials_0xb0c[2] = getActorManager_801F856C()->spawnActor_801F9484(
                                ACTOR_PCDM, mLayer_0x3b4, &pos, ((lbl_8046B070[mFacing_0x3dc][1] & 7) << 4) | (index & 3),
                                -1, index);
                            if (mSlingshotSerials_0xb0c[2] >= 0) {
                                field_0xb4c[2] = mLayer_0x3b4;
                            }
                        }
                    }
                    mStateTimer_0x3fc = 0;
                    break;
                }
            }
        }
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 10) {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 2;
    } else {
        mAnimTimer_0x3fa++;
        mPose_0x3fb = 3;
    }
    if (mAnimTimer_0x3fa >= 14) {
        if (fn_8022C274(mPlayerNo_0x4)) {
            mAnimTimer_0x3fa = 0;
            mPose_0x3fb = 0;
        } else {
            requestState_80251AEC(0);
        }
    }
    fn_80268B3C();
    if (field_0x40c < 30) {
        field_0x40c++;
    } else {
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (!camera->mFocusOverride_0x7f4) {
            camera->mShowWholeArea_0x7f5 = 1;
        }
    }
}

void Player_804A6F38::fn_8025BDD8() {
    if (mItemChargeEmitter_0x12cc != NULL) {
        mItemChargeEmitter_0x12cc->clearStatus(JPAEMIT_Immortal);
        mItemChargeEmitter_0x12cc->becomeInvalidEmitter();
        mItemChargeEmitter_0x12cc = NULL;
    }
    if (mItemChargeEmitter_0x12e4 != NULL) {
        mItemChargeEmitter_0x12e4->clearStatus(JPAEMIT_Immortal);
        mItemChargeEmitter_0x12e4->becomeInvalidEmitter();
        mItemChargeEmitter_0x12e4 = NULL;
    }
    s32 serial = mChargeEffectSerial_0x12c0;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->mAlive = false;
        }
        mChargeEffectSerial_0x12c0 = -1;
    }
    field_0x40c = 0;
}

void Player_804A6F38::fn_8025BDC8() {
    field_0x458 = 0;
    field_0x40c = 0;
}

void Player_804A6F38::fn_8025B3F0() {
    s32 full = 48;
    if (fn_8022B244()) {
        full = 25;
    }
    if (mAnimTimer_0x3fa == 0 && fn_8022C274(mPlayerNo_0x4)) {
        updateFacing_8027EB04();
        mStateTimer_0x3fc++;
        if (mStateTimer_0x3fc == full) {
            field_0xb9b = true;
            const Vec& point = lbl_8046B090[mFacing_0x3dc];
            mChargeEffectPos_0x12a4.set(point.x, point.y, point.z);
            mChargeEffectPos_0x12a4 += mPos;
            s32 layer = mLayer_0x3b4;
            if (layer < 8) {
                if (mChargeEmitter_0x1274 == NULL) {
                    mChargeEmitter_0x1274 = spawnEmitter_8013CC50(layer, &mChargeEffectPos_0x12a4, 0xcb, 0, 0, NULL, NULL, true, 1.0f);
                    if (mChargeEmitter_0x1274 != NULL) {
                        mChargeEmitter_0x1274->setStatus(JPAEMIT_Immortal);
                    }
                }
                if (mChargeEmitter_0x128c == NULL) {
                    mChargeEmitter_0x128c =
                        spawnEmitter_8013CC50(mLayer_0x3b4, &mChargeEffectPos_0x12a4, 0xcc, 0, 0, NULL, NULL, true, 1.0f);
                    if (mChargeEmitter_0x128c != NULL) {
                        mChargeEmitter_0x128c->setStatus(JPAEMIT_Immortal);
                    }
                }
            } else {
                getActorManager_801F856C()->spawnActor_801F9484(ACTOR_PEFF, layer, &mChargeEffectPos_0x12a4,
                                                                (mPlayerNo_0x4 & 3) | 8, -1, -1);
            }
            playSound_8024F0E8(0x2000d);
        } else {
            if (mStateTimer_0x3fc >= (u8)(full + 1)) {
                mStateTimer_0x3fc = full + 1;
            }
        }
    } else {
        mAnimTimer_0x3fa++;
    }
    if (mAnimTimer_0x3fa == 0) {
        if (mStateTimer_0x3fc < 2) {
            mPose_0x3fb = 2;
        } else if (mStateTimer_0x3fc < 4) {
            mPose_0x3fb = 1;
        } else {
            mPose_0x3fb = 0;
        }
    } else if (!field_0xb9b) {
        if (mAnimTimer_0x3fa < 3) {
            mPose_0x3fb = 1;
        } else if (mAnimTimer_0x3fa < 5) {
            mPose_0x3fb = 2;
        } else if (mAnimTimer_0x3fa < 13) {
            if (mLayer_0x3b4 < 8) {
                mPose_0x3fb = 5;
            } else if (mAnimTimer_0x3fa < 7) {
                mPose_0x3fb = 3;
            } else if (mAnimTimer_0x3fa < 9) {
                mPose_0x3fb = 4;
            } else {
                mPose_0x3fb = 5;
            }
        } else {
            mPose_0x3fb = 6;
        }
        if (mAnimTimer_0x3fa >= 17) {
            requestState_80251AEC(0);
        }
    } else {
        if (mAnimTimer_0x3fa < 2) {
            mPose_0x3fb = 1;
        } else if (mAnimTimer_0x3fa < 3) {
            mPose_0x3fb = 2;
        } else if (mAnimTimer_0x3fa < 4) {
            mPose_0x3fb = 3;
        } else if (mAnimTimer_0x3fa < 5) {
            mPose_0x3fb = 4;
        } else if (mAnimTimer_0x3fa < 6) {
            mPose_0x3fb = 3;
        } else if (mAnimTimer_0x3fa < 7) {
            mPose_0x3fb = 4;
        } else if (mAnimTimer_0x3fa < 8) {
            mPose_0x3fb = 3;
        } else if (mAnimTimer_0x3fa < 9) {
            mPose_0x3fb = 4;
        } else if (mAnimTimer_0x3fa < 11) {
            mPose_0x3fb = 5;
        } else if (mAnimTimer_0x3fa < 13) {
            mPose_0x3fb = 6;
        } else if (mAnimTimer_0x3fa < 15) {
            mPose_0x3fb = 5;
        } else if (mAnimTimer_0x3fa < 20) {
            mPose_0x3fb = 6;
        } else {
            mPose_0x3fb = 7;
        }
        if (mAnimTimer_0x3fa >= 30) {
            requestState_80251AEC(0);
        }
        if (mLayer_0x3b4 < 8) {
            if (mPose_0x3fb == 3) {
                mPose_0x3fb = 5;
            } else if (mPose_0x3fb == 4) {
                mPose_0x3fb = 6;
            }
        }
    }
    if (field_0xb9b == true) {
        const Vec& point = lbl_8046B0C0[mFacing_0x3dc + 4];
        field_0x5c0.set(point.x, point.y, point.z);
    } else {
        const Vec& point = lbl_8046B0C0[mFacing_0x3dc];
        field_0x5c0.set(point.x, point.y, point.z);
    }
    field_0x5c0 += mPos;
    field_0x5c0.z = 0.0f;
    bool wasHit = field_0xb99;
    if (fn_8022B244()) {
        if (mAnimTimer_0x3fa == 1) {
            field_0xb99 = true;
        } else if (mAnimTimer_0x3fa == 3) {
            field_0xb99 = false;
        }
    } else {
        if (mAnimTimer_0x3fa == 5) {
            field_0xb99 = true;
        } else if (mAnimTimer_0x3fa == 7) {
            field_0xb99 = false;
        }
    }
    if (field_0xb99 == true && !wasHit) {
        if (fn_8022B244()) {
            field_0x5b0.set(-16.0f, -16.0f, 16.0f, 16.0f);
        } else {
            field_0x5b0.set(-8.0f, -8.0f, 8.0f, 8.0f);
        }
        field_0x5b0.i.x += field_0x5c0.x;
        field_0x5b0.i.y += field_0x5c0.y;
        field_0x5b0.f.x += field_0x5c0.x;
        field_0x5b0.f.y += field_0x5c0.y;
        const Vec& step = lbl_8046B120[mFacing_0x3dc];
        JGeometry::TVec3f front;
        front.x = step.x;
        front.y = step.y;
        front.z = step.z;
        front += mPos;
        gBgRewrite_8053AC98->fn_802A1D44(&front, mPlayerNo_0x4, mLayer_0x3b4);
        f32 strength = 1.0f;
        if (fn_802486A0() != 5 && !fn_80240AF0()) {
            strength /= fn_802487FC();
        }
        if (fn_802415CC() <= 1) {
            strength *= 0.5f;
        }
        if (field_0xb9b == true) {
            if (mLayer_0x3b4 < 8) {
                gGameRoot2_8053AB10->field_0x20->startHeightGridEffect_802F601C(&field_0x5c0, 2, NULL, 1.0f, 2.0f * strength);
            }
            playSound_8024F0E8(0x2002b);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23b, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23c, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23d, 0, 0, NULL, NULL, true, 5.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23e, 0, 0, NULL, NULL, true, 2.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23f, 0, 0, NULL, NULL, true, 2.0f);
            u16 tile = getTileCollisionCode_8022461C(&field_0x5c0, mLayer_0x3b4);
            if (tile == 0x39 || tile == 0xd) {
                spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x3d0, 0, 0, NULL, NULL, true, 2.0f);
            } else {
                spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x3cf, 0, 0, NULL, NULL, true, 2.0f);
            }
            getGameView_802D6A9C()->fn_802D79E8(mLayer_0x3b4, 4.0f, 0.0f, false);
            gGameRoot_8053AAF8->startRumble_80134DE4(2, 0);
        } else {
            if (mLayer_0x3b4 < 8) {
                gGameRoot2_8053AB10->field_0x20->startHeightGridEffect_802F601C(&field_0x5c0, 1, NULL, 2.0f, strength);
            }
            playSound_8024F0E8(0x20024);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23d, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23e, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x23f, 0, 0, NULL, NULL, true, 1.0f);
            u16 tile = getTileCollisionCode_8022461C(&field_0x5c0, mLayer_0x3b4);
            if (tile == 0x39 || tile == 0xd) {
                spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x3d0, 0, 0, NULL, NULL, true, 1.0f);
            } else {
                spawnEmitter_8013CC50(mLayer_0x3b4, &field_0x5c0, 0x3cf, 0, 0, NULL, NULL, true, 1.0f);
            }
            gGameRoot_8053AAF8->startRumble_80134DE4(1, 0);
        }
    }
    if (field_0xb9b == true) {
        if (mAnimTimer_0x3fa == 18) {
            field_0xb9a = true;
        } else if (mAnimTimer_0x3fa == 20) {
            field_0xb9a = false;
        }
    }
    fn_80268B3C();
    if (field_0x40c < 30) {
        field_0x40c++;
    } else {
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (!camera->mFocusOverride_0x7f4) {
            camera->mShowWholeArea_0x7f5 = 1;
        }
    }
}

void Player_804A6F38::fn_8025B368() {
    field_0xb99 = false;
    field_0xb9a = false;
    field_0xb9b = false;
    if (mChargeEmitter_0x1274 != NULL) {
        mChargeEmitter_0x1274->clearStatus(JPAEMIT_Immortal);
        mChargeEmitter_0x1274->becomeInvalidEmitter();
        mChargeEmitter_0x1274 = NULL;
    }
    if (mChargeEmitter_0x128c != NULL) {
        mChargeEmitter_0x128c->clearStatus(JPAEMIT_Immortal);
        mChargeEmitter_0x128c->becomeInvalidEmitter();
        mChargeEmitter_0x128c = NULL;
    }
    field_0x40c = 0;
}

void Player_804A6F38::fn_8025B35C() {
    field_0x458 = 0;
}

// FIXME: 95.59 %, scheduling+regalloc; behaviour clean.
// scheduling+regalloc: the original keeps this in r31 and field_0x4 in r30 (ours
// the other way round) and builds the spawn argument from field_0x4 & 3 first.
void Player_804A6F38::fn_8025B22C() {
    if (mAnimTimer_0x3fa < 2) {
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 4) {
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 6) {
        mPose_0x3fb = 2;
    } else {
        mPose_0x3fb = 3;
    }
    if (mAnimTimer_0x3fa == 6) {
        if (fn_802415CC() >= 2) {
            s32 index = mPlayerNo_0x4;
            field_0xb18 = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_BOME, mLayer_0x3b4, &mPos,
                                                     ((mMoveDir_0x3e8 & 7) << 4) | (index & 3) | 0x100, -1, index);
        } else {
            s32 index = mPlayerNo_0x4;
            field_0xb18 = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_BOME, mLayer_0x3b4, &mPos,
                                                     ((mMoveDir_0x3e8 & 7) << 4) | (index & 3), -1, index);
        }
    } else if (mAnimTimer_0x3fa == 16) {
        requestState_80251AEC(0);
    }
    mAnimTimer_0x3fa++;
    fn_80268B3C();
}

void Player_804A6F38::fn_8025B218() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
}

void Player_804A6F38::fn_8025AED0() {
    mStateTimer_0x3fc++;
    s32 turn = (mStateTimer_0x3fc >> 1) & 3;
    if (turn == 0) {
        mFacing_0x3dc = 0;
        mMoveDir_0x3e8 = 0;
    } else if (turn == 1) {
        mFacing_0x3dc = 2;
        mMoveDir_0x3e8 = 2;
    } else if (turn == 2) {
        mFacing_0x3dc = 1;
        mMoveDir_0x3e8 = 1;
    } else {
        mFacing_0x3dc = 3;
        mMoveDir_0x3e8 = 3;
    }
    fn_80268B3C();
    if (field_0xafc != 1 && !field_0xbf0->anyFlag48()) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(field_0x11f8);
        if (actor != NULL && actor->mLayer_0x4 == mLayer_0x3b4) {
            JGeometry::TVec3f dir = actor->mPos - mPos;
            dir.z = 0.0f;
            dir.normalize();
            if (!dir.isZero()) {
                dir *= 6.0f;
                mPos += dir;
            }
            mPos.z = 0.0f;
            return;
        }
    }
    mStateLocked_0x124d = 0;
    requestState_80251AEC(0);
}

void Player_804A6F38::fn_8025AEC4() {
    field_0x11f8 = -1;
}

void Player_804A6F38::fn_8025AEB0() {
    mStateLocked_0x124d = 1;
    field_0x458 = 0;
}

// State 0x43 (from act_arrow via fn_8022E1A0): the player is drawn toward the arrow
// actor 0x11F8 at min(distance, 6) a frame across the floor, facing along the pull,
// until the arrow is gone or its field_0x230 leaves 0, field_0xafc is 1 or a side flag
// of field_0xbf0 is set (anyFlag48); the animation frame 0x3FD steps at 10 and 14.
void Player_804A6F38::fn_8025AB00() {
    mStateTimer_0x3fc++;
    if (mStateTimer_0x3fc < 10) {
        mSubPose_0x3fd = 0;
    } else if (mStateTimer_0x3fc < 14) {
        mSubPose_0x3fd = 1;
    } else {
        mSubPose_0x3fd = 2;
    }
    fn_80268B3C();
    if (field_0xafc != 1 && !field_0xbf0->anyFlag48()) {
        ArrowPlayer_804A05E0* arrow =
            static_cast<ArrowPlayer_804A05E0*>(getActorManager_801F856C()->findActorBySerial_801F666C(field_0x11f8));
        if (arrow != NULL && arrow->field_0x230 == 0) {
            JGeometry::TVec3f dir = arrow->mPos - mPos;
            f32 dist = dir.length();
            f32 speed = dist;
            if (dist > 6.0f) {
                speed = 6.0f;
            }
            dir.z = 0.0f;
            dir.normalize();
            if (!dir.isZero()) {
                dir.scale(speed);
                mPos += dir;
                if (dir.x + dir.y < 0.0f) {
                    if (dir.x - dir.y < 0.0f) {
                        mFacing_0x3dc = 0;
                        mMoveDir_0x3e8 = 0;
                    } else {
                        mFacing_0x3dc = 2;
                        mMoveDir_0x3e8 = 2;
                    }
                } else if (dir.x - dir.y < 0.0f) {
                    mFacing_0x3dc = 3;
                    mMoveDir_0x3e8 = 3;
                } else {
                    mFacing_0x3dc = 1;
                    mMoveDir_0x3e8 = 1;
                }
            }
            mPos.z = 0.0f;
            return;
        }
    }
    mStateLocked_0x124d = 0;
    requestState_80251AEC(0);
}

void Player_804A6F38::fn_8025AAF4() {
    field_0x11f8 = -1;
}

bool Player_804A6F38::fn_8025AA8C(s32 arg) {
    if (field_0x11f8 >= 0) {
        return false;
    }
    if (requestState_80251AEC(0x42)) {
        field_0x11f8 = arg;
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_8025AA24(s32 arg) {
    if (field_0x11f8 >= 0) {
        return false;
    }
    if (requestState_80251AEC(0x43)) {
        field_0x11f8 = arg;
        return true;
    }
    return false;
}

void Player_804A6F38::fn_8025A9C0() {
    field_0x458 = 0;
    if (fn_80241794() <= 0) {
        mButtonHeld_0xb84 = false;
    } else {
        mButtonHeld_0xb84 = true;
    }
    field_0xb88 = 0;
    mFireSerial_0xb28 = -1;
    field_0xb8c = 0;
}

// regalloc-only: the box's f.x / f.y take their fadds operands the other way
// round in cases 0 and 2.
// State 0x44 (item 6 below level 2; enter fn_8025A9C0, exit fn_8025A0B0): the Fire Rod
// flame of state 0x45 without the Somaria block. Frame 11 holds while the button is held
// (0xB84): the player walks at 1.125 with the 28-frame walk cycle (lbl_8046B150), a 'FIRE'
// actor (0xB28, argument 0x1040 | facing << 4 | player; 60 frames 0xB8C after it goes)
// sits on the box field_0x580 in front and the emitters 0x1AB / 0x54A (0x12C4 / 0x12C8)
// turn to the facing. Letting go stops them and, when no flame is out and no 'FIRE' of
// field_0xb1c is alive, places one 8 in front (argument 0x1000 | facing << 4 | player)
// into the first free slot of field_0xb1c (its layer in field_0xb5c, field_0xb78 set).
// FIXME: 99.96 %, regalloc-only; behaviour clean.
void Player_804A6F38::fn_8025A15C() {
    if (field_0xb8c > 0) {
        field_0xb8c--;
    }
    if (mButtonHeld_0xb84 == 1 && !fn_8022C274(mPlayerNo_0x4)) {
        mButtonHeld_0xb84 = 0;
    }
    if (mAnimTimer_0x3fa < 2) {
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 3) {
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 4) {
        mPose_0x3fb = 2;
    } else if (mAnimTimer_0x3fa < 10) {
        mPose_0x3fb = 3;
    } else if (mAnimTimer_0x3fa < 11) {
        mPose_0x3fb = 4;
    } else if (mAnimTimer_0x3fa < 12) {
        mPose_0x3fb = 5;
    } else {
        mPose_0x3fb = 5;
        requestState_80251AEC(0);
    }
    if (mAnimTimer_0x3fa == 11) {
        JGeometry::TVec3f pos = mPos;
        JGeometry::TBox2f box;
        switch (mFacing_0x3dc) {
        case 0:
            pos.x += 18.0f;
            box.set(pos.x, pos.y - 6.0f, pos.x + 36.0f, pos.y + 6.0f);
            field_0x580.set(box);
            break;
        case 1:
            pos.x -= 18.0f;
            box.set(pos.x - 36.0f, pos.y - 6.0f, pos.x, pos.y + 6.0f);
            field_0x580.set(box);
            break;
        case 2:
            pos.x += 4.0f;
            pos.y += 12.0f;
            box.set(pos.x - 6.0f, pos.y, pos.x + 6.0f, pos.y + 36.0f);
            field_0x580.set(box);
            break;
        case 3:
            pos.x -= 4.0f;
            pos.y -= 20.0f;
            box.set(pos.x - 6.0f, pos.y - 36.0f, pos.x + 6.0f, pos.y);
            field_0x580.set(box);
            break;
        }
        if (mButtonHeld_0xb84) {
            moveByStick_802808E8(1.125f);
            if (epsilonEquals(mPos, mLastPos_0x14, JGeometry::TUtilf::epsilon())) {
                mWalkAnimCounter_0x3f8 = 0;
            } else {
                mWalkAnimCounter_0x3f8++;
                if (mWalkAnimCounter_0x3f8 >= 28) {
                    mWalkAnimCounter_0x3f8 = 0;
                }
            }
            if (mWalkAnimCounter_0x3f8 < 4) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][0];
            } else if (mWalkAnimCounter_0x3f8 < 6) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][1];
            } else if (mWalkAnimCounter_0x3f8 < 10) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][2];
            } else if (mWalkAnimCounter_0x3f8 < 14) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][3];
            } else if (mWalkAnimCounter_0x3f8 < 18) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][4];
            } else if (mWalkAnimCounter_0x3f8 < 20) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][5];
            } else if (mWalkAnimCounter_0x3f8 < 24) {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][6];
            } else {
                mWalkFrame_0x3f9 = lbl_8046B150[mFacing_0x3dc][7];
            }
            if (!fn_80247594(1)) {
                mButtonHeld_0xb84 = 0;
            }
            if (field_0xb8c <= 0 && mFireSerial_0xb28 < 0) {
                s32 index = mPlayerNo_0x4;
                mFireSerial_0xb28 = getActorManager_801F856C()->spawnActor_801F9484(
                    ACTOR_FIRE, mLayer_0x3b4, &pos, ((mFacing_0x3dc & 3) << 4) | 0x1040 | (index & 3), -1, index);
            }
            s32 fire = mFireSerial_0xb28;
            if (fire >= 0) {
                FireRodFire_804A89F0* actor = static_cast<FireRodFire_804A89F0*>(
                    getActorManager_801F856C()->findActorBySerial_801F666C(fire));
                if (actor != NULL) {
                    actor->fn_802BAA0C(&box);
                } else {
                    mFireSerial_0xb28 = -1;
                    field_0xb8c = 60;
                }
            }
            if (field_0x12c4 == NULL) {
                field_0x12c4 = spawnEmitter_8013CC50(mLayer_0x3b4, &pos, 0x1ab, 0, 2, NULL, NULL, true, 1.0f);
            }
            if (field_0x12c4 != NULL) {
                field_0x12c4->setGlobalRotation(JGeometry::TVec3<s16>(0L, lbl_8046B170[mFacing_0x3dc], -0x8000L));
                setEmitterPos_8013CB44(field_0x12c4, &pos, 0, 2, true);
            }
            if (field_0x12c8 == NULL) {
                field_0x12c8 = spawnEmitter_8013CC50(mLayer_0x3b4, &pos, 0x54a, -1, 0, NULL, NULL, true, 1.0f);
            }
            if (field_0x12c8 != NULL) {
                field_0x12c8->setGlobalRotation(JGeometry::TVec3<s16>(0L, 0L, lbl_8046B180[mFacing_0x3dc]));
                setEmitterPos_8013CB44(field_0x12c8, &pos, -1, 0, true);
            }
            playSound_8024F0E8(0x30011);
        } else {
            if (field_0x12c8 != NULL) {
                field_0x12c8->stopCreateParticle();
            }
            if (field_0x12c4 != NULL) {
                field_0x12c4->stopCreateParticle();
            }
            if (mFireSerial_0xb28 < 0) {
                s32 count = 0;
                for (int i = 0; i < 1; i++) {
                    if (field_0xb1c[i] >= 0) {
                        count++;
                    }
                }
                if (count < 1 && (!hasLeader_80240808() || fn_802486A0() == 4 || fn_8023B400(getLeader_80240644())) &&
                    fn_80247594(0)) {
                    JGeometry::TVec3f placed = mPos;
                    switch (mFacing_0x3dc) {
                    case 0:
                        placed.x += 8.0f;
                        break;
                    case 1:
                        placed.x -= 8.0f;
                        break;
                    case 2:
                        placed.y += 8.0f;
                        break;
                    case 3: // no step up: the original's dispatch keeps this empty case
                        break;
                    }
                    for (int i = 0; i < 3; i++) {
                        if (field_0xb1c[i] < 0) {
                            field_0xb1c[i] = getActorManager_801F856C()->spawnActor_801F9484(
                                ACTOR_FIRE, mLayer_0x3b4, &placed, ((mFacing_0x3dc & 3) << 4) | 0x1000 | (mPlayerNo_0x4 & 3),
                                -1, mPlayerNo_0x4);
                            if (field_0xb1c[i] >= 0) {
                                field_0xb5c[i] = mLayer_0x3b4;
                                field_0xb78 = true;
                                break;
                            }
                        }
                    }
                }
            }
            mAnimTimer_0x3fa++;
        }
    } else {
        mAnimTimer_0x3fa++;
    }
    fn_80268B3C();
}

void Player_804A6F38::fn_8025A0B0() {
    if (field_0x12c4 != NULL) {
        field_0x12c4->becomeInvalidEmitter();
        field_0x12c4 = NULL;
    }
    if (field_0x12c8 != NULL) {
        field_0x12c8->becomeInvalidEmitter();
        field_0x12c8 = NULL;
    }
    s32 serial = mFireSerial_0xb28;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->fn_802BAA00();
        }
        mFireSerial_0xb28 = -1;
    }
}

void Player_804A6F38::fn_8025A04C() {
    field_0x458 = 0;
    if (fn_80241794() <= 0) {
        mButtonHeld_0xb84 = false;
    } else {
        mButtonHeld_0xb84 = true;
    }
    field_0xb88 = 0;
    mFireSerial_0xb28 = -1;
    field_0xb8c = 0;
}

// FIXME: 99.96 %, regalloc-only; behaviour clean.
// FIXME: regalloc-only: the box's f.x / f.y (pos + 36) take their fadds
// operands the other way round in cases 0 and 2.
// State 0x45 (item 6 at level >= 2; enter fn_8025A04C, exit fn_80259784): frame 11 places
// a Cane of Somaria block ('SMRI', 16 in front, field_0xb2c; message 0xE8 once, 0xBB8)
// or acts on the one that exists (fn_8030EA00); while the button is held (0xB84,
// fn_8022C274) the player stays at frame 12 walking at 1.125 (moveByStick_802808E8) with the
// 28-frame walk cycle, keeps a 'FIRE' actor (0xB28; 60 frames 0xB8C after it goes) on
// the box field_0x580 in front (fn_802BAA0C) and the emitters 0x1AB / 0x54A
// (0x12C4 / 0x12C8) turned to the facing; letting go stops them and runs to frame 13.
void Player_804A6F38::fn_80259830() {
    if (field_0xb8c > 0) {
        field_0xb8c--;
    }
    if (mButtonHeld_0xb84 == 1 && !fn_8022C274(mPlayerNo_0x4)) {
        mButtonHeld_0xb84 = 0;
    }
    if (mAnimTimer_0x3fa < 2) {
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 3) {
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 4) {
        mPose_0x3fb = 2;
    } else if (mAnimTimer_0x3fa < 10) {
        mPose_0x3fb = 3;
    } else if (mAnimTimer_0x3fa < 11) {
        mPose_0x3fb = 4;
    } else if (mAnimTimer_0x3fa < 13) {
        mPose_0x3fb = 5;
    } else {
        mPose_0x3fb = 5;
        requestState_80251AEC(0);
    }
    if (!mButtonHeld_0xb84 && mAnimTimer_0x3fa == 11) {
        if (!field_0xbb8) {
            lbl_8053AED0->requestMessage_8043D67C(0xe8, mPlayerNo_0x4, 0);
            field_0xbb8 = 1;
        }
        CaneOfSomariaBlock_804AA9F0* block = static_cast<CaneOfSomariaBlock_804AA9F0*>(
            getActorManager_801F856C()->findActorBySerial_801F666C(field_0xb2c));
        if (block == NULL) {
            if (fn_80247594(0)) {
                JGeometry::TVec3f pos = mPos;
                switch (mFacing_0x3dc) {
                case 0:
                    pos.x += 16.0f;
                    break;
                case 1:
                    pos.x -= 16.0f;
                    break;
                case 2:
                    pos.y += 16.0f;
                    break;
                case 3:
                    pos.y -= 16.0f;
                    break;
                }
                field_0xb2c = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_SMRI, mLayer_0x3b4, &pos, 0, -1,
                                                                              mPlayerNo_0x4);
            }
        } else {
            block->fn_8030EA00();
        }
    }
    if (mAnimTimer_0x3fa == 12) {
        JGeometry::TVec3f pos = mPos;
        JGeometry::TBox2f box;
        switch (mFacing_0x3dc) {
        case 0:
            pos.x += 18.0f;
            box.set(pos.x, pos.y - 6.0f, pos.x + 36.0f, pos.y + 6.0f);
            field_0x580.set(box);
            break;
        case 1:
            pos.x -= 18.0f;
            box.set(pos.x - 36.0f, pos.y - 6.0f, pos.x, pos.y + 6.0f);
            field_0x580.set(box);
            break;
        case 2:
            pos.x += 4.0f;
            pos.y += 12.0f;
            box.set(pos.x - 6.0f, pos.y, pos.x + 6.0f, pos.y + 36.0f);
            field_0x580.set(box);
            break;
        case 3:
            pos.x -= 4.0f;
            pos.y -= 20.0f;
            box.set(pos.x - 6.0f, pos.y - 36.0f, pos.x + 6.0f, pos.y);
            field_0x580.set(box);
            break;
        }
        if (mButtonHeld_0xb84) {
            moveByStick_802808E8(1.125f);
            if (epsilonEquals(mPos, mLastPos_0x14, JGeometry::TUtilf::epsilon())) {
                mWalkAnimCounter_0x3f8 = 0;
            } else {
                mWalkAnimCounter_0x3f8++;
                if (mWalkAnimCounter_0x3f8 >= 28) {
                    mWalkAnimCounter_0x3f8 = 0;
                }
            }
            if (mWalkAnimCounter_0x3f8 < 4) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][0];
            } else if (mWalkAnimCounter_0x3f8 < 6) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][1];
            } else if (mWalkAnimCounter_0x3f8 < 10) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][2];
            } else if (mWalkAnimCounter_0x3f8 < 14) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][3];
            } else if (mWalkAnimCounter_0x3f8 < 18) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][4];
            } else if (mWalkAnimCounter_0x3f8 < 20) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][5];
            } else if (mWalkAnimCounter_0x3f8 < 24) {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][6];
            } else {
                mWalkFrame_0x3f9 = lbl_8046B190[mFacing_0x3dc][7];
            }
            if (!fn_80247594(1)) {
                mButtonHeld_0xb84 = 0;
            }
            if (field_0xb8c <= 0 && mFireSerial_0xb28 < 0) {
                s32 index = mPlayerNo_0x4;
                mFireSerial_0xb28 = getActorManager_801F856C()->spawnActor_801F9484(
                    ACTOR_FIRE, mLayer_0x3b4, &pos, ((mFacing_0x3dc & 3) << 4) | 0x1040 | (index & 3), -1, index);
            }
            s32 fire = mFireSerial_0xb28;
            if (fire >= 0) {
                FireRodFire_804A89F0* actor = static_cast<FireRodFire_804A89F0*>(
                    getActorManager_801F856C()->findActorBySerial_801F666C(fire));
                if (actor != NULL) {
                    actor->fn_802BAA0C(&box);
                } else {
                    mFireSerial_0xb28 = -1;
                    field_0xb8c = 60;
                }
            }
            if (field_0x12c4 == NULL) {
                field_0x12c4 = spawnEmitter_8013CC50(mLayer_0x3b4, &pos, 0x1ab, 0, 2, NULL, NULL, true, 1.0f);
            }
            if (field_0x12c4 != NULL) {
                JGeometry::TVec3<s16> rot;
                rot.x = 0;
                rot.y = lbl_8046B1B0[mFacing_0x3dc];
                rot.z = -0x8000;
                field_0x12c4->setGlobalRotation(rot);
                setEmitterPos_8013CB44(field_0x12c4, &pos, 0, 2, true);
            }
            if (field_0x12c8 == NULL) {
                field_0x12c8 = spawnEmitter_8013CC50(mLayer_0x3b4, &pos, 0x54a, -1, 0, NULL, NULL, true, 1.0f);
            }
            if (field_0x12c8 != NULL) {
                JGeometry::TVec3<s16> rot;
                rot.x = 0;
                rot.y = 0;
                rot.z = lbl_8046B1C0[mFacing_0x3dc];
                field_0x12c8->setGlobalRotation(rot);
                setEmitterPos_8013CB44(field_0x12c8, &pos, -1, 0, true);
            }
            playSound_8024F0E8(0x30011);
        } else {
            mAnimTimer_0x3fa++;
            if (field_0x12c8 != NULL) {
                field_0x12c8->stopCreateParticle();
            }
            if (field_0x12c4 != NULL) {
                field_0x12c4->stopCreateParticle();
            }
        }
    } else {
        mAnimTimer_0x3fa++;
    }
    fn_80268B3C();
}

void Player_804A6F38::fn_80259784() {
    if (field_0x12c4 != NULL) {
        field_0x12c4->becomeInvalidEmitter();
        field_0x12c4 = NULL;
    }
    if (field_0x12c8 != NULL) {
        field_0x12c8->becomeInvalidEmitter();
        field_0x12c8 = NULL;
    }
    s32 serial = mFireSerial_0xb28;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->fn_802BAA00();
        }
        mFireSerial_0xb28 = -1;
    }
}

void Player_804A6F38::fn_80259778() {
    mVel_0x344.z = 5.4f;
}

void Player_804A6F38::fn_80259660() {
    field_0x458 = 0;
    updateFacing_8027EB04();
    mVel_0x344.set(0.0f, 0.0f, 0.0f);
    mVel_0x344.z = 5.4f;
    playSound_8024F0E8(0x10002);
    mKeptVel_0x368.set(0.0f, 0.0f, 0.0f);
    field_0xb79 = 0;
    field_0x1204 = 0;
    if (fn_8024B24C()) {
        gatherOthers();
    }
    if (fn_8024B24C()) {
        field_0x1204 = 1;
    } else {
        field_0x1204 = 0;
    }
}

// FIXME: 99.69 %, scheduling+regalloc; behaviour clean.
// scheduling+regalloc: before the x/y speed test of the no-pad branch the original
// already computes &mVel_0x344 (PSVECMag's argument, addi r3) and so takes r4 for
// __float_epsilon; ours computes it after the test. Otherwise only .sdata2 literal names.
// The jump (enter fn_80259660, exit fn_80258BBC): the pad steers mVel_0x344 in x / y
// (0.8 from rest, 0.3 a frame rising, 0.2 falling, stopped by the side flags of
// field_0xbf0) at most 1.6 a frame; gravity 0.25 down to -4; on a 2D layer
// (viewLayerFlag) z moves y. Landing keeps the last speed in mKeptVel_0x368 and gathers the
// others (gatherOthers); at item level >= 2 (fn_802415CC) a second jump (field_0xb79);
// after 33 frames on the floor the button jumps again, else 3 frames end it (state 0);
// 0x3FB is the animation frame.
void Player_804A6F38::fn_80258BE8() {
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
            f32 height = mPos.z;
            if (viewLayerFlag(mLayer_0x3b4)) {
                mPos.x += mVel_0x344.x;
                mPos.y -= mVel_0x344.z;
                mPos.z = 0.5f;
                if (mVel_0x344.z < 0.0f && field_0xbf0->field_0x48[2]) {
                    mKeptVel_0x368.set(mVel_0x344.x, mVel_0x344.y, 0.0f);
                    mVel_0x344.set(0.0f, 0.0f, 0.0f);
                    mPos.z = 0.0f;
                    if (height > 0.0f && (fn_8024B24C() || field_0x1204)) {
                        gatherOthers();
                    }
                }
            } else {
                mPos += mVel_0x344;
                if (mVel_0x344.z < 0.0f && mPos.z <= 0.0f) {
                    mPos.z = 0.0f;
                    mKeptVel_0x368.set(mVel_0x344.x, mVel_0x344.y, 0.0f);
                    mVel_0x344.set(0.0f, 0.0f, 0.0f);
                    if (height > 0.0f && (fn_8024B24C() || field_0x1204)) {
                        gatherOthers();
                    }
                }
            }
        }
    }

    if (!field_0xb79 && mPos.z > 0.0f && fn_802415CC() >= 2 && fn_8022C310(mPlayerNo_0x4)) {
        mPose_0x3fb = 0;
        mAnimTimer_0x3fa = 0;
        mStateTimer_0x3fc = 0;
        mVel_0x344.z = 5.4f;
        field_0xb79 = 1;
    }

    if (mAnimTimer_0x3fa < 4) {
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 9) {
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 13) {
        mPose_0x3fb = 2;
    } else if (mAnimTimer_0x3fa < 17) {
        mPose_0x3fb = 3;
    } else if (mAnimTimer_0x3fa < 21) {
        mPose_0x3fb = 4;
    } else if (mAnimTimer_0x3fa < 25) {
        mPose_0x3fb = 5;
    } else if (mAnimTimer_0x3fa < 29) {
        mPose_0x3fb = 6;
    } else if (mAnimTimer_0x3fa < 33) {
        mPose_0x3fb = 7;
    } else if (mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        if (fn_8022C310(mPlayerNo_0x4)) {
            mPose_0x3fb = 0;
            mAnimTimer_0x3fa = 0;
            mStateTimer_0x3fc = 0;
            mVel_0x344.z = 5.4f;
            field_0xb79 = 0;
            if (fn_8024B24C()) {
                gatherOthers();
            }
            if (fn_8024B24C()) {
                field_0x1204 = 1;
            } else {
                field_0x1204 = 0;
            }
        } else {
            mPose_0x3fb = 0;
            mStateTimer_0x3fc++;
            if (mStateTimer_0x3fc >= 3) {
                requestState_80251AEC(0);
            }
        }
    } else {
        mPose_0x3fb = 7;
    }

    if (mAnimTimer_0x3fa >= 8 && viewLayerFlag(mLayer_0x3b4) && mVel_0x344.z < 0.0f && mPos.z <= 1.0f) {
        JGeometry::TVec3f probe;
        fn_8024B298(2, &probe);
        probe.y += 2.0f;
        if (getTileCollisionCode_8022461C(&probe, mLayer_0x3b4) != 2) {
            requestState_80251AEC(0);
        }
    }

    if (!field_0x458 && mAnimTimer_0x3fa >= 8 && fn_8022C3AC(mPlayerNo_0x4)) {
        if (fn_80243234()) {
            requestState_80251AEC(7);
        } else {
            requestState_80251AEC(5);
        }
    }
    mAnimTimer_0x3fa++;
    fn_802685D4();
}

void Player_804A6F38::fn_80258BBC() {
    if (mRequestedState_0x1244 != 5 && mRequestedState_0x1244 != 7) {
        mPos.z = 0.0f;
    }
    field_0xb79 = 0;
    field_0x1204 = 0;
}

void Player_804A6F38::fn_80258BA8() {
    field_0x458 = 0;
    field_0xb9f = false;
    field_0xba0 = 0;
}

// Update of state 0x47 (item 9): a dig 16 in front of the player at floor level.
// Frame 11 sets the 8x8 box field_0x5cc there (field_0xb9e), frame 12 asks the
// background (fn_802A1844) and plays a sound by the result; results 1-4 give an
// effect by facing at frame 28. 0x3FD selects the short animation (frames 4-6).
void Player_804A6F38::fn_80258890() {
    JGeometry::TVec3f pos = mPos;
    pos.z = 0.0f;
    switch (mFacing_0x3dc) {
    case 0:
        pos.x += 16.0f;
        break;
    case 1:
        pos.x -= 16.0f;
        break;
    case 2:
        pos.y += 16.0f;
        break;
    case 3:
        pos.y -= 16.0f;
        break;
    }
    if (mSubPose_0x3fd == 1) {
        if (mAnimTimer_0x3fa < 16) {
            mPose_0x3fb = 4;
        } else if (mAnimTimer_0x3fa < 19) {
            mPose_0x3fb = 5;
        } else if (mAnimTimer_0x3fa < 22) {
            mPose_0x3fb = 6;
        } else {
            mPose_0x3fb = 6;
            requestState_80251AEC(0);
        }
    } else {
        if (mAnimTimer_0x3fa < 12) {
            mPose_0x3fb = 0;
        } else if (mAnimTimer_0x3fa < 28) {
            mPose_0x3fb = 1;
        } else if (mAnimTimer_0x3fa < 32) {
            mPose_0x3fb = 2;
        } else if (mAnimTimer_0x3fa < 40) {
            mPose_0x3fb = 3;
        } else {
            mPose_0x3fb = 3;
            requestState_80251AEC(0);
        }
        if (mAnimTimer_0x3fa == 11) {
            field_0xb9e = true;
            field_0x5cc.set(pos.x - 4.0f, pos.y - 4.0f, pos.x + 4.0f, pos.y + 4.0f);
        }
        if (mAnimTimer_0x3fa == 12) {
            field_0xba0 = gBgRewrite_8053AC98->fn_802A1844(&pos, mPlayerNo_0x4, mLayer_0x3b4);
            if ((field_0xba0 > 0 && field_0xba0 <= 4) || field_0xb9f) {
                playSound_8024F0E8(0x20044);
            } else {
                mPose_0x3fb = 0;
                mSubPose_0x3fd = 1;
                if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0x39) {
                    playSound_8024F0E8(0x20040);
                } else if (field_0xba0 == 8) {
                    playSound_8024F0E8(0x2004F);
                } else {
                    playSound_8024F0E8(0x20066);
                }
            }
            field_0xb9e = false;
            field_0xb9f = false;
        }
        if (mAnimTimer_0x3fa == 28 && mSubPose_0x3fd == 0 && field_0xba0 > 0 && field_0xba0 <= 4) {
            spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, lbl_8046B200[field_0xba0 - 1][mFacing_0x3dc], 0, 0, NULL, NULL, true, 1.0f);
        }
    }
    mAnimTimer_0x3fa++;
    fn_80268B3C();
}

void Player_804A6F38::fn_8025888C() {}

void Player_804A6F38::fn_80258880() {
    field_0x458 = 0;
}

// Update of state 0x48 (item 2, the Fire Rod: it spawns ACTOR_FIRE, "Fire Rod fire"):
// at frame 2, when fn_80247594(0) allows it and the player's last fire (field_0xb3c)
// is gone, one is spawned by the player, with the item level fn_802415CC in bits 12+
// of its argument; the state ends after 8 frames.
void Player_804A6F38::fn_802586D0() {
    if (mAnimTimer_0x3fa < 2) {
        mPose_0x3fb = 0;
    } else if (mAnimTimer_0x3fa < 6) {
        mPose_0x3fb = 1;
    } else if (mAnimTimer_0x3fa < 8) {
        mPose_0x3fb = 0;
    } else {
        mPose_0x3fb = 0;
        requestState_80251AEC(0);
    }
    if (mAnimTimer_0x3fa == 2 && fn_80247594(0) && field_0xb3c < 0) {
        JGeometry::TVec3f pos = mPos;
        switch (mFacing_0x3dc) {
        case 0:
            pos.y -= 2.0f;
            break;
        case 1:
            pos.y -= 2.0f;
            break;
        case 2:
            pos.x += 2.0f;
            break;
        case 3:
            pos.x -= 2.0f;
            break;
        }
        s32 serial; // declared before index: the original gives serial the higher saved register
        s32 index = mPlayerNo_0x4;
        serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_FIRE, mLayer_0x3b4, &pos,
                                                (fn_802415CC() << 12) | ((mFacing_0x3dc & 3) << 4) | (index & 3) | 0x140,
                                                -1, index);
        if (getActorManager_801F856C()->findActorBySerial_801F666C(serial) != NULL) {
            field_0xb3c = serial;
        }
    }
    mAnimTimer_0x3fa++;
    fn_80268B3C();
}

void Player_804A6F38::fn_802586CC() {}

void Player_804A6F38::fn_802585F4() {
    for (s32 i = 0; i < 3; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mArrowSerials_0xb00[i]);
        if (actor == NULL) {
            mArrowSerials_0xb00[i] = -1;
            field_0xb40[i] = -1;
        } else {
            JGeometry::TVec2<f32> pos;
            pos.x = actor->mPos.x;
            pos.y = actor->mPos.y;
            field_0xb40[i] = actor->mLayer_0x4;
            field_0x4e0[i].set(actor->mBox);
            field_0x4e0[i].addPos(pos);
        }
    }
}

void Player_804A6F38::fn_8025851C() {
    for (s32 i = 0; i < 3; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(mSlingshotSerials_0xb0c[i]);
        if (actor == NULL) {
            mSlingshotSerials_0xb0c[i] = -1;
            field_0xb4c[i] = -1;
        } else {
            JGeometry::TVec2<f32> pos;
            pos.x = actor->mPos.x;
            pos.y = actor->mPos.y;
            field_0xb4c[i] = actor->mLayer_0x4;
            field_0x510[i].set(actor->mBox);
            field_0x510[i].addPos(pos);
        }
    }
}

void Player_804A6F38::fn_80258470() {
    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(field_0xb18);
    if (actor == NULL) {
        field_0xb18 = -1;
        field_0xb58 = -1;
    } else {
        JGeometry::TVec2<f32> pos;
        pos.x = actor->mPos.x;
        pos.y = actor->mPos.y;
        field_0xb58 = actor->mLayer_0x4;
        field_0x540.set(actor->mBox);
        field_0x540.addPos(pos);
    }
}

void Player_804A6F38::fn_80258384() {
    s32 serial; // declared outside the loop: the original keeps the counter in the lower saved register
    for (s32 i = 0; i < 3; i++) {
        serial = field_0xb1c[i];
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor == NULL) {
                field_0xb1c[i] = -1;
                field_0xb5c[i] = -1;
            } else {
                JGeometry::TVec2<f32> pos;
                pos.x = actor->mPos.x;
                pos.y = actor->mPos.y;
                field_0xb5c[i] = actor->mLayer_0x4;
                field_0x550[i].set(actor->mBox);
                field_0x550[i].addPos(pos);
            }
        }
    }
}

void Player_804A6F38::fn_8025825C() {
    s32 serial = field_0xb38;
    if (serial < 0) {
        field_0xb68 = 0;
        return;
    }
    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
    if (actor == NULL) {
        field_0xb68 = 0;
        field_0xb38 = -1;
        field_0xb6c = -1;
        return;
    }
    if (actor->mVel_0x3c.isZero()) {
        field_0xb68 = 0;
    } else {
        field_0xb68 = 1;
    }
    JGeometry::TVec2<f32> pos;
    pos.x = actor->mPos.x;
    pos.y = actor->mPos.y;
    field_0xb6c = actor->mLayer_0x4;
    field_0x590.set(actor->mBox);
    field_0x590.addPos(pos);
}

void Player_804A6F38::fn_8025819C() {
    s32 serial = field_0xb3c;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor == NULL) {
            field_0xb3c = -1;
            field_0xb70 = -1;
        } else {
            JGeometry::TVec2<f32> pos;
            pos.x = actor->mPos.x;
            pos.y = actor->mPos.y;
            field_0xb70 = actor->mLayer_0x4;
            field_0x5a0.set(actor->mBox);
            field_0x5a0.addPos(pos);
        }
    }
}

void Player_804A6F38::fn_802580E0() {
    f32 target;
    if (isDeadState_80270A8C()) {
        target = 0.0f;
    } else if (gGameRoot_8053AAF8->field_0x7f) {
        if (fn_802416C4() == 2) {
            target = 100.0f;
        } else {
            target = 0.0f;
        }
    } else {
        target = 85.0f;
    }
    if (field_0xb90 > target) {
        field_0xb90 -= 1.0f;
        if (field_0xb90 < target) {
            field_0xb90 = target;
        }
    } else if (field_0xb90 < target) {
        field_0xb90 += 1.0f;
        if (field_0xb90 > target) {
            field_0xb90 = target;
        }
    }
}

void Player_804A6F38::fn_80258070() {
    s32 serial = field_0xb2c;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor == NULL) {
            field_0xb2c = -1;
        } else if (!static_cast<CaneOfSomariaBlock_804AA9F0*>(actor)->fn_8030EA0C()) {
            field_0xb2c = -1;
        }
    }
}

void Player_804A6F38::fn_80257FF4() {
    s32 serial; // declared outside the loop: the original keeps the counter in the lower saved register
    for (s32 i = 0; i < 2; i++) {
        serial = field_0xb30[i];
        if (serial >= 0 && !getActorManager_801F856C()->findActorBySerial_801F666C(serial)) {
            field_0xb30[i] = -1;
        }
    }
}

bool Player_804A6F38::fn_80257DB4() {
    if (field_0x1174) {
        return false;
    }
    if (hasLeader_80240808() && fn_80230188(getLeader_80240644())) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (isDeadState_80270A8C()) {
        return false;
    }
    if (isLifting_8025F684() && fn_802416C4() != 7) {
        return false;
    }
    if (isGrabbing_8025F448()) {
        return false;
    }
    if (fn_802412A4()) {
        return false;
    }
    if (fn_8024C448()) {
        return false;
    }
    if (fn_80241230()) {
        return false;
    }
    if (isPushing_80250D30()) {
        return false;
    }
    if (fn_802411C4()) {
        return false;
    }
    if (fn_80240B70()) {
        return false;
    }
    if (fn_80240B5C()) {
        return false;
    }
    if (fn_80240B84()) {
        return false;
    }
    if (fn_8024BBB0()) {
        return false;
    }
    if (fn_80240D64()) {
        return false;
    }
    if (fn_80240DAC()) {
        return false;
    }
    if (mPos.z > 0.0f) {
        return false;
    }
    if (fn_8026FCB0()) {
        return false;
    }
    if (fn_802412C4()) {
        return false;
    }
    if (field_0xd74 != 0) {
        return false;
    }
    return field_0xd3c < 0;
}

// FIXME: 95.16 %, different; behaviour clean.
// try.py "different", but only block order and registers: the original lays
// out item 7's mPos.z test (off view-flag layers) after the floor probe, ours before it (the
// source order `if (viewLayerFlag) { probe } else if (mPos.z > 0) return;` gives the
// original's layout, 98.07 %, but pools 2.0f before 0.0f and drops 11 functions of the
// unit to reloc-name); and this / the item index swap r30 / r31.
// The per-frame item check: while fn_8022B334 and fn_8022C310 hold, the selected
// item fn_802416C4 (1-10) requests its state. Items 3 and 6 allow three of their
// actors (mArrowSerials_0xb00 / field_0xb1c) at a time, item 8 ('HBMB') two (field_0xb30).
void Player_804A6F38::fn_8025785C() {
    if (field_0xb74 > 0) {
        field_0xb74--;
    }
    if (fn_8022B334(mPlayerNo_0x4)) {
        if (hasLeader_80240808()) {
            if (fn_802373EC(getLeader_80240644()) != fn_802416C4()) {
                fn_80241618(fn_802373EC(getLeader_80240644()), 1);
            }
        }
        if (fn_8022C310(mPlayerNo_0x4)) {
            if (fn_802416C4() == 1) {
                requestState_80251AEC(0x39);
            } else if (fn_802416C4() == 2) {
                requestState_80251AEC(0x48);
            } else if (fn_802416C4() == 3) {
                if (field_0xb18 < 0) {
                    s32 count = 0;
                    for (s32 i = 0; i < 3; i++) {
                        if (mArrowSerials_0xb00[i] >= 0) {
                            count++;
                        }
                    }
                    if (count < 3) {
                        requestState_80251AEC(0x3d);
                    }
                }
            } else if (fn_802416C4() == 4) {
                if (field_0xb18 < 0 && field_0xb74 <= 0 &&
                    (!fn_8022B160() || ((!hasLeader_80240808() || fn_802486A0() == 4) && !lbl_8053ABB8[mPlayerNo_0x4]))) {
                    requestState_80251AEC(0x3a);
                }
            } else if (fn_802416C4() == 5) {
                requestState_80251AEC(0x3c);
            } else if (fn_802416C4() == 6) {
                if (fn_802415CC() >= 2) {
                    requestState_80251AEC(0x45);
                } else {
                    requestFlame_8025785C();
                }
            } else if (fn_802416C4() == 7) {
                requestJump_8025785C();
            } else if (fn_802416C4() == 8) {
                for (s32 i = 0; i < 2; i++) {
                    if (field_0xb30[i] < 0) {
                        const Vec& d = lbl_8046B220[mFacing_0x3dc];
                        JGeometry::TVec3f pos;
                        pos.x = d.x;
                        pos.y = d.y;
                        pos.z = d.z;
                        pos += mPos;
                        pos.z = 0.0f;
                        if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 0xe) {
                            break;
                        }
                        s32 serial;
                        if (fn_802415CC() >= 2) {
                            serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_HBMB, mLayer_0x3b4, &pos, 8, -1, mPlayerNo_0x4);
                        } else {
                            serial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_HBMB, mLayer_0x3b4, &pos, 0, -1, mPlayerNo_0x4);
                        }
                        if (getActorManager_801F856C()->findActorBySerial_801F666C(serial) != NULL) {
                            field_0xb30[i] = serial;
                            playSound_8024F0E8(0x20006);
                            break;
                        }
                    }
                }
            } else if (fn_802416C4() == 9) {
                requestState_80251AEC(0x47);
            } else if (fn_802416C4() == 10) {
                if (field_0xb18 < 0) {
                    requestState_80251AEC(0x3b);
                }
            } else if (fn_80246228()) {
                field_0x1200 = true;
            }
        }
    }
    field_0x1174 = 0;
}

// Item 6 below level 2 (state 0x44): at most three flames (field_0xb1c) at a time.
// requestFlame / requestJump: inlined into fn_8025785C, no out-of-line copy survives (inline
// here so none is emitted). Their evidence is fn_8025785C's asm: every refusal branches
// straight to its final field_0x1174 store (a return out of the inlined body), and the
// flame count starts after the second fn_802415CC call.
inline void Player_804A6F38::requestFlame_8025785C() {
    if (fn_802415CC() < 2) {
        s32 count = 0;
        for (s32 i = 0; i < 3; i++) {
            if (field_0xb1c[i] >= 0) {
                count++;
            }
        }
        if (count >= 3) {
            return;
        }
    }
    requestState_80251AEC(0x44);
}

// Item 7 (the jump, state 0x46; 0x14 while lifting) is refused while a living actor is held,
// any player holds 'RCK2', the floor probe above the hand point is tile 2 on view-flag
// layers, or the player is in the air elsewhere.
inline void Player_804A6F38::requestJump_8025785C() {
    if (fn_8025F500()) {
        ActorBase_804A5770* held = heldActor();
        if (held != NULL && held->field_0x1a4 != 0) {
            return;
        }
    }
    for (s32 i = 0; i < 4; i++) {
        if (fn_80243194(i) && getPlayerHeldActorId_8023EEB4(i) == 'RCK2') {
            return;
        }
    }
    if (!viewLayerFlag(mLayer_0x3b4)) {
        if (mPos.z > 0.0f) {
            return;
        }
    } else if (!fn_802518AC(true)) {
        JGeometry::TVec3f pos;
        fn_8024B298(2, &pos);
        pos.y += 2.0f;
        if (getTileCollisionCode_8022461C(&pos, mLayer_0x3b4) == 2) {
            return;
        }
    }
    if (isLifting_8025F684()) {
        requestState_80251AEC(0x14);
    } else {
        requestState_80251AEC(0x46);
    }
}

bool Player_804A6F38::fn_802577E0() {
    if (fn_80270C88()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (mState_0x1240 == 0x3f || mState_0x1240 == 0xc) {
        return false;
    }
    return requestState_80251AEC(0x3f);
}

bool Player_804A6F38::fn_802576CC(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    for (s32 i = 0; i < 3; i++) {
        if (mArrowSerials_0xb00[i] >= 0 && field_0xb40[i] == layer) {
            JGeometry::TBox2f overlap(field_0x4e0[i]);
            if (overlap.intersect(*box)) {
                out->x = field_0x4e0[i].f.x + (field_0x4e0[i].i.x - field_0x4e0[i].f.x) * 0.5f;
                out->y = field_0x4e0[i].f.y + (field_0x4e0[i].i.y - field_0x4e0[i].f.y) * 0.5f;
                field_0xb7c = i;
                return true;
            }
        }
    }
    field_0xb7c = -1;
    return false;
}

bool Player_804A6F38::fn_802575B8(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    for (s32 i = 0; i < 3; i++) {
        if (mSlingshotSerials_0xb0c[i] >= 0 && field_0xb4c[i] == layer) {
            JGeometry::TBox2f overlap(field_0x510[i]);
            if (overlap.intersect(*box)) {
                out->x = field_0x510[i].f.x + (field_0x510[i].i.x - field_0x510[i].f.x) * 0.5f;
                out->y = field_0x510[i].f.y + (field_0x510[i].i.y - field_0x510[i].f.y) * 0.5f;
                field_0xb80 = i;
                return true;
            }
        }
    }
    field_0xb80 = -1;
    return false;
}

bool Player_804A6F38::fn_802574CC(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    if (field_0xb18 < 0) {
        return false;
    }
    if (field_0xb58 != layer) {
        return false;
    }
    JGeometry::TBox2f overlap(field_0x540);
    if (overlap.intersect(*box)) {
        out->x = field_0x540.f.x + (field_0x540.i.x - field_0x540.f.x) * 0.5f;
        out->y = field_0x540.f.y + (field_0x540.i.y - field_0x540.f.y) * 0.5f;
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_802573E0(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    if (field_0xb38 < 0) {
        return false;
    }
    if (field_0xb6c != layer) {
        return false;
    }
    JGeometry::TBox2f overlap(field_0x590);
    if (overlap.intersect(*box)) {
        out->x = field_0x590.f.x + (field_0x590.i.x - field_0x590.f.x) * 0.5f;
        out->y = field_0x590.f.y + (field_0x590.i.y - field_0x590.f.y) * 0.5f;
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_802570BC(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    if (fn_80249068()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    for (s32 i = 0; i < 3; i++) {
        if (field_0xb1c[i] >= 0 && field_0xb5c[i] == layer) {
            JGeometry::TBox2f overlap(field_0x550[i]);
            if (overlap.intersect(*box)) {
                out->x = field_0x550[i].f.x + (field_0x550[i].i.x - field_0x550[i].f.x) * 0.5f;
                out->y = field_0x550[i].f.y + (field_0x550[i].i.y - field_0x550[i].f.y) * 0.5f;
                return true;
            }
        }
    }
    if (mFireSerial_0xb28 >= 0 && layer == mLayer_0x3b4) {
        JGeometry::TBox2f overlap(field_0x580);
        if (overlap.intersect(*box)) {
            out->x = field_0x580.f.x + (field_0x580.i.x - field_0x580.f.x) * 0.5f;
            out->y = field_0x580.f.y + (field_0x580.i.y - field_0x580.f.y) * 0.5f;
            return true;
        }
    }
    if (field_0xb3c >= 0 && field_0xb70 == layer) {
        JGeometry::TBox2f overlap(field_0x5a0);
        if (overlap.intersect(*box)) {
            out->x = field_0x5a0.f.x + (field_0x5a0.i.x - field_0x5a0.f.x) * 0.5f;
            out->y = field_0x5a0.f.y + (field_0x5a0.i.y - field_0x5a0.f.y) * 0.5f;
            return true;
        }
    }
    return false;
}

u8 Player_804A6F38::fn_80256F44(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    if (fn_80249068()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (!fn_802413F0()) {
        return false;
    }
    if (layer != mLayer_0x3b4) {
        return false;
    }
    if (!field_0xb99) {
        return false;
    }
    JGeometry::TBox2f overlap(field_0x5b0);
    if (overlap.intersect(*box)) {
        out->x = field_0x5b0.f.x + (field_0x5b0.i.x - field_0x5b0.f.x) * 0.5f;
        out->y = field_0x5b0.f.y + (field_0x5b0.i.y - field_0x5b0.f.y) * 0.5f;
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_80256DCC(const JGeometry::TBox2f* box, s32 layer, JGeometry::TVec2f* out) {
    if (fn_80249068()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (!field_0xb9e) {
        return false;
    }
    if (!fn_802413F0()) {
        return false;
    }
    if (layer != mLayer_0x3b4) {
        return false;
    }
    JGeometry::TBox2f overlap(field_0x5cc);
    if (overlap.intersect(*box)) {
        out->x = field_0x5cc.f.x + (field_0x5cc.i.x - field_0x5cc.f.x) * 0.5f;
        out->y = field_0x5cc.f.y + (field_0x5cc.i.y - field_0x5cc.f.y) * 0.5f;
        return true;
    }
    return false;
}

bool Player_804A6F38::fn_80256C74(const JGeometry::TBox2f* box, s32 layer) {
    if (fn_80249068()) {
        return false;
    }
    if (fn_802413E8()) {
        return false;
    }
    if (!isDeadState_80270A8C()) {
        return false;
    }
    if (!field_0xba4) {
        return false;
    }
    if (!fn_802413F0()) {
        return false;
    }
    if (layer != mLayer_0x3b4) {
        return false;
    }
    JGeometry::TBox2f overlap(field_0x5dc);
    if (overlap.intersect(*box)) {
        return true;
    }
    return false;
}
