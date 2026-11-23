#ifndef GAME_ACT_ACT_UNKNOWN_LNBM_801BEDBC_H
#define GAME_ACT_ACT_UNKNOWN_LNBM_801BEDBC_H

// Code: game/act_unknown_lnbm_801BEDBC.cpp.

#include "game/EnemyBase.h"

// Named after its vtable, 0x804A4188.
class UnknownLNBM_804A4188 : public EnemyBase_804A4808 {
public:
    UnknownLNBM_804A4188();
    /* 0x08 */ virtual ~UnknownLNBM_804A4188();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x4C */ virtual bool isBlocked_0x4C(s32 layer, const JGeometry::TVec3f* pos);
    /* 0x54 */ virtual void onFirstUpdate_0x54();

    // Called from act_armos and act_on_off_switch with an actor found by
    // serial (findActorBySerial_801F666C). Spawns the burst effects and sets FLAG_BURST
    // unless the actor is dead or has already burst.
    void fn_801BEDBC();

    // Bit of mFlags (0x230) beyond EnemyFlag_804A4808: update() sets it when
    // the actor leaves the view or is blocked, and releases the emitter on
    // the next frame.
    enum { FLAG_BURST = 0x2000 };

private:
    void spawnBurst();
    void moveEmitter();
    bool blockedAt(f32 dx, f32 dy);
    void burnTiles();

public:
    /* 0x2A8 */ JPABaseEmitter* mEmitter2a8; // released by the destructor
}; // Size: 0x2AC

#endif
