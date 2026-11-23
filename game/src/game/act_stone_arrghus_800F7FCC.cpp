#include "JSystem/JMath/JMath.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Camera.h"
#include "game/Effect.h"
#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/act/act_stone_arrghus_800F7FCC.h"

// Data this unit owns, not written as source yet (shotgun pass): the position/slot
// tables in .rodata.
extern const char lbl_80463A58[];
extern const s16 lbl_80463800[];
extern const s16 lbl_804638C0[];

static const char* lbl_8049C5D0[] = {
    "wart_eye1.bti",      "wart_eye2.bti",     "wart_eye3.bti",     "wart_eye4.bti",
    "wart_eye_red1.bti",  "wart_eye_red2.bti", "wart_eye_red3.bti", "wart_eye_left.bti",
    "wart_eye_right.bti", "wart_eye_down.bti", "wart_eye_up.bti",
};
static f32 lbl_8049C5FC[4][2] = {{1.4f, 1.4f}, {0.0f, 1.4f}, {-1.4f, 1.4f}, {1.4f, 1.4f}};
static u8 lbl_8049C61C[] = {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 0xFF, 0};
static u8 lbl_8049C628[] = {3, 3, 3, 3, 2, 2, 5, 3, 3, 3, 0xFF, 0};
static f32 lbl_8049C634[5] = {38.0f, 46.0f, 32.0f, 17.0f, 15.0f};
static const char* lbl_8049C648[] = {
    "wart_child1.bti",        "wart_child2.bti",
    "wart_child1_green.bti",  "wart_child2_green.bti",
    "wart_child1_red.bti",    "wart_child2_red.bti",
    "wart_child1_blue.bti",   "wart_child2_blue.bti",
    "wart_child1_purple.bti", "wart_child2_purple.bti",
    "wart_child1_purple.bti", "wart_child2_purple.bti",
};
static u8 lbl_80539E80[] = {3, 2, 1, 0, 0xFF, 0, 0, 0};
static u8 lbl_80539E88[] = {3, 3, 3, 3, 0xFF, 0, 0, 0};
static u8 lbl_80539E90[] = {4, 5, 6, 0xFF};
static u8 lbl_80539E94[] = {5, 5, 5, 0xFF};
static u8 lbl_80539E98[] = {7, 8, 9, 10, 3, 0xFF, 0, 0};
static u8 lbl_80539EA0[] = {8, 8, 8, 8, 8, 0xFF, 0, 0};

// Which ring of slots a puff index belongs to and the angle step between them.
static inline s32 slotRing(u32 idx, s32& step) {
    s32 ring;
    if (idx < 8) {
        ring = 0;
        step = 0x2000;
    } else if (idx < 0x10) {
        ring = 1;
        step = 0x2000;
    } else if (idx < 0x15) {
        ring = 2;
        step = 0x3332;
    } else if (idx < 0x18) {
        ring = 3;
        step = 0x5554;
    } else if (idx < 0x1A) {
        ring = 4;
        step = 0x8000;
    } else {
        ring = 5;
        step = 0x2000;
    }
    return ring;
}

// A value cut to four decimals (the float round trip through s32).
static inline f32 cut4(f32 v) {
    return (f32)(s32)(10000.0f * v) * 0.0001f;
}

static inline void moveToward(JGeometry::TVec3f& pos, JGeometry::TVec3f target, f32 speed) {
    pos.x += cut4(target.x - pos.x) * speed;
    pos.y += cut4(target.y - pos.y) * speed;
    pos.z += cut4(target.z - pos.z) * speed;
}

// The slot of puff index `idx` around `parent`.
static inline void slotPos(StoneArrghus_8049C718* parent, u32 idx, JGeometry::TVec3f& out) {
    s32 step;
    s32 ring = slotRing(idx, step);
    u16 ang = parent->mAngle + idx * step + (ring << 12);
    f32 r = lbl_8049C634[ring];
    f32 s = JMASinShort(ang);
    f32 c = JMACosShort(ang);
    out.x = parent->mRadius * (r * s) + parent->mPos.x;
    out.y = parent->mRadius * (r * c) + parent->mPos.y;
    out.z = (f32)(ring * 13) + (10.0f + parent->mPos.z);
    parent->mSlot++;
}

static inline JGeometry::TVec3f slotPosOf(StoneArrghus_8049C718* parent, u32 idx) {
    JGeometry::TVec3f out;
    slotPos(parent, idx, out);
    return out;
}

static inline JGeometry::TVec3f linePosOf(StoneArrghus_8049C718* parent, s32 slot) {
    f32 off;
    s16 ang;
    if (slot & 1) {
        ang = parent->mAngleA;
        off = 20.0f + (f32)(((slot >> 1) + 1) * 20);
    } else {
        ang = parent->mAngleB;
        off = (f32)(((slot >> 1) + 1) * -20) - 20.0f;
    }
    JGeometry::TVec3f out(parent->mPos.x + off * JMASinShort(ang),
                          parent->mPos.y + off * JMACosShort(ang),
                          10.0f + parent->mPos.z);
    parent->mSlot++;
    return out;
}

// The camera and the boss's fade-in that the intro and the first rise share.
static inline void focusCamera(StoneArrghus_8049C718* p) {
    cls_804A03A8* sub = gGameRoot2_8053AB10->field_0x30;
    if (sub->field_0x284 == 0) {
        sub->field_0x284 = 1;
        sub->field_0x28c = 0.05f;
    }
    Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
    f32 y = p->mPos.y - 20.0f;
    f32 x = p->mPos.x;
    if (camera->mShowWholeArea_0x7f5 == 0) {
        camera->mFocusOverride_0x7f4 = 1;
        camera->mFocusMode_0x7f8 = 4;
        camera->mFocusPos_0x7fc.x = x;
        camera->mFocusPos_0x7fc.y = y;
        camera->mFocusPos_0x7fc.z = 0.0f;
        camera->mOverrideBlend_0x7f6 = 0;
    }
}

StoneArrghus_8049C718::~StoneArrghus_8049C718() {}

void StoneArrghus_8049C718::init() {
    mHome.x = mPos.x;
    mHome.y = mPos.y;
    mHome.z = mPos.z;
    mOrigin.x = mPos.x;
    mOrigin.y = mPos.y;
    mOrigin.z = mPos.z;
    field_0x258 = 0;
    mState = 0;
    mTimer = 500;
    field_0x254 = 0;
    field_0x264 = 0;
    field_0x255 = 0;
    mCooldown = 0;
    mSignal = 0;
    mWobble = 0;
    field_0x262 = 0;
    mAlpha = 1.0f;
    mTint = 0.0f;
    mScale = 1.0f;
    field_0x260 = 0;
    mZBase = 0.0f;
    mFlags = 0;
    field_0x257 = 0;
    mSlotUsed[0] = 0;
    mSlotUsed[1] = 0;
    mDir_0x98 = 2;
    getGameView_802D6A9C()->fn_802D7440(0);
    field_0x196 = 1;
    if (mSpawnParam_0x90 == 1) {
        mFlags |= 8;
    }
    if (mFlags & 8) {
        mLife = 0xF0;
    } else {
        mLife = 0x1E0;
    }
    field_0xbc = 3;
    f32 x0 = -26.0f * mScale;
    f32 y0 = -50.0f * mScale;
    f32 x1 = 26.0f * mScale;
    mBox.set(x0, y0, x1, 0.0f);
    mMapBox_0x80.set(x0, y0, x1, 0.0f);
    mAnimState = 0;
    startSound_801F0E34(0x40026, 0);
    mAnimPrev = 9;
    fn_800FA6B0();
    gAudioMgr_8053AF80->stopBgm_80458F9C(0x3C);
    u32 low = mSerial & 0xFFFF;
    for (s32 i = 0; i < 0x18; i++) {
        getActorManager_801F856C()->spawnActor_801F9484('WRTC', mLayer_0x4, &mPos, low + (i << 16) + 0x1000000, -1, -1);
        mSlot++;
    }
    if (mFlags & 8) {
        for (s32 i = 0; i < 2; i++) {
            mPartner[i] = getActorManager_801F856C()->spawnActor_801F9484('WRTC', mLayer_0x4, &mPos,
                                                                         low + (i << 16) + 0x2000000, -1, -1);
        }
    }
}

void StoneArrghus_8049C718::fn_800FCCCC() {
    u16 angle = 0;
    f32 dist = 0.0f;
    s32 dir = 0;
    s32 player = 0;
    mTimer++;
    switch (field_0x254) {
    case 0:
        mAnimState = 3;
        mZBase = 0.0f;
        findTargetPlayer_801EEC98(mSerial, &angle, &dir, &dist, &player, 0, 0);
        if (player >= 0) {
            if (getPlayerPos_8023E724(player)->y - mPos.y < 140.0f) {
                field_0x254++;
                mTimer = 0;
            }
        }
        break;
    case 1:
        if (getActorManager_801F856C()->fn_801F68E0() == 1) {
            field_0x254++;
            mTimer = 0;
        }
        break;
    case 2:
        focusCamera(this);
        if (mTimer > 200) {
            mAnimState = 0;
        }
        if (mTimer > 0x15E) {
            field_0x254++;
            mTimer = 0;
            startSound_801F0E34(0x40028, 0);
        }
        break;
    case 3:
        focusCamera(this);
        mZBase += 0.2f;
        if (mZBase >= 10.0f) {
            mZBase = 10.0f;
        }
        field_0x260 += 0x200;
        mPos.z = 5.0f * JMath::sincosTable_.sinShort((s16)field_0x260) + mZBase;
        if (mTimer > 0x96) {
            mState = 1;
            field_0x254 = 0;
            mTimer = 1000;
        }
        break;
    }
}

// FIXME(shotgun): 90.02 %, missing/extra + reloc-name; behaviour drift.
// Ends the state early when the slot counter is 0 (the boss swaps to state 8).
static inline bool trySwap(StoneArrghus_8049C718* p) {
    if (p->mSlot == 0) {
        p->field_0xb0 = 4;
        p->mAnimState = 1;
        p->mTimer = 0;
        p->mState = 8;
        p->startSound_801F0E34(0x40005, 0);
        p->mSignal = 1;
        return true;
    }
    return false;
}

// Advances the wobble; true while it is running.
static inline bool stepWobble(StoneArrghus_8049C718* p) {
    if (p->mWobble != 0) {
        p->mWobble += 0x2000;
        if (p->mWobble > 0x20000) {
            p->mWobble = 0;
        }
        return true;
    }
    return false;
}

// Plain functions without a copy of their own in the original (see their definitions).
f32 pool_8053C094();
s32 pool_8053C050(f32 x, f32 y);

void StoneArrghus_8049C718::fn_800FC984() {
    if (trySwap(this)) {
        return;
    }
    if (mCooldown != 0) {
        mCooldown--;
    }
    if (mFrame == 3 || mFrame == 6) {
        field_0xb0 = 0x10;
    } else {
        field_0xb0 = 4;
    }
    fn_800FA1A4();
    if (stepWobble(this) == 1) {
        updateHitReaction_801F3D94(1);
        return;
    }
    mAngleSpeed = 0x40;
    mAngle += mAngleSpeed;
    mRadius = 1.0f;
    fn_801F1184()->setField_0x48(0.0f);
    startSound_801F0E34(0x4002F, 0);
    mTimer--;
    if (mTimer <= 0) {
        if ((mFlags & 8) && mSlot <= 12) {
            mState = 4;
            mTimer = 0;
            mSlotUsed[0] = pool_8053C094();
            mSlotUsed[1] = pool_8053C094();
            mAngleA = 0x3000;
            mAngleB = 0x5000;
        } else {
            mState = 3;
            mTimer = 0x258;
        }
    }
    mHome.x = mOrigin.x;
    mHome.y = mOrigin.y;
    mHome.z = mOrigin.z;
    field_0x260 += 0x200;
    mPos.z = 5.0f * JMath::sincosTable_.sinShort((s16)field_0x260) + mZBase;
    field_0x262 += 0xB0;
    mPos.x = 100.0f * JMath::sincosTable_.sinShort((s16)field_0x262) + mHome.x;
    mPos.y = (50.0f * JMath::sincosTable_.cosShort((s16)field_0x262) + mHome.y) - 50.0f;
    updateHitReaction_801F3D94(1);
}

void StoneArrghus_8049C718::fn_800FC4A8() {
    if (trySwap(this)) {
        return;
    }
    if (mCooldown != 0) {
        mCooldown--;
    }
    if (mFrame == 3 || mFrame == 6) {
        field_0xb0 = 0x10;
    } else {
        field_0xb0 = 4;
    }
    if (stepWobble(this) == 1) {
        updateHitReaction_801F3D94(1);
        return;
    }
    if (mTimer > 300) {
        mAngleSpeed += 4;
        if (mAngleSpeed > 0x400) {
            mAngleSpeed = 0x400;
        }
        mRadius += 0.01f;
        if (mRadius > 2.0f) {
            mRadius = 2.0f;
        }
    } else if (mTimer < 100) {
        mAngleSpeed -= 0x10;
        if (mAngleSpeed < 0x40) {
            mAngleSpeed = 0x40;
        }
        mRadius -= 0.03f;
        if (mRadius < 1.0f) {
            mRadius = 1.0f;
        }
    }
    mAngle += mAngleSpeed;
    f32 spin = (f32)(mAngleSpeed - 0x40) / 960.0f;
    fn_801F1184()->field_0x48 = spin;
    startSound_801F0E34(0x4002F, 0);
    mTimer--;
    if (mTimer <= 0) {
        mState = 2;
        mTimer = (s32)(200.0f + (f32)(s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()));
    }
    field_0x260 += 0x200;
    mPos.z = 5.0f * JMath::sincosTable_.sinShort((s16)field_0x260) + mZBase;
    if (mTimer == 0x15E) {
        field_0x264 = (s32)(3.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
        switch (field_0x264) {
        case 0:
            mAnimState = 5;
            break;
        case 1:
            mAnimState = 6;
            break;
        case 2:
            mAnimState = 4;
            break;
        case 3:
            mAnimState = 5;
            break;
        }
    }
    if (mTimer <= 0x12C && mTimer > 0xC8) {
        mHome.x += lbl_8049C5FC[field_0x264][0];
        mHome.y += lbl_8049C5FC[field_0x264][1];
    } else if (mTimer <= 0xC8 && mTimer > 100) {
        mHome.x -= lbl_8049C5FC[field_0x264][0];
        mHome.y -= lbl_8049C5FC[field_0x264][1];
        field_0x262 += 0xB0;
    } else {
        field_0x262 += 0xB0;
    }
    mPos.x = 100.0f * JMath::sincosTable_.sinShort((s16)field_0x262) + mHome.x;
    mPos.y = (50.0f * JMath::sincosTable_.cosShort((s16)field_0x262) + mHome.y) - 50.0f;
    updateHitReaction_801F3D94(1);
    if (mTimer == 0x244) {
        mAnimState = 0;
    } else if (mTimer == 0x32) {
        mAnimState = 1;
    }
}

// A slot's time (mSlotUsed): 400 to 499 frames. No copy of its own in the original: both
// callers in fn_800FC984 inline it and the linker dropped the body. It was a plain function
// generated before fn_800FC984 (below it here), which is where the .sdata2 literal 400.0f stands.
// R2 pool helper (inlined at 2 call sites; one-statement body); was randomSlotTime.
f32 pool_8053C094() {
    return 400.0f + (f32)(s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
}

void StoneArrghus_8049C718::fn_800FC128() {
    if (trySwap(this)) {
        mSlotUsed[0] = 0;
        mSlotUsed[1] = 0;
        JGeometry::TVec3f zero(0.0f, 0.0f, 0.0f);
        ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(mPartner[0]);
        ActorBase_804A5770* b = getActorManager_801F856C()->findActorBySerial_801F666C(mPartner[1]);
        a->mVel_0x3c = zero;
        b->mVel_0x3c = zero;
        return;
    }
    if (mCooldown != 0) {
        mCooldown--;
    }
    if (mFrame == 3 || mFrame == 6) {
        field_0xb0 = 0x10;
    } else {
        field_0xb0 = 4;
    }
    if (stepWobble(this) == 1) {
        updateHitReaction_801F3D94(1);
        return;
    }
    if (mTimer >= 100) {
        mAngleA += 0x400;
        mAngleB -= 0x400;
    }
    if (mTimer == 20) {
        mAnimState = 0;
    }
    if (mAngleA == 0x4000 || mAngleA == 0xC000) {
        startSound_801F0E34(0x400CB, 0);
    }
    if (mSlotUsed[0] > 0) {
        mSlotUsed[0]--;
    }
    if (mSlotUsed[1] > 0) {
        mSlotUsed[1]--;
    }
    if (mSlotUsed[0] < 50 && mAngleA > 0x3000 && mAngleA < 0x5000) {
        mSlotUsed[0] = 0;
    }
    if (mSlotUsed[1] < 50 && mAngleB > 0x3000 && mAngleB < 0x5000) {
        mSlotUsed[1] = 0;
    }
    mTimer++;
    if (mSlotUsed[0] == 0 && mSlotUsed[1] == 0) {
        mAnimState = 1;
        mState = 2;
        mTimer = (s32)(300.0f + (f32)(s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()));
    }
    field_0x260 += 0x200;
    mPos.z = 5.0f * JMath::sincosTable_.sinShort((s16)field_0x260) + mZBase;
    field_0x262 += 0xB0;
    mPos.x = 100.0f * JMath::sincosTable_.sinShort((s16)field_0x262) + mHome.x;
    mPos.y = (50.0f * JMath::sincosTable_.cosShort((s16)field_0x262) + mHome.y) - 50.0f;
    updateHitReaction_801F3D94(1);
}

// FIXME(shotgun): 99.52 %, regalloc-only; behaviour clean.
void StoneArrghus_8049C718::fn_800FBD90() {
    if (mCooldown != 0) {
        mCooldown--;
    }
    if (mFrame == 3 || mFrame == 6) {
        field_0xb0 = 0x10;
    } else {
        field_0xb0 = 4;
    }
    fn_800FA1A4();
    mTimer--;
    if (mTimer >= 0) {
        f32 s = 1.4f * mScale;
        mBox.set(-26.0f * s, -50.0f * s, 26.0f * s, 0.0f);
        return;
    }
    mFlags &= ~0x10;
    f32 s = mScale;
    mBox.set(-26.0f * s, -50.0f * s, 26.0f * s, 0.0f);
    mMapBox_0x80.set(-26.0f * s, -50.0f * s, 26.0f * s, 0.0f);
    if (checkPlayerAttacks_801F2FAC()) {
        mHitTimer_0xd8 = 10;
        mHitTimerStart_0xdc = 10;
        mState = 9;
        mFlags |= 1;
        mFlags &= ~4;
        mVel_0x3c.set(0.0f, 0.0f, 5.0f);
        mWobble = 0;
        field_0x264 = 0;
        mBox.set(-26.0f * mScale, -50.0f * mScale, 26.0f * mScale, 0.0f);
        startSound_801F0E34(0x40005, 0);
        if (mLife <= 10) {
            die_0x24();
        }
        return;
    }
    updateHitReaction_801F3D94(1);
    if (field_0x264 != 0) {
        if (mWobble != 0) {
            mWobble += 0x2000;
            if (mWobble > 0x10000) {
                mWobble = 0;
            }
        }
        field_0x264++;
        if (field_0x264 == 0x118) {
            field_0x264 = 0;
            mFlags |= 1;
            mFlags |= 4;
            field_0xb0 = 4;
            mVel_0x3c.set(0.0f, 0.0f, 10.0f);
            mState = 6;
            mWobble = 1;
            field_0x264 = 0;
            if (!(mFlags & 8)) {
                if ((f32)mLife / 480.0f < 0.4f) {
                    field_0x257 = 3;
                } else {
                    field_0x257 = 2;
                }
            }
            startSound_801F0E34(0x4002B, 0);
        }
    } else {
        mPos.add(mVel_0x3c);
        startSound_801F0E34(0x4002E, 0);
        JGeometry::TVec3f n;
        if (sweepMapBox_801F2618(&n, false)) {
            f32 k = 2.0f * (mVel_0x3c.x * n.x + mVel_0x3c.y * n.y + mVel_0x3c.z * n.z);
            mVel_0x3c.x -= k * n.x;
            mVel_0x3c.y -= k * n.y;
            mVel_0x3c.z -= k * n.z;
            startSound_801F0E34(0x4002D, 0);
        }
    }
}

void StoneArrghus_8049C718::fn_800FBA14() {
    if (updateHitReaction_801F3D94(1) == 1) {
        mVel_0x3c.set(0.0f, 0.0f, 10.0f);
        return;
    }
    if (mWobble != 0) {
        mWobble += 0x800;
        if (mWobble < 0x8000) {
            return;
        }
        if (mWobble > 0x10000) {
            mWobble = 0;
        }
    }
    u16 angle = 0;
    f32 dist = 0.0f;
    s32 dir = 0;
    s32 player = 0;
    findTargetPlayer_801EEC98(mSerial, &angle, &dir, &dist, &player, 0, 0);
    if (mVel_0x3c.z > -0.5f) {
        if (player >= 0) {
            mPos.x += JMath::sincosTable_.sinShort((s16)angle);
            mPos.y += JMath::sincosTable_.cosShort((s16)angle);
        }
    } else {
        mFlags &= ~1;
        mFlags |= 0x10;
    }
    mVel_0x3c.z -= 0.1f;
    mPos += mVel_0x3c;
    if (mPos.z <= 0.0f) {
        if (mFlags & 8) {
            field_0x257 = 0;
            fn_800FB6E0(0);
        } else {
            mVel_0x3c.set((f32)field_0x257, -1.0f * field_0x257, 0.0f);
            mPos.z = 0.0f;
            mState = 5;
            mTimer = 0x19;
        }
        mRing.fn_800F8210(mLayer_0x4, mPos, 1);
        getGameView_802D6A9C()->fn_802D79E8(mLayer_0x4, 8.0f, 0.0f, false);
        startSound_801F0E34(0x4002C, 0);
        gGameRoot_8053AAF8->startRumble_80134DE4(2, 0);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E6, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E7, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E8, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E9, 0, 0, NULL, NULL, true, 1.0f);
    }
}

// FIXME(shotgun): 99.84 %, operands; behaviour clean.
void StoneArrghus_8049C718::fn_800FB6E0(s32 mode) {
    u16 angle = 0;
    f32 dist = 0.0f;
    s32 dir = 0;
    s32 player = 0;
    findTargetPlayer_801EEC98(mSerial, &angle, &dir, &dist, &player, 0, 0);
    if (player != -1 && dist < 50.0f) {
        field_0x262 = angle;
    } else {
        if (!(mFlags & 8)) {
            dist = 10000.0f;
        } else {
            ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(mPartner[0]);
            ActorBase_804A5770* b = getActorManager_801F856C()->findActorBySerial_801F666C(mPartner[1]);
            JGeometry::TVec3f da = mPos;
            JGeometry::TVec3f db = mPos;
            da.sub(a->mPos);
            db.sub(b->mPos);
            f32 lenA = PSVECMag((Vec*)&da);
            f32 lenB = PSVECMag((Vec*)&db);
            if (lenA < lenB) {
                angle = pool_8053C050(-da.x, -da.y);
            } else {
                angle = pool_8053C050(-db.x, -db.y);
            }
        }
        if (dist < 70.0f) {
            field_0x262 = angle;
        } else {
            field_0x262 += (s16)(0x2000 - (s32)(8192.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()));
        }
    }
    mVel_0x3c.x = 1.5f * JMath::sincosTable_.sinShort((s16)field_0x262);
    mVel_0x3c.y = 1.5f * JMath::sincosTable_.cosShort((s16)field_0x262);
    if (mode == 1) {
        mVel_0x3c.z = 8.0f;
        field_0x257 = 0;
        startSound_801F0E34(0x4002B, 0);
    } else {
        mVel_0x3c.z = 5.0f;
        field_0x257++;
        startSound_801F0E34(0x400CA, 0);
    }
    mFlags |= 4;
    mFlags &= ~0x10;
    f32 s = 1.4f * mScale;
    mBox.set(-26.0f * s, -50.0f * s, 26.0f * s, 0.0f);
    mState = 7;
    mWobble = 1;
    field_0x264 = 0;
}

// FIXME(shotgun): 99.75 %, regalloc-only; behaviour clean.
void StoneArrghus_8049C718::fn_800FB32C() {
    fn_800FA1A4();
    if (mCooldown != 0) {
        mCooldown--;
    }
    if (mFrame == 3 || mFrame == 6) {
        field_0xb0 = 0x10;
    } else {
        field_0xb0 = 4;
    }
    if (updateHitReaction_801F3D94(1) == 1) {
        return;
    }
    if (mWobble != 0) {
        mWobble += 0x800;
        if (mWobble < 0x8000) {
            return;
        }
        if (mWobble > 0x10000) {
            mWobble = 0;
        }
    }
    mVel_0x3c.z -= 0.25f;
    mPos.add(mVel_0x3c);
    if (mPos.z <= 0.0f) {
        if (field_0x257 == 0) {
            fn_800FB6E0(0);
            startSound_801F0E34(0x4002C, 0);
            mRing.fn_800F8210(mLayer_0x4, mPos, 0);
            getGameView_802D6A9C()->fn_802D79E8(mLayer_0x4, 5.0f, 0.0f, false);
            gGameRoot_8053AAF8->startRumble_80134DE4(2, 0);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E6, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E7, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E8, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4E9, 0, 0, NULL, NULL, true, 1.0f);
        } else {
            if (field_0x257 == 4) {
                fn_800FB6E0(1);
            } else {
                fn_800FB6E0(0);
            }
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4ED, 0, 0, NULL, NULL, true, 1.0f);
        }
        mPos.z = 0.0f;
    }
    JGeometry::TVec3f n;
    if (sweepMapBox_801F2618(&n, false)) {
        f32 k = 2.0f * (mVel_0x3c.x * n.x + mVel_0x3c.y * n.y + mVel_0x3c.z * n.z);
        mVel_0x3c.x -= k * n.x;
        mVel_0x3c.y -= k * n.y;
        mVel_0x3c.z -= k * n.z;
        field_0x262 = pool_8053C050(mVel_0x3c.x, mVel_0x3c.y);
    }
    if (mVel_0x3c.z > 0.0f) {
        mFlags |= 1;
    } else {
        mFlags &= ~1;
    }
}

void StoneArrghus_8049C718::fn_800FB070() {
    mVel_0x3c.z -= 0.3f;
    mPos.add(mVel_0x3c);
    if (mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        mVel_0x3c.z = 0.0f;
    }
    if (mCooldown != 0) {
        mCooldown--;
    }
    if (mFrame == 3 || mFrame == 6) {
        field_0xb0 = 0x10;
    } else {
        field_0xb0 = 4;
    }
    f32 scale = mScale;
    f32 x0 = -26.0f * scale;
    f32 y0 = -50.0f * scale;
    f32 x1 = 26.0f * scale;
    mBox.set(x0, y0, x1, 0.0f);
    mMapBox_0x80.set(x0, y0, x1, 0.0f);
    if (mWobble != 0) {
        mWobble += 0x2000;
        if (mWobble > 0x10000) {
            mWobble = 0;
        }
    }
    field_0x264++;
    if (updateHitReaction_801F3D94(1) != 1) {
        if (field_0x264 >= 0x118) {
            field_0x264 = 0;
            mFlags |= 1;
            mFlags |= 4;
            field_0xb0 = 4;
            mVel_0x3c.set(0.0f, 0.0f, 10.0f);
            mState = 6;
            mWobble = 1;
            field_0x264 = 0;
            if (!(mFlags & 8)) {
                if ((f32)mLife / 480.0f < 0.4f) {
                    field_0x257 = 3;
                } else {
                    field_0x257 = 2;
                }
            }
            startSound_801F0E34(0x4002B, 0);
        } else if (checkPlayerAttacks_801F2FAC()) {
            mHitTimer_0xd8 = 20;
            mHitTimerStart_0xdc = 20;
            mVel_0x3c.set(0.0f, 0.0f, 0.0f);
            mWobble = 1;
            startSound_801F0E34(0x40005, 0);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4EB, 0, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x4EC, 0, 0, NULL, NULL, true, 1.0f);
            if (mLife <= 10) {
                die_0x24();
            }
        }
    }
}

void StoneArrghus_8049C718::update() {
    mBox2.set(-21.0f, -35.0f + mPos.z, 22.0f, -2.0f + mPos.z);
    if (updateBossDeath_0x2C(0.0f, -20.0f, 50.0f)) {
        return;
    }
    if (!(mFlags & 1)) {
        if (mFlags & 0x10) {
            damagePlayersInBox_801F2B7C(4, -1, 6);
        } else {
            damagePlayersInBox_801F2B7C(1, -1, 6);
        }
    }
    switch (mState) {
    case 0:
        fn_800FCCCC();
        break;
    case 1:
        mAngleSpeed = 0;
        mAngle += mAngleSpeed;
        mRadius = 1.0f;
        field_0x260 += 0x200;
        mPos.z = 5.0f * JMath::sincosTable_.sinShort((s16)field_0x260) + mZBase;
        switch (field_0x254) {
        case 0: {
            focusCamera(this);
            if (mSlot >= 0x18) {
                getActorManager_801F856C()->fn_801F6884();
                gAudioMgr_8053AF80->startBgm_80458FF0(0x1000004, false, 0);
                mTimer = 0;
                field_0x254++;
            }
            break;
        }
        case 1:
            mTimer++;
            if (mTimer > 100) {
                mAnimState = 1;
                mState = 2;
                mTimer = (s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 200;
            }
            break;
        }
        break;
    case 2:
        fn_800FC984();
        break;
    case 3:
        fn_800FC4A8();
        break;
    case 4:
        fn_800FC128();
        break;
    case 5:
        fn_800FBD90();
        break;
    case 6:
        fn_800FBA14();
        break;
    case 7:
        fn_800FB32C();
        break;
    case 8:
        updateHitReaction_801F3D94(1);
        if (mWobble != 0) {
            mWobble += 0x2000;
            if (mWobble > 0x20000) {
                mWobble = 0;
            }
        }
        if (mTimer == 0x3C) {
            mAnimState = 0;
        }
        if (mTimer == 100) {
            mAnimState = 2;
            startSound_801F0E34(0x4002A, 0);
        }
        if (mTimer > 0x96) {
            mPos.z = 0.0f;
            mFlags |= 1;
            mFlags |= 4;
            field_0xb0 = 4;
            mVel_0x3c.set(0.0f, 0.0f, 10.0f);
            mState = 6;
            mWobble = 1;
            field_0x264 = 0;
            if (!(mFlags & 8)) {
                if ((f32)mLife / 480.0f < 0.4f) {
                    field_0x257 = 3;
                } else {
                    field_0x257 = 2;
                }
            }
            startSound_801F0E34(0x4002B, 0);
        } else {
            mVel_0x3c.z -= 0.2f;
            mPos.z += mVel_0x3c.z;
            if (mPos.z <= 0.0f) {
                mPos.z = 0.0f;
                mVel_0x3c.z = 0.0f;
            }
            mTimer++;
        }
        break;
    case 9:
        fn_800FB070();
        break;
    }
    field_0x258++;
    mSlot = 0;
    gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
    Camera_804AA9E4* cam = gGameRoot2_8053AB10->field_0x24;
    if (cam->field_0x179 == 0) {
        cam->field_0x178 = 1;
    }
    mRing.fn_800F802C();
    fn_800FA6B0();
}

// FIXME(shotgun): 91.96 %, scheduling+regalloc; behaviour clean.
bool StoneArrghus_8049C718::isBlocked_0x4C(s32 layer, const JGeometry::TVec3f* pos) {
    s32 kind = getTileCollisionCode_8022461C(pos, layer);
    if (kind == 3 || kind == 14 || kind == 2) {
        return false;
    }
    return fn_802265DC(pos, layer, 0);
}

void StoneArrghus_8049C718::die_0x24() {
    getGameView_802D6A9C()->fn_802D7440(0);
    startSound_801F0E34(0x40006, 0);
    mSignal = 2;
    startBossDeath_0x28(0, 0.0f, -20.0f);
}

void StoneArrghus_8049C718::vfunc_0x44() {
    if (field_0xb0 == 4 || field_0xb0 == 0x12 || mCooldown != 0) {
        return;
    }
    if (mHitTimer_0xd8 == 0) {
        field_0x255++;
        mCooldown = 0x32;
        if (mWobble == 0) {
            mWobble = 1;
        }
        takeHit_801F37AC(0, &mPos, 3, 0, -1);
        startSound_801F0E34(0x40005, 0);
        mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, 0.0f);
    }
    if (mSignal != 0) {
        if (mState == 9) {
            return;
        }
        mState = 9;
        mFlags |= 1;
        mFlags &= ~4;
        mVel_0x3c.set(0.0f, 0.0f, 5.0f);
        mWobble = 0;
        field_0x264 = 0;
        f32 scale = mScale;
        mBox.i.x = -26.0f * scale;
        mBox.i.y = -50.0f * scale;
        mBox.f.x = 26.0f * scale;
        mBox.f.y = 0.0f;
        startSound_801F0E34(0x40005, 0);
        if (mLife <= 10) {
            die_0x24();
        }
    } else if (field_0x255 == 3) {
        mSignal = 1;
    }
}

// atan2 as a 16-bit angle. No copy of its own in the original either (inlined into
// fn_800FB6E0 and fn_800FB32C, body dropped): a plain function at this place, which puts
// 10430.378f in front of vfunc_0x44's literals as in the original's .sdata2.
// R2 pool helper (inlined at 3 call sites; one-statement body); was headingOf.
s32 pool_8053C050(f32 x, f32 y) {
    return (s32)(JMath::atanTable_.atan2_(x, y) * 10430.378f);
}

// FIXME(shotgun): 99.96 %, scheduling+regalloc; behaviour drift.
void StoneArrghus_8049C718::fn_800FA6B0() {
    if (mAnimState != mAnimPrev) {
        mAnimPrev = mAnimState;
        mAnimTime = 0;
        mAnimStep = 0;
    }
    switch (mAnimState) {
    case 0:
        mFrame = lbl_8049C61C[mAnimStep];
        if (mFrame == 0xFF) {
            mFrame = 3;
            mAnimTime = 0;
        } else if (mAnimTime == lbl_8049C628[mAnimStep]) {
            mAnimTime = 0;
            mAnimStep++;
        }
        if (mAnimTime == 0 && (mAnimStep == 0 || mAnimStep == 6)) {
            startSound_801F0E34(0x40026, 0);
        }
        break;
    case 1:
        mFrame = lbl_80539E80[mAnimStep];
        if (mFrame == 0xFF) {
            mFrame = 0;
            mAnimTime = 0;
        } else if (mAnimTime == lbl_80539E88[mAnimStep]) {
            mAnimTime = 0;
            mAnimStep++;
        }
        if (mAnimTime == 0 && mAnimStep == 0) {
            startSound_801F0E34(0x40027, 0);
        }
        break;
    case 2:
        mFrame = lbl_80539E90[mAnimStep];
        if (mFrame == 0xFF) {
            mFrame = 6;
            mAnimTime = 0;
        } else if (mAnimTime == lbl_80539E94[mAnimStep]) {
            mAnimTime = 0;
            mAnimStep++;
        }
        break;
    case 3:
        mFrame = 0;
        break;
    case 4:
        mFrame = 7;
        break;
    case 5:
        mFrame = 8;
        break;
    case 6:
        mFrame = 9;
        break;
    case 7:
        mFrame = 10;
        break;
    case 8:
        mFrame = lbl_80539E98[mAnimStep];
        if (mFrame == 0xFF) {
            mFrame = 3;
            mAnimTime = 0;
        } else if (mAnimTime == lbl_80539EA0[mAnimStep]) {
            mAnimTime = 0;
            mAnimStep++;
        }
        break;
    }
    mAnimTime++;
}

void StoneArrghus_8049C718::fn_800FA1A4() {
    if (!(mFlags & 8)) {
        return;
    }
    ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(mPartner[0]);
    ActorBase_804A5770* b = getActorManager_801F856C()->findActorBySerial_801F666C(mPartner[1]);
    JGeometry::TVec3f posA = a->mPos;
    JGeometry::TVec3f posB = b->mPos;
    JGeometry::TVec3f d = posA;
    d.sub(posB);
    if (PSVECMag((Vec*)&d) < 20.0f) {
        JGeometry::TVec3f tA = a->mVel_0x3c;
        JGeometry::TVec3f tB = b->mVel_0x3c;
        a->mVel_0x3c = tB;
        b->mVel_0x3c = tA;
        JGeometry::TVec3f off;
        JGeometry::TVec3f mid = posA;
        mid.add(posB);
        mid.scale(0.5f);
        d.normalize();
        off.set(d);
        off.scale(10.0f);
        off.add(mid);
        a->mPos = off;
        off.set(d);
        off.scale(-10.0f);
        off.add(mid);
        b->mPos = off;
    }
    if (mSlot == 0) {
        d.set(mPos.x, mPos.y - 20.0f, mPos.z);
        d.sub(posA);
        if (PSVECMag((Vec*)&d) < 50.0f) {
            d.normalize();
            d.scale(-5.0f);
            a->mVel_0x3c = d;
        }
        d.set(mPos.x, mPos.y - 20.0f, mPos.z);
        d.sub(posB);
        if (PSVECMag((Vec*)&d) < 50.0f) {
            d.normalize();
            d.scale(-5.0f);
            b->mVel_0x3c = d;
        }
    }
}

// The 3x3 part of `m` as the scale x, y, z; the translation column is the caller's.
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

void StoneArrghus_8049C718::draw() {
    f32 s = JMath::sincosTable_.sinShort((s16)mWobble);
    f32 a = 0.4f * s;
    f32 b = -0.4f * s;
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    setScaleMtx(mtx, mScale + a, mScale, mScale + b);
    params.mMtx = mtx;
    params.field_0x14.r = (s32)(255.0f * mTint);
    params.field_0x14.g = (s32)(255.0f * mTint);
    params.field_0x14.b = (s32)(255.0f * mTint);
    params.field_0x10.a = (s32)(255.0f * mAlpha);
    if (mFlags & 2) {
        params.field_0x30 = 0xD;
    } else if (mFlags & 4) {
        params.field_0x30 = 0x11;
    }
    void* res = getBossArchiveResource_804032E8('WART', lbl_8049C5D0[mFrame]);
    drawSpriteRes_801EFD34(res, &params, 5, 0.0f, -22.0f * (mScale + b), 0.0f);
}

// FIXME(shotgun): 99.73 %, regalloc-only; behaviour clean.
void StoneArrghusChildPuff_8049C678::init() {
    mBox2.set(-13.0f, -14.0f, 13.0f, 8.0f);
    mLife = 4;
    mFrame = 0;
    mState = 0;
    mPhase = 0;
    mAngle = 0;
    mFlag249 = 0;
    mIndex = (mSpawnParam_0x90 >> 16) & 0xFF;
    mFlag248 = mSpawnParam_0x90 >> 24;
    if (mFlag248 == 2) {
        mMapBox_0x80.set(-13.0f, -14.0f, 13.0f, 8.0f);
        mBox.set(-13.0f, -14.0f, 13.0f, 8.0f);
        mPos.x += (f32)lbl_804638C0[mIndex * 2];
        mPos.y += (f32)lbl_804638C0[mIndex * 2 + 1];
        field_0xb0 = 0x40004;
    } else {
        if (mIndex < 0x30) {
            mPos.x += (f32)lbl_80463800[mIndex * 2];
            mPos.y += (f32)lbl_80463800[mIndex * 2 + 1];
        } else {
            mPos.x += (f32)lbl_80463800[0];
            mPos.y += (f32)lbl_80463800[1];
        }
        mPos.z = 0.0f;
        mDelay = 0;
        for (s32 i = 0; i < mIndex; i++) {
            mDelay += (mIndex < 30) ? 40 - mIndex : 10;
        }
        f32 n = gGameRoot_8053AAF8->getPlayerCount_80138A30();
        if (1.0f == n) {
            mVariant = -1;
            field_0x94 = 0;
        } else {
            mVariant = (s32)(n * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
            field_0x94 = mVariant + 1;
            if ((s32)(3.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) < 1) {
                mVariant = -1;
                field_0x94 = 0;
            }
        }
    }
}

// FIXME(shotgun): 92.16 %, missing/extra; behaviour clean.
void StoneArrghusChildPuff_8049C678::fn_800F953C() {
    field_0xb0 = 0x80004;
    mParamDstr_0x1a8 = true;
    StoneArrghus_8049C718* parent = mParent;
    JGeometry::TVec3f target;
    f32 speed;
    if (parent->mState == 3) {
        target.set(slotPosOf(parent, parent->mSlot));
        speed = 0.3f;
        mFlag249 = 1;
    } else if (parent->mState == 4) {
        s32 slot = parent->mSlot;
        target.set(linePosOf(parent, slot));
        speed = (0.36f + 0.001f * (f32)lbl_8049C3B8[8]) - (0.032f + 0.001f * (f32)lbl_8049C3B8[9]) * (f32)((slot >> 1) + 1);
        mFlag249 = 1;
    } else if (parent->mState == 1) {
        target.set(slotPosOf(parent, parent->mSlot));
        speed = 0.08f;
        mFlag249 = 1;
    } else {
        target.set(slotPosOf(parent, parent->mSlot));
        speed = 0.2f;
        mFlag249 = 0;
    }
    if (updateHitReaction_801F3D94(1) == 0) {
        if (findOverlappingPlayer_801F285C(NULL, NULL, true, -1) >= 0) {
            JGeometry::TVec2f xy(mPos.x, mPos.y);
            hitPlayerGroup_80238548(0, 0, &xy, 0x10);
        }
        moveToward(mPos, target, speed);
        damagePlayersInBox_801F2B7C(2, -1, 1);
    }
}

// FIXME(shotgun): 84.23 %, missing/extra + reloc-name; behaviour near.
void StoneArrghusChildPuff_8049C678::fn_800F9170() {
    mFlag249 = 1;
    StoneArrghus_8049C718* parent = mParent;
    JGeometry::TVec3f target = slotPosOf(parent, parent->mSlot);
    s32 serial = field_0xa0;
    if (serial != -1) {
        ActorBase_804A5770* other = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (other != NULL && other->mActorId == 'BOME') {
            mAngle = 0x4000;
            mCount = 0;
            return;
        }
    }
    if (updateHitReaction_801F3D94(1) != 0) {
        return;
    }
    if (checkHits_801F305C()) {
        return;
    }
    field_0xb0 = 0x80002;
    mCount++;
    JGeometry::TVec3f v = mParent->mPos;
    v.sub(mPos);
    if (PSVECMag((Vec*)&v) < 40.0f && mCount > 0x50) {
        mState = 1;
    }
    v.normalize();
    v.scale(0.8f);
    v.z = 0.0f;
    if (mPos.z < 10.0f || mCount >= 0x50) {
        v.zero();
    }
    mVel_0x3c.set(v);
    mPos.add(mVel_0x3c);
    mAngle += 0x480;
    mPos.z = 25.0f * JMAAbs(JMath::sincosTable_.sinShort((s16)mAngle));
    if (mPos.z <= 1.5f) {
        startSound_801F0E34(0x50083, 0);
    }
}

// FIXME(shotgun): 88.45 %, missing/extra + reloc-name; behaviour drift.
void StoneArrghusChildPuff_8049C678::fn_800F8BAC() {
    StoneArrghus_8049C718* parent = mParent;
    if (parent->mSlotUsed[mIndex] != 0) {
        f32 off;
        s16 ang;
        if (!mIndex) {
            s32 k = (parent->mSlot >> 1) + 1;
            ang = parent->mAngleA;
            off = 20.0f + (f32)(k * 20);
        } else {
            s32 k = (parent->mSlot >> 1) + 1;
            ang = parent->mAngleB;
            off = (f32)(k * -20) - 20.0f;
        }
        JGeometry::TVec3f target;
        f32 s = JMath::sincosTable_.sinShort(ang);
        f32 c = JMath::sincosTable_.cosShort(ang);
        target = JGeometry::TVec3f(parent->mPos.x + off * s, parent->mPos.y + off * c,
                                   10.0f + parent->mPos.z);
        f32 speed = 0.36f - 0.032f * (f32)((parent->mSlot >> 1) + 1);
        mFlag249 = 1;
        mVel_0x3c.set(mPos.x, mPos.y, mPos.z);
        moveToward(mPos, target, speed);
        mVel_0x3c.sub(mPos);
        mVel_0x3c.scale(-0.5f);
    } else {
        mVel_0x3c.z -= 0.2f;
        mPos.add(mVel_0x3c);
        if (mPos.z <= 0.0f) {
            mPos.z = 0.0f;
            mVel_0x3c.z = 0.0f;
            mVel_0x3c.x *= 0.99f;
            mVel_0x3c.y *= 0.99f;
        }
        JGeometry::TVec3f n;
        if (sweepMapBox_801F2618(&n, false)) {
            f32 k = 2.0f * (mVel_0x3c.x * n.x + mVel_0x3c.y * n.y + mVel_0x3c.z * n.z);
            mVel_0x3c.x -= k * n.x;
            mVel_0x3c.y -= k * n.y;
            mVel_0x3c.z -= k * n.z;
            if (mSoundTimer == 0) {
                startSound_801F0E34(0x4002D, 0);
                mSoundTimer = 1;
            }
        }
    }
    if (mSoundTimer != 0) {
        mSoundTimer++;
        if (mSoundTimer >= 0x1E) {
            mSoundTimer = 0;
        }
    }
    if (JMAAbs(mVel_0x3c.x) < 1.0f && JMAAbs(mVel_0x3c.y) < 1.0f) {
        JGeometry::TBox2f box(mPos.x - 10.0f, mPos.y - 10.0f, mPos.x + 10.0f, mPos.y + 10.0f);
        fn_80239914(&box, mLayer_0x4, mSerial);
        if (getPlayersInBox_801F2740(-1)) {
            mVel_0x3c.x = 0.0f;
            mVel_0x3c.y = 0.0f;
        }
    } else {
        damagePlayersInBox_801F2B7C(2, -1, 1);
    }
    if (JMAAbs(mVel_0x3c.x) < 1.0f && JMAAbs(mVel_0x3c.y) < 1.0f) {
        s32 player = findOverlappingPlayer_801F285C(NULL, NULL, true, -1);
        if (player >= 0) {
            JGeometry::TVec3f d(0.0f, 0.0f, 0.0f);
            d.x = getPlayerPos_8023E724(player)->x - mPos.x;
            d.y = getPlayerPos_8023E724(player)->y - mPos.y;
            d.normalize();
            d.scale(-1.0f);
            mVel_0x3c.x = 1.5f * d.x;
            mVel_0x3c.y = 1.5f * d.y;
            JGeometry::TVec2f xy(mPos.x, mPos.y);
            hitPlayerGroup_80238548(player, 0, &xy, 0x10);
        }
    }
}

void StoneArrghusChildPuff_8049C678::update() {
    if (mFrame == 0) {
        mParent = (StoneArrghus_8049C718*)getActorManager_801F856C()->findActorBySerial_801F666C(mSpawnParam_0x90 & 0xFFFF);
    }
    mFrame++;
    if (mFlag248 == 1) {
        switch (mState) {
        case 0:
            switch (mPhase) {
            case 0:
                if (mParent->mState == 1) {
                    mStep = 0;
                    mPhase++;
                }
                break;
            case 1:
                mStep++;
                if (mStep > mDelay) {
                    mState = 1;
                    field_0xb0 = 0x80004;
                    startSound_801F0E34(0x40029, 0);
                }
                break;
            }
            break;
        case 1:
            fn_800F953C();
            break;
        case 2:
            fn_800F9170();
            break;
        }
        if (mParent->mSignal == 1) {
            die_0x24();
        }
    } else {
        fn_800F8BAC();
        if (mParent->mSignal == 2) {
            die_0x24();
        }
    }
}

void StoneArrghusChildPuff_8049C678::vfunc_0x40() {
    mState = 2;
    startSound_801F0E34(0x50084, 0);
}

// FIXME: 98.96 %, regalloc-only; behaviour clean.
// FIXME(shotgun): scheduling+regalloc differs; behaviour matches.
void StoneArrghusChildPuff_8049C678::die_0x24() {
    s32 count;
    if (mIndex == 0x14) {
        count = 4;
    } else {
        count = (u32)(1.3f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 1;
    }
    s32 serial = getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &mPos, count);
    if (serial >= 0) {
        ActorBase_804A5770* gem = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        JGeometry::TVec3f vel(
            2.0f - 4.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1(),
            2.0f - 4.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1(),
            3.0f + 3.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()
        );
        gem->mVel_0x3c = vel;
    }
    if ((s32)(5.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) == 3) {
        getActorManager_801F856C()->spawnActor_801F9484(ACTOR_HART, mLayer_0x4, &mPos, 0, -1, -1);
    }
    mParamDasu_0x18c = 0;
    ActorBase_804A5770::die_0x24();
}

void StoneArrghusChildPuff_8049C678::draw() {
    f32 a = 0.0f;
    f32 b = 0.0f;
    if (mState == 2) {
        f32 s = JMAAbs(JMath::sincosTable_.sinShort((s16)(mAngle + 0x4000)));
        a = 0.1f * s;
        b = -0.1f * s;
    }
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    if (mFlag248 == 2) {
        mtx[0][0] = 1.5f;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = 1.5f;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = 1.5f;
        params.mMtx = mtx;
        drawSpriteScaled_801F02BC(lbl_80463A58, 0x25, &params, 0, 0, false, 1.0f, 1.0f);
    } else {
        mtx[0][0] = 1.0f + a;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = 1.0f;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = 1.0f + b;
        params.mMtx = mtx;
        if (mFlag249 != 0) {
            params.field_0x30 = 0xF;
        }
        void* res;
        if (mState == 2) {
            res = getBossArchiveResource_804032E8('WART', lbl_8049C648[(mVariant + 1) * 2 + 1]);
        } else {
            res = getBossArchiveResource_804032E8('WART', lbl_8049C648[(mVariant + 1) * 2]);
        }
        drawSpriteRes_801EFD34(res, &params, 0, 0.0f, 0.0f, 0.0f);
    }
}

// Moves one axis of `cur` toward `target` by `step`; returns 1 once it got there.
static inline s32 stepAxis(f32& cur, f32 target, f32 step) {
    cur = cut4(cur);
    target = cut4(target);
    if (cur > target) {
        cur -= step;
        if (cur <= target) {
            cur = target;
            return 1;
        }
    } else {
        cur += step;
        if (cur >= target) {
            cur = target;
            return 1;
        }
    }
    return 0;
}

s8 fn_800F82CC(JGeometry::TVec3f* cur, const JGeometry::TVec3f* target, f32 step) {
    u8 arrived = stepAxis(cur->x, target->x, step);
    arrived += stepAxis(cur->y, target->y, step);
    arrived += stepAxis(cur->z, target->z, step);
    return arrived == 3;
}

void HitRing_800F802C::fn_800F8210(s32 layer, JGeometry::TVec3f pos, u8 type) {
    mPos.x = pos.x;
    mPos.y = pos.y;
    mPos.z = pos.z;
    mActive = true;
    mRadius = 0.0f;
    mTimer = 0;
    mHitMask = 0;
    mType = type;
    f32 step;
    switch (mType) {
    case 0:
        step = 0.3f;
        break;
    case 1:
        step = 0.4f;
        break;
    case 2:
        step = 0.8f;
        break;
    default:
        step = 0.3f;
        break;
    }
    if (layer < 8) {
        gGameRoot2_8053AB10->field_0x20->startHeightGridEffect_802F601C(&mPos, 2, NULL, step, 1.0f);
    }
}

void HitRing_800F802C::fn_800F802C() {
    if (!mActive) {
        return;
    }
    f32 step;
    s32 duration;
    switch (mType) {
    case 0:
        step = 3.0f;
        duration = 40;
        break;
    case 1:
        step = 4.0f;
        duration = 40;
        break;
    case 2:
        step = 8.0f;
        duration = 20;
        break;
    default:
        step = 3.0f;
        duration = 40;
        break;
    }
    mRadius = mTimer * step;
    mTimer++;
    if (mTimer > duration) {
        mActive = false;
    }
    for (s32 i = 0; i < 4; i++) {
        if (isPlayerActive_8023F264(i) && getRepresentativePlayer_8022EA0C(i) == i && !(mHitMask & (1 << i)) &&
            getPlayerLayer_8023DE58(i) == 0) {
            // One stack vector: the player's offset (its length is TVec2's
            // TUtilf::sqrt), then the position the hit is reported at.
            JGeometry::TVec2f v;
            v.x = getPlayerPos_8023E724(i)->x - mPos.x;
            v.y = getPlayerPos_8023E724(i)->y - mPos.y;
            if (v.length() < mRadius) {
                v.x = mPos.x;
                v.y = mPos.y;
                hitPlayerGroup_80238548(i, 0, &v, 0xB);
                mHitMask |= 1 << i;
            }
        }
    }
}

StoneArrghusChildPuff_8049C678::~StoneArrghusChildPuff_8049C678() {}
