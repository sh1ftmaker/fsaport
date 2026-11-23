#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/EmitterDelete.h"
#include "game/GameGlobals.h"
#include "game/Map.h"
#include "game/EnemyBase.h"
#include "game/cls_802150B8.h"
#include "game/cls_804A64A8.h"
#include "game/act/act_shadow_link_2.h"

// Effect ids of the two flag states (.sdata2 u16 arrays, no name evidence).
// They come before the first float literal so the .sdata2 layout matches the
// original (0x8053FB90, 0x8053FB98, then the floats).
static const u16 lbl_8053FB90[4] = {0x0413, 0x0414, 0x0415, 0x0000};
static const u16 lbl_8053FB98[4] = {0x0418, 0x0419, 0x041A, 0x0000};

// The depth bias setEmitterPos_8013CB44 and spawnEmitter_8013CC50 also apply (depthPos in effect_system_8013C5C8.cpp).
static inline f32 depthLift(const JGeometry::TVec3f* pos) {
    s32 height = 0;
    s32 depth = 1;
    if (pos->z > 0.0f) {
        height++;
        depth--;
    }
    return height * 1024 + depth * 128;
}

// Copies the callback's vector into the particle (mOffsetPosition, at 0x18).
void cls_804A64A8::execute(JPABaseEmitter*, JPABaseParticle* particle) {
    if (field_0x10 == 0) {
        particle->mOffsetPosition.x = field_0x4.x;
        particle->mOffsetPosition.y = field_0x4.y;
        particle->mOffsetPosition.z = field_0x4.z;
    }
}

cls_802150B8::cls_802150B8() : field_0x0(0), field_0x18(0), field_0x30(-1), mOwner(0) {}

// The two callbacks at 0x04 and 0x1C are destroyed after this body, as members.
// FIXME: 95.56 %, scheduling+regalloc; behaviour clean.
// Register allocation only: the original keeps the delete flag
// in r30 (copied before this), this in r29 and the serial in r31; ours takes
// this into r31 first.
cls_802150B8::~cls_802150B8() {
    deleteEmitter(&field_0x0);
    s32 serial = field_0x30;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->mAlive = false;
        }
        field_0x30 = -1;
    }
    deleteEmitter(&field_0x18);
    static_cast<EnemyBase_804A4808*>(mOwner)->fn_801CDC8C(0x4000E);
}

void cls_802150B8::fn_80214E50() {
    if (field_0x0 != NULL) {
        return;
    }
    JGeometry::TVec3f pos = mOwner->mPos;
    pos.y += -24.0f;
    f32 lift = depthLift(&pos);
    f32 x = pos.x;
    f32 y = pos.y + lift;
    f32 z = pos.z + lift;
    f32 ground = gGameRoot2_8053AB10->field_0x20->getHeightAt_802F5E9C(&pos);
    z += ground - pos.z;
    field_0x4.field_0x4.x = x;
    field_0x4.field_0x4.y = y;
    field_0x4.field_0x4.z = z;
    field_0x0 = spawnEmitter_8013CC50(mOwner->mLayer_0x4, &pos, 0x33B, 0, 1, NULL, &field_0x4, true, 1.0f);
    if (field_0x0 != NULL) {
        field_0x0->setStatus(JPAEMIT_Immortal);
    }
}

void cls_802150B8::fn_80214CE4() {
    if (field_0x0 == NULL) {
        return;
    }
    JGeometry::TVec3f pos = mOwner->mPos;
    pos.y += -24.0f;
    f32 lift = depthLift(&pos);
    f32 x = pos.x;
    f32 y = pos.y + lift;
    f32 z = pos.z + lift;
    f32 ground = gGameRoot2_8053AB10->field_0x20->getHeightAt_802F5E9C(&pos);
    z += ground - pos.z;
    field_0x4.field_0x4.x = x;
    field_0x4.field_0x4.y = y;
    field_0x4.field_0x4.z = z;
    setEmitterPos_8013CB44(field_0x0, &pos, 0, 1, true);
    if (field_0x0->isEnableDeleteEmitter()) {
        deleteEmitter(&field_0x0);
    }
}

void cls_802150B8::fn_80214AF8() {
    s32 serial = field_0x30;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->mAlive = false;
        }
        field_0x30 = -1;
    }
    deleteEmitter(&field_0x18);
    static_cast<EnemyBase_804A4808*>(mOwner)->fn_801CDC8C(0x4000E);
    JGeometry::TVec3f pos = mOwner->mPos;
    pos.y += -16.0f;
    if (isUpperLayer_801CD2F8(mOwner->mLayer_0x4)) {
        field_0x30 = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_SLEF, mOwner->mLayer_0x4, &pos, 0, -1, -1);
    } else {
        f32 lift = depthLift(&pos);
        f32 x = pos.x;
        f32 y = pos.y + lift;
        f32 z = pos.z + lift;
        f32 ground = gGameRoot2_8053AB10->field_0x20->getHeightAt_802F5E9C(&pos);
        z += ground - pos.z;
        field_0x1c.field_0x4.x = x;
        field_0x1c.field_0x4.y = y;
        field_0x1c.field_0x4.z = z;
        field_0x18 = spawnEmitter_8013CC50(mOwner->mLayer_0x4, &pos, 0x565, 0, 1, NULL, &field_0x1c, true, 0.5f);
    }
    mOwner->startSound_801F0E34(0x4000E, 0);
}

void cls_802150B8::fn_80214994() {
    JGeometry::TVec3f pos = mOwner->mPos;
    pos.y += -16.0f;
    if (isUpperLayer_801CD2F8(mOwner->mLayer_0x4)) {
        s32 serial = field_0x30;
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->mPos = pos;
            }
        }
    } else if (field_0x18 != NULL) {
        f32 lift = depthLift(&pos);
        f32 x = pos.x;
        f32 y = pos.y + lift;
        f32 z = pos.z + lift;
        f32 ground = gGameRoot2_8053AB10->field_0x20->getHeightAt_802F5E9C(&pos);
        z += ground - pos.z;
        field_0x1c.field_0x4.x = x;
        field_0x1c.field_0x4.y = y;
        field_0x1c.field_0x4.z = z;
        setEmitterPos_8013CB44(field_0x18, &pos, 0, 1, true);
    }
}

void cls_802150B8::fn_80214900() {
    s32 serial = field_0x30;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->mAlive = false;
        }
        field_0x30 = -1;
    }
    deleteEmitter(&field_0x18);
    static_cast<EnemyBase_804A4808*>(mOwner)->fn_801CDC8C(0x4000E);
}

void cls_802150B8::fn_80214740(s32 layer, const JGeometry::TVec3f* pos, bool flag) {
    ShadowLink2_804A43E8* owner = static_cast<ShadowLink2_804A43E8*>(mOwner);
    s32 extra = 0;
    if (owner->field_0x524.field_0x0 & 0x400) {
        extra = 1;
    }
    // field_0x50 is a u32 in GameGlobals.h, but the original tests its sign
    // (cmpwi; blt). Retyping it breaks fn_801C53F8 (act_shadow_link_2).
    if ((s32)getShadowLinkFight_801C0D70()->field_0x50 >= 0) {
        spawnEmitter_8013CC50(layer, pos, 0x1D6, 0, extra, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(layer, pos, 0x1D7, 0, extra, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(layer, pos, 0x1D8, 0, extra, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(layer, pos, 0x1D9, 0, extra, NULL, NULL, true, 1.0f);
        mOwner->startSound_801F0E34(0x400D6, 0);
    } else {
        const u16* ids = lbl_8053FB90;
        if (flag) {
            ids = lbl_8053FB98;
        }
        spawnEmitter_8013CC50(layer, pos, ids[0], 0, extra - 1, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(layer, pos, ids[1], 0, extra, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(layer, pos, ids[2], 0, extra, NULL, NULL, true, 1.0f);
        u32 soundId = 0x40048;
        if (flag) {
            soundId = 0x4004E;
        }
        static_cast<EnemyBase_804A4808*>(mOwner)->fn_801CDCD0(layer, pos, soundId);
    }
}

void cls_802150B8::fn_802146A4() const {
    ShadowLink2_804A43E8* owner = static_cast<ShadowLink2_804A43E8*>(mOwner);
    s32 flag = 0;
    if (owner->field_0x524.field_0x0 & 0x400) {
        flag = 1;
    }
    spawnEmitter_8013CC50(owner->getField_0x4(), owner->getPos(), 0x416, 0, flag - 1, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mOwner->getField_0x4(), mOwner->getPos(), 0x417, 0, flag, NULL, NULL, true, 1.0f);
}

void cls_802150B8::fn_80214578() {
    JGeometry::TVec3f pos = mOwner->mPos;
    switch (mOwner->mDir_0x98) {
    case 0:
        pos.x -= 24.0f;
        break;
    case 1:
        pos.x += 24.0f;
        break;
    case 2:
        pos.x += 2.0f;
        pos.y -= 26.0f;
        break;
    case 3:
        pos.x -= 4.0f;
        pos.y += 18.0f;
        break;
    }
    spawnEmitter_8013CC50(mOwner->mLayer_0x4, &pos, 0xCB, 0, 1, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(mOwner->mLayer_0x4, &pos, 0xCC, 0, 1, NULL, NULL, true, 1.0f);
}

void cls_802150B8::fn_802144F0() {
    mOwner->startSound_801F0E34(0x400C3, 0);
    JGeometry::TVec3f pos = mOwner->mPos;
    pos.y += 6.0f;
    spawnEmitter_8013CC50(mOwner->mLayer_0x4, &pos, 0x3CF, 0, 0, NULL, NULL, true, 1.0f);
}
