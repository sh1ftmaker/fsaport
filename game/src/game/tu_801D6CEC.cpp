#include "game/GameAudio.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "game/act/act_vaati_boss.h"
#include "game/act/act_unknown_bgbl_801D7AFC.h"
#include "JSystem/JMath/JMath.h"

void cls_801D6A88::fn_801D789C() {
    field_0x0 = 0;
    field_0x4 = 0;
    field_0x8 = 0;
    field_0xc.fn_8021BD18(2);
}

void cls_801D6A58::fn_801D7874() {
    field_0x0 = -1;
    field_0x4 = 0;
    field_0x5 = 0;
    vel.set(0.0f, 0.0f, 0.0f);
}

void cls_801D6A58::fn_801D7830() {
    vel.x *= 0.97f;
    vel.y *= 0.97f;
    vel.z += -0.2f;
    if (vel.z < -4.0f) {
        vel.z = -4.0f;
    }
}

// An inline of its own: the unrolled loop's pointer and counter take the registers the original
// has (docs/REGALLOC.md, rule 4).
static inline void clearSlots(cls_801D7730* self) {
    for (u32 i = 0; i < 16; i++) {
        self->field_0x0[i] = -1;
    }
}

cls_801D7730::cls_801D7730() {
    clearSlots(this);
    field_0x40 = 0;
    field_0x44 = 0;
    field_0x48.set(0.0f, 0.0f, 0.0f);
    field_0x54 = 0;
    field_0x56 = 0;
    field_0x58 = 0;
    field_0x5c = 0.0f;
    field_0x60 = 0.0f;
    field_0x64 = 0;
}

// FIXME(shotgun): 98.94 %, regalloc-only; behaviour clean.
cls_801D7730::~cls_801D7730() {
    fn_801D7638();
}

void cls_801D7730::fn_801D76B4() {
    for (u32 i = 0; i < 16; i++) {
        field_0x0[i] = -1;
    }
    field_0x40 = 0;
    field_0x44 = 0;
    field_0x48.set(0.0f, 0.0f, 0.0f);
    field_0x54 = 0;
    field_0x56 = 0;
    field_0x58 = 0;
    field_0x5c = 0.0f;
    field_0x60 = 0.0f;
    field_0x64 = 0;
}

// Clears mAlive of each actor named by one of the 16 serials (serials below 0
// are empty slots). Vaati calls it on field_0x4c4 right before resetting it with fn_801D76B4.
void cls_801D7730::fn_801D7638() {
    s32* p = field_0x0;
    s32 serial;
    for (s32 i = 0; i < 0x10; i++, p++) {
        serial = *p;
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->mAlive = false;
            }
        }
    }
}

void cls_801D7730::fn_801D74E0(const JGeometry::TVec3f* pos, s32 layer) {
    fn_801D76B4();
    field_0x40 = layer;
    field_0x48.set(pos->x, -22.0f + pos->y, pos->z);
    if (isUpperLayer_801CD2F8(field_0x40)) {
        field_0x48.y = -25.0f + pos->y;
    }
    s32 count;
    if (isUpperLayer_801CD2F8(field_0x40)) {
        field_0x44 = 1;
        count = 1;
    } else {
        field_0x44 = 2;
        count = 16;
    }
    for (s32 i = 0; i < count; i++) {
        field_0x0[i] = getActorManager_801F856C()->spawnActor_801F9484('BGBL', field_0x40, &field_0x48, 0, -1, -1);
    }
}

// The ring's slot i is on a circle of radius field_0x5c around (field_0x48.x, field_0x48.y).
// FIXME: 99.49 %, regalloc-only; behaviour clean.
// FIXME(shotgun): the original stores d.x/d.y as one paired-single pair; ours uses separate stores.
void cls_801D7730::fn_801D72C0(const JGeometry::TVec3f* pos) {
    field_0x48.set(pos->x, -22.0f + pos->y, pos->z);
    if (isUpperLayer_801CD2F8(field_0x40)) {
        field_0x48.y = -25.0f + pos->y;
    }
    s32 serial;
    for (s32 i = 0; i < 16; i++) {
        serial = field_0x0[i];
        if (serial >= 0) {
            ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (a != NULL) {
                s16 ang = -(s16)(field_0x54 + i * 0xFFF);
                f32 s = JMath::sincosTable_.sinShort(ang);
                f32 c = JMath::sincosTable_.cosShort(ang);
                JGeometry::TVec3f off(0.0f, field_0x5c, 0.0f);
                JGeometry::TVec3f d(field_0x48.x + (off.x * c - off.y * s), field_0x48.y + (off.x * s + off.y * c),
                                    field_0x48.z);
                d.sub(field_0x48);
                d.normalize();
                d.scale(10.0f);
                a->mVel_0x3c = JGeometry::TVec3f(d.x, d.y, 0.0f);
            }
        }
    }
    field_0x44 = 4;
}

void cls_801D7730::fn_801D70BC(const JGeometry::TVec3f* pos, s32 player) {
    field_0x48.set(pos->x, -22.0f + pos->y, pos->z);
    if (isUpperLayer_801CD2F8(field_0x40)) {
        field_0x48.y = -25.0f + pos->y;
    }
    s32 serial = field_0x0[0];
    if (serial >= 0) {
        ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (a != NULL) {
            JGeometry::TVec2f v;
            if (player >= 0 && player < 4) {
                JGeometry::TVec2f d(getPlayerPos_8023E724(player)->x - a->mPos.x,
                                    getPlayerPos_8023E724(player)->y - a->mPos.y);
                if (d.isZero()) {
                    d.set(1.0f, 0.0f);
                } else {
                    d.normalize();
                }
                s16 ang = -(s16)(randInt_801CD664(0xE39) - 0x71C);
                f32 c = JMath::sincosTable_.cosShort(ang);
                f32 s = JMath::sincosTable_.sinShort(ang);
                v.set(d.x * c - d.y * s, d.x * s + d.y * c);
            } else {
                s16 ang = randInt_801CD664(0x10000);
                v.set(JMath::sincosTable_.sinShort(ang), JMath::sincosTable_.cosShort(ang));
            }
            v.scale(5.0f);
            a->mVel_0x3c = JGeometry::TVec3f(v.x, v.y, 0.0f);
        }
    }
    field_0x44 = 4;
}

// FIXME(shotgun): 97.93 %, missing/extra; behaviour clean.
void cls_801D7730::fn_801D6E20(const JGeometry::TVec3f* pos) {
    field_0x48.set(pos->x, -22.0f + pos->y, pos->z);
    if (isUpperLayer_801CD2F8(field_0x40)) {
        field_0x48.y = -25.0f + pos->y;
    }
    switch (field_0x44) {
    case 1: {
        s32 serial = field_0x0[0];
        if (serial >= 0) {
            ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (a != NULL) {
                a->mPos = field_0x48;
            }
        }
        break;
    }
    case 2: {
        field_0x64++;
        if (field_0x64 >= 0x2D) {
            field_0x44 = 3;
        } else {
            f32 t = (f32)(0x2D - field_0x64);
            if (t < FLT_EPSILON) {
                field_0x60 = 1.0f;
            } else {
                field_0x60 = (1.0f / t) * (1.0f - field_0x60) + field_0x60;
            }
            if (t < FLT_EPSILON) {
                field_0x5c = 60.0f;
            } else {
                field_0x5c = (1.0f / t) * (60.0f - field_0x5c) + field_0x5c;
            }
        }
        break;
    }
    case 3:
        if (field_0x58 != 0) {
            stepAngle_801CD930(&field_0x56, 0, 0x20);
        } else {
            stepAngle_801CD930(&field_0x56, 0x100, 0x10);
        }
        field_0x54 += field_0x56;
        break;
    case 0:
    case 4:
        break;
    }
    if (field_0x44 < 4 && field_0x44 >= 2) {
        for (s32 i = 0; i < 16; i++) {
            s32 serial = field_0x0[i];
            if (serial >= 0) {
                ActorBase_804A5770* a = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (a != NULL) {
                    s16 ang = -(s16)(field_0x54 + i * 0xFFF);
                    f32 s = JMath::sincosTable_.sinShort(ang);
                    f32 c = JMath::sincosTable_.cosShort(ang);
                    JGeometry::TVec2f r(0.0f, field_0x5c);
                    JGeometry::TVec3f p(field_0x48.x + (r.x * c - r.y * s), field_0x48.y + (r.x * s + r.y * c), field_0x48.z);
                    a->mPos = p;
                    if (field_0x44 == 2) {
                        ((UnknownBGBL_804A4BB0*)a)->fn_801D7DB4(field_0x60);
                    }
                }
            }
        }
    }
}

void ShakeOffset_801D6CEC::fn_801D6D38(const JGeometry::TVec2f* dir) {
    JGeometry::TVec2f v(dir->x, dir->y);
    if (v.squared() <= JGeometry::TUtil<f32>::epsilon()) {
        v.x = 0.0f;
        v.y = -1.0f;
    } else {
        v.normalize();
    }
    v.x *= 20.0f;
    v.y *= 20.0f;
    fn_801E4AB0(&field_0x0, 0x3C, 0x2000, v.x);
    fn_801E4AB0(&field_0x14, 0x3C, 0x2000, v.y);
}

void ShakeOffset_801D6CEC::fn_801D6D08() {
    fn_801E4AB0(&field_0x28, 0x1E, 0x400, -3.0f);
}

// FIXME(shotgun): 81.43 %, scheduling+regalloc; behaviour clean.
void ShakeOffset_801D6CEC::fn_801D6CEC(JGeometry::TVec3f* out) {
    out->x = field_0x0.value;
    out->y = field_0x14.value;
    out->z = 0.0f;
}
