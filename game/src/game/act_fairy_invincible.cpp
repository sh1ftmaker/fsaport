#include "game/GameAudio.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "game/act/act_fairy_invincible.h"

// Not in the binary: a plain function that draw inlines (defined at the bottom of the file).
void setUniformScaleMtx(Mtx m, f32 scale);

// Moves both emitters to the fairy.
inline void InvincibilityFairy_804AB4C0::followEmitters() {
    if (mEmitter264 != NULL) {
        mEmitter264->setGlobalTranslation(mPos);
    }
    if (mEmitter268 != NULL) {
        mEmitter268->setGlobalTranslation(mPos);
    }
}

// Stops both emitters from making particles and forgets them.
inline void InvincibilityFairy_804AB4C0::releaseEmitters() {
    if (mEmitter264 != NULL) {
        mEmitter264->becomeInvalidEmitter();
        mEmitter264 = NULL;
    }
    if (mEmitter268 != NULL) {
        mEmitter268->becomeInvalidEmitter();
        mEmitter268 = NULL;
    }
}

// The target player and the counters of the state 0 search, zeroed.
inline void InvincibilityFairy_804AB4C0::clearTracking() {
    field_0x244 = 0;
    field_0x248 = 0;
    field_0x24c = 0;
}

// Zeroes the timers, counters and targets, and sets the life; init() then
// sets the values it starts from over them.
inline void InvincibilityFairy_804AB4C0::clearWork() {
    clearTracking();
    mCounters[0] = 0;
    mCounters[1] = 0;
    mCounters[2] = 0;
    mCounters[3] = 0;
    mLife = 8;
    field_0x250 = 0;
    field_0x258 = 0.0f;
    field_0x25c = 0.0f;
    field_0x260 = 0.0f;
    mVel_0x3c.set(0.0f, 0.0f, 0.0f);
}

InvincibilityFairy_804AB4C0::~InvincibilityFairy_804AB4C0() {
    releaseEmitter_801EE434(mEmitter264);
    releaseEmitter_801EE434(mEmitter268);
}

void InvincibilityFairy_804AB4C0::vfunc_0x44() {
    followEmitters();
}

// FIXME: 94.06 %, scheduling+regalloc; behaviour clean.
// Same .sdata2 literal numbering as update(). Register
// assignment differs: the original keeps the 0x660d multiplier in r12 and
// the 0x400 in r30, and loads the random object into r29 for each call.
void InvincibilityFairy_804AB4C0::init() {
    mBox.set(-4.0f, -4.0f, 4.0f, 4.0f);
    clearWork();
    mCounters[0] = 0x400;
    field_0x260 = 10.0f;
    mCounters[2] = 10;
    mCounters[3] = 30 + (s32)(gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 30.0f);
    field_0x248 = (s32)(gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 32768.0f);
    field_0x230 = 0;
    field_0xb0 = 0x20000;
    mEmitter264 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x183, 0, 0, NULL, NULL, true, 1.5f);
    mEmitter268 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x184, 0, 0, NULL, NULL, true, 1.5f);
}

void InvincibilityFairy_804AB4C0::update() {
    addCalc2_801F71A4(&mVel_0x3c.x, field_0x258, 0.1f, 0.5f);
    addCalc2_801F71A4(&mVel_0x3c.y, field_0x25c, 0.1f, 0.5f);
    addCalc2_801F71A4(&mPos.z, field_0x260, 0.3f, 0.5f);
    mPos += mVel_0x3c;
    followEmitters();

    switch (field_0x230) {
    case 0:
        if (mCounters[2] == 0) {
            field_0x244 += 0x300;
            field_0x260 = 2.0f * JMASinShort(field_0x244) + 7.0f;
            if (field_0x24c == 0) {
                if (mCounters[3] != 0) {
                    field_0x248 += 0x100;
                } else {
                    mCounters[3] = 30 + (s32)(gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 30.0f);
                    field_0x24c = 1;
                    field_0x258 = 0.0f;
                    field_0x25c = 0.0f;
                }
            } else if (mCounters[3] == 0) {
                field_0x248 += (s32)(gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 32768.0f);
                mCounters[3] = 40 + (s32)(gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 40.0f);
                field_0x24c = 0;
            }

            JGeometry::TBox2f box(mBox);
            box.addPos(mPos.x, mPos.y);
            s32 player = fn_8023AF14(&box, mLayer_0x4, true, 0.0f);
            if (player >= 0) {
                clearTracking();
                mCounters[0] = 0x1f4;
                field_0x244 = player;
                s32 layer = mLayer_0x4;
                if (layer >= 8) {
                    getActorManager_801F856C()->spawnActor_801F9484(ACTOR_PEFF, layer, &mPos, (player & 3) | 4, -1, -1);
                }
                mCounters[2] = 0;
                field_0x254 = 0;
                field_0x248 = 0;
                field_0xb0 = 0;
                field_0x230 = 1;
                field_0x11e = 1;
            }
        }
        break;
    case 1:
        fn_8023C830(field_0x244);
        mCounters[0] = 1000;
        field_0x248 += 1;
        if (mCounters[2] == 0) {
            f32 dx = mPos.x - getPlayerPos_8023E724(field_0x244)->x;
            f32 dy = mPos.y - getPlayerPos_8023E724(field_0x244)->y;
            field_0x254 = JMath::atanTable_.atan2_(-dx, -dy) *
                          (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180());
            mCounters[2] = 5;
        }
        field_0x258 = 5.0f * JMASinShort(field_0x254);
        field_0x25c = 5.0f * JMACosShort(field_0x254);
        if (field_0x248 > 0x2D0) {
            releaseEmitters();
            mAlive = false;
        }
        break;
    }

    if (mCounters[1] == 0) {
        mCounters[1] = 4;
        field_0x250 += 1;
        if (field_0x250 > 2) {
            field_0x250 = 0;
        }
    }
    for (s32 i = 0; i < 4; i++) {
        if (mCounters[i] > 0) {
            mCounters[i]--;
        }
        if (mCounters[0] <= 0) {
            releaseEmitters();
            mAlive = false;
        }
    }
}

void InvincibilityFairy_804AB4C0::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    setUniformScaleMtx(mtx, 1.5f);
    params.mMtx = mtx;
    if (mCounters[0] >= 0x100 || (mCounters[0] & 2) == 0) {
        params.field_0x2d = 1;
        drawSprite_801F06F0("enemy_both2_sob.bin", field_0x250 + 0x5E, &params, 0, 1.0f, 1.0f);
        showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, field_0x250 + 0x5E, false, 2, -1, 0, 0, 0);
    }
}

// A uniform scale as a 3x4 matrix (zero translation, then diagonal scale).
// Below every other function: the pool has its 0.0f ahead of draw's 1.5f.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 14-line definition): split out only where the .sdata2 order needs it.
void setUniformScaleMtx(Mtx m, f32 scale) {
    m[0][3] = 0.0f;
    m[1][3] = 0.0f;
    m[2][3] = 0.0f;
    m[0][0] = scale;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = scale;
    m[1][2] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = scale;
}
