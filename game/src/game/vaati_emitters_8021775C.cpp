// VaatiEmitters_80217F80, the emitter set at 0x448 of Vaati (see its header). The unit's
// __sinit (the 9 JAudio pools) comes from the GameAudio.h include.

#include "game/GameAudio.h"
#include <float.h>
#include "game/GameGlobals.h"
#include "game/ActorBase.h"
#include "game/Effect.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "game/act/act_vaati_boss.h"

// The emitter scale the spawners below give their emitters.
static JGeometry::TVec3f lbl_804CD838(lbl_8053F0BC, lbl_8053F0BC, lbl_8053F0BC);

// Effect ids for the six emitters of fn_80217ED4.
static const u16 lbl_8046A5A0[8] = {0x022A, 0x022B, 0x022C, 0x022D, 0x0549, 0x022E, 0x0000, 0x0000};

VaatiEmitters_80217F80::VaatiEmitters_80217F80() {
    mEmitters[0] = NULL;
    mEmitters[1] = NULL;
    mEmitters[2] = NULL;
    mEmitters[3] = NULL;
    mEmitters[4] = NULL;
    mEmitters[5] = NULL;
    mOwner = NULL;
}

VaatiEmitters_80217F80::~VaatiEmitters_80217F80() {
    releaseEmitter_801EE434(mEmitters[0]);
    releaseEmitter_801EE434(mEmitters[1]);
    mEmitters[0] = NULL;
    mEmitters[1] = NULL;
    releaseEmitter_801EE434(mEmitters[2]);
    releaseEmitter_801EE434(mEmitters[3]);
    mEmitters[2] = NULL;
    mEmitters[3] = NULL;
    releaseEmitter_801EE434(mEmitters[4]);
    releaseEmitter_801EE434(mEmitters[5]);
    mEmitters[4] = NULL;
    mEmitters[5] = NULL;
}

// FIXME: 98.95 %, regalloc-only; behaviour clean.
// Register allocation only.
void VaatiEmitters_80217F80::fn_80217ED4() {
    VaatiBoss_804A4958* owner = mOwner;
    const JGeometry::TVec3f* pos = &owner->mPos;
    for (s32 i = 0; i < 6; i++) {
        JPABaseEmitter* emitter =
            spawnEmitter_8013CC50(owner->mLayer_0x4, pos, lbl_8046A5A0[i], 1, 1, NULL, NULL, true, 1.0f);
        if (emitter != NULL) {
            emitter->setGlobalScale(lbl_804CD838);
        }
    }
}

// Scheduling only: the original loads mOwner for the first spawn before li r9/r10.
// FIXME: 96.61 %, scheduling; behaviour clean.
void VaatiEmitters_80217F80::fn_80217DE8() {
    JPABaseEmitter* emitter =
        spawnEmitter_8013CC50(mOwner->mLayer_0x4, mOwner->getPos(), 0x231, 1, 1, NULL, NULL, true, 1.0f);
    JPABaseEmitter* emitter2 =
        spawnEmitter_8013CC50(mOwner->mLayer_0x4, mOwner->getPos(), 0x232, 1, 1, NULL, NULL, true, 1.0f);
    if (emitter != NULL) {
        emitter->setGlobalScale(lbl_804CD838);
    }
    if (emitter2 != NULL) {
        emitter2->setGlobalScale(lbl_804CD838);
    }
}

void VaatiEmitters_80217F80::fn_80217CD8() {
    if (mEmitters[0] == NULL) {
        mEmitters[0] = spawnEmitter_8013CC50(mOwner->mLayer_0x4, mOwner->getPos(), 0x228, 1, 1, NULL, NULL, true, 1.0f);
        if (mEmitters[0] != NULL) {
            mEmitters[0]->setGlobalScale(lbl_804CD838);
        }
    }
    if (mEmitters[1] == NULL) {
        mEmitters[1] = spawnEmitter_8013CC50(mOwner->mLayer_0x4, mOwner->getPos(), 0x229, 1, 1, NULL, NULL, true, 1.0f);
        if (mEmitters[1] != NULL) {
            mEmitters[1]->setGlobalScale(lbl_804CD838);
        }
    }
}

void VaatiEmitters_80217F80::fn_80217BC8() {
    if (mEmitters[2] == NULL) {
        mEmitters[2] = spawnEmitter_8013CC50(mOwner->mLayer_0x4, mOwner->getPos(), 0x22F, 1, 1, NULL, NULL, true, 1.0f);
        if (mEmitters[2] != NULL) {
            mEmitters[2]->setGlobalScale(lbl_804CD838);
        }
    }
    if (mEmitters[3] == NULL) {
        mEmitters[3] = spawnEmitter_8013CC50(mOwner->mLayer_0x4, mOwner->getPos(), 0x230, 1, 1, NULL, NULL, true, 1.0f);
        if (mEmitters[3] != NULL) {
            mEmitters[3]->setGlobalScale(lbl_804CD838);
        }
    }
}

void VaatiEmitters_80217F80::fn_80217AB8() {
    if (mEmitters[4] == NULL) {
        mEmitters[4] = spawnEmitter_8013CC50(mOwner->field_0x5fc, &mOwner->field_0x60c, 0x224, 1, 1, NULL, NULL, true, 1.0f);
        if (mEmitters[4] != NULL) {
            mEmitters[4]->setGlobalScale(lbl_804CD838);
        }
    }
    if (mEmitters[5] == NULL) {
        mEmitters[5] = spawnEmitter_8013CC50(mOwner->field_0x5fc, &mOwner->field_0x60c, 0x225, 1, 1, NULL, NULL, true, 1.0f);
        if (mEmitters[5] != NULL) {
            mEmitters[5]->setGlobalScale(lbl_804CD838);
        }
    }
}

void VaatiEmitters_80217F80::fn_80217A74() {
    releaseEmitter_801EE434(mEmitters[0]);
    releaseEmitter_801EE434(mEmitters[1]);
    mEmitters[0] = NULL;
    mEmitters[1] = NULL;
}

void VaatiEmitters_80217F80::fn_80217A30() {
    releaseEmitter_801EE434(mEmitters[2]);
    releaseEmitter_801EE434(mEmitters[3]);
    mEmitters[2] = NULL;
    mEmitters[3] = NULL;
}

void VaatiEmitters_80217F80::fn_802179EC() {
    releaseEmitter_801EE434(mEmitters[4]);
    releaseEmitter_801EE434(mEmitters[5]);
    mEmitters[4] = NULL;
    mEmitters[5] = NULL;
}

void VaatiEmitters_80217F80::fn_8021798C() {
    setEmitterPos_8013CB44(mEmitters[0], &mOwner->mPos, 1, 1, true);
    setEmitterPos_8013CB44(mEmitters[1], &mOwner->mPos, 1, 1, true);
}

static inline void approachStep(f32* dst, f32 target, f32 t) {
    if (t < FLT_EPSILON) {
        *dst = target;
    } else {
        *dst += (1.0f / t) * (target - *dst);
    }
}

static inline void approachVec(JGeometry::TVec3f* dst, JGeometry::TVec3f target, f32 t) {
    approachStep(&dst->x, target.x, t);
    approachStep(&dst->y, target.y, t);
    approachStep(&dst->z, target.z, t);
}

void VaatiEmitters_80217F80::fn_80217874(f32 t) {
    JGeometry::TVec3f pos = mOwner->mPos;
    approachVec(&pos, mOwner->field_0x60c, t);
    setEmitterPos_8013CB44(mEmitters[2], &pos, 1, 1, true);
    setEmitterPos_8013CB44(mEmitters[3], &pos, 1, 1, true);
}

void VaatiEmitters_80217F80::fn_8021775C(f32 t) {
    JGeometry::TVec3f pos = mOwner->field_0x60c;
    approachVec(&pos, mOwner->field_0x600, t);
    setEmitterPos_8013CB44(mEmitters[4], &pos, 1, 1, true);
    setEmitterPos_8013CB44(mEmitters[5], &pos, 1, 1, true);
}
