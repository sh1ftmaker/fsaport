#include "game/GameAudio.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/act/act_enemy_for_ending_control.h"
#include "game/act/act_maiden_ending.h"

static const s32 lbl_80472718[] = { 0x17F, 0x1CD, 0x199, 0x1B3, 0x1E7, 0x201 };
static const s32 lbl_80472730[] = { 0x1D0, 0x1E2, 0x1D6, 0x1DC, 0x1E8, 0x1EE };
static const s16 sAngles[] = { (s16)0xF800, (s16)0xE400, (s16)0x8800, (s16)0xD400, (s16)0x9C00, (s16)0xAC00 };
static const u16 sEffects1[] = { 0x24, 0x28, 0x20, 0x26, 0x2A, 0x22 };
static const u16 sEffects2[] = { 0x25, 0x29, 0x21, 0x27, 0x2B, 0x23 };
static const u8 sColors[][3] = {
    { 0x4B, 0xFF, 0x2D },
    { 0xFF, 0x41, 0x37 },
    { 0x2D, 0x78, 0xFF },
    { 0xDC, 0x1E, 0xDC },
    { 0xC8, 0xA0, 0x28 },
    { 0x1E, 0xA0, 0xBE },
};
static const s32 lbl_80472780[] = { 0x32, 0x82, 0x9B, 0xAA, 0xB9, 0x64 };
static const s16 lbl_80472798[] = { (s16)0x1A00, (s16)0xFC00, (s16)0x6600, (s16)0xDE00, (s16)0x8400, (s16)0xA200 };
static const s32 lbl_804727A4[] = { 0x87, 0x78, 0x3C, 0x55, 0x69, 0x82 };

static void (MaidenEnding_804AD5B8::*lbl_804AD528[6])() = {
    &MaidenEnding_804AD5B8::fn_8035D6C4,
    &MaidenEnding_804AD5B8::fn_8035D4B8,
    &MaidenEnding_804AD5B8::fn_8035D1CC,
    &MaidenEnding_804AD5B8::fn_8035CF50,
    &MaidenEnding_804AD5B8::fn_8035CAF8,
    &MaidenEnding_804AD5B8::fn_8035C940,
};

static JGeometry::TVec3f (MaidenEnding_804AD5B8::*lbl_804AD594[3])() = {
    &MaidenEnding_804AD5B8::fn_8035CE78,
    &MaidenEnding_804AD5B8::fn_8035CDC0,
    &MaidenEnding_804AD5B8::fn_8035CD00,
};

void MaidenEnding_804AD5B8::init() {
    mState = 0;
    field_0x234 = 0;
    field_0x238 = 0;
    field_0x240 = 0;
    field_0x244 = 0;
    field_0x248 = 8;
    field_0x24c = 0;
    field_0x250 = lbl_80472718[mSpawnParam_0x90 & 7];
    field_0x254 = 1.0f;
    field_0x258 = 85.0f;
    field_0x25c = 55.0f;
    field_0x260 = sAngles[mSpawnParam_0x90 & 7];
    field_0x264 = 1280.0f;
    field_0x268 = 1795.0f;
    field_0x26c = 0.0f;
    field_0x270 = 0;
    field_0x272 = 0;
    mPos.x = field_0x264 + field_0x258 * JMASCos(field_0x260);
    mPos.y = field_0x268 + field_0x25c * JMASSin(field_0x260);
    field_0x274 = NULL;
    field_0x278 = NULL;
}

void MaidenEnding_804AD5B8::update() {
    if (field_0x234 != 0) {
        field_0x234--;
    }
    if (field_0x244 != 0) {
        field_0x244--;
    }
    (this->*lbl_804AD528[mState])();
}

// FIXME: 99.89 %, same-text; behaviour clean.
void MaidenEnding_804AD5B8::draw() {
    if (field_0x240 == 0) {
        drawSprite_801F06F0("enemy_gc_sob.bin", field_0x250 + field_0x248, NULL, 7, 1.0f, 1.0f);
    } else if (field_0x240 == 1) {
        drawSprite_801F06F0("enemy_both_sob.bin", field_0x250 + field_0x248, NULL, 7, 1.0f, 1.0f);
    } else if (field_0x240 == 2) {
        JGeometry::TVec3f savedPos = mPos;
        SpriteParams_801F5AE0 params;
        if (field_0x272) {
            mPos.y -= mPos.z;
            mPos.z = 0.0f;
            params.field_0x1d = -1;
        }
        drawSprite_801F06F0("enemy_both2_sob.bin", field_0x250 + field_0x248, &params, -1, 1.0f, 1.0f);
        mPos = savedPos;

        Mtx mtx;

        f32 scale = (512.0f - (mPos.z - 13.0f)) / 512.0f;
        mtx[0][0] = scale;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = scale;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = scale;
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;

        params.mMtx = mtx;
        params.field_0x1d = 0;
        if (mPos.z < 268.0f) {
            params.field_0x10.a = 268.0f - mPos.z;
        } else {
            params.field_0x10.a = 0;
        }
        s32 frame = field_0x250 + field_0x248;
        if (mSpriteParams.mMtx == NULL) {
            drawShadow_801EF830(mLayer_0x4, &mPos, &params, 0x407, "enemy_both2_sob.bin", frame, NULL);
        }
    }
}

MaidenEnding_804AD5B8::~MaidenEnding_804AD5B8() {
    if (field_0x274 != NULL) {
        field_0x274->becomeInvalidEmitter();
        field_0x274 = NULL;
    }
    if (field_0x278 != NULL) {
        field_0x278->becomeInvalidEmitter();
        field_0x278 = NULL;
    }
}

void MaidenEnding_804AD5B8::fn_8035D6C4() {
    if (field_0x238 != 0) {
        field_0x234 = 0x400;
        mState = 1;
        field_0x244 = 0x38;
        field_0x238 = 4;
    }
}

// FIXME: 95.68 %, missing/extra; behaviour near.
// 3 fewer instructions in field_0x248 ladder.
void MaidenEnding_804AD5B8::fn_8035D4B8() {
    // The sequencer tests below are written `== true`: the original compares the bool
    // with cmplwi 1 where a plain test gives clrlwi. (92 % instead of 95.7 % here).
    if (field_0x238 == 0) {
        if (stepSequence_8035C15C(7) == true) {
            startSequence_8035C1C4(7, lbl_80472718[mSpawnParam_0x90 & 7]);
        }
    } else if (field_0x238 == 1) {
        if (stepSequence_8035C15C(9) == true) {
            field_0x24c = 0;
            field_0x238 = 2;
        }
    } else if (field_0x238 == 2) {
        if (stepSequence_8035C15C(8) == true) {
            startSequence_8035C1C4(8, lbl_80472718[mSpawnParam_0x90 & 7]);
        }
    } else if (field_0x238 == 3) {
        if (field_0x248 < 0x14) {
            field_0x248 = 0x14;
            field_0x244 = 9;
        } else if (field_0x244 == 0) {
            if (field_0x248 == 0x14) {
                field_0x248 = 0x18;
                field_0x244 = 0x3C;
            } else if (field_0x248 == 0x18) {
                field_0x248 = 0x19;
                field_0x244 = 3;
            } else {
                field_0x248 = 8;
                field_0x24c = 0;
                mState = 2;
                field_0x234 = lbl_80472780[mSpawnParam_0x90 & 7] + 0x128;
                field_0x238 = 0;
            }
        }
    } else if (field_0x238 == 4) {
        if (field_0x244 == 0) {
            field_0x24c = randInt_801CD664(6);
            startSequence_8035C1C4(7, lbl_80472718[mSpawnParam_0x90 & 7]);
            field_0x244 = randInt_801CD664(4);
            field_0x238 = 0;
        } else if (field_0x244 == 0x28) {
            field_0x248 = 0xA;
        } else if (field_0x244 == 0xA) {
            field_0x248 = 0xB;
        }
    }
}

void MaidenEnding_804AD5B8::fn_8035D1CC() {
    if (field_0x234 > 0x128) {
        return;
    }
    if (field_0x234 == 0x128) {
        field_0x240 = 3;
        JGeometry::TVec3f savedPos = mPos;
        mPos.z += 6.0f;
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, sEffects1[mSpawnParam_0x90 & 7], 0, 0, NULL, NULL, true, 1.0f);
        mPos.z += 7.0f;
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, sEffects2[mSpawnParam_0x90 & 7], 0, 0, NULL, NULL, true, 1.0f);
        mPos = savedPos;
        startSound_801F0E34(0x400D9, 0);
    } else if (field_0x234 < 0x100) {
        if (field_0x234 == 0xFF) {
            field_0x240 = 2;
            if (field_0x274 != NULL) {
                setEmitterPos_8013CB44(field_0x274, &mPos, -1, -1, true);
            }
            if (field_0x278 != NULL) {
                setEmitterPos_8013CB44(field_0x278, &mPos, -1, -1, true);
            }
            if (stepSequence_8035C15C(10) == true) {
                startSequence_8035C1C4(10, lbl_80472730[mSpawnParam_0x90 & 7]);
            }
            mPos.z = 13.0f;
            field_0x274 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x32C, -1, -1, NULL, NULL, true, 1.0f);
            field_0x278 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x184, -1, -1, NULL, NULL, true, 1.0f);
            if (field_0x274 != NULL) {
                field_0x274->setGlobalEnvColor(sColors[mSpawnParam_0x90 & 7][0], sColors[mSpawnParam_0x90 & 7][1], sColors[mSpawnParam_0x90 & 7][2]);
            }
            field_0x248 = 0;
            field_0x250 = lbl_80472730[mSpawnParam_0x90 & 7];
        } else {
            if (field_0x274 != NULL) {
                setEmitterPos_8013CB44(field_0x274, &mPos, -1, -1, true);
            }
            if (field_0x278 != NULL) {
                setEmitterPos_8013CB44(field_0x278, &mPos, -1, -1, true);
            }
            if (stepSequence_8035C15C(10) == true) {
                startSequence_8035C1C4(10, lbl_80472730[mSpawnParam_0x90 & 7]);
            }
            if (field_0x238 == 1) {
                mState = 3;
                field_0x234 = 0xA0;
                field_0x238 = 0;
            }
        }
    }
}

void MaidenEnding_804AD5B8::fn_8035CF50() {
    if (mPos.y < 1790.0f) {
        f32 diffY = mPos.y - mPos.z;
        if (diffY < 1744.0f) {
            field_0x272 = -2;
        } else if (diffY < 1781.0f) {
            field_0x272 = -1;
        } else {
            field_0x272 = 0;
        }
    } else {
        field_0x272 = 0;
    }

    if (field_0x274 != NULL) {
        setEmitterPos_8013CB44(field_0x274, &mPos, -1, -1, true);
    }
    if (field_0x278 != NULL) {
        setEmitterPos_8013CB44(field_0x278, &mPos, -1, -1, true);
    }
    if (stepSequence_8035C15C(10) == true) {
        startSequence_8035C1C4(10, lbl_80472730[mSpawnParam_0x90 & 7]);
    }

    if ((field_0x270 & 0xC000U) == 0xC000U) {
        if (field_0x268 < 1795.0f) {
            field_0x268 += 1.0f;
        }
    } else {
        if (field_0x254 < 1.8f) {
            field_0x254 += 0.05f;
        }
        if (field_0x268 > 1780.0f) {
            field_0x268 -= 0.5f;
        }
    }

    s16 prevAngle = field_0x260;
    JGeometry::TVec3f target;
    target = (this->*lbl_804AD594[field_0x270 & 3])();
    moveTowards_inline(target);

    if ((field_0x270 & 0xC000U) == 0xC000U) {
        s16 targetAngle = lbl_80472798[mSpawnParam_0x90 & 7];
        if ((u16)field_0x260 >= (u16)targetAngle && (u16)prevAngle < (u16)targetAngle) {
            mState = 4;
            field_0x260 = lbl_80472798[mSpawnParam_0x90 & 7];
            field_0x272 = 0;
        }
    }
}

JGeometry::TVec3f MaidenEnding_804AD5B8::fn_8035CE78() {
    if (field_0x25c > 55.0f) {
        field_0x25c -= 0.1f;
    }
    s16 prevAngle = field_0x260;
    field_0x260 += 0x140;
    JGeometry::TVec3f ret;
    ret = JGeometry::TVec3f(
        field_0x264 + field_0x258 * JMASCos(field_0x260),
        field_0x268 + field_0x25c * JMASSin(field_0x260),
        mPos.z
    );
    if (field_0x260 >= 0x4000 && prevAngle < 0x4000) {
        field_0x270 |= 1;
    } else if ((u16)field_0x260 >= 0xC000 && (u16)prevAngle < 0xC000) {
        field_0x270 |= 2;
    }
    return ret;
}

JGeometry::TVec3f MaidenEnding_804AD5B8::fn_8035CDC0() {
    JGeometry::TVec3f ret;
    s16 prevAngle = field_0x260;
    field_0x260 += 0x400;
    ret = JGeometry::TVec3f(
        (field_0x264 - 5.0f) + 25.0f * JMASCos(field_0x260),
        (field_0x268 + field_0x25c - 25.0f) + 25.0f * JMASSin(field_0x260),
        mPos.z);
    if (field_0x260 >= 0x4000 && prevAngle < 0x4000) {
        if (field_0x270 < 0) {
            field_0x270 = -0x4000;
        } else {
            field_0x270 = -0x8000;
        }
    }
    return ret;
}

JGeometry::TVec3f MaidenEnding_804AD5B8::fn_8035CD00() {
    JGeometry::TVec3f ret;
    s16 prevAngle = field_0x260;
    field_0x260 += 0x400;
    ret = JGeometry::TVec3f(
        (field_0x264 + 5.0f) + 25.0f * JMASCos(field_0x260),
        (field_0x268 - field_0x25c + 25.0f) + 25.0f * JMASSin(field_0x260),
        mPos.z);
    if ((u16)field_0x260 >= 0xC000 && (u16)prevAngle < 0xC000) {
        if (field_0x270 < 0) {
            field_0x270 = -0x4000;
        } else {
            field_0x270 = -0x8000;
        }
    }
    return ret;
}

inline void MaidenEnding_804AD5B8::moveTowards_inline(JGeometry::TVec3f target) {
    JGeometry::TVec3f diff;
    diff.x = target.x - mPos.x;
    diff.y = target.y - mPos.y;
    diff.z = 0.0f;
    f32 dist = fn_80093280(&diff);
    mPos.x += (diff.x * field_0x254) / dist;
    mPos.y += (diff.y * field_0x254) / dist;
}

void MaidenEnding_804AD5B8::fn_8035CAF8() {
    if (field_0x274 != NULL) {
        setEmitterPos_8013CB44(field_0x274, &mPos, -1, -1, true);
    }
    if (field_0x278 != NULL) {
        setEmitterPos_8013CB44(field_0x278, &mPos, -1, -1, true);
    }
    if (stepSequence_8035C15C(10) == true) {
        startSequence_8035C1C4(10, lbl_80472730[mSpawnParam_0x90 & 7]);
    }

    JGeometry::TVec3f target;
    target = JGeometry::TVec3f(
        field_0x264 + field_0x258 * JMASCos(field_0x260),
        field_0x268 + field_0x25c * JMASSin(field_0x260),
        mPos.z
    );
    moveTowards_inline(target);

    f32 dy = __fabsf(mPos.y - target.y);
    f32 dx = __fabsf(mPos.x - target.x);
    if (dx + dy < 2.0f) {
        field_0x254 = 0.0f;
        mPos.x = field_0x264 + field_0x258 * JMASCos(field_0x260);
        mPos.y = field_0x268 + field_0x25c * JMASSin(field_0x260);
        field_0x234 = lbl_804727A4[mSpawnParam_0x90 & 7] + 0x50;
        mState = 5;
    }
}

void MaidenEnding_804AD5B8::fn_8035C940() {
    if (field_0x274 != NULL) {
        setEmitterPos_8013CB44(field_0x274, &mPos, -1, -1, true);
    }
    if (field_0x278 != NULL) {
        setEmitterPos_8013CB44(field_0x278, &mPos, -1, -1, true);
    }
    if (stepSequence_8035C15C(10) == true) {
        startSequence_8035C1C4(10, lbl_80472730[mSpawnParam_0x90 & 7]);
    }
    if (field_0x234 == 0) {
        mPos.z += 1.5f;
        if (mPos.z >= 350.0f) {
            if ((mSpawnParam_0x90 & 7) == 0) {
                EnemyForEndingControl_804AD48C* partner =
                    static_cast<EnemyForEndingControl_804AD48C*>(getActorManager_801F856C()->findActorBySerial_801F666C(field_0x23c));
                if (partner != NULL) {
                    // Both branches are in the original asm (written without the else: 83.8 %).
                    partner->field_0x230 = 5;
                    if (partner->field_0x230 == 5) {
                        partner->field_0x258 = (1765.0f - partner->field_0x23c.y) / 130.0f;
                        partner->field_0x25c = (partner->field_0x254 - 125.0f) / 130.0f;
                    } else {
                        partner->field_0x258 = (1781.0f - partner->field_0x23c.y) / 100.0f;
                        partner->field_0x25c = (partner->field_0x254 - 110.0f) / 100.0f;
                    }
                }
            }
            if (field_0x274 != NULL) {
                field_0x274->becomeInvalidEmitter();
                field_0x274 = NULL;
            }
            if (field_0x278 != NULL) {
                field_0x278->becomeInvalidEmitter();
                field_0x278 = NULL;
            }
            mAlive = false;
        }
    }
}
