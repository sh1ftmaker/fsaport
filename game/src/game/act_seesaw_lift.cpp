#include "game/GameAudio.h"
#include "game/act/act_seesaw_lift.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "game/BgRewrite.h"
#include "game/Map.h"
#include "game/Effect.h"
#include "game/GameRootSub20.h"
#include "JSystem/JMath/JMath.h"
#include <math.h>
#include <float.h>
#include "game/ActorManager.h"
#include "JSystem/ResTIMG.h"

// The state table. Not const, so its initialiser is copied into it by __sinit_act_seesaw_lift_cpp.
static void (SeesawLift_804AD348::*lbl_804AD30C[5])() = {
    &SeesawLift_804AD348::fn_8035A210,
    &SeesawLift_804AD348::fn_803597EC,
    &SeesawLift_804AD348::fn_80358F98,
    &SeesawLift_804AD348::fn_80358F98,
    &SeesawLift_804AD348::fn_803584C0,
};

static const char* lbl_8053A1B8[2] = {"formation_lift.bti", "sand5.bti"};

static const u16 lbl_80542868[2] = {864, 12};

static inline bool contains(const JGeometry::TBox2f& box, const JGeometry::TVec3f& p) {
    return p.x >= box.i.x && p.y >= box.i.y && p.x < box.f.x && p.y < box.f.y;
}

// At full tilt: false when any of a player's eight points lies inside the box.
inline bool SeesawLift_804AD348::isClearOfPlayers() {
    if (field_0x230 != 0x3E00 && field_0x230 != -0x3E00) {
        return true;
    }
    JGeometry::TBox2f box = mBox;
    for (s32 p = 0; p < 4; p++) {
        for (s32 i = 0; i < 8; i++) {
            JGeometry::TVec3f rel;
            fn_8022E088(p, i, &rel);
            rel.sub(mPos);
            rel.z = 0.0f;
            if (contains(box, rel)) {
                return false;
            }
        }
    }
    return true;
}

// Shared per-player helpers inlined into all four state functions (fn_8035A210,
// fn_803597EC, fn_80358F98, fn_803584C0).
inline void SeesawLift_804AD348::checkContactBox(s32 p) {
    JGeometry::TBox2f box;
    fn_801F317C(&box);
    f32 centerY = mPos.y;
    f32 cosVal = JMath::sincosTable_.cosShort(field_0x230);
    box.i.y = (box.i.y - centerY) * JGeometry::TUtilf::abs(cosVal) + centerY;
    box.f.y = (box.f.y - centerY) * JGeometry::TUtilf::abs(cosVal) + centerY;
    JGeometry::TVec2f out;
    s8 side;
    if (fn_8023AAF8(p, &box, mLayer_0x4, &out)) {
        f32 d = centerY - out.y;
        if (field_0x24e && d == 0.0f) {
            d = 0.1f;
        }
        if (!field_0x24e && JGeometry::TUtilf::abs(d) <= 8.0f) {
            d = 0.0f;
        }
        field_0x240 += 20.0f * d;
        if (d >= 0.0f) {
            side = 1;
        } else {
            side = -1;
        }
    } else {
        side = 0;
    }
    field_0x24a[p] = side;
}

inline void SeesawLift_804AD348::pushPlayer(s32 p) {
    if (isPlayerActive_8023F264(p) && field_0x246[p] != 0 && !fn_8022D75C(p) && !fn_80230188(p) && !hasPlayerLeader_8022F514(p)) {
        f32 sinVal = JMath::sincosTable_.sinShort(field_0x230);
        f32 push = 1.8f * (sinVal * JGeometry::TUtilf::abs(sinVal));
        if (gGameRoot_8053AAF8->field_0x7e != 0 && getKind() >= 2) {
            push *= 5.5f;
        }
        if (field_0x244 == 0) {
            if (field_0x258 == 1) {
                JGeometry::TVec3f v(0.0f, -1.0f * push, 0.0f);
                fn_8023DF68(p, &v);
            }
        } else if (field_0x258 == 1) {
            JGeometry::TVec3f v(0.0f, push, 0.0f);
            fn_8023DF68(p, &v);
        }
    }
}

void SeesawLift_804AD348::init() {
    field_0x234 = static_cast<const ResTIMG*>(
        gGameRoot_8053AAF8->field_0xa4->getResource('????', lbl_8053A1B8[mSpawnParam_0x90 & 0x7F]));
    mBox.set(-field_0x234->mSizeX / 2, -field_0x234->mSizeY / 2, field_0x234->mSizeX / 2, field_0x234->mSizeY / 2);
    mLife = 0;
    field_0x230 = 0;
    field_0x238 = 0;
    field_0x23a = 0;
    field_0x23c = 0;
    field_0x23e = 0x18;
    field_0x240 = 0.0f;
    field_0x244 = 0;
    field_0x245 = 0;
    field_0x24e = 0;
    for (int i = 0; i < 4; i++) {
        field_0x246[i] = 0;
        field_0x24a[i] = 0;
    }
    field_0x250 = 0;
    field_0x26c = 0;
    // Original bug: loops to 8 instead of 4, overwriting field_0x26c, field_0x270, and two words past the 0x274 allocation.
    for (int i = 0; i < 8; i++) {
        field_0x25c[i] = NULL;
    }
    pool_8054286C(0);
}

s32 SeesawLift_804AD348::fn_8035AE9C(s32 player) {
    if (fn_8022D75C(player)) {
        return 0;
    }
    if (fn_80230188(player)) {
        return 0;
    }
    s32 layer = mLayer_0x4;
    s32 playerLayer = getPlayerLayer_8023DE58(player);
    if (playerLayer != layer) {
        return 0;
    }
    if (getPlayerPos_8023E724(player)->z > 0.0f) {
        fn_8022DF48(player);
        return 0;
    }
    for (s32 i = 0; i < 8; i++) {
        if ((field_0x230 >= 0x3C00 && field_0x230 <= 0x4400) || (field_0x230 <= -0x3C00 && field_0x230 >= -0x4400)) {
            return 0;
        }
        JGeometry::TVec3f rel;
        fn_8022E088(player, i, &rel);
        rel.sub(mPos);
        rel.z = 0.0f;
        JGeometry::TBox2f box = mBox;
        f32 cosVal = __fabsf(JMath::sincosTable_.cosShort(field_0x230));
        box.i.y *= cosVal;
        box.f.y *= cosVal;
        if (contains(box, rel)) {
            fn_8022DF48(player);
            f32 d = mPos.y - getPlayerPos_8023E724(player)->y;
            if (field_0x24e && d == 0.0f) {
                d = 0.1f;
            }
            if (!field_0x24e && __fabsf(d) <= 8.0f) {
                d = 0.0f;
            }
            field_0x240 += d;
            fn_8035A860(d, player);
            if (d >= 0.0f) {
                return 1;
            }
            return -1;
        }
    }
    return 0;
}

s32 SeesawLift_804AD348::fn_8035AC84(s32 player) {
    if (fn_8022D75C(player)) {
        return 0;
    }
    if (fn_80230188(player)) {
        return 0;
    }
    s32 layer = mLayer_0x4;
    s32 playerLayer = getPlayerLayer_8023DE58(player);
    if (playerLayer != layer) {
        return 0;
    }
    if (getPlayerPos_8023E724(player)->z > 0.0f) {
        fn_8022DF48(player);
        return 0;
    }
    for (s32 i = 0; i < 8; i++) {
        if ((field_0x230 >= 0x3C00 && field_0x230 <= 0x4400) || (field_0x230 <= -0x3C00 && field_0x230 >= -0x4400)) {
            return 0;
        }
        JGeometry::TVec3f rel;
        fn_8022E088(player, i, &rel);
        rel.sub(mPos);
        rel.z = 0.0f;
        JGeometry::TBox2f box = mBox;
        f32 cosVal = __fabsf(JMath::sincosTable_.cosShort(field_0x230));
        box.i.y *= cosVal;
        box.f.y *= cosVal;
        if (contains(box, rel)) {
            fn_8022DF48(player);
            f32 d = mPos.y - getPlayerPos_8023E724(player)->y;
            if (d == 0.0f) {
                d = 0.1f;
            }
            field_0x240 += d;
            fn_8035A860(d, player);
            if (d >= 0.0f) {
                return 1;
            }
            return -1;
        }
    }
    return 0;
}

// FIXME(shotgun): 99.57 %, different; behaviour clean.
// the branches on field_0x240
void SeesawLift_804AD348::fn_8035AA54() {
    u8 near = 0;
    s32 moved = 0;
    bool all = field_0x246[0] && field_0x246[1] && field_0x246[2] && field_0x246[3];
    if (all && __fabsf(field_0x240) < 33.0f) {
        near = 1;
    }
    u8 flag = near;
    if (!gGameRoot_8053AAF8->field_0x7e) {
        flag = near & (fn_8022CAF8(0) == 1);
    }
    if (field_0x240 != 0.0f) {
        if (flag || field_0x250 > 0) {
            if (!gGameRoot_8053AAF8->field_0x7e) {
                if (field_0x240 > 0.0f) {
                    field_0x240 = -1500.0f;
                } else {
                    field_0x240 = 1500.0f;
                }
            } else {
                if (field_0x240 > 0.0f) {
                    field_0x240 = -10.0f;
                } else {
                    field_0x240 = 10.0f;
                }
            }
            moved = 1;
        } else {
            if (!gGameRoot_8053AAF8->field_0x7e) {
                if (field_0x240 > 0.0f) {
                    field_0x240 = 1500.0f;
                } else {
                    field_0x240 = -1500.0f;
                }
            } else {
                if (field_0x240 > 0.0f) {
                    field_0x240 = 10.0f;
                } else {
                    field_0x240 = -10.0f;
                }
            }
        }
        field_0x23a = (s32)field_0x240;
    } else {
        field_0x23a = (s32)((-(0.1f * (f32)field_0x238) - 0.01f * (f32)field_0x230) / 15.0f);
    }
    field_0x238 += field_0x23a;
    if (moved) {
        field_0x240 = 0.0f;
    }
}

// Not auto-inlined: fn_8035AC84 and fn_8035AE9C call fn_8035A860 out of line in the
// original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// FIXME: 99.16 %, regalloc-only; behaviour clean.
void SeesawLift_804AD348::fn_8035A860(f32 weight, s32 player) {
    if (isPlayerInside(player)) {
        f32 slide = 0.3f * __fabsf(JMath::sincosTable_.sinShort(field_0x230)) *
                    (JMAAbs(weight) / (mBox.f.y * __fabsf(JMath::sincosTable_.cosShort(field_0x230))));
        if (slide > 0.3f) {
            slide = 0.3f;
        }
        f32 scale;
        if (!field_0x244) {
            if ((f32)field_0x230 * weight >= 0.0f) {
                scale = 1.0f - slide;
            } else {
                scale = 1.0f + slide;
            }
        } else {
            if ((f32)field_0x230 * weight >= 0.0f) {
                scale = 1.0f + slide;
            } else {
                scale = 1.0f - slide;
            }
        }
        JGeometry::TVec3f v(scale, 1.0f, scale);
        fn_8022BE24(player, &v);
    }
}
#pragma auto_inline reset

void SeesawLift_804AD348::fn_8035A210() {
    field_0x240 = 0.0f;
    for (s32 p = 0; p < 4; p++) {
        field_0x246[p] = 0;
        field_0x24a[p] = 0;
        if (isPlayerActive_8023F264(p)) {
            field_0x246[p] = fn_8035AE9C(p);
            checkContactBox(p);
        }
    }
    if ((field_0x24a[0] | field_0x24a[1] | field_0x24a[2] | field_0x24a[3]) != 0 && field_0x240 != 0.0f) {
        playSe_801F0D20(3);
    }
    for (s32 p = 0; p < 4; p++) {
        pushPlayer(p);
    }
    for (s32 p = 0; p < 4; p++) {
        if (field_0x24a[p] != 0) {
            for (s32 q = 0; q < 4; q++) {
                if (!hasPlayerLeader_8022F514(q) && field_0x24a[q] == 0 && field_0x246[q] != 0 && field_0x24a[p] * field_0x246[q] < 0) {
                    fn_802314D8(q, JGeometry::TVec3f(0.0f, 0.0f, 10.0f));
                }
            }
        }
    }
    if (field_0x244 == 0) {
        field_0x23a = (field_0x240 - 0.15f * (f32)field_0x238 - 0.008f * (f32)field_0x230) / 15.0f;
    } else if (field_0x230 > 0) {
        s32 mirrored = field_0x230 - 0x8000;
        field_0x23a = (field_0x240 - 0.15f * (f32)field_0x238 - 0.008f * (f32)mirrored) / 15.0f;
    } else {
        s32 mirrored = field_0x230 + 0x8000;
        field_0x23a = (field_0x240 - 0.15f * (f32)field_0x238 - 0.008f * (f32)mirrored) / 15.0f;
    }
    if (field_0x238 == 0 && field_0x23a != 0) {
        pool_8054286C(1);
    }
    field_0x238 += field_0x23a;
    s16 oldAngle = field_0x230;
    field_0x230 += field_0x238;
    if (field_0x238 < 0) {
        if ((oldAngle > 0x4000 && field_0x230 <= 0x4000) || (oldAngle >= -0x4000 && field_0x230 < -0x4000)) {
            field_0x244 ^= 1;
        }
    } else if (field_0x238 > 0) {
        if ((oldAngle <= 0x4000 && field_0x230 > 0x4000) || (oldAngle < -0x4000 && field_0x230 >= -0x4000)) {
            field_0x244 ^= 1;
        }
    }
}

void SeesawLift_804AD348::update() {
    (this->*lbl_804AD30C[(mSpawnParam_0x90 >> 7) & 0x1F])();
}

// The rotation of game/RotMatrix.h's setRotScaleMtx, with the angles as s16
// read from JMath's sincos table.
static inline void setRotScaleMtxS16(Mtx m, s16 x, s16 y, s16 z, f32 scale) {
    f32 sx = JMath::sincosTable_.sinShort(x);
    f32 sy = JMath::sincosTable_.sinShort(y);
    f32 sz = JMath::sincosTable_.sinShort(z);
    f32 cx = JMath::sincosTable_.cosShort(x);
    f32 cy = JMath::sincosTable_.cosShort(y);
    f32 cz = JMath::sincosTable_.cosShort(z);
    m[0][0] = scale * (cy * cz);
    m[1][0] = scale * (cy * sz);
    m[2][0] = scale * -sy;
    m[0][1] = scale * (sx * sy * cz - cx * sz);
    m[1][1] = scale * (sx * sy * sz + cx * cz);
    m[2][1] = scale * (sx * cy);
    m[0][2] = scale * (cx * cz * sy + sx * sz);
    m[1][2] = scale * (cx * sz * sy - sx * cz);
    m[2][2] = scale * (cx * cy);
}

// Sets the 3x3 part of m to a uniform scale; the translation column is the caller's.
static inline void setScaleMtx(Mtx m, f32 s) {
    m[0][0] = s;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = s;
    m[1][2] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = s;
}

// Float register allocation differs in setRotScaleMtxS16.
// FIXME: 99.21 %, regalloc-only; behaviour clean.
void SeesawLift_804AD348::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    if (getKind() == 4 && field_0x230 == 0) {
        setRotScaleMtxS16(mtx, field_0x23c, 0, 0, 1.0f);
    } else {
        setRotScaleMtxS16(mtx, field_0x230, 0, 0, 1.0f);
    }
    params.mMtx = mtx;
    params.field_0x21 = 1;
    params.field_0x1d = -2;
    params.field_0x25 = 1;
    if (getMode() == 1) {
        params.field_0x24 = 1;
    }
    const char* name = lbl_8053A1B8[getMode()];
    if (mLayer_0x4 < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('TIMG', name);
        gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&mPos, res, &params);
    }
    if (getKind() < 2) {
        params.field_0x1c = -1;
        params.field_0x1d = -2;
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;
        setScaleMtx(mtx, 1.5f);
        params.mMtx = mtx;
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('TIMG', "otosiana_bg_s.bti");
        JGeometry::TVec3f pos(mPos.x, mPos.y, 0.0f);
        if (mLayer_0x4 < 8) {
            gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&pos, res, &params);
        }
    }
}

void SeesawLift_804AD348::fn_803597EC() {
    s32 mask = 0;
    field_0x240 = 0.0f;
    field_0x24e = 0;
    if (field_0x254 != 0) {
        field_0x254--;
        if (field_0x254 == 0) {
            playSe_801F0D20(3);
        }
    }
    for (s32 p = 0; p < 4; p++) {
        field_0x246[p] = 0;
        field_0x24a[p] = 0;
        if (isPlayerActive_8023F264(p)) {
            field_0x246[p] = fn_8035AE9C(p);
            if (fn_80235448(p) && field_0x246[p] != 0) {
                field_0x24e |= 1 << p;
            }
        }
        checkContactBox(p);
        mask |= field_0x24e;
        mask |= field_0x24a[p] << p;
    }
    if (mask != 0) {
        if (field_0x238 == 0 && field_0x240 != 0.0f) {
            field_0x254 = 30;
            pool_8054286C(1);
        }
        if (field_0x240 > 0.0f) {
            field_0x238 = 0x200;
        } else if (field_0x240 < 0.0f) {
            field_0x238 = -0x200;
        }
    }
    for (s32 p = 0; p < 4; p++) {
        pushPlayer(p);
    }
    for (s32 p = 0; p < 4; p++) {
        if (field_0x24a[p] != 0) {
            for (s32 q = 0; q < 4; q++) {
                if (!hasPlayerLeader_8022F514(q) && field_0x24a[q] == 0 && field_0x246[q] != 0 && field_0x24a[p] * field_0x246[q] < 0) {
                    fn_802314D8(q, JGeometry::TVec3f(0.0f, 0.0f, 10.0f));
                }
            }
        }
    }
    s16 oldAngle = field_0x230;
    field_0x230 += field_0x238;
    if (field_0x238 < 0) {
        if ((oldAngle > 0x4000 && field_0x230 <= 0x4000) || (oldAngle >= -0x4000 && field_0x230 < -0x4000)) {
            field_0x244 ^= 1;
        }
    } else if (field_0x238 > 0) {
        if ((oldAngle <= 0x4000 && field_0x230 > 0x4000) || (oldAngle < -0x4000 && field_0x230 >= -0x4000)) {
            field_0x244 ^= 1;
        }
    }
    if (field_0x230 == 0 || field_0x230 == -0x8000) {
        if (field_0x238 != 0) {
            pool_8054286C(0);
        }
        field_0x238 = 0;
    }
}

void SeesawLift_804AD348::fn_80358F98() {
    if (field_0x245) {
        GameRootSub20_801902C0* sub = fn_801902C0();
        if (sub->field_0x79c < 16) {
            GameRootSub20_801902C0::SandFall& fall = sub->mSandFalls[sub->field_0x79c];
            fall.mPos.x = mPos.x;
            fall.mPos.y = mPos.y;
            fall.mPos.z = mPos.z;
            sub->mSandFalls[sub->field_0x79c].field_0xc[0] = 0;
            sub->field_0x79c++;
        }
    }
    field_0x240 = 0.0f;
    for (s32 p = 0; p < 4; p++) {
        field_0x246[p] = 0;
        field_0x24a[p] = 0;
        if (isPlayerActive_8023F264(p)) {
            field_0x246[p] = fn_8035AE9C(p);
            checkContactBox(p);
        }
    }
    if ((field_0x24a[0] | field_0x24a[1] | field_0x24a[2] | field_0x24a[3]) != 0 && field_0x240 != 0.0f) {
        playSe_801F0D20(3);
    }
    for (s32 p = 0; p < 4; p++) {
        pushPlayer(p);
    }
    for (s32 p = 0; p < 4; p++) {
        if (field_0x24a[p] != 0) {
            for (s32 q = 0; q < 4; q++) {
                if (!hasPlayerLeader_8022F514(q) && field_0x24a[q] == 0 && field_0x246[q] != 0 && field_0x24a[p] * field_0x246[q] < 0) {
                    fn_802314D8(q, JGeometry::TVec3f(0.0f, 0.0f, 10.0f));
                }
            }
        }
    }
    if (field_0x240 != 0.0f) {
        if (gGameRoot_8053AAF8->field_0x7e == 0) {
            if (field_0x240 > 0.0f) {
                field_0x240 = 1500.0f;
            } else {
                field_0x240 = -1500.0f;
            }
        } else if (field_0x240 > 0.0f) {
            field_0x240 = 10.0f;
        } else {
            field_0x240 = -10.0f;
        }
        field_0x23a = (s16)field_0x240;
    } else {
        field_0x23a = (s16)((-(0.1f * (f32)field_0x238) - 0.01f * (f32)field_0x230) / 15.0f);
    }
    field_0x238 += field_0x23a;
    s16 oldAngle = field_0x230;
    field_0x230 += field_0x238;
    if (getKind() == 2) {
        if (field_0x230 <= -0x3E00) {
            field_0x230 = -0x3E00;
            field_0x238 = 0;
            field_0x23a = 0;
        } else if (field_0x230 >= 0) {
            field_0x230 = 0;
            field_0x238 = 0;
            field_0x23a = 0;
            field_0x245 = 0;
            pool_8054286C(0);
        }
    } else {
        if (field_0x230 >= 0x3E00) {
            field_0x230 = 0x3E00;
            field_0x238 = 0;
            field_0x23a = 0;
        } else if (field_0x230 <= 0) {
            field_0x230 = 0;
            field_0x238 = 0;
            field_0x23a = 0;
            field_0x245 = 0;
            pool_8054286C(0);
        }
    }
    if (oldAngle == 0 && field_0x230 != 0) {
        pool_80542878();
        field_0x245 = 1;
        pool_8054286C(1);
    }
}

// FIXME(shotgun): 98.51 %, missing/extra; behaviour near.
void SeesawLift_804AD348::fn_803584C0() {
    if (field_0x250 > 0) {
        field_0x250--;
    }
    if (field_0x26c > 0) {
        field_0x26c--;
    }
    if (field_0x245) {
        GameRootSub20_801902C0* sub = fn_801902C0();
        if (sub->field_0x79c < 16) {
            GameRootSub20_801902C0::SandFall& fall = sub->mSandFalls[sub->field_0x79c];
            fall.mPos.x = mPos.x;
            fall.mPos.y = mPos.y;
            fall.mPos.z = mPos.z;
            sub->mSandFalls[sub->field_0x79c].field_0xc[0] = 0;
            sub->field_0x79c++;
        }
    }
    field_0x240 = 0.0f;
    for (s32 p = 0; p < 4; p++) {
        field_0x246[p] = 0;
        field_0x24a[p] = 0;
        if (isPlayerActive_8023F264(p)) {
            field_0x246[p] = fn_8035AC84(p);
            checkContactBox(p);
            if (field_0x25c[p] == NULL) {
                if (fn_8023EFEC(p) && getPlayerHeldActorId_8023EEB4(p) == 'YASI') {
                    field_0x25c[p] = getActorManager_801F856C()->findActorBySerial_801F666C(fn_8023EF50(p));
                }
            } else if (!field_0x25c[p]->mAlive) {
                field_0x25c[p] = NULL;
            } else if (field_0x230 == 0) {
                if (!field_0x25c[p]->mVel_0x3c.isZero() &&
                    field_0x25c[p]->mPos.z + field_0x25c[p]->mVel_0x3c.z < 1.0f) {
                    JGeometry::TBox2f box = mBox;
                    JGeometry::TVec3f diff = field_0x25c[p]->mPos;
                    diff.sub(mPos);
                    if (contains(box, diff)) {
                        field_0x25c[p]->vfunc_0x48();
                        field_0x25c[p] = NULL;
                        if (diff.y >= 0.0f) {
                            field_0x270 = 0x20;
                        } else {
                            field_0x270 = -0x20;
                        }
                        field_0x26c = 200;
                    }
                }
            }
        }
    }
    if (field_0x250 == -1) {
        for (s32 i = 0; i < 4; i++) {
            if (field_0x246[i] != 0) {
                for (s32 j = 0; j < 4; j++) {
                    if (field_0x246[j] == 0) {
                        field_0x250 = 6;
                        break;
                    }
                }
            }
        }
    }
    if ((field_0x24a[0] | field_0x24a[1] | field_0x24a[2] | field_0x24a[3]) != 0 && field_0x240 != 0.0f) {
        playSe_801F0D20(3);
    }
    for (s32 p = 0; p < 4; p++) {
        pushPlayer(p);
    }
    for (s32 p = 0; p < 4; p++) {
        if (field_0x24a[p] != 0) {
            for (s32 q = 0; q < 4; q++) {
                if (!hasPlayerLeader_8022F514(q) && field_0x24a[q] == 0 && field_0x246[q] != 0 && field_0x24a[p] * field_0x246[q] < 0) {
                    fn_802314D8(q, JGeometry::TVec3f(0.0f, 0.0f, 10.0f));
                }
            }
        }
    }
    fn_8035AA54();
    s16 oldAngle = field_0x230;
    if (!isClearOfPlayers()) {
        field_0x238 = 0;
    }
    field_0x230 += field_0x238;
    if (field_0x238 > 0 && field_0x230 >= 0x3E00) {
        field_0x230 = 0x3E00;
        field_0x238 = 0;
        field_0x23a = 0;
    } else if (field_0x238 < 0 && field_0x230 <= -0x3E00) {
        field_0x230 = -0x3E00;
        field_0x238 = 0;
        field_0x23a = 0;
    } else if (field_0x240 == 0.0f && field_0x230 * oldAngle <= 0) {
        field_0x230 = 0;
        field_0x238 = 0;
        field_0x23a = 0;
        field_0x245 = 0;
        pool_8054286C(0);
        if (field_0x250 == 0) {
            field_0x250 = -1;
        }
    }
    if (oldAngle == 0 && field_0x230 != 0) {
        pool_80542878();
        field_0x245 = 1;
        pool_8054286C(1);
    }
    if (field_0x230 != 0) {
        field_0x250 = 0;
    }
    if (field_0x230 == 0 && ((field_0x246[0] | field_0x246[1] | field_0x246[2] | field_0x246[3]) != 0 || field_0x26c != 0)) {
        if (field_0x26c != 0) {
            field_0x23c += field_0x270;
            if (field_0x23c >= 0xC80 || field_0x23c < -0xC80) {
                field_0x23e *= -1;
            }
        } else {
            field_0x23c += field_0x23e;
            if (field_0x23c >= 0x640 || field_0x23c < -0x640) {
                field_0x23e *= -1;
            }
        }
    } else {
        field_0x23c = 0;
    }
}

SeesawLift_804AD348::~SeesawLift_804AD348() {}

// Dead-stripped helpers inlined into their callers; compiled first (bottom of the
// file), their out-of-line copies emitted the leading .sdata2 literals (0.0f,
// 16.0f, 7.0f, 1.0f) ahead of fn_803584C0's block, as in the original.
// R2 pool helper (inlined at 2 call sites); was spawnSandEmitters.
void SeesawLift_804AD348::pool_80542878() {
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3DE, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3DF, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3E0, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3E1, 0, 0, NULL, NULL, true, 1.0f);
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 9-line definition): split out only where the .sdata2 order needs it.
u8 SeesawLift_804AD348::isPlayerInside(s32 player) const {
    JGeometry::TVec3f rel = *getPlayerPos_8023E724(player);
    JGeometry::TBox2f box(mBox);
    box.i.x += 7.0f;
    box.f.x -= 7.0f;
    rel.sub(mPos);
    rel.z = 0.0f;
    return (rel.x >= box.i.x && rel.y >= box.i.y && rel.x < box.f.x && rel.y < box.f.y) ? true : false;
}

// R2 pool helper (inlined at 9 call sites); was fillTiles.
void SeesawLift_804AD348::pool_8054286C(s32 state) {
    JGeometry::TVec3f p;
    JGeometry::TVec3f base;
    p = mPos + JGeometry::TVec3f(mBox.i.x, mBox.i.y, 0.0f);
    base = p;
    field_0x258 = state;
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 6; col++) {
            gBgRewrite_8053AC98->writeTile_802A381C(&p, mLayer_0x4, lbl_80542868[state]);
            p.y += 16.0f;
        }
        p.y = base.y;
        p.x += 16.0f;
    }
}
