#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/Player.h"
#include "game/cls_804B1E1C.h"
#include "JSystem/JMath/JMath.h"
#include "game/act/act_score.h"

// FIXME(shotgun): 98.99 %, scheduling; behaviour clean.
void Score_804AF4C0::init() {
    mValue = mSpawnParam_0x90 & 0xFFFF;
    mPlayer = (mSpawnParam_0x90 >> 16) & 0xFF;
    mState = mSpawnParam_0x90 >> 24;
    field_0x248 = mPos.x - getPlayerPos_8023E724(mPlayer)->x;
    field_0x24c = mPos.y - getPlayerPos_8023E724(mPlayer)->y - 24.0f;
    field_0x240 = mValue;
    field_0x244 = lbl_80529DEC.field_0x1f4[mPlayer];
    if (mState == 0 || mState == 4) {
        if (gGameRoot_8053AAF8->field_0x7f == 0 || fn_802DE814()) {
            mAlive = false;
        }
        if (mValue == 0) {
            mAlive = false;
        } else if (mValue >= 100) {
            mValue = 100;
        }
        if (mState == 0) {
            mVel_0x3c.x = 0.0f;
            mVel_0x3c.y = -0.2f;
            mVel_0x3c.z = 0.0f;
        } else {
            mVel_0x3c.x = 0.0f;
            mVel_0x3c.y = 0.0f;
            mVel_0x3c.z = 0.0f;
        }
        mTimer = 90;
        lbl_80529DEC.field_0x1f4[mPlayer] = field_0x240;
    } else if (mState == 1) {
        mVel_0x3c.x = 0.0f;
        mVel_0x3c.y = -1.0f;
        mVel_0x3c.z = 0.0f;
        mTimer = 90;
        playSe_801F0D20(0x41);
    } else if (mState == 2) {
        if (mValue >= 12) {
            mAlive = false;
        }
        mVel_0x3c.x = 0.0f;
        mVel_0x3c.y = -0.2f;
        mVel_0x3c.z = 0.0f;
        mTimer = 90;
    } else if (mState == 3) {
        mTimer = 0x257;
        mPos.set(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y - 32.0f,
                 getPlayerPos_8023E724(mPlayer)->z);
    } else if (mState == 5) {
        mTimer = 0x1a3;
        mPos.set(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y - 32.0f,
                 getPlayerPos_8023E724(mPlayer)->z);
    } else if (mState == 6) {
        mTimer = 0x12b;
        mPos.set(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y - 32.0f,
                 getPlayerPos_8023E724(mPlayer)->z);
    } else if (mState == 7) {
        mTimer = 0xb3;
        mPos.set(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y - 32.0f,
                 getPlayerPos_8023E724(mPlayer)->z);
    }
}

void Score_804AF4C0::update() {
    if (mState == 0 || mState == 4) {
        fn_80393514();
    } else if (mState == 1) {
        updateState1();
    } else if (mState == 2) {
        updateState2();
    } else if (mState == 3 || mState == 5 || mState == 6 || mState == 7) {
        mPos.set(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y - 32.0f,
                 getPlayerPos_8023E724(mPlayer)->z);
        if (!fn_80230188(mPlayer) && fn_8023BC38(mPlayer) <= 0) {
            mAlive = false;
        }
    }
}

void Score_804AF4C0::draw() {
    if (mState == 0 || mState == 4) {
        fn_80393228();
    } else if (mState == 1) {
    } else if (mState == 2) {
        fn_80391F58();
    } else if (mState == 3 || mState == 5 || mState == 6 || mState == 7) {
        if (!fn_80230188(mPlayer)) {
            fn_80391D44();
        }
    }
}

// Per-player digit colours.
static const u8 lbl_80543180[4] = {0x28, 0xFF, 0x32, 0xBE};
static const u8 lbl_8054317C[4] = {0xFF, 0x50, 0xC8, 0x78};
static const u8 lbl_80543184[4] = {0x28, 0x50, 0xFF, 0xFF};
static const u8 lbl_80543170[4] = {0x28, 0xFF, 0x32, 0xBE};
static const u8 lbl_80543174[4] = {0xFF, 0x50, 0xC8, 0x78};
static const u8 lbl_80543178[4] = {0x28, 0x50, 0xFF, 0xFF};

// Not auto-inlined: draw and update call these out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// Force Gem / fairy sounds by tens digit.
static const u32 sScoreSounds[10] = {0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C};

// FIXME(shotgun): 99.69 %, operands; behaviour drift.
void Score_804AF4C0::fn_80393514() {
    if (mTimer == 90) {
        // The original copies this 12-entry table (rodata 0x80475738) to the stack and passes entry 0.
        const f32 emitScale[12] = {0.3f, 0.3f, 0.3f, 0.35f, 0.35f, 0.4f, 0.45f, 0.5f, 0.55f, 0.6f, 0.6f, 0.6f};
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xCB, 0, 0, NULL, NULL, true, emitScale[0]);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xCC, 0, 0, NULL, NULL, true, emitScale[0]);
        s32 idx = 9;
        if (mValue < 10) {
            idx = mValue - 1;
        }
        gAudioMgr_8053AF80->playSystemSound_80458880(sScoreSounds[idx], -1);
    }
    s16 angle = (mTimer % 60) << 10;
    if (mState == 0) {
        mVel_0x3c.x = 0.2f * JMASSin(angle);
        mPos += mVel_0x3c;
    } else {
        field_0x248 += 0.2f * JMASSin(angle);
        field_0x24c -= 0.2f;
        mPos.x = getPlayerPos_8023E724(mPlayer)->x + field_0x248;
        mPos.y = getPlayerPos_8023E724(mPlayer)->y + field_0x24c;
    }
    if (mTimer-- == 0) {
        s32 tensNew = field_0x240 / 10;
        s32 tensOld = field_0x244 / 10;
        s32 kind = tensOld + 1;
        if (tensNew > tensOld) {
            s32 n = tensNew - tensOld;
            for (s32 i = 0; i < n; i++, kind++) {
                s32 serial = -1;
                switch (kind) {
                case 1:
                case 2: {
                    JGeometry::TVec3f p(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y, 300.0f);
                    serial = getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &p, 2);
                    break;
                }
                case 3:
                case 4:
                case 5: {
                    JGeometry::TVec3f p(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y, 300.0f);
                    serial = getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &p, 3);
                    break;
                }
                default: {
                    JGeometry::TVec3f* pos = getPlayerPos_8023E724(mPlayer);
                    getActorManager_801F856C()->spawnActor_801F9484('FARY', mLayer_0x4, pos, 0, -1, -1);
                }
                }
                if (serial >= 0) {
                    ActorBase_804A5770* gem = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (gem != NULL) {
                        gem->mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, -1.0f);
                    }
                }
            }
        }
        if (field_0x240 >= 15 && field_0x244 < 15) {
            JGeometry::TVec3f p(getPlayerPos_8023E724(mPlayer)->x, getPlayerPos_8023E724(mPlayer)->y, 300.0f);
            s32 serial = getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &p, 2);
            if (serial >= 0) {
                ActorBase_804A5770* gem = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (gem != NULL) {
                    gem->mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, -1.0f);
                }
            }
        }
        mAlive = false;
    }
}

// Queues one digit sprite at a position passed by value (the original copies
// the Vec, then builds the TVec3f from the copy).
static inline void putTimeDigit(f32 scale, s32 digit, JGeometry::TVec3f v, JUtility::TColor color) {
    JGeometry::TVec3f pos(v);
    lbl_8053AF50->fn_80455770(scale, digit, &pos, color);
}

void Score_804AF4C0::fn_80393228() {
    f32 fade = 255.0f * ((f32)mTimer / 90.0f);
    if (mValue < 10) {
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, mValue, mPos,
                         JUtility::TColor(lbl_80543170[mPlayer], lbl_80543174[mPlayer], lbl_80543178[mPlayer], fade));
        }
    } else if (mValue < 100) {
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, mValue / 10, JGeometry::TVec3f(mPos.x - 8.333f, mPos.y, mPos.z),
                         JUtility::TColor(lbl_80543170[mPlayer], lbl_80543174[mPlayer], lbl_80543178[mPlayer], fade));
        }
        fade = 255.0f * ((f32)mTimer / 90.0f);
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, mValue % 10, JGeometry::TVec3f(8.333f + mPos.x, mPos.y, mPos.z),
                         JUtility::TColor(lbl_80543170[mPlayer], lbl_80543174[mPlayer], lbl_80543178[mPlayer], fade));
        }
    } else {
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, 10, mPos,
                         JUtility::TColor(lbl_80543170[mPlayer], lbl_80543174[mPlayer], lbl_80543178[mPlayer], fade));
        }
    }
}
#pragma auto_inline reset

// Auto-inlined into update(); placed here to match the original .sdata2 literal pool order.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 10-line definition): split out only where the .sdata2 order needs it.
void Score_804AF4C0::updateState1() {
    if (mTimer >= 0x50) {
        s32 d = 90 - mTimer;
        mVel_0x3c.y = -3.0f * (1.0f - (f32)(d * d / 100));
        mPos.add(mVel_0x3c);
    }
    if (mTimer-- == 0) {
        mAlive = false;
    }
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 7-line definition): split out only where the .sdata2 order needs it.
void Score_804AF4C0::updateState2() {
    mVel_0x3c.x = 0.2f * JMath::sincosTable_.sinShort((mTimer % 60) << 10);
    mPos.add(mVel_0x3c);
    if (mTimer-- == 0) {
        mAlive = false;
    }
}

#pragma auto_inline off
// Draws one digit of the gem value at x, in the gem-yellow colour.
static void pool_80543188(const Score_804AF4C0* self, f32 fade, f32 x, u8 digit);

// Digits are drawn left to right; the middle digit of a 3-digit value is drawn at mPos itself.
static inline void putScoreDigitMid(const Score_804AF4C0* self, f32 fade, u8 digit) {
    if (self->mLayer_0x4 < 8) {
        JGeometry::TVec3f pos = JGeometry::TVec3f(self->mPos);
        lbl_8053AF50->fn_80455770(0.3f, digit, &pos, JUtility::TColor(255, 0xD2, 0, (s32)fade));
    }
}

#define SCORE_2(a, b) \
    pool_80543188(this, fade, mPos.x - 5.0f, a); \
    pool_80543188(this, fade, mPos.x + 5.0f, b)
#define SCORE_3(a, b, c) \
    pool_80543188(this, fade, mPos.x - 10.0f, a); \
    putScoreDigitMid(this, fade, b); \
    pool_80543188(this, fade, mPos.x + 10.0f, c)
#define SCORE_4(a, b, c, d) \
    pool_80543188(this, fade, mPos.x - 5.0f - 10.0f, a); \
    pool_80543188(this, fade, mPos.x - 5.0f, b); \
    pool_80543188(this, fade, mPos.x + 5.0f, c); \
    pool_80543188(this, fade, 10.0f + (mPos.x + 5.0f), d)

// FIXME: 96.72 %, operands; behaviour drift.
void Score_804AF4C0::fn_80391F58() {
    f32 fade = 255.0f * ((f32)mTimer / 90.0f);
    switch (mValue) {
    case 0: putScoreDigitMid(this, fade, 1); break;
    case 1: putScoreDigitMid(this, fade, 5); break;
    case 2: SCORE_2(2, 0); break;
    case 3: SCORE_2(5, 0); break;
    case 4: SCORE_3(1, 0, 0); break;
    case 5: SCORE_3(1, 5, 0); break;
    case 6: SCORE_3(2, 0, 0); break;
    case 7: SCORE_3(3, 0, 0); break;
    case 8: SCORE_4(1, 0, 0, 0); break;
    case 9: SCORE_4(1, 5, 0, 0); break;
    case 10: SCORE_4(2, 0, 0, 0); break;
    case 11: SCORE_4(3, 0, 0, 0); break;
    }
}

void Score_804AF4C0::fn_80391D44() {
    s32 n = fn_8023BC38(mPlayer) / 60 + 1;
    JGeometry::TVec3f v;
    if (n < 10) {
        v.x = mPos.x;
        v.y = mPos.y;
        v.z = mPos.z;
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, n, v,
                         JUtility::TColor(lbl_8054317C[mPlayer], lbl_80543180[mPlayer], lbl_80543184[mPlayer], 255));
        }
    } else {
        v.x = mPos.x;
        v.y = mPos.y;
        v.z = mPos.z;
        v.x -= 10.0f;
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, 1, v,
                         JUtility::TColor(lbl_8054317C[mPlayer], lbl_80543180[mPlayer], lbl_80543184[mPlayer], 255));
        }
        v.x = mPos.x;
        v.y = mPos.y;
        v.z = mPos.z;
        v.x += 10.0f;
        if (mLayer_0x4 < 8) {
            putTimeDigit(0.5f, 0, v,
                         JUtility::TColor(lbl_8054317C[mPlayer], lbl_80543180[mPlayer], lbl_80543184[mPlayer], 255));
        }
    }
}
#pragma auto_inline reset

// R2 pool helper (inlined at 8 call sites); was putScoreDigit.
static void pool_80543188(const Score_804AF4C0* self, f32 fade, f32 x, u8 digit) {
    Vec v;
    v.x = x;
    v.y = self->mPos.y;
    v.z = self->mPos.z;
    if (self->mLayer_0x4 < 8) {
        JGeometry::TVec3f pos = JGeometry::TVec3f(v);
        lbl_8053AF50->fn_80455770(0.3f, digit, &pos, JUtility::TColor(255, 0xD2, 0, (s32)fade));
    }
}

Score_804AF4C0::~Score_804AF4C0() {}
