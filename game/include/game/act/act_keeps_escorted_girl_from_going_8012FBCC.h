#ifndef GAME_ACT_ACT_KEEPS_ESCORTED_GIRL_FROM_GOING_8012FBCC_H
#define GAME_ACT_ACT_KEEPS_ESCORTED_GIRL_FROM_GOING_8012FBCC_H

// Code: game/act_keeps_escorted_girl_from_going_8012FBCC.cpp.

#include "game/ActorBase.h"

// Named after its vtable, 0x8049EA00.
class KeepsEscortedGirlFromGoing_8049EA00 : public ActorBase_804A5770 {
public:
    KeepsEscortedGirlFromGoing_8049EA00() : mFlags(0) {}
    /* 0x08 */ virtual ~KeepsEscortedGirlFromGoing_8049EA00();
    /* 0x0C */ virtual void init();
    // True when there is no escorted woman ('OPGL') actor, or the 2-int
    // vector at v points the wrong way for a flag in mFlags (fn_80130030
    // calls it with an actor of this class).
    bool fn_8012FC2C(const s32* v);
    /* 0x230 */ u32 mFlags; // mSpawnParam_0x90 & 0xF; bit 0/1: v[0] must be
                            // positive/negative, bit 2/3: v[1] must be
                            // positive/negative (else fn_8012FC2C is true)
}; // Size: 0x234

#endif
