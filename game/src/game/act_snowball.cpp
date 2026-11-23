#include "game/GameAudio.h"
#include "game/ActorManager.h"
#include "game/EmitterDelete.h"
#include "game/Effect.h"
#include "game/EnemyBase.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/act/act_snowball.h"

// Animations: (cel, duration) pairs, ended by a cel of -1.
static const s16 lbl_80464308[] = {0, 10, 1, 10, -1, -1};
static const s16 lbl_80464314[] = {2, 3, 3, 3, 4, 3, 5, 3, -1, -1};
static const u32 lbl_80464328[] = {0x00260001, 0x00260004, 0x00260007};
static const s16* lbl_80539EE8[] = {lbl_80464308, lbl_80464314};

static inline void setTrans(Mtx m, f32 x, f32 y, f32 z) {
    m[0][3] = x;
    m[1][3] = y;
    m[2][3] = z;
}

static inline void setScaleMtx(Mtx m, f32 sx, f32 sy, f32 sz) {
    m[0][0] = sx;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = sy;
    m[1][2] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = sz;
}

Snowball_8049D020::Snowball_8049D020()
    : field_0x230(0), field_0x234(0), field_0x238(0), field_0x23c(-1), field_0x240(0),
      field_0x244(0), field_0x248(0), field_0x24c(0), field_0x250(0), field_0x254(0),
      field_0x258(0), field_0x25c(0), field_0x260(1.0f), field_0x264(0.0f),
      field_0x268(0.0f), field_0x26c(1.0f), field_0x270(1.0f), mSprite(),
      mRail(), field_0x374(NULL), field_0x378(NULL), field_0x37c(NULL) {}

Snowball_8049D020::~Snowball_8049D020() {
    releaseEmitter_801EE434(field_0x374);
    field_0x374 = NULL;
    releaseEmitter_801EE434(field_0x378);
    field_0x378 = NULL;
    releaseEmitter_801EE434(field_0x37c);
    field_0x37c = NULL;
}

void Snowball_8049D020::init() {
    ActorBase_804A5770::init();
    field_0x234 = mSpawnParam_0x90 & 0xF;
    switch ((s32)((mSpawnParam_0x90 >> 4) & 0xF)) {
    case 1:
        field_0x260 = 2.0f;
        field_0x54.set(field_0x260, field_0x260, field_0x260);
        break;
    }
    field_0x258 = (mSpawnParam_0x90 >> 8) & 0xFF;
    field_0x25c = (mSpawnParam_0x90 >> 16) & 0xFF;
    if (field_0x258 > 0 && field_0x260 == 2.0f) {
        mBox.i.x = -16.0f;
        mBox.i.y = -16.0f;
        mBox.f.x = 16.0f;
        mBox.f.y = 16.0f;
    } else {
        mBox.i.x = -8.0f;
        mBox.i.y = -8.0f;
        mBox.f.x = 8.0f;
        mBox.f.y = 8.0f;
    }
    mBox2.i.x = mBox.i.x;
    mBox2.i.y = mBox.i.y;
    mBox2.f.x = mBox.f.x;
    mBox2.f.y = mBox.f.y;
    mMapBox_0x80.i.x = mBox.i.x;
    mMapBox_0x80.i.y = mBox.i.y;
    mMapBox_0x80.f.x = mBox.f.x;
    mMapBox_0x80.f.y = mBox.f.y;
    mLife = 1000;
    field_0xb8 = 1;
    mKnockbackKind_0xf8 = 2;
    field_0xbc = 3;
    field_0x1a4 = 1;
    mParamAtcn_0x118 = 2;
    mParamAtcp_0x114 = 2;
    field_0x240 = mDir_0x98;
    field_0xb0 = 0x0A040304;
    mSprite.setKeyTables_801D2608(lbl_80539EE8, 2);
    mSprite.startAnim_801D22DC(0, 2, false);
    mSprite.setFrame_801D1F14(0.0f);
    if (field_0x258 > 0) {
        fn_8010C294(3);
    }
}

void Snowball_8049D020::update() {
    ActorBase_804A5770::update();
    field_0x26c = 1.0f;
    JGeometry::TBox2<f32> box = mBox;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    if (field_0x238 != 12 && field_0x238 != 13) {
        JGeometry::TVec2<f32> playerPos;
        if (fn_8023AB94(&box, mLayer_0x4, &playerPos) >= 0) {
            fn_8010C294(12);
        }
    }
    switch (field_0x238) {
    case 0: {
        checkActorHit_801F2DB4('NULL', 'NULL', NULL);
        field_0x26c = 0.0f;
        u16 angle;
        s32 dir;
        s32 player;
        f32 dist;
        findTargetPlayer_801EEC98(mSerial, &angle, &dir, &dist, &player, 0, 0);
        if (player >= 0 && dist < 64.0f) {
            field_0x23c = player;
            fn_8010C294(1);
        }
        break;
    }
    case 1:
        field_0x26c = 0.8f;
        field_0x24c++;
        if (field_0x24c >= 50) {
            fn_8010C294(2);
        }
        break;
    case 2:
        if (field_0x23c >= 0) {
            chase(field_0x23c);
        }
        grow();
        checkActorHit_801F2DB4('NULL', 'NULL', NULL);
        if (damagePlayersInBox_801F2B7C(-100, -1, 1)) {
            fn_8010C294(7);
        }
        {
            JGeometry::TVec3f unkVec;
            if (sweepMapBox_801F2618(&unkVec, false)) {
                fn_8010C294(5);
            }
        }
        break;
    case 3:
        mRail.fn_800FE3EC(field_0x258, mPos, mLayer_0x4);
        fn_8010C294(4);
        break;
    case 4:
        if (field_0x258 > 0) {
            followRail();
        }
        checkActorHit_801F2DB4('NULL', 'NULL', NULL);
        damagePlayersInBox_801F2B7C(-100, -1, 1);
        break;
    case 5: {
        if (pool_8053C4AC()) {
            fn_8010C294(6);
        }
        break;
    }
    case 6:
        field_0x26c = 0.0f;
        checkActorHit_801F2DB4('NULL', 'NULL', NULL);
        fn_80239914(&box, mLayer_0x4, mSerial);
        field_0x244++;
        if (field_0x244 >= 180) {
            fn_8010C294(0);
        }
        break;
    case 7: {
        field_0x248++;
        if (field_0x248 > 60) {
            damagePlayersInBox_801F2B7C(-100, -1, 1);
            field_0x248 = 60;
        }
        JGeometry::TVec3f unkVec;
        if (sweepMapBox_801F2618(&unkVec, false)) {
            fn_8010C294(5);
            break;
        }
        if (moveToward_801CD950(&field_0x264, 0.1f, 0.015f) == 0) {
            fn_8010C294(2);
        }
        pool_8053C490();
        pool_8053C494();
        break;
    }
    case 8:
        field_0x26c = 0.0f;
        if (field_0xc4 != 0) {
            JGeometry::TVec3f* pPos = fn_8023E7B0(field_0xc8);
            mPos.x = pPos->x;
            mPos.y = pPos->y;
            mPos.z = pPos->z;
            mLayer_0x4 = getPlayerLayer_8023DE58(field_0xc8);
        } else {
            fn_8010C294(9);
        }
        break;
    case 9: {
        JGeometry::TVec3f unkVec;
        if (damagePlayersInBox_801F2B7C(mParamAtcp_0x114, field_0x23c, 1) || fn_801F2B20(0) || sweepMapBox_801F2618(&unkVec, false)) {
            fn_8010C294(10);
            break;
        }
        if (pool_8053C4AC()) {
            fn_8010C294(10);
        }
        break;
    }
    case 10: {
        fn_80239914(&box, mLayer_0x4, mSerial);
        if (mSprite.isStopped_801D21F0()) {
            if (pool_8053C4AC()) {
                fn_8010C294(11);
            }
        }
        break;
    }
    case 11:
        fn_80239914(&box, mLayer_0x4, mSerial);
        break;
    case 12:
        mDrawEnabled = false;
        fn_80239914(&box, mLayer_0x4, mSerial);
        field_0x250++;
        if (field_0x250 >= 90) {
            fn_8010C294(13);
        }
        break;
    case 13:
        break;
    }
    field_0x54.set(field_0x260, field_0x260, field_0x260);
    mSprite.mFrameCtrl.setRate(field_0x26c);
    mSprite.update_801D1C18();
}

static inline void drawSprite(s32 layer, const JGeometry::TVec3f* pos, const char* name,
                              s32 frame, SpriteParams_801F5AE0* params) {
    if (layer < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', name);
        gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pos, res, frame, params, 1.0f, 1.0f);
    }
}

// One instruction short: &mPos stays folded into r31+0xc instead of moving to r30 before the layer check.
// FIXME: 99.17 %, missing/extra; behaviour clean.
void Snowball_8049D020::draw() {
    ActorBase_804A5770::draw();
    SpriteParams_801F5AE0 params2;
    SpriteParams_801F5AE0 params;
    Mtx mtx1;
    Mtx mtx2;
    if (!field_0x54.isZero()) {
        setTrans(mtx1, 0.0f, 0.0f, 0.0f);
        setScaleMtx(mtx1, field_0x54.x, field_0x54.y, field_0x54.z);
        params2.mMtx = mtx1;
        setTrans(mtx2, 0.0f, 4.0f * field_0x260, 0.0f);
        setScaleMtx(mtx2, field_0x54.x, field_0x54.y, field_0x54.z);
        params.mMtx = mtx2;
    }
    params.field_0x10.a = 0x80;
    drawShadow_801EF830(mLayer_0x4, &mPos, &params, 0x407, "enemy_gc_sob.bin", mSprite.mCel + 0x165, NULL);
    drawSprite(mLayer_0x4, &mPos, "enemy_gc_sob.bin", mSprite.mCel + 0x165, &params2);
}

// -v by value, as a unary TVec3 operator- would give it.
static inline JGeometry::TVec3f negated(const JGeometry::TVec3f& v) {
    JGeometry::TVec3f result;
    result.negate(v);
    return result;
}

// FIXME: 95.64 %, missing/extra + reloc-name; behaviour drift.
// missing/extra + reloc-name: ours has 2 fewer instrs (274 vs 276): the original keeps the table, the actor manager tag and the serial in r27-r29 (stmw/lmw r27) and negates case 5 through two stack TVec3f copies; the single-case switch in case 13 has an extra bge in the original.
bool Snowball_8049D020::fn_8010C294(s32 state) {
    switch (state) {
    case 1:
        field_0x24c = 0;
        field_0x254 = 5;
        break;
    case 2: {
        field_0x248 = 0;
        field_0x24c = 0;
        u16 outAngle;
        f32 outDist;
        s32 outPlayer;
        s32 outDir;
        outAngle = 0;
        outDir = 0;
        outPlayer = 0;
        outDist = 0.0f;
        findTargetPlayer_801EEC98(mSerial, &outAngle, &outDir, &outDist, &outPlayer, field_0x23c, 2);
        field_0x230 = outAngle;
        break;
    }
    case 3:
        mRail.clear();
        break;
    case 4:
        field_0x264 = 1.5f;
        if (field_0x258 > 0 && field_0x25c > 0) {
            u8 v = field_0x25c;
            if (v < mRail.field_0x2) {
                mRail.field_0x3 = v;
            }
        }
        break;
    case 5:
        mVel_0x3c = negated(mVel_0x3c);
        mVel_0x3c.z = 1.0f;
        getGameView_802D6A9C()->fn_802D79E8(mLayer_0x4, 4.0f, 0.0f, false);
        if (field_0x374 != NULL) {
            field_0x374->stopCreateParticle();
        }
        break;
    case 6:
        field_0x264 = 0.0f;
        field_0x244 = 0;
        break;
    case 7:
        field_0x248 = 0;
        break;
    case 8:
        field_0x23c = field_0xc8;
        field_0x264 = 0.0f;
        mVel_0x3c.set(0.0f, 0.0f, 0.0f);
        break;
    case 10:
        mVel_0x3c.x *= -0.1f;
        mVel_0x3c.y *= -0.1f;
        mVel_0x3c.z = 0.0f;
        mHitTimer_0xd8 = 0;
        mSprite.startAnim_801D22DC(1, 0, false);
        if (mLayer_0x4 == 0) {
            field_0x378 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x93, 0, -1, NULL, NULL, true, field_0x260);
            field_0x37c = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x94, 1, 0, NULL, NULL, true, field_0x260);
        }
        break;
    case 12: {
        deleteEmitter(&mEmitter100);
        deleteEmitter(&mEmitter104);
        if (field_0x374 != NULL) {
            field_0x374->stopCreateParticle();
        }
        spawnBurst();
        startSound_801F0E34(0x50077, 0);
        field_0x270 = field_0x260;
        break;
    }
    case 13:
        mVel_0x3c.set(0.0f, 0.0f, 0.0f);
        mPos.z = 0.0f;
        switch (field_0x234) {
        case 1:
            playSe_801F0D20(3);
            getActorManager_801F856C()->spawnActor_801F9484('KEY0', mLayer_0x4, &mPos, 0x60000000, -1, -1);
            break;
        case 0:
        default: {
            dropRupee();
            break;
        }
        }
        markDead_801F3668();
        break;
    }
    field_0x238 = state;
    mSprite.setFrame_801D1F14(0.0f);
    return true;
}

// Plain functions that update and fn_8010C294 inline; the linker dropped their own copies. The pool has
// their literals before fn_8010C294's, in this order from the bottom of the file up.

// Turns towards the player, picks the speed by the distance and rolls on.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 26-line definition): split out only where the .sdata2 order needs it.
void Snowball_8049D020::chase(s32 target) {
    u16 angle = 0;
    s32 dir = 0;
    s32 player = 0;
    f32 dist = 0.0f;
    findTargetPlayer_801EEC98(mSerial, &angle, &dir, &dist, &player, target, 2);
    s32 step;
    if (JMath::sincosTable_.cosShort((s16)(field_0x230 - angle)) >= 0.0f) {
        step = (s16)((s32)fabs(300.0f * JMath::sincosTable_.sinShort((s16)(field_0x230 - angle))) + 100);
    } else {
        step = 700;
    }
    stepAngle_801CD930(&field_0x230, (s16)angle, step);
    if (dist >= 64.0f) {
        moveToward_801CD950(&field_0x264, 1.6f, 0.08f);
    } else {
        moveToward_801CD950(&field_0x264, 1.2f, 0.03f);
    }
    pool_8053C490();
    field_0x26c = PSVECMag(&mVel_0x3c) / 1.6f;
    if (field_0x26c >= 1.0f) {
        field_0x26c = 1.0f;
    }
    pool_8053C494();
    startSound_801F0E34(0x3007B, 0);
}

// Moves along the rail and updates the trail emitter.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 13-line definition): split out only where the .sdata2 order needs it.
void Snowball_8049D020::followRail() {
    u16 angle = 0;
    JGeometry::TVec3f outStep(0.0f, 0.0f, 0.0f);
    s32 layer = mLayer_0x4;
    JGeometry::TVec3f pos = mPos;
    mRail.stepAlongRail_800FDEA0(&pos, 1.5f, &outStep, &layer, &angle, 0x200);
    mVel_0x3c.x = outStep.x;
    mVel_0x3c.y = outStep.y;
    mVel_0x3c.z = outStep.z;
    mPos += mVel_0x3c;
    pool_8053C494();
    startSound_801F0E34(0x3007B, 0);
}

// Grows with the distance rolled, up to twice the size; the boxes follow.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 19-line definition): split out only where the .sdata2 order needs it.
void Snowball_8049D020::grow() {
    field_0x260 += (0.002f * field_0x264) / 1.6f;
    if (field_0x260 >= 2.0f) {
        field_0x260 = 2.0f;
    }
    f32 boxHalf = 8.0f * field_0x260;
    mBox.i.x = -boxHalf;
    mBox.i.y = -boxHalf;
    mBox.f.x = boxHalf;
    mBox.f.y = boxHalf;
    mBox2.i.x = mBox.i.x;
    mBox2.i.y = mBox.i.y;
    mBox2.f.x = mBox.f.x;
    mBox2.f.y = mBox.f.y;
    mMapBox_0x80.i.x = mBox.i.x;
    mMapBox_0x80.i.y = mBox.i.y;
    mMapBox_0x80.f.x = mBox.f.x;
    mMapBox_0x80.f.y = mBox.f.y;
}

// Applies gravity to the velocity and moves; true once landed.
// R2 pool helper (inlined at 3 call sites); was fallStep.
bool Snowball_8049D020::pool_8053C4AC() {
    moveToward_801CD950(&mVel_0x3c.z, -10.0f, -0.15f);
    mPos += mVel_0x3c;
    bool atGround;
    if (mPos.z <= 0.0f) {
        mPos.z = 0.0f;
        mVel_0x3c.z = 0.0f;
        atGround = true;
    } else {
        atGround = false;
    }
    return atGround;
}

// The rupee a destroyed snowball leaves, by its size.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 15-line definition): split out only where the .sdata2 order needs it.
void Snowball_8049D020::dropRupee() {
    s32 kind = 0;
    if (field_0x270 >= 2.0f) {
        kind = 2;
    } else if (field_0x270 >= 1.4f) {
        kind = 1;
    }
    if (field_0x258 >= 1) {
        kind = 1;
    }
    s32 serial = getActorManager_801F856C()->spawnActorSimple_801F8544('RUPY', mLayer_0x4, &mPos, lbl_80464328[kind]);
    if (serial >= 0) {
        getActorManager_801F856C()->findActorBySerial_801F666C(serial);
    }
}

// The three burst emitters, at the snowball's foot.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 7-line definition): split out only where the .sdata2 order needs it.
void Snowball_8049D020::spawnBurst() {
    JGeometry::TVec3f pos = mPos;
    pos.y -= 16.0f * field_0x260;
    spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x44A, 0, 1, NULL, NULL, true, field_0x260);
    spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x44B, 0, 1, NULL, NULL, true, field_0x260);
    spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x44C, 0, 1, NULL, NULL, true, field_0x260);
}

// Emits and updates the trail emitter.
// R2 pool helper (inlined at 3 call sites); was updateTrailEmitter.
void Snowball_8049D020::pool_8053C494() {
    if (mLayer_0x4 == 0) {
        if (field_0x374 == NULL) {
            field_0x374 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3D3, 0, 0, NULL, NULL, true, 1.5f);
        }
        JGeometry::TVec3f emitterPos = mPos;
        emitterPos.y = mPos.y + -2.0f * field_0x260;
        setEmitterPos_8013CB44(field_0x374, &emitterPos, 0, 0, true);
        field_0x374->setGlobalAlpha(0x80);
        if (field_0x264 < 0.2f) {
            field_0x374->stopCreateParticle();
        } else {
            field_0x374->playCreateParticle();
        }
    }
}

// Moves along the heading at the current speed.
// R2 pool helper (inlined at 2 call sites); was advance.
void Snowball_8049D020::pool_8053C490() {
    mVel_0x3c.x = field_0x264 * JMath::sincosTable_.sinShort(field_0x230);
    mVel_0x3c.y = field_0x264 * JMath::sincosTable_.cosShort(field_0x230);
    mVel_0x3c.z = 0.0f;
    mPos += mVel_0x3c;
}
