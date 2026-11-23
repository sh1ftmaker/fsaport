#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameMath.h"
#include "game/Map.h"
#include "game/RotMatrix.h"
#include "game/SpriteParams.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "std/math.h"
#include "game/act/act_obj_lift.h"

static inline bool isTiny(const JGeometry::TVec3f& v) {
    return v.squared() <= 32.0f * FLT_EPSILON;
}

// Distance of two points in the plane. Provisional name (always inlined). In fn_803511F0 the
// squared sum is coloured before sqrt's own local, so sqrt sits one inline level below the sum.
static inline f32 distanceXY(const JGeometry::TVec3f& a, const JGeometry::TVec3f& b) {
    f32 sq = square(a.x - b.x) + square(a.y - b.y);
    return JGeometry::TUtil<f32>::sqrt(sq);
}

// The effect shown where the lift breaks or respawns: an emitter on the main layers, a KMRI
// actor from layer 8 up (fn_803513C0 inlines it behind its own layer test).
inline void OBJLift_804ACA00::showBreakEffect(s32 layer) {
    if (layer >= 8) {
        getActorManager_801F856C()->spawnActor_801F9484('KMRI', layer, &mPos, 0x83, -1, -1);
        field_0x28c = 1.0f;
    } else {
        spawnEmitter_8013CC50(layer, &mPos, 0x503, 1, 0, NULL, NULL, true, 2.0f);
    }
}

// Provisional name (always inlined): returns the push direction for state 0, or -1.
inline s32 OBJLift_804ACA00::checkPushDir() {
    if (field_0x260 == 0) {
        return -1;
    }
    switch (field_0x258) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
        if (field_0x281 == 1 && field_0x26c >= 4) {
            if (field_0x25c == 1 && !isScreenFlagSet_0x34()) {
                setScreenFlag_0x30(0xFF, true);
            }
            startSound_801F0E34(0x30009, 0);
            field_0x244 = 30;
            field_0x289 = 1;
            field_0x230 = 5;
            return field_0x268;
        }
        break;
    case 5:
        return fn_803506E0();
    case 6:
    case 7:
    case 8:
    case 9:
        if (field_0x281 == 1 && field_0x280 == 1) {
            if (field_0x25c == 1 && !isScreenFlagSet_0x34()) {
                setScreenFlag_0x30(0xFF, true);
            }
            startSound_801F0E34(0x30009, 0);
            field_0x230 = 1;
            return field_0x268;
        }
        break;
    case 12:
        return fn_80350530();
    }
    return -1;
}

// Four strips along the lift's edges, then ten boxes by state (two per state from 16 on).
static JGeometry::TBox2f lbl_8050ADF8[4] = {
    JGeometry::TBox2f(-31.0f, -31.0f, 31.0f, -17.0f),
    JGeometry::TBox2f(-31.0f, 17.0f, 31.0f, 31.0f),
    JGeometry::TBox2f(-31.0f, -31.0f, -17.0f, 31.0f),
    JGeometry::TBox2f(17.0f, -31.0f, 31.0f, 17.0f),
};
static JGeometry::TBox2f lbl_8050AE38[5][2] = {
    {JGeometry::TBox2f(-14.0f, -6.0f, 0.0f, 22.0f), JGeometry::TBox2f(0.0f, -22.0f, 14.0f, 6.0f)},
    {JGeometry::TBox2f(-14.0f, -22.0f, 0.0f, -8.0f), JGeometry::TBox2f(0.0f, -22.0f, 14.0f, 22.0f)},
    {JGeometry::TBox2f(-22.0f, -14.0f, 6.0f, 0.0f), JGeometry::TBox2f(-6.0f, 0.0f, 22.0f, 14.0f)},
    {JGeometry::TBox2f(-14.0f, 8.0f, 0.0f, 22.0f), JGeometry::TBox2f(0.0f, -22.0f, 14.0f, 22.0f)},
    {JGeometry::TBox2f(-14.0f, -22.0f, 0.0f, 22.0f), JGeometry::TBox2f(0.0f, -22.0f, 14.0f, -8.0f)},
};

// 0x803513C0.
// FIXME(shotgun): 99.91 %, scheduling+regalloc; behaviour drift.
void OBJLift_804ACA00::init() {
    field_0x258 = (mSpawnParam_0x90 & 0xF) + ((mSpawnParam_0x90 >> 12) & 0xF0);
    field_0x260 = (mSpawnParam_0x90 >> 4) & 0xF;
    field_0x264 = (mSpawnParam_0x90 >> 8) & 0xF;
    field_0x25c = (mSpawnParam_0x90 >> 12) & 0xF;
    field_0x268 = field_0x264;
    field_0x26c = 0;
    field_0x244 = 0;
    field_0x248 = 10;
    field_0x254 = 0;
    field_0x240 = field_0x258;
    field_0x287 = (mSpawnParam_0x90 >> 24) & 1;
    field_0x230 = 0;
    field_0x270 = 0;
    field_0x280 = 0;
    field_0x281 = 1;
    field_0x274 = -1;
    field_0x27c = 0;
    field_0x250 = 0;
    field_0x24c = 0;
    field_0x282 = 0;
    field_0x288 = 0;
    field_0x290 = 0.0f;
    field_0x294.z = 0.0f;
    field_0x294.y = 0.0f;
    field_0x294.x = 0.0f;
    field_0x283[0] = 0;
    field_0x283[1] = 0;
    field_0x283[2] = 0;
    field_0x283[3] = 0;
    field_0x234 = mPos;
    switch (field_0x258) {
    case 0:
        mMapBox_0x80.i.x = -31.0f;
        mMapBox_0x80.i.y = -31.0f;
        mMapBox_0x80.f.x = 31.0f;
        mMapBox_0x80.f.y = 31.0f;
        break;
    case 1:
        mMapBox_0x80.i.x = -15.0f;
        mMapBox_0x80.i.y = -15.0f;
        mMapBox_0x80.f.x = 15.0f;
        mMapBox_0x80.f.y = 15.0f;
        break;
    case 2:
        mMapBox_0x80.i.x = -15.0f;
        mMapBox_0x80.i.y = -31.0f;
        mMapBox_0x80.f.x = 15.0f;
        mMapBox_0x80.f.y = 31.0f;
        break;
    case 3:
        mMapBox_0x80.i.x = -31.0f;
        mMapBox_0x80.i.y = -15.0f;
        mMapBox_0x80.f.x = 31.0f;
        mMapBox_0x80.f.y = 15.0f;
        break;
    case 4:
        mMapBox_0x80.i.x = -31.0f;
        mMapBox_0x80.i.y = -31.0f;
        mMapBox_0x80.f.x = 31.0f;
        mMapBox_0x80.f.y = 31.0f;
        break;
    case 5:
        mMapBox_0x80.i.x = -39.0f;
        mMapBox_0x80.i.y = -39.0f;
        mMapBox_0x80.f.x = 39.0f;
        mMapBox_0x80.f.y = 39.0f;
        break;
    case 6:
    case 7:
    case 8:
    case 9:
        mMapBox_0x80.i.x = -7.0f;
        mMapBox_0x80.i.y = -7.0f;
        mMapBox_0x80.f.x = 7.0f;
        mMapBox_0x80.f.y = 7.0f;
        break;
    case 10: {
        mMapBox_0x80.i.x = -7.0f;
        mMapBox_0x80.i.y = -7.0f;
        mMapBox_0x80.f.x = 7.0f;
        mMapBox_0x80.f.y = 7.0f;
        bool free[4];
        free[0] = true;
        free[1] = true;
        free[2] = true;
        free[3] = true;
        s32 serial = -1;
        s32 taken = 0;
        for (;;) {
            if (taken >= 4) {
                break;
            }
            serial = getActorManager_801F856C()->findNextActorById_801F78F4('OBLF', serial);
            if (serial < 0) {
                break;
            }
            if (serial == mSerial) {
                continue;
            }
            OBJLift_804ACA00* other = static_cast<OBJLift_804ACA00*>(getActorManager_801F856C()->findActorBySerial_801F666C(serial));
            if (other == NULL) {
                continue;
            }
            s32 state = other->field_0x258;
            if (mLayer_0x4 != other->mLayer_0x4) {
                continue;
            }
            if (state < 6 || state > 9) {
                continue;
            }
            if (free[state - 6] != true) {
                continue;
            }
            taken++;
            free[state - 6] = false;
        }
        if (taken >= 4 || taken == 0) {
            field_0x258 = (s32)(4.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 6;
        } else {
            s32 list[4];
            s32 n = 0;
            for (s32 i = 0; i < 4; i++) {
                if (free[i] == true) {
                    list[n] = i;
                    n++;
                }
            }
            field_0x258 = list[(s32)((f32)n * gGameRoot_8053AAF8->mRandom.get_ufloat_1())] + 6;
        }
        field_0x240 = field_0x258;
        break;
    }
    case 11:
        mMapBox_0x80.i.x = -23.0f;
        mMapBox_0x80.i.y = -23.0f;
        mMapBox_0x80.f.x = 23.0f;
        mMapBox_0x80.f.y = 23.0f;
        mBox.i.x = -23.0f;
        mBox.i.y = -23.0f;
        mBox.f.x = 23.0f;
        mBox.f.y = 23.0f;
        field_0x230 = 7;
        break;
    case 12: {
        mMapBox_0x80.i.x = -23.0f;
        mMapBox_0x80.i.y = -23.0f;
        mMapBox_0x80.f.x = 23.0f;
        mMapBox_0x80.f.y = 23.0f;
        mBox.i.x = -23.0f;
        mBox.i.y = -23.0f;
        mBox.f.x = 23.0f;
        mBox.f.y = 23.0f;
        field_0x240 = 10;
        fn_803513C0();
        break;
    }
    case 16:
    case 17:
        mMapBox_0x80.i.x = -15.0f;
        mMapBox_0x80.i.y = -23.0f;
        mMapBox_0x80.f.x = 15.0f;
        mMapBox_0x80.f.y = 23.0f;
        field_0x240 = field_0x258 - 3;
        break;
    case 18:
    case 19:
        mMapBox_0x80.i.x = -23.0f;
        mMapBox_0x80.i.y = -15.0f;
        mMapBox_0x80.f.x = 23.0f;
        mMapBox_0x80.f.y = 15.0f;
        field_0x240 = field_0x258 - 3;
        break;
    case 20:
        mMapBox_0x80.i.x = -15.0f;
        mMapBox_0x80.i.y = -23.0f;
        mMapBox_0x80.f.x = 15.0f;
        mMapBox_0x80.f.y = 23.0f;
        field_0x240 = 14;
        break;
    }
    field_0x28c = 1.0f;
    switch (field_0x25c) {
    case 2:
        field_0x230 = 4;
        field_0x28c = 0.0f;
        break;
    }
}

void OBJLift_804ACA00::fn_803513C0() {
    s32 layer = mLayer_0x4;
    if (layer >= 8) {
        showBreakEffect(layer);
        startSound_801F0E34(0x30003, 0);
    } else {
        spawnEmitter_8013CC50(layer, &mPos, 0x459, 0, -3, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x45A, 0, -3, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x45B, 0, -3, NULL, NULL, true, 1.0f);
    }
    startSound_801F0E34(0x30065, 0);
    field_0x254 = 30;
    field_0x258 = 12;
    field_0x230 = 0;
    field_0x240 = 10;
}

// 0x803511F0.
void OBJLift_804ACA00::fn_803511F0() {
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x97, 0, -3, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x98, 0, -3, NULL, NULL, true, 1.0f);
    field_0x258 = 11;
    field_0x282 = 0;
    field_0x24c = 0;
    field_0x230 = 7;
    field_0x240 = 11;
    field_0x248 = 10;
    field_0x274 = -1;
    field_0x268 = field_0x264;
    if (distanceXY(mPos, mSpawnPos) > 48.0f) {
        mPos = mSpawnPos;
        s32 layer = mLayer_0x4;
        if (layer >= 8) {
            getActorManager_801F856C()->spawnActor_801F9484('KMRI', layer, &mPos, 0x83, -1, -1);
            field_0x28c = 1.0f;
        } else {
            spawnEmitter_8013CC50(layer, &mPos, 0x503, 1, 0, NULL, NULL, true, 2.0f);
        }
        startSound_801F0E34(0x30003, 0);
    } else {
        mPos = mSpawnPos;
    }
    if (mLayer_0x4 < 8) {
        field_0x28c = 0.0f;
    }
}

// 0x80351020.
// Not auto-inlined: fn_80350974 calls fn_80351020 out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// FIXME(shotgun): 96.72 %, scheduling+regalloc; behaviour clean.
// float register assignment differs (the box test is probably an inline).
bool OBJLift_804ACA00::fn_80351020(s32 player) {
    s32 layer = mLayer_0x4;
    if (getPlayerLayer_8023DE58(player) != layer) {
        return false;
    }
    for (s32 i = 0; i < 8; i++) {
        JGeometry::TVec3f v;
        fn_8022E088(player, i, &v);
        v -= mPos;
        v.z = 0.0f;
        s32 angle = (s32)(JMath::atanTable_.atan2_(v.y, v.x) * 10430.378f);
        f32 dist = JGeometry::TUtil<f32>::sqrt(v.x * v.x + v.y * v.y);
        v.z = 0.0f;
        v.x = dist * JMath::sincosTable_.cosShort(angle);
        v.y = dist * JMath::sincosTable_.sinShort(angle);
        f32 minX = mMapBox_0x80.i.x - -2.0f;
        f32 minY = mMapBox_0x80.i.y - -2.0f;
        f32 maxX = mMapBox_0x80.f.x + -2.0f;
        f32 maxY = mMapBox_0x80.f.y + -2.0f;
        bool inside = v.x >= minX && v.y >= minY && v.x < maxX && v.y < maxY;
        if (inside) {
            fn_8022DF48(player);
            return true;
        }
    }
    return false;
}

// 0x803506E0.
#pragma auto_inline reset

// FIXME: 99.16 %, scheduling+regalloc; behaviour clean.
// FIXME(shotgun): missing/extra; behaviour drift (register allocation, push magnitude not vectorized to ps).
s32 OBJLift_804ACA00::fn_80350974() {
    field_0x283[0] = 0;
    field_0x283[1] = 0;
    field_0x283[2] = 0;
    field_0x283[3] = 0;
    if (field_0x230 == 7) {
        return 0;
    }
    u8 onLift = 0;
    s32 count = 0;
    JGeometry::TVec3f push;
    push.zero();
    for (s32 i = 0; i < 4; i++) {
        if (field_0x258 == 4) {
            JGeometry::TBox2f saved;
            saved.set(mMapBox_0x80);
            for (s32 j = 0; j < 4; j++) {
                mMapBox_0x80.set(lbl_8050ADF8[j]);
                bool hit = fn_80351020(i);
                field_0x283[i] = hit;
                if (hit == 1 && getPlayerPos_8023E724(i)->z <= 0.0f) {
                    count++;
                    break;
                }
            }
            mMapBox_0x80.set(saved);
        } else if (field_0x258 >= 0x10 && field_0x258 <= 0x14) {
            JGeometry::TBox2f saved;
            saved.set(mMapBox_0x80);
            for (s32 j = 0; j < 2; j++) {
                mMapBox_0x80.set(lbl_8050AE38[field_0x258 - 0x10][j]);
                bool hit = fn_80351020(i);
                field_0x283[i] = hit;
                if (hit == 1 && getPlayerPos_8023E724(i)->z <= 0.0f) {
                    count++;
                    break;
                }
            }
            mMapBox_0x80.set(saved);
        } else if (field_0x258 >= 6 && field_0x258 <= 9) {
            bool hit = fn_80351020(i);
            field_0x283[i] = hit;
            if (i == field_0x258 - 6) {
                field_0x280 = hit;
            }
        } else {
            bool hit = fn_80351020(i);
            field_0x283[i] = hit;
            if (hit == 1 && getPlayerPos_8023E724(i)->z <= 0.0f) {
                count++;
            }
        }
    }

    if (field_0x258 >= 6 && field_0x258 <= 9) {
        push = mVel_0x3c;
    } else if (field_0x230 == 1 || field_0x230 == 3 ||
               ((field_0x258 == 5 || field_0x258 == 0xC) && PSVECMag(&mVel_0x3c) > 0.0f)) {
        push = mVel_0x3c;
    } else if (field_0x230 == 5) {
        push.y = 0.1f;
    } else if (field_0x230 == 6) {
        push.y = -0.1f;
    }

    for (s32 i = 0; i < 4; i++) {
        if (field_0x283[i] == 1 && (fn_80230188(i) == 1 || fn_8022CCAC(i) == 1)) {
            onLift = 1;
        }
    }

    if (field_0x258 == 5 && count == 0 &&
        (mPos.x - mSpawnPos.x) * (mPos.x - mSpawnPos.x) + (mPos.y - mSpawnPos.y) * (mPos.y - mSpawnPos.y) > 8.0f) {
        field_0x250++;
        if (field_0x250 > 0xF0) {
            showBreakEffect(mLayer_0x4);
            mPos = mSpawnPos;
            field_0x28c = 0.0f;
            showBreakEffect(mLayer_0x4);
            startSound_801F0E34(0x30003, 0);
        }
    } else {
        field_0x250 = 0;
    }
    field_0x288 = onLift;

    if (field_0x289 == 1) {
        for (s32 i = 0; i < 4; i++) {
            if (field_0x283[i] == 0) {
                if (getTileCollisionCode_8022461C(getPlayerPos_8023E724(i), getPlayerLayer_8023DE58(i)) == 0) {
                    field_0x289 = 0;
                    break;
                }
            }
        }
    }

    for (s32 i = 0; i < 4; i++) {
        if (field_0x283[i] == 1 && getPlayerPos_8023E724(i)->z <= 0.0f && field_0x288 == 0) {
            if (isTiny(push) != TRUE) {
                if (!fn_8022F478(i) ||
                    (field_0x283[getPlayerLeader_8022F3DC(i)] == 1 && (fn_8022CAF8(i) == 5 || fn_8022CAF8(i) == 4)) ||
                    fn_80235580(i) == TRUE) {
                    if (field_0x289 == 1 && field_0x258 != 5 && field_0x258 != 0xC &&
                        !(field_0x258 >= 6 && field_0x258 <= 9)) {
                        fn_8023DEDC(i, &push);
                    } else {
                        fn_8023DEDC(i, &push);
                        for (s32 k = 0; k < 3; k++) {
                            fn_8022CB94(i);
                        }
                    }
                }
            }
        }
    }
    return count;
}

s32 OBJLift_804ACA00::fn_803506E0() {
    static JGeometry::TVec2f lbl_8050AED8[4] = {
        JGeometry::TVec2f(32.0f, 0.0f),
        JGeometry::TVec2f(-32.0f, 0.0f),
        JGeometry::TVec2f(0.0f, 32.0f),
        JGeometry::TVec2f(0.0f, -32.0f),
    };
    s32 d;
    s32 prev = field_0x270;
    field_0x270 = 0;
    for (s32 i = 0; i < 4; i++) {
        if (hasPlayerLeader_8022F514(i) == true && fn_8022CAF8(i) == 5) {
            continue;
        }
        for (d = 0; d < 4; d++) {
            JGeometry::TVec2f pos = lbl_8050AED8[d];
            pos.x += mPos.x;
            pos.y += mPos.y;
            s32 held = prev & (1 << d);
            f32 limit = 64.0f;
            if (held) {
                limit = square(8.0f + field_0x260);
            }
            JGeometry::TVec3f* pl = getPlayerPos_8023E724(i);
            f32 dx = pos.x - pl->x;
            f32 dy = pos.y - pl->y;
            if (square(dx) + square(dy) < limit && getPlayerPos_8023E724(i)->z == 0.0f) {
                if (held == 0) {
                    startSound_801F0E34(0x30009, 0);
                }
                field_0x270 |= (1 << d);
            }
        }
    }
    s32 mask = field_0x270;
    if ((mask & 1) && ((mask >> 1) & 1)) {
        mask &= 0xC;
    }
    if (((mask >> 2) & 1) && ((mask >> 3) & 1)) {
        mask &= 3;
    }
    if (mask == 1) {
        field_0x268 = 0;
    } else if (mask == 2) {
        field_0x268 = 1;
    } else if (mask == 4) {
        field_0x268 = 2;
    } else if (mask == 8) {
        field_0x268 = 3;
    } else if (mask == 5) {
        field_0x268 = 4;
    } else if (mask == 6) {
        field_0x268 = 5;
    } else if (mask == 9) {
        field_0x268 = 6;
    } else if (mask == 10) {
        field_0x268 = 7;
    } else {
        field_0x268 = -1;
    }
    return field_0x268;
}

// 0x80350530.
// Not auto-inlined: update calls fn_80350530 out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
s32 OBJLift_804ACA00::fn_80350530() {
    s32 prev = field_0x270;
    field_0x270 = 0;
    for (s32 i = 0; i < 4; i++) {
        if (hasPlayerLeader_8022F514(i) == true && fn_8022CAF8(i) == 5) {
            continue;
        }
        f32 cx = mPos.x;
        f32 cy = mPos.y;
        f32 limit = 64.0f;
        if (prev != 0) {
            limit = square(8.0f + field_0x260);
        }
        JGeometry::TVec3f* pos = getPlayerPos_8023E724(i);
        f32 dx = cx - pos->x;
        f32 dy = cy - pos->y;
        if (square(dx) + square(dy) < limit && getPlayerPos_8023E724(i)->z == 0.0f) {
            if (prev == 0) {
                startSound_801F0E34(0x30009, 0);
                field_0x27c = 0;
                field_0x274 = i;
                if (hasPlayerLeader_8022F514(i) == true) {
                    field_0x274 = getPlayerLeader_8022F3DC(i);
                }
            }
            field_0x270 = 1;
            if (field_0x244 <= 0) {
                return field_0x268;
            }
            return -1;
        }
    }
    field_0x27c = 0;
    field_0x274 = -1;
    field_0x244 = 0;
    return -1;
}
#pragma auto_inline reset

// 0x803502CC.
// Still differs: the second loop's index is computed with one counter register; the original keeps a separate one.
u8 OBJLift_804ACA00::fn_803502CC() {
    JGeometry::TBox2f saved;
    saved.set(mMapBox_0x80);
    if (field_0x258 == 4) {
        for (s32 i = 0; i < 4; i++) {
            mMapBox_0x80.set(lbl_8050ADF8[i]);
            if (field_0x268 == 0 || field_0x268 == 1) {
                mMapBox_0x80.f.y -= 3.0f;
            } else {
                mMapBox_0x80.i.y += field_0x290;
                mMapBox_0x80.f.y += field_0x290;
            }
            if (sweepMapBox_801F2618(NULL, false) == 1) {
                mMapBox_0x80.set(saved);
                return 1;
            }
        }
    } else if (field_0x258 >= 16 && field_0x258 <= 20) {
        for (s32 j = 0; j < 2; j++) {
            mMapBox_0x80.set(lbl_8050AE38[field_0x258 - 16][j]);
            if (field_0x268 == 0 || field_0x268 == 1) {
                mMapBox_0x80.f.y -= 3.0f;
            } else {
                mMapBox_0x80.i.y += field_0x290;
                mMapBox_0x80.f.y += field_0x290;
            }
            if (sweepMapBox_801F2618(NULL, false) == 1) {
                mMapBox_0x80.set(saved);
                return 1;
            }
        }
    } else {
        return sweepMapBox_801F2618(NULL, false);
    }
    mMapBox_0x80.set(saved);
    return 0;
}

// 0x80350274.
bool OBJLift_804ACA00::isBlocked_0x4C(s32 layer, const JGeometry::TVec3f* pos) {
    s32 code = getTileCollisionCode_8022461C(pos, layer);
    if (code == 0x3A || code == 0x3 || code == 0xD || code == 0xE || code == 0x3B) {
        return false;
    }
    return true;
}

// 0x8035001C.
// Not auto-inlined: update calls fn_8035001C out of line in the original (docs/INLINING.md, pragma rule 1).
// Remaining difference: the jump table's symbol name (jumptable_804ACAC8).
#pragma auto_inline off
void OBJLift_804ACA00::fn_8035001C(u32 dir) {
    switch (dir) {
    case 0:
        mVel_0x3c.set(0.5f * field_0x260, 0.0f, 0.0f);
        break;
    case 1:
        mVel_0x3c.set(0.5f * -(f32)field_0x260, 0.0f, 0.0f);
        break;
    case 2:
        mVel_0x3c.set(0.0f, 0.5f * field_0x260, 0.0f);
        break;
    case 3:
        mVel_0x3c.set(0.0f, 0.5f * -(f32)field_0x260, 0.0f);
        break;
    case 4:
        mVel_0x3c.set(0.5f * field_0x260, 0.5f * field_0x260, 0.0f);
        break;
    case 5:
        mVel_0x3c.set(0.5f * -(f32)field_0x260, 0.5f * field_0x260, 0.0f);
        break;
    case 6:
        mVel_0x3c.set(0.5f * field_0x260, 0.5f * -(f32)field_0x260, 0.0f);
        break;
    case 7:
        mVel_0x3c.set(0.5f * -(f32)field_0x260, 0.5f * -(f32)field_0x260, 0.0f);
        break;
    default:
        mVel_0x3c.zero();
        break;
    }
}
#pragma auto_inline reset

// 0x8034F43C.
void OBJLift_804ACA00::update() {
    if (field_0x230 == 4) {
        if (isScreenFlagSet_0x34() == 1) {
            showBreakEffect(mLayer_0x4);
            startSound_801F0E34(0x30003, 0);
            playSe_801F0D20(0x42);
            field_0x25c = 0;
            if (field_0x258 == 0xB) {
                field_0x230 = 7;
            } else {
                field_0x230 = 0;
            }
        }
        return;
    }
    BOOL present = FALSE;
    for (s32 i = 0; i < 4; i++) {
        if (mLayer_0x4 == getPlayerLayer_8023DE58(i) && !fn_8022F478(i)) {
            present = TRUE;
            break;
        }
    }
    if (field_0x254 > 0) {
        field_0x254--;
        return;
    }
    if (field_0x282 == 1) {
        if (field_0x24c % 40 == 0x13) {
            startSound_801F0E34(0x30068, 0);
        }
        field_0x24c--;
        if (field_0x24c <= 0) {
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x97, 0, -3, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x98, 0, -3, NULL, NULL, true, 1.0f);
            field_0x258 = 11;
            field_0x282 = 0;
            field_0x24c = 0;
            field_0x230 = 7;
            field_0x240 = 11;
            field_0x248 = 10;
            field_0x274 = -1;
            field_0x268 = field_0x264;
            if (distanceXY(mPos, mSpawnPos) > 48.0f) {
                mPos = mSpawnPos;
                showBreakEffect(mLayer_0x4);
                startSound_801F0E34(0x30003, 0);
            } else {
                mPos = mSpawnPos;
            }
            if (mLayer_0x4 < 8) {
                field_0x28c = 0.0f;
            }
            return;
        }
    }
    if (field_0x28c < 1.0f) {
        field_0x28c += 0.035f;
        if (field_0x28c >= 1.0f) {
            field_0x28c = 1.0f;
        }
        return;
    }
    field_0x234 = mPos;
    field_0x26c = 0;
    field_0x26c = fn_80350974();
    if (field_0x288 == 0 &&
        (field_0x230 == 1 || field_0x230 == 3 ||
         ((field_0x258 == 5 || field_0x258 == 0xC) && PSVECMag(&mVel_0x3c) > 0.0f))) {
        mPos += mVel_0x3c;
    }

    switch (field_0x230) {
    case 0:
        fn_8035001C(checkPushDir());
        if (field_0x268 != -1) {
            JGeometry::TVec3f saved = mPos;
            mPos += mVel_0x3c;
            if (fn_803502CC() == 1) {
                mVel_0x3c.z = 0.0f;
                mVel_0x3c.y = 0.0f;
                mVel_0x3c.x = 0.0f;
                field_0x230 = 0;
                if (field_0x258 != 0xC) {
                    field_0x244 = 0;
                } else {
                    field_0x244 = 0x78;
                    field_0x268 ^= 1;
                }
            }
            mPos = saved;
        } else if (field_0x258 >= 6 && field_0x258 <= 9) {
            if (field_0x280 == 0) {
                field_0x281 = 1;
                field_0x244 = 0;
            }
        } else if (field_0x26c <= 0) {
            field_0x281 = 1;
            field_0x244 = 0;
        }
        break;
    case 5:
        if (present == TRUE && field_0x288 == 0) {
            startSound_801F0E34(0x30066, 0);
        }
        mPos.y += 0.1f;
        field_0x290 -= 0.1f;
        if (field_0x244 <= 0) {
            if (field_0x268 == field_0x264) {
                field_0x230 = 1;
            } else {
                field_0x230 = 3;
            }
            field_0x244 = 10;
        }
        break;
    case 1:
    case 3:
        if (fn_803502CC() == 1) {
            mVel_0x3c.z = 0.0f;
            mVel_0x3c.y = 0.0f;
            mVel_0x3c.x = 0.0f;
            mPos = field_0x234;
            if (field_0x258 <= 4 || (field_0x258 >= 0x10 && field_0x258 <= 0x14)) {
                field_0x230 = 6;
                field_0x281 = 0;
                field_0x244 = 30;
            } else if (field_0x258 == 5) {
                field_0x230 = 0;
                field_0x244 = 0;
                if (present == TRUE && field_0x288 == 0) {
                    startSound_801F0E34(0x30067, 0);
                }
            } else {
                field_0x268 ^= 1;
                field_0x244 = 0x78;
                field_0x281 = 0;
                if (field_0x230 == 1) {
                    field_0x230 = 2;
                } else if (field_0x287 == 1) {
                    field_0x230 = 0;
                } else {
                    field_0x230 = 2;
                }
                if (present == TRUE && field_0x288 == 0) {
                    startSound_801F0E34(0x30067, 0);
                }
            }
        }
        if (present == TRUE && field_0x288 == 0) {
            startSound_801F0E34(0x30066, 0);
        }
        break;
    case 6:
        mPos.y -= 0.1f;
        field_0x290 += 0.1f;
        if (field_0x244 <= 0) {
            if (field_0x268 == field_0x264) {
                field_0x230 = 2;
            } else if (field_0x287 == 1) {
                field_0x230 = 0;
            } else {
                field_0x230 = 2;
            }
            field_0x244 = 0x78;
            field_0x268 ^= 1;
            if (present == TRUE && field_0x288 == 0) {
                startSound_801F0E34(0x30067, 0);
            }
        }
        if (field_0x26c <= 0) {
            field_0x281 = 1;
        }
        if (field_0x230 == 6 && present == TRUE && field_0x288 == 0) {
            startSound_801F0E34(0x30066, 0);
        }
        break;
    case 2:
        if (field_0x281 == 1 && field_0x244 == 0) {
            fn_8035001C(field_0x268);
            if (present == TRUE && field_0x288 == 0) {
                startSound_801F0E34(0x30009, 0);
            }
            if (field_0x258 <= 4 || (field_0x258 >= 0x10 && field_0x258 <= 0x14)) {
                field_0x244 = 30;
                field_0x230 = 5;
            } else {
                field_0x230 = 3;
            }
        } else if (field_0x258 >= 6 && field_0x258 <= 9) {
            if (field_0x280 == 0) {
                field_0x281 = 1;
            }
            if (field_0x281 == 1 && field_0x280 == 1) {
                field_0x244 = 0;
            }
        } else {
            if (field_0x26c <= 0) {
                field_0x281 = 1;
            }
            if (field_0x281 == 1 && field_0x26c >= 4) {
                field_0x244 = 0;
            }
        }
        break;
    case 7:
        field_0x248--;
        if (field_0x248 <= 0) {
            field_0x248 = 10;
            field_0x240++;
            if (field_0x240 > 12) {
                field_0x240 = 11;
            }
        }
        break;
    case 4:
        break;
    }

    if (field_0x244 > 0) {
        field_0x244--;
        if (field_0x244 <= 0 && (field_0x230 == 0 || field_0x230 == 2)) {
            field_0x281 = 1;
        }
    }
    if (present == TRUE && field_0x288 == 0 && (field_0x258 == 5 || field_0x258 == 0xC)) {
        if (isTiny(mVel_0x3c) == false) {
            startSound_801F0E34(0x30066, 0);
        } else if (isTiny(field_0x294) == false) {
            startSound_801F0E34(0x30067, 0);
        }
    }
    field_0x294 = mVel_0x3c;
}

// FIXME(shotgun): 99.08 %, regalloc-only; behaviour clean.
void OBJLift_804ACA00::draw() {
    s32 alpha = 0xFF;
    if (field_0x230 == 4) {
        return;
    }
    if (field_0x282 == 1 && field_0x24c % 40 < 20) {
        return;
    }
    if (field_0x254 > 0) {
        return;
    }
    if (field_0x28c <= 0.0f) {
        return;
    }
    if (field_0x250 > 0x78) {
        if (mLayer_0x4 < 8) {
            s32 phase = field_0x250 % 40;
            if (phase > 20) {
                alpha = (s32)(255.0f * (((f32)phase - 20.0f) / 20.0f));
            } else {
                alpha = (s32)(255.0f * ((20.0f - (f32)phase) / 20.0f));
            }
        } else {
            s32 phase = field_0x250 % 40;
            if (phase < 20) {
                return;
            }
        }
    }
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    PSMTXIdentity(mtx);
    params.field_0x10.a = alpha;
    params.field_0x20 = 0;
    if (field_0x258 == 0xB) {
        f32 big = 1.5f * field_0x28c;
        mtx[0][0] = big;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = big;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = big;
    } else if (field_0x258 == 0x11) {
        mtx[0][0] = -field_0x28c;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = field_0x28c;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = field_0x28c;
    } else if (field_0x258 == 0x13) {
        // Euler rotation (0, 270 deg, 0) from sincosTable_ entries (the original reads them
        // at run time: table index 0 and index 1536).
        f32 sx = JMath::sincosTable_.sinShort(0);
        f32 sy = JMath::sincosTable_.sinShort(-0x4000);
        f32 sz = JMath::sincosTable_.sinShort(0);
        f32 cx = JMath::sincosTable_.cosShort(0);
        f32 cy = JMath::sincosTable_.cosShort(-0x4000);
        f32 cz = JMath::sincosTable_.cosShort(0);
        mtx[0][0] = field_0x28c * (cy * cz);
        mtx[1][0] = field_0x28c * (cy * sz);
        mtx[2][0] = field_0x28c * -sy;
        mtx[0][1] = field_0x28c * (sx * sy * cz - cx * sz);
        mtx[1][1] = field_0x28c * (sx * sy * sz + cx * cz);
        mtx[2][1] = field_0x28c * (sx * cy);
        mtx[0][2] = field_0x28c * (cx * cz * sy + sx * sz);
        mtx[1][2] = field_0x28c * (cx * sz * sy - sx * cz);
        mtx[2][2] = field_0x28c * (cx * cy);
    } else if (field_0x258 == 5) {
        f32 angle = 2.0f * (3.14f * (1.0f - field_0x28c));
        JGeometry::TVec3f rot(0.0f, angle, 0.0f);
        setRotScaleMtx(mtx, rot, field_0x28c);
    } else {
        mtx[0][0] = field_0x28c;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = field_0x28c;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = field_0x28c;
    }
    params.mMtx = mtx;
    params.field_0x21 = 1;
    if ((field_0x258 <= 4 && field_0x268 == 2 && (field_0x230 == 1 || field_0x230 == 3 || field_0x230 == 6)) ||
        (field_0x268 == 3 && (((field_0x230 == 1 || field_0x230 == 3) && field_0x244 > 0) || field_0x230 == 5))) {
        params.field_0x1c = -1;
    } else {
        params.field_0x1c = 0;
    }
    params.field_0x1d = -3;
    if (field_0x258 == 0xB) {
        params.field_0x1d = -4;
    }
    drawSprite_801F06F0("enemy_both_sob.bin", field_0x240 + 0x14, &params, -1, 1.0f, 1.0f);
    fn_80307AC8(mSerial, mLayer_0x4, &mPos, field_0x240 + 0x14, 0, 0, -1);
}

OBJLift_804ACA00::~OBJLift_804ACA00() {}
