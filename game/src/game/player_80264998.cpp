#include "game/GameAudio.h"
#include "game/Effect.h"
#include "game/GameMath.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/PlayerBit.h"
#include "game/DrawShadow.h"
#include "game/SpriteParams.h"
#include "game/ViewLayer.h"

// Draws frame `frame` of the "SOB " sprite sheet `name` at pos.
static inline void drawSob(const char* name, const JGeometry::TVec3f& pos, s32 frame,
                           SpriteParams_801F5AE0* params) {
    void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', name);
    gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, frame, params, 1.0f, 1.0f);
}

// drawSob on the TV layers (below 8); the GBA layers draw through
// showGbaSprite_803075AC instead. The draw tests the layer again after loading the
// frame (a second bge on the same compare), so the test is part of a helper.
static inline void drawSobOnTv(s32 layer, const char* name, const JGeometry::TVec3f& pos, s32 frame,
                               SpriteParams_801F5AE0* params) {
    if (layer < 8) {
        drawSob(name, pos, frame, params);
    }
}

// The scale part of m (its translation column is the caller's). The draw
// computes the three scales z first, as arguments of an inline call.
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

// Frame `frame` on the GBA layers (showGbaSprite_803075AC: serial, layer, pos, id). The
// draw builds the position as the call's argument, with no named copy.
static inline void drawGba(s32 serial, s32 layer, const JGeometry::TVec3f& pos, s32 frame) {
    showGbaSprite_803075AC(serial, layer, &pos, frame, 0, 1, -1, false, 0, 0);
}

// The body frame mCel_0x3c4 by facing (mFacing_0x3dc: right, left, down, up).
static inline void setFacingFrame(Player_804A6F38* player, u16 right, u16 left, u16 down, u16 up) {
    if (player->mFacing_0x3dc == 0) {
        player->mCel_0x3c4 = right;
    } else if (player->mFacing_0x3dc == 1) {
        player->mCel_0x3c4 = left;
    } else if (player->mFacing_0x3dc == 2) {
        player->mCel_0x3c4 = down;
    } else {
        player->mCel_0x3c4 = up;
    }
}

// The standing frame (0x3C-0x3F), or the carried pose (frames 0x1BF-0x1C3)
// of player 4 of a group outside the leader's own slot (getGroupTurn_80240888 !=
// getFollowerIndex_80248778). Inlined by the frame
// functions below; fn_80265E88, defined before it, calls the out-of-line copy
// MWCC emits at the end of the file (0x80268194).
inline void Player_804A6F38::setStandingFrame_80268194() {
    if (fn_802486A0() == 4 && getGroupTurn_80240888() != getFollowerIndex_80248778()) {
        setFacingFrame(this, 0x1BF, 0x1C1, 0x1C3, 0x1C3);
    } else {
        setFacingFrame(this, 0x3C, 0x3D, 0x3E, 0x3F);
    }
}

// FIXME: 96.69. Case 4 of the switch gets its own copy of the closing
// `mCel_0x3c4 += mPose_0x3fb` in the original (cases 0-3 and the default
// share the other); ours shares one copy for all.
void Player_804A6F38::fn_80267ECC() {
    if (mPose_0x3fb >= 8) {
        setStandingFrame_80268194();
        return;
    }
    switch (mAttackKind_0x45c) {
    case 0:
        setFacingFrame(this, 0x20, 0x27, 0x2E, 0x35);
        break;
    case 1:
        setFacingFrame(this, 0x232, 0x23A, 0x242, 0x24A);
        break;
    case 2:
        setFacingFrame(this, 0x252, 0x256, 0x25A, 0x25E);
        break;
    case 3:
        setFacingFrame(this, 0x27A, 0x27E, 0x282, 0x286);
        break;
    case 4:
        setFacingFrame(this, 0x262, 0x268, 0x26E, 0x274);
        break;
    }
    mCel_0x3c4 += mPose_0x3fb;
}

void Player_804A6F38::fn_80267E18() {
    if (mSwordCharge_0x403 >= 0xA8) {
        setFacingFrame(this, 0x6F, 0x77, 0x7F, 0x87);
    } else {
        setFacingFrame(this, 0x43, 0x4B, 0x53, 0x61);
    }
    mCel_0x3c4 += mWalkFrame_0x3f9;
}

// The spin: frames 0x8F-0x96 from the facing's own one, by mPose_0x3fb.
// The frame number is a byte (clrlwi 24 before each sth).
void Player_804A6F38::fn_80267D30() {
    u8 f;
    if (mFacing_0x3dc == 0) {
        if (mPose_0x3fb < 8) {
            f = (mPose_0x3fb & 7) + 0x8F;
            mCel_0x3c4 = f;
        } else {
            f = mPose_0x3fb + 0x8F;
            mCel_0x3c4 = f;
        }
    } else if (mFacing_0x3dc == 1) {
        if (mPose_0x3fb < 8) {
            f = ((mPose_0x3fb + 4) & 7) + 0x8F;
            mCel_0x3c4 = f;
        } else {
            f = mPose_0x3fb + 0x93;
            mCel_0x3c4 = f;
        }
    } else if (mFacing_0x3dc == 2) {
        if (mPose_0x3fb < 8) {
            f = ((mPose_0x3fb + 2) & 7) + 0x8F;
            mCel_0x3c4 = f;
        } else {
            f = mPose_0x3fb + 0x97;
            mCel_0x3c4 = f;
        }
    } else {
        if (mPose_0x3fb < 8) {
            f = ((mPose_0x3fb + 6) & 7) + 0x8F;
            mCel_0x3c4 = f;
        } else {
            f = mPose_0x3fb + 0x9B;
            mCel_0x3c4 = f;
        }
    }
}

void Player_804A6F38::fn_80267C6C() {
    if (mSubPose_0x3fd == 1) {
        setFacingFrame(this, 0x33A, 0x33D, 0x340, 0x343);
        mCel_0x3c4 += mPose_0x3fb;
    } else {
        setFacingFrame(this, 0x346, 0x34A, 0x34E, 0x352);
        mCel_0x3c4 += mPose_0x3fb;
    }
}

void Player_804A6F38::fn_80267BA8() {
    if (mPose_0x3fb == 1) {
        setFacingFrame(this, 0x322, 0x327, 0x32C, 0x331);
        mCel_0x3c4 += mSubPose_0x3fd;
    } else {
        setFacingFrame(this, 0x36D, 0x36F, 0x371, 0x373);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    }
}

void Player_804A6F38::fn_80267B18() {
    if (mTransitionKind_0xcf4 == 0xD) {
        mCel_0x3c4 = 0x121;
    } else if (mPose_0x3fb == 0) {
        mCel_0x3c4 = mSubPose_0x3fd + 0xB7;
    } else {
        setFacingFrame(this, 0xA7, 0xAB, 0xAF, 0xB3);
        mCel_0x3c4 += mSubPose_0x3fd;
    }
}

void Player_804A6F38::fn_80267A54() {
    if (mPose_0x3fb == 0) {
        setFacingFrame(this, 0x0, 0x8, 0x10, 0x18);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    } else {
        setFacingFrame(this, 0xC5, 0xC9, 0xCD, 0xD4);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    }
}

void Player_804A6F38::fn_80267990() {
    if (mPose_0x3fb == 1) {
        setFacingFrame(this, 0x14F, 0x152, 0x155, 0x158);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    } else {
        setFacingFrame(this, 0x13F, 0x143, 0x147, 0x14B);
        mCel_0x3c4 += mPose_0x3fb;
    }
}

void Player_804A6F38::fn_802678CC() {
    if (mPose_0x3fb == 1) {
        setFacingFrame(this, 0x2F6, 0x2F9, 0x2FC, 0x2FF);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    } else {
        setFacingFrame(this, 0x2E6, 0x2EA, 0x2EE, 0x2F2);
        mCel_0x3c4 += mPose_0x3fb;
    }
}

void Player_804A6F38::fn_80267818() {
    if (field_0xb9b == true) {
        setFacingFrame(this, 0x16B, 0x173, 0x17B, 0x183);
    } else {
        setFacingFrame(this, 0x123, 0x12A, 0x131, 0x138);
    }
    mCel_0x3c4 += mPose_0x3fb;
}

void Player_804A6F38::fn_80267748() {
    if (mButtonHeld_0xb84 && mPose_0x3fb >= 5) {
        setFacingFrame(this, 0x1D2, 0x1D6, 0x1DA, 0x1E1);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    } else {
        setFacingFrame(this, 0x18B, 0x191, 0x197, 0x19D);
        mCel_0x3c4 += mPose_0x3fb;
    }
}

void Player_804A6F38::fn_80267684() {
    if (field_0x458 == 1) {
        setFacingFrame(this, 0x262, 0x268, 0x26E, 0x274);
        mCel_0x3c4 += mPose_0x3fb;
    } else {
        setFacingFrame(this, 0xF1, 0xF9, 0x101, 0x109);
        mCel_0x3c4 += mPose_0x3fb;
    }
}

void Player_804A6F38::fn_80267598() {
    setStandingFrame_80268194();
}

void Player_804A6F38::fn_80267470() {
    if (!isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4) && !hasLeader_80240808()) {
        mCel_0x3c4 = 0x31D;
    } else {
        setStandingFrame_80268194();
    }
}

void Player_804A6F38::fn_80267384() {
    setStandingFrame_80268194();
}

void Player_804A6F38::fn_802671C8() {
    if (fn_802486A0() == 4 && getGroupTurn_80240888() != getFollowerIndex_80248778()) {
        setFacingFrame(this, 0x1BF, 0x1C1, 0x1C3, 0x1C3);
        return;
    }
    switch (mPose_0x3fb) {
    case 0:
        setFacingFrame(this, 0x1E8, 0x1EC, 0x1F0, 0x1F4);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 1:
        mCel_0x3c4 = 0x1F8;
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 2:
        setFacingFrame(this, 0x1FC, 0x200, 0x204, 0x208);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 3:
        mCel_0x3c4 = 0x358;
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    }
}

void Player_804A6F38::fn_802670CC() {
    if (fn_802486A0() == 4 && getGroupTurn_80240888() != getFollowerIndex_80248778()) {
        setFacingFrame(this, 0x1BF, 0x1C1, 0x1C3, 0x1C3);
    } else {
        setFacingFrame(this, 0x20C, 0x214, 0x21C, 0x224);
        mCel_0x3c4 += mWalkFrame_0x3f9;
    }
}

void Player_804A6F38::fn_80267018() {
    if (mLayer_0x3b4 < 8) {
        setFacingFrame(this, 0x375, 0x37D, 0x385, 0x38D);
    } else {
        setFacingFrame(this, 0x395, 0x39D, 0x3A5, 0x3AD);
    }
    mCel_0x3c4 += mWalkFrame_0x3f9;
}

// The body frame mCel_0x3c4 for this frame (fn_80285E58, every frame;
// the previous one is kept in mPrevCel_0x3c6): fixed frames for the overrides
// (0xCFE; field_0x456 / fn_802413E8 take field_0x450 and the alpha
// field_0xca2), else one per state (mState_0x1240), mostly by facing plus a
// walk / action counter (mWalkFrame_0x3f9, mPose_0x3fb, mSubPose_0x3fd).
void Player_804A6F38::fn_80265E88() {
    mPrevCel_0x3c6 = mCel_0x3c4;
    if (field_0xcfe) {
        if (mPos.z > 0.0f) {
            mCel_0x3c4 = 0xC2;
        } else {
            mCel_0x3c4 = 0xC3;
        }
        return;
    }
    if (field_0x456 == 1) {
        mCel_0x3c4 = field_0x450;
        mAlpha_0xca0 = field_0xca2;
        field_0x456 = 0;
        return;
    }
    if (fn_802413E8()) {
        mCel_0x3c4 = field_0x450;
        mAlpha_0xca0 = field_0xca2;
        return;
    }
    if (!fn_80241230() && field_0x488 == 0 && (field_0x464 & 2) == 2) {
        mCel_0x3c4 = 0x121;
        return;
    }
    switch (mState_0x1240) {
    case 0x00:
        setStandingFrame_80268194();
        break;
    case 0x01:
        mCel_0x3c4 = 0x31D;
        break;
    case 0x02:
    case 0x03:
    case 0x38:
        setFacingFrame(this, 0x0, 0x8, 0x10, 0x18);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x04:
        fn_80267ECC();
        break;
    case 0x05:
    case 0x06:
        setFacingFrame(this, 0x1C6, 0x1C9, 0x1CC, 0x1CF);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x07:
        setFacingFrame(this, 0x262, 0x268, 0x26E, 0x274);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x08:
        fn_80267E18();
        break;
    case 0x09:
    case 0x0A:
        fn_80267D30();
        break;
    case 0x0B:
        fn_80267C6C();
        break;
    case 0x0C:
        if (mSubPose_0x3fd == 0) {
            mCel_0x3c4 = 0x3E;
        } else if (mSubPose_0x3fd == 1) {
            mCel_0x3c4 = 0x3D;
        } else if (mSubPose_0x3fd == 2) {
            mCel_0x3c4 = 0x3F;
        } else {
            mCel_0x3c4 = 0x3C;
        }
        break;
    case 0x11:
        setFacingFrame(this, 0x2B0, 0x2B4, 0x2B8, 0x2BC);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x12:
        setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x13:
        setFacingFrame(this, 0x36D, 0x36F, 0x371, 0x373);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x14:
        fn_80267BA8();
        break;
    case 0x15:
        setFacingFrame(this, 0x322, 0x327, 0x32C, 0x331);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x16:
    case 0x17:
        setFacingFrame(this, 0x322, 0x327, 0x32C, 0x331);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x18:
        setFacingFrame(this, 0x322, 0x327, 0x32C, 0x331);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x19:
        if (field_0x121c) {
            mCel_0x3c4 = 0x31D;
        } else {
            mCel_0x3c4 = mSubPose_0x3fd + 0x296;
        }
        break;
    case 0x0D:
        setFacingFrame(this, 0x29A, 0x29E, 0x2A2, 0x2A9);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x0E:
        setFacingFrame(this, 0x29A, 0x29E, 0x2A2, 0x2A9);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x1A:
        setFacingFrame(this, 0x2D6, 0x2DA, 0x2DE, 0x2E2);
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x1B:
        if (!field_0x43f) {
            if (isLifting_8025F684()) {
                setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
                mCel_0x3c4 += mWalkFrame_0x3f9;
            } else if (mSubPose_0x3fd == 0) {
                setFacingFrame(this, 0x0, 0x8, 0x10, 0x18);
                mCel_0x3c4 += mWalkFrame_0x3f9;
            } else if (mSubPose_0x3fd == 1) {
                fn_802671C8();
            } else if (mSubPose_0x3fd == 2) {
                fn_802670CC();
            } else {
                setFacingFrame(this, 0x33A, 0x33D, 0x340, 0x343);
                mCel_0x3c4 += mWalkFrame_0x3f9;
            }
        }
        break;
    case 0x1C:
    case 0x1D:
        if (field_0xceb == 1) {
            if (viewLayerFlag(mLayer_0x3b4)) {
                fn_802670CC();
            } else {
                fn_802671C8();
            }
        } else if (field_0xcec == 1) {
            setFacingFrame(this, 0x33A, 0x33D, 0x340, 0x343);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else if (isLifting_8025F684()) {
            setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else {
            setFacingFrame(this, 0x0, 0x8, 0x10, 0x18);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        }
        break;
    case 0x1E:
        mCel_0x3c4 = mSubPose_0x3fd + 0xB7;
        break;
    case 0x1F:
        setStandingFrame_80268194();
        break;
    case 0x20:
    case 0x21:
        if (isLifting_8025F684()) {
            setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else if (mSubPose_0x3fd == 1) {
            mCel_0x3c4 = 0x31C;
        } else if (mLayer_0x3b4 < 8) {
            mCel_0x3c4 = 0x121;
        } else {
            setStandingFrame_80268194();
        }
        break;
    case 0x22:
    case 0x23:
        if (isLifting_8025F684()) {
            setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else {
            setStandingFrame_80268194();
        }
        break;
    case 0x26:
        if (mWalkFrame_0x3f9 >= 0xB) {
            mCel_0x3c4 = 0x121;
        } else {
            mCel_0x3c4 = 0x3C2;
            mCel_0x3c4 += mWalkFrame_0x3f9;
        }
        break;
    case 0x27:
        fn_802670CC();
        break;
    case 0x28:
        fn_802670CC();
        break;
    case 0x29:
        fn_802671C8();
        break;
    case 0x24:
        if (isLifting_8025F684()) {
            setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else if (mSubPose_0x3fd == 1) {
            mCel_0x3c4 = 0x31C;
        } else {
            mCel_0x3c4 = 0x121;
        }
        break;
    case 0x25:
        if (isLifting_8025F684()) {
            setFacingFrame(this, 0x2C0, 0x2C4, 0x2C8, 0x2CF);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else {
            setStandingFrame_80268194();
        }
        break;
    case 0x2A:
    case 0x2B:
        setStandingFrame_80268194();
        break;
    case 0x2C:
    case 0x2D:
        mCel_0x3c4 = 0xB0;
        break;
    case 0x2E:
    case 0x2F:
        mCel_0x3c4 = 0xB0;
        break;
    case 0x30:
        mCel_0x3c4 = 0x22C;
        mCel_0x3c4 += mWalkFrame_0x3f9;
        break;
    case 0x31:
        if (mLayer_0x3b4 < 8) {
            mCel_0x3c4 = 0x358;
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else {
            mCel_0x3c4 = 0x3E;
        }
        break;
    case 0x32:
        fn_80267B18();
        break;
    case 0x0F:
    case 0x10:
        setFacingFrame(this, 0xA7, 0xAB, 0xAF, 0xB3);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x33:
        if (field_0xd2d == 1 && field_0xd30 >= 0x3C) {
            mCel_0x3c4 = 0x3E;
        } else {
            mCel_0x3c4 = mSubPose_0x3fd + 0xBD;
        }
        break;
    case 0x34:
        if (field_0xd30 >= 0x1A4) {
            mCel_0x3c4 = 0x3E;
        } else {
            mCel_0x3c4 = 0xC3;
        }
        break;
    case 0x35:
        setFacingFrame(this, 0x28A, 0x28D, 0x290, 0x293);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x36:
        fn_80267ECC();
        break;
    case 0x39:
        fn_80267A54();
        break;
    case 0x3A:
        fn_80267990();
        break;
    case 0x3B:
        fn_802678CC();
        break;
    case 0x3C:
        fn_80267818();
        break;
    case 0x3D:
        setFacingFrame(this, 0x15B, 0x15F, 0x163, 0x167);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x3E:
        fn_80267598();
        break;
    case 0x3F:
        fn_80267470();
        break;
    case 0x40:
        fn_80267384();
        break;
    case 0x41:
        setFacingFrame(this, 0xA7, 0xAB, 0xAF, 0xB3);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x42:
        setStandingFrame_80268194();
        break;
    case 0x43:
        setFacingFrame(this, 0xA7, 0xAB, 0xAF, 0xB3);
        mCel_0x3c4 += mSubPose_0x3fd;
        break;
    case 0x44:
    case 0x45:
        fn_80267748();
        break;
    case 0x46:
        fn_80267684();
        break;
    case 0x47:
        setFacingFrame(this, 0x1A3, 0x1AA, 0x1B1, 0x1B8);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x48:
        setFacingFrame(this, 0x362, 0x364, 0x366, 0x368);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x49:
        if (field_0x1178 == 0) {
            mCel_0x3c4 = mSubPose_0x3fd + 0x35F;
        } else if (field_0x1178 == 1) {
            mCel_0x3c4 = mSubPose_0x3fd + 0x3D0;
        } else if (field_0x1178 == 2) {
            mCel_0x3c4 = mSubPose_0x3fd + 0x296;
        }
        break;
    case 0x37:
        setFacingFrame(this, 0x111, 0x115, 0x119, 0x11D);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x4A:
        fn_802671C8();
        break;
    case 0x4B:
        fn_802670CC();
        break;
    case 0x4C:
        mCel_0x3c4 = 0x3D0;
        break;
    case 0x4D:
        if (mStateTimer_0x3fc) {
            mCel_0x3c4 = 0x121;
        } else if (isLifting_8025F684()) {
            setFacingFrame(this, 0x36D, 0x36F, 0x371, 0x373);
            mCel_0x3c4 += mWalkFrame_0x3f9;
        } else {
            mCel_0x3c4 = 0xB0;
        }
        break;
    case 0x4E:
        mCel_0x3c4 = 0xB0;
        break;
    case 0x4F:
        setFacingFrame(this, 0x302, 0x308, 0x30E, 0x314);
        mCel_0x3c4 += mPose_0x3fb;
        break;
    case 0x50:
        fn_80267018();
        break;
    case 0x51:
        setFacingFrame(this, 0x31F, 0x31E, 0x321, 0x320);
        break;
    }
}

// The shadow's alpha mShadowAlpha_0xca1: none on the frames 0x122 and 0xB7-0xBC and
// above the GBA layer 8, full on layer 8; otherwise it fades out while
// isGrabbing_8025F448 holds and the actor mHeldPlayer_0xd70 is not on layer 8, else in.
void Player_804A6F38::fn_80265DB0() {
    if (mCel_0x3c4 == 0x122 || (mCel_0x3c4 >= 0xB7 && mCel_0x3c4 < 0xBD) || mLayer_0x3b4 > 8) {
        mShadowAlpha_0xca1 = 0;
    } else if (mLayer_0x3b4 == 8) {
        mShadowAlpha_0xca1 = 0xFF;
    } else if (isGrabbing_8025F448() && (mHeldPlayer_0xd70 < 0 || getPlayerLayer_8023DE58(mHeldPlayer_0xd70) != 8)) {
        if (mShadowAlpha_0xca1 >= 8) {
            mShadowAlpha_0xca1 -= 8;
        } else {
            mShadowAlpha_0xca1 = 0;
        }
    } else if (mShadowAlpha_0xca1 <= 0xF7) {
        mShadowAlpha_0xca1 += 8;
    } else {
        mShadowAlpha_0xca1 = 0xFF;
    }
}

// Fades it back in, 8 a frame, up to 0xFF.
void Player_804A6F38::fn_80265D8C() {
    if (mAlpha_0xca0 <= 0xF7) {
        mAlpha_0xca0 += 8;
    } else {
        mAlpha_0xca0 = 0xFF;
    }
}

// Fades the body sprite out (mAlpha_0xca0, the alpha fn_802413F0 tests), 8 a frame.
void Player_804A6F38::fn_80265D68() {
    if (mAlpha_0xca0 >= 8) {
        mAlpha_0xca0 -= 8;
    } else {
        mAlpha_0xca0 = 0;
    }
}

// The idle animation counters: field_0x3d0 (0-15, held at 0 after 8 still
// frames, field_0x3cc) picks the floor frame mFloorCel_0x3c8 and field_0x3d4
// (0-11) the frame mFloorCel_0x3ca. While the player moves or changes frame on a
// tile 0x39 at floor level, effect 0x4D2 follows the player (field_0x1384).
void Player_804A6F38::fn_80265AE4() {
    if (mCel_0x3c4 != mPrevCel_0x3c6 || magnitude(mPos - mLastPos_0x14) > 0.5f) {
        field_0x3d0++;
        if (field_0x3d0 >= 0x10) {
            field_0x3d0 = 0;
        }
        field_0x3cc = 8;
    } else if (field_0x3cc > 0) {
        field_0x3cc--;
    } else {
        field_0x3d0 = 0;
    }
    if (field_0x3d0 < 4) {
        mFloorCel_0x3c8 = 0x195;
    } else if (field_0x3d0 < 8) {
        mFloorCel_0x3c8 = 0x196;
    } else if (field_0x3d0 < 0xC) {
        mFloorCel_0x3c8 = 0x197;
    } else {
        mFloorCel_0x3c8 = 0x196;
    }
    field_0x3d4++;
    if (field_0x3d4 >= 0xC) {
        field_0x3d4 = 0;
    }
    if (field_0x3d4 < 3) {
        mFloorCel_0x3ca = 0x18F;
    } else if (field_0x3d4 < 6) {
        mFloorCel_0x3ca = 0x190;
    } else if (field_0x3d4 < 9) {
        mFloorCel_0x3ca = 0x191;
    } else {
        mFloorCel_0x3ca = 0x190;
    }
    if (mPos.z <= 0.0f && getTileCollisionCode_8022461C(&mPos, mLayer_0x3b4) == 0x39 &&
        (mCel_0x3c4 != mPrevCel_0x3c6 || magnitude(mPos - mLastPos_0x14) > 0.5f))
    {
        if (field_0x1384 == NULL) {
            field_0x1384 = spawnEmitter_8013CC50(mLayer_0x3b4, &mPos, 0x4D2, 0, 0, NULL, NULL, true, 1.0f);
        } else {
            setEmitterPos_8013CB44(field_0x1384, &mPos, 0, 0, true);
        }
    } else if (field_0x1384 != NULL) {
        field_0x1384->becomeInvalidEmitter();
        field_0x1384 = NULL;
    }
}

// FIXME: 99.74. Stack slots and registers only: the temporaries of the
// position sums sit in other slots (e.g. 0x30 vs 0x3C), the view offset add
// and the marker frame's index * 3 use other registers.
// The player's draw (fn_8023F2BC, every player each frame): the shadow, the
// GBA-layer marker above the player, the body sprite (frame mCel_0x3c4 of
// "player_sob.bin") with its tint and scale, the height shadow on the GBA
// layers, and the floor overlays (frames mFloorCel_0x3c8 / mFloorCel_0x3ca of
// "enemy_both2_sob.bin").
void Player_804A6F38::fn_80264998() {
    if (!mActive) {
        return;
    }
    if (field_0xcee == 1) {
        return;
    }
    if (field_0x1215 && !isPlayerBitSet(gGameRoot_8053AAF8, mPlayerNo_0x4)) {
        return;
    }
    u8 alpha = fn_8023FC5C();
    s32 index = fn_80240868();
    if (mCel_0x3c4 >= 0x3D5) {
        mCel_0x3c4 = 0x3D4;
    }
    if (field_0xb90 > 0.0f) {
        drawShadow(mLayer_0x3b4, &field_0x338, JUtility::TColor(0, 0, 0, 0xFF), field_0xb90);
    }
    if (mLayer_0x3b4 > 8 && fn_80246B10(0x40)) {
        SpriteParams_801F5AE0 markParams;
        markParams.field_0x1c = 1;
        markParams.field_0x2d = 1;
        s32 counter = gGameRoot_8053AAF8->field_0x5c;
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
        gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&mLastTvPos_0xcb0, res, 0x73 + index * 3 + counter / 6 % 3,
                                              &markParams, 1.0f, 1.0f);
    }
    SpriteParams_801F5AE0 shadowParams;
    if (!(hasLeader_80240808() && fn_80240DAC()) && fn_802413F0()) {
        shadowParams.field_0x1d = -2;
        JGeometry::TVec3f pos;
        if (fn_80240DAC()) {
            pos.set(mPos.x, mGlideTarget_0x324.y - 0.01f, -5.0f);
            shadowParams.field_0x1d += 2;
        } else {
            pos.set(mPos.x, mPos.y - 0.01f, -5.0f);
        }
        if (mLayer_0x3b4 == 8) {
            GameView_804A9950* view = getGameView_802D6A9C();
            pos += JGeometry::TVec3f(view->mVisibleBox.i.x, view->mVisibleBox.i.y, 0.0f);
            pos += JGeometry::TVec3f(0.0f, -mPos.z, 0.0f);
        }
        if (mShadowAlpha_0xca1 < 0xFF) {
            shadowParams.field_0x2c = 0;
        }
        if (alpha < mShadowAlpha_0xca1) {
            shadowParams.field_0x10.a = alpha;
        } else {
            shadowParams.field_0x10.a = mShadowAlpha_0xca1;
        }
        if (shadowParams.field_0x10.a >= 0x9F) {
            shadowParams.field_0x10.a = 0x9F;
        }
        if (mPos.z > 0.0f) {
            f32 a = shadowParams.field_0x10.a - 0.5f * mPos.z;
            shadowParams.field_0x10.a = 79.0f >= a ? 79.0f : a;
        }
        Mtx scale;
        if (mLayer_0x3b4 == 8) {
            PSMTXScale(scale, 1.0f, 1.0f, 1.0f);
        } else {
            PSMTXScale(scale, 0.9f, 0.5f, 0.5f);
        }
        shadowParams.field_0x10.r = 0;
        shadowParams.mMtx = scale;
        shadowParams.field_0x10.g = 0;
        shadowParams.field_0x10.b = 0;
        shadowParams.field_0x14.r = 0x28;
        shadowParams.field_0x14.g = 0x28;
        shadowParams.field_0x14.b = 0x28;
        if (field_0x43f != true) {
            if (mLayer_0x3b4 == 8) {
                drawSob("player_sob.bin", pos + mDrawOffset_0x2b8, mCel_0x3c4, &shadowParams);
            } else {
                drawSobOnTv(mLayer_0x3b4, "player_sob.bin", pos + mDrawOffset_0x2b8, mCel_0x3c4, &shadowParams);
            }
        }
        if (mPos.z > 0.0f) {
            s32 layer = mLayer_0x3b4;
            if (layer >= 8 && !viewLayerFlag(layer)) {
                JGeometry::TVec3f groundPos;
                if (fn_80240DAC()) {
                    groundPos.set(mPos.x, mGlideTarget_0x324.y - 0.01f, 0.0f);
                } else {
                    groundPos.set(mPos.x, mPos.y - 0.01f, 0.0f);
                }
                drawGba(-40000 - mPlayerNo_0x4, mLayer_0x3b4, groundPos + mDrawOffset_0x2b8, 0x3CD);
            }
        }
    }
    SpriteParams_801F5AE0 bodyParams;
    if (fn_80241544() >= 2) {
        bodyParams.field_0x0[10] = 6;
    }
    bodyParams.field_0x0[7] = index + 0xC;
    bodyParams.field_0x2d = 1;
    if (alpha < 0xFF) {
        shadowParams.field_0x2c = 0;
    }
    if (field_0x11e9) {
        bodyParams.field_0x30 = index + 4;
    }
    bodyParams.field_0x10.a = alpha;
    Mtx bodyScale;
    bodyScale[0][3] = 0.0f;
    bodyScale[1][3] = 0.0f;
    bodyScale[2][3] = 0.0f;
    setScaleMtx(bodyScale, mSquashScale_0xc84.x * mDrawScale_0xc90.x, mSquashScale_0xc84.y * mDrawScale_0xc90.y,
                mSquashScale_0xc84.z * mDrawScale_0xc90.z);
    bodyParams.mMtx = bodyScale;
    bodyParams.field_0x20 = 0;
    if (fn_8025F418() && !fn_802413E8()) {
        bodyParams.field_0x1d = fn_8023CB24(mHeldPlayer_0xd70) + 1;
        bodyParams.field_0x1c = fn_8023CA88(mHeldPlayer_0xd70);
    } else if (mPos.z >= 20.0f) {
        bodyParams.field_0x1d++;
        bodyParams.field_0x1c++;
    } else if (mPos.z > 0.0f) {
        bodyParams.field_0x1d++;
    } else if (fn_80240818()) {
        bodyParams.field_0x1d++;
    }
    if (fn_802413E8()) {
        if (field_0xca3 != -0x80) {
            bodyParams.field_0x1c = field_0xca3;
        }
        if (field_0xca4 != -0x80) {
            bodyParams.field_0x1d = field_0xca4;
        }
    }
    if (field_0x43f == true || field_0x441 == true) {
        bodyParams.field_0x1f = 0;
    }
    if (mFlashTimer_0xca8 > 0) {
        mFlashTimer_0xca8--;
        switch (index) {
        case 0:
            bodyParams.field_0x10.r = 0;
            bodyParams.field_0x10.g = 0xFF;
            bodyParams.field_0x10.b = 0;
            break;
        case 1:
            bodyParams.field_0x10.r = 0xFF;
            bodyParams.field_0x10.g = 0;
            bodyParams.field_0x10.b = 0;
            break;
        case 2:
            bodyParams.field_0x10.r = 0;
            bodyParams.field_0x10.g = 0xFF;
            bodyParams.field_0x10.b = 0xFF;
            break;
        case 3:
            bodyParams.field_0x10.r = 0xFF;
            bodyParams.field_0x10.g = 0;
            bodyParams.field_0x10.b = 0xFF;
            break;
        }
    } else if (field_0x400 && !fn_802413E8()) {
        s32 level = 0xFF - field_0x400 * 32;
        if (level < 0) {
            level = 0;
        }
        bodyParams.field_0x10.r = level;
        bodyParams.field_0x10.g = level;
        bodyParams.field_0x10.b = level;
    } else if (!gGameRoot_8053AAF8->field_0x7f && field_0x1208 < 0x40) {
        bodyParams.field_0x10.r = field_0x1208 * 4;
        bodyParams.field_0x10.g = field_0x1208 * 4;
        bodyParams.field_0x10.b = field_0x1208 * 4;
    }
    field_0x3d8 = bodyParams.field_0x1c;
    field_0x3d9 = bodyParams.field_0x1d;
    if (!fn_802413F0()) {
        return;
    }
    if (alpha < 0x20) {
        return;
    }
    if (hasLeader_80240808() && fn_802486A0() != 4 && !fn_8023188C(getLeader_80240644())) {
        return;
    }
    u16 frame = mCel_0x3c4;
    if (frame == 0x121) {
        return;
    }
    // The body is drawn at whole units. The original keeps the sum in the
    // temporary itself (no named copy), as a reference bound to it does.
    const JGeometry::TVec3f& bodyPos = mPos + mDrawOffset_0x2b8;
    JGeometry::TVec3f drawPos(bodyPos.x, bodyPos.y, bodyPos.z);
    drawPos.set((s32)drawPos.x, (s32)drawPos.y, (s32)drawPos.z);
    if (mLayer_0x3b4 < 8) {
        drawSobOnTv(mLayer_0x3b4, "player_sob.bin", drawPos, frame, &bodyParams);
    }
    SpriteParams_801F5AE0 floorParams;
    if (field_0x458 == 1 && mFacing_0x3dc == 2) {
        floorParams.field_0x10.a = 0x80;
    }
    u16 tile = getTileCollisionCode_8022461C(&mPos, mLayer_0x3b4);
    JGeometry::TVec3f floorPos(mPos);
    floorPos.y += 1.0f;
    if (mPos.z <= 0.0f && fn_8022517C(&mPos, mLayer_0x3b4)) {
        s32 layer = mLayer_0x3b4;
        if (layer < 8) {
            drawSobOnTv(layer, "enemy_both2_sob.bin", floorPos, mFloorCel_0x3c8, &floorParams);
        } else {
            drawGba(-10000 - mPlayerNo_0x4, layer, floorPos, mFloorCel_0x3c8);
        }
    } else if (mPos.z <= 0.0f && isDeadState_80270A8C() && (tile == 0x39 || tile == 0xD || tile == 0x3B)) {
        if (mCel_0x3c4 >= 0xC2 && mCel_0x3c4 < 0xC5) {
            s32 layer = mLayer_0x3b4;
            if (layer < 8) {
                drawSobOnTv(layer, "enemy_both2_sob.bin", floorPos, mFloorCel_0x3ca + 3, &floorParams);
            } else {
                drawGba(-20000 - mPlayerNo_0x4, layer, floorPos, mFloorCel_0x3ca + 3);
            }
        } else {
            s32 layer = mLayer_0x3b4;
            if (layer < 8) {
                drawSobOnTv(layer, "enemy_both2_sob.bin", floorPos, mFloorCel_0x3ca, &floorParams);
            } else {
                drawGba(-20000 - mPlayerNo_0x4, layer, floorPos, mFloorCel_0x3ca);
            }
        }
    } else if (mPos.z <= 0.0f) {
        if (fn_802412A4() && !fn_80241200()) {
            return;
        }
        if (fn_8024BBB0()) {
            return;
        }
        if (tile != 0x39 && !fn_80225BE4(&mPos, mLayer_0x3b4)) {
            return;
        }
        s32 layer = mLayer_0x3b4;
        if (layer < 8) {
            drawSobOnTv(layer, "enemy_both2_sob.bin", floorPos, mFloorCel_0x3ca, &floorParams);
        } else {
            drawGba(-20000 - mPlayerNo_0x4, layer, floorPos, mFloorCel_0x3ca);
        }
    }
}
