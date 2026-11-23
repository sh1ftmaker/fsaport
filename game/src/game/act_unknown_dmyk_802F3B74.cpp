// An actor (vtable 0x804AA6A0, 4CC not identified) that shows a message to
// one player with a PEFF ("Player effect") actor beside it. The spawner
// allocates 0x240 bytes and calls fn_801FF9CC, which stores the vtable.

#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Message.h"
#include "game/act/act_unknown_dmyk_802F3B74.h"

void UnknownDMYK_804AA6A0::init() {
    mMsg = mSpawnParam_0x90 & 0xFFF;
    field_0x234 = (mSpawnParam_0x90 >> 12) & 0xF;
    mPlayer = (mSpawnParam_0x90 >> 16) & 0xF;
    mEffectSerial = -1;
    if (field_0x234 == mPlayer) {
        mEffectSerial = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_PEFF, mLayer_0x4, &mPos,
                                                   (field_0x234 & 3) | 0x24, -1, -1);
        open(mLayer_0x4, mPos, 0x21);
    } else {
        mAlive = false;
    }
}

void UnknownDMYK_804AA6A0::die_0x24() {
    s32 serial = mEffectSerial;
    if (serial >= 0) {
        ActorBase_804A5770* effect = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (effect != NULL) {
            effect->mAlive = false;
        }
        mEffectSerial = -1;
    }
    mAlive = false;
}

void UnknownDMYK_804AA6A0::update() {
    if (fn_80388EEC(mPlayer)) {
        die_0x24();
    }
}

void UnknownDMYK_804AA6A0::draw() {}

UnknownDMYK_804AA6A0::~UnknownDMYK_804AA6A0() {}
