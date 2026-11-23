#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "game/act/act_bee_80343A70.h"

// 0x803450AC.
// FIXME(shotgun): 98.45 %, scheduling+regalloc; behaviour clean.
void Bee_804AC600::init() {
    field_0x270 = mSpawnParam_0x90 & 1;
    mLife = 1;
    for (s32 i = 0; i < 7; i++) {
        field_0x2a4[i].set(0.0f, 0.0f, 0.0f);
        field_0x2f8[i].set(0.0f, 0.0f, 0.0f);
        field_0x34c[i].set(0.0f, 0.0f, 0.0f);
        field_0x278[i] = 2.0f + gGameRoot_8053AAF8->mRandom.get_ufloat_1();
        field_0x254[i] = 0;
        field_0x294[i] = 0;
    }
    field_0x278[0] = 1.5f + 0.5f * gGameRoot_8053AAF8->mRandom.get_ufloat_1();
    field_0x2a4[0] = mPos;
    field_0x238 = 0;
    field_0x23c = 0;
    field_0x240 = 0;
    field_0x244 = -1;
    field_0x24c = -1;
    field_0x250 = 0;
    field_0x2a2 = 0;
    field_0x248 = -1;
    field_0x274 = 0;
    field_0x234 = 0;
    mBox.set(-4.0f, -4.0f, 4.0f, 4.0f);
    field_0x230 = 0;
    field_0x194 = 1;
}

// The six trailing pieces fly off the head in a ring (update, states 0).
static inline void spreadPieces(Bee_804AC600* self) {
    s32 shift = 0x10000;
    for (s32 i = 1; i < 7; i++) {
        self->field_0x2f8[i] = self->field_0x2f8[0];
        self->field_0x294[i] = shift / 6;
        shift += 0x10000;
        self->field_0x2f8[i].x += 3.0f * JMath::sincosTable_.sinShort(self->field_0x294[i]);
        self->field_0x2f8[i].y += 3.0f * JMath::sincosTable_.cosShort(self->field_0x294[i]);
    }
}

// The six trailing pieces pick a new heading every tenth frame of field_0x238 and
// move at their own speed (update, states 2-5).
static inline void steerPieces(Bee_804AC600* self) {
    for (s32 i = 1; i < 7; i++) {
        if (self->field_0x238 % 10 == 0) {
            self->field_0x294[i] = (s32)(65536.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
            f32 x;
            f32 y;
            if (self->field_0x230 == 5 || (self->field_0x248 >= 0 && self->field_0x244 < 0)) {
                x = -self->field_0x2a4[i].x;
                y = -self->field_0x2a4[i].y;
            } else {
                x = 12.0f * JMath::sincosTable_.sinShort(self->field_0x294[i]) + -self->field_0x2a4[i].x;
                y = 12.0f * JMath::sincosTable_.cosShort(self->field_0x294[i]) + -self->field_0x2a4[i].y;
            }
            self->field_0x294[i] = (s32)(JMath::atanTable_.atan2_(x, y) * 10430.378f);
        }
        self->field_0x34c[i].x = self->field_0x278[i] * JMath::sincosTable_.sinShort(self->field_0x294[i]);
        self->field_0x34c[i].y = self->field_0x278[i] * JMath::sincosTable_.cosShort(self->field_0x294[i]);
    }
}

// Serial of the nearest live bomb ('HBMB') within 1024 units that is not in spawn state 1, 2 or 4,
// else -1 (update, states 2 and 3). The original's loop counter copy (mr) shows the search was inlined.
static inline s32 findNearestBomb(Bee_804AC600* self) {
    f32 best = 1024.0f;
    s32 found = -1;
    s32 count = getActorManager_801F856C()->mActorEnd;
    for (s32 i = 0; i < count; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->mActors[i];
        if (actor != NULL && actor->mActorId == ACTOR_HBMB && actor->mAlive == 1 && actor->mSpawnParam_0x90 != 1 &&
            actor->mSpawnParam_0x90 != 2 && actor->mSpawnParam_0x90 != 4) {
            f32 d = PSVECDistance(&self->mPos, &actor->mPos);
            if (d < best) {
                found = actor->mSerial;
                best = d;
            }
        }
    }
    return found;
}

// 0x80343D90.
// FIXME(shotgun): 99.01 %, scheduling+regalloc; behaviour clean.
void Bee_804AC600::update() {
    JGeometry::TVec3f diff;
    diff.x = diff.y = diff.z = 0.0f;
    if (field_0x230 != 0) {
        if (field_0x230 != 6 && field_0x230 != 1) {
            if (updateHitReaction_801F3D94(1) == 0) {
                bool handled = false;
                s32 serial = findOverlappingActor_801F2718('HBMB');
                if (serial >= 0) {
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL && actor->field_0x94 != 0) {
                        JGeometry::TVec3f from = mPos;
                        from -= actor->mVel_0x3c;
                        handled = true;
                        field_0x274 = 1;
                        takeHit_801F37AC(actor->mParamAtcn_0x118, &from, 3, actor->field_0x168, actor->field_0x110);
                    }
                }
                if (!handled) {
                    s32 serial2 = getActorManager_801F856C()->findOverlappingActor_801F82CC(this, field_0x195, 32.0f);
                    if (serial2 >= 0) {
                        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial2);
                        if (actor != NULL && actor->field_0x168 == 5) {
                            JGeometry::TVec3f from = mPos;
                            from -= actor->mVel_0x3c;
                            handled = true;
                            field_0x274 = 2;
                            takeHit_801F37AC(actor->mParamAtcn_0x118, &from, 3, actor->field_0x168, actor->field_0x110);
                        }
                    }
                }
                if (!handled) {
                    s32 player = findOverlappingPlayer_801F285C(NULL, NULL, true, -1);
                    if (player >= 0) {
                        field_0x24c = player;
                        field_0x238 = 10;
                        mLife = 1;
                        diff.x = field_0x2a4[0].x - getPlayerPos_8023E724(field_0x24c)->x;
                        diff.y = field_0x2a4[0].y - getPlayerPos_8023E724(field_0x24c)->y;
                        field_0x2a2 = (s32)(JMath::atanTable_.atan2_(diff.x, diff.y) * 10430.378f);
                        field_0x294[0] = field_0x2a2;
                        field_0x34c[0].x = 2.0f * JMath::sincosTable_.sinShort(field_0x294[0]);
                        field_0x34c[0].y = 2.0f * JMath::sincosTable_.cosShort(field_0x294[0]);
                        field_0x2f8[0] = field_0x34c[0];
                        field_0x230 = 4;
                    }
                }
            } else {
                field_0x2a4[0] = mPos;
                if (mLife <= 0) {
                    startSound_801F0E34(0x4007E, 0);
                    return;
                }
            }
        }
        startSound_801F0E34(0x4007E, 0);
        addCalc2_801F71A4(&field_0x2f8[0].x, field_0x34c[0].x, 0.1f, 0.3f);
        addCalc2_801F71A4(&field_0x2f8[0].y, field_0x34c[0].y, 0.1f, 0.3f);
        field_0x2a4[0] += field_0x2f8[0];
        for (s32 i = 1; i < 7; i++) {
            addCalc2_801F71A4(&field_0x2f8[i].x, field_0x34c[i].x, 0.1f, 0.3f);
            addCalc2_801F71A4(&field_0x2f8[i].y, field_0x34c[i].y, 0.1f, 0.3f);
            field_0x2a4[i] += field_0x2f8[i];
        }
        addCalcAngle2_801F714C(field_0x294, field_0x2a2, 4, 0x400);
        mPos = field_0x2a4[0];
        if (field_0x230 != 4 && field_0x230 - 5 > 1U) {
            if (damagePlayersInBox_801F2B7C(1, -1, 1)) {
                field_0x250++;
                if (field_0x250 >= 5) {
                    gGameRoot_8053AAF8->mRandom.get_ufloat_1();
                    field_0x34c[0].x = 2.0f * JMath::sincosTable_.sinShort(field_0x2a2);
                    field_0x34c[0].y = 2.0f * JMath::sincosTable_.cosShort(field_0x2a2);
                    for (s32 i = 1; i < 7; i++) {
                        field_0x34c[i].x = 0.0f;
                        field_0x34c[i].y = 0.0f;
                    }
                    field_0x230 = 6;
                    return;
                }
                field_0x238 = 0x78;
                field_0x2f8[0].zero();
                field_0x34c[0].zero();
                field_0x24c = -1;
                field_0x244 = field_0x24c;
                field_0x230 = 5;
            }
        }
    }
    switch (field_0x230) {
    case 0:
        switch (field_0x270) {
        case 0:
            for (s32 p = 0; p < 4; p++) {
                if (fn_8023A51C(p)) {
                    diff.x = getPlayerPos_8023E724(p)->x - mPos.x;
                    diff.y = getPlayerPos_8023E724(p)->y - mPos.y;
                    if (!(diff.x * diff.x + diff.y * diff.y > 6400.0f)) {
                        field_0x238 = 60;
                        spreadPieces(this);
                        field_0x230 = 1;
                        return;
                    }
                }
            }
            break;
        case 1:
            for (s32 p = 0; p < 4; p++) {
                diff.x = getPlayerPos_8023E724(p)->x - mPos.x;
                diff.y = getPlayerPos_8023E724(p)->y - mPos.y;
                if (!(diff.x * diff.x + diff.y * diff.y > 4096.0f)) {
                    field_0x238 = 120;
                    spreadPieces(this);
                    field_0x230 = 1;
                    return;
                }
            }
            break;
        }
        break;
    case 1:
        if (field_0x238 <= 0) {
            field_0x238 = 30;
            field_0x230 = 2;
            return;
        }
        break;
    case 2:
    case 3:
        if (field_0x238 <= 0) {
            field_0x248 = findNearestBomb(this);
            s32 serial = field_0x248;
            if (serial >= 0) {
                ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (actor != NULL) {
                    JGeometry::TVec3f target = actor->mPos;
                    diff.x = target.x - mPos.x;
                    diff.y = target.y - mPos.y;
                    steerPieces(this);
                    if (PSVECMag(&diff) > 16.0f) {
                        field_0x2a2 = (s32)(JMath::atanTable_.atan2_(diff.x, diff.y) * 10430.378f);
                        field_0x34c[0].x = 2.0f * JMath::sincosTable_.sinShort(field_0x294[0]);
                        field_0x34c[0].y = 2.0f * JMath::sincosTable_.cosShort(field_0x294[0]);
                        field_0x238 = 5;
                    } else {
                        diff.scale(0.125f);
                        field_0x34c[0] = diff;
                        field_0x238 = 10;
                    }
                    if (target.x > field_0x2a4[0].x) {
                        field_0x254[0] = 0;
                    } else {
                        field_0x254[0] = 2;
                    }
                    for (s32 i = 1; i < 7; i++) {
                        if (target.x > field_0x2a4[i].x) {
                            field_0x254[i] = 0;
                        } else {
                            field_0x254[i] = 2;
                        }
                    }
                }
            } else {
                if (field_0x230 == 2) {
                    findTargetPlayer_801EEC98(mSerial, NULL, NULL, NULL, &field_0x244, 0, 0);
                } else {
                    findTargetPlayer_801EEC98(mSerial, NULL, NULL, NULL, &field_0x244, field_0x24c, 2);
                }
                if (field_0x244 >= 0) {
                    diff.x = getPlayerPos_8023E724(field_0x244)->x - mPos.x;
                    diff.y = getPlayerPos_8023E724(field_0x244)->y - mPos.y;
                    field_0x2a2 = (s32)(JMath::atanTable_.atan2_(diff.x, diff.y) * 10430.378f);
                }
                steerPieces(this);
            }
        }
        if (field_0x248 < 0) {
            if (field_0x244 >= 0) {
                field_0x34c[0].x = 2.0f * JMath::sincosTable_.sinShort(field_0x294[0]);
                field_0x34c[0].y = 2.0f * JMath::sincosTable_.cosShort(field_0x294[0]);
                if (getPlayerPos_8023E724(field_0x244)->x > field_0x2a4[0].x) {
                    field_0x254[0] = 0;
                } else {
                    field_0x254[0] = 2;
                }
                for (s32 i = 1; i < 7; i++) {
                    if (getPlayerPos_8023E724(field_0x244)->x > field_0x2a4[i].x) {
                        field_0x254[i] = 0;
                    } else {
                        field_0x254[i] = 2;
                    }
                }
            } else {
                field_0x34c[0].x = 0.0f;
                field_0x34c[0].y = 0.0f;
            }
            if (field_0x230 == 3 && field_0x244 != field_0x24c) {
                field_0x230 = 2;
                field_0x238 = 1;
                field_0x24c = -1;
                field_0x244 = field_0x24c;
            }
        }
        if (field_0x238 <= 0) {
            field_0x238 = 10;
        }
        break;
    case 5:
        if (field_0x238 <= 0) {
            field_0x238 = 0;
            field_0x230 = 2;
            return;
        }
        steerPieces(this);
        break;
    case 4:
        if (field_0x238 <= 0) {
            field_0x238 = 0;
            field_0x230 = 3;
            return;
        }
        steerPieces(this);
        break;
    case 6:
        if (isOutsideView_801F6758(&mPos, mLayer_0x4, 16.0f)) {
            bool gone = true;
            for (s32 i = 1; i < 7; i++) {
                if (!isOutsideView_801F6758(&field_0x2a4[i], mLayer_0x4, 16.0f)) {
                    gone = false;
                    break;
                }
            }
            if (gone) {
                mAlive = false;
            }
        }
        break;
    }
    field_0x23c--;
    if (field_0x23c <= 0) {
        field_0x240++;
        field_0x240 &= 1;
        field_0x23c = 2;
    }
    if (field_0x238 > 0) {
        field_0x238--;
    }
}

// 0x80343B48.
// FIXME(shotgun): 97.66 %, scheduling+regalloc; behaviour clean.
void Bee_804AC600::draw() {
    SpriteParams_801F5AE0 params;
    params.field_0x1d = -1;
    params.field_0x1c = 1;
    if (field_0x274 == 2) {
        params.field_0x20 = 0;
    }
    if (field_0x230 != 0) {
        for (s32 i = 0; i < 7; i++) {
            if (i > 0) {
                mPos = field_0x2a4[i] + field_0x2a4[0];
            }
            if (mPos.z <= 0.0f) {
                mPos.z = 0.1f;
            }
            drawSprite_801F06F0("enemy_both2_sob.bin", field_0x240 + field_0x254[0] + 0x1CC, &params, 1, 1.0f, 1.0f);
        }
        mPos = field_0x2a4[0];
        if (field_0x274 == 2 && mPos.z <= 0.0f) {
            mPos.z = 0.1f;
        }
    }
}

// 0x80343AD0.
void Bee_804AC600::die_0x24() {
    s32 kind;
    if (field_0x274 == 1) {
        kind = 3;
    } else if (field_0x274 == 2) {
        kind = 1;
    }
    getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &mPos, kind | 0x261E00);
    ActorBase_804A5770::die_0x24();
}

// Destructor at 0x80343A70.
Bee_804AC600::~Bee_804AC600() {}
