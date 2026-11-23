#ifndef GAME_ACT_ACT_UNKNOWN_FBAL_802201D8_H
#define GAME_ACT_ACT_UNKNOWN_FBAL_802201D8_H

// Code: game/act_unknown_fbal_802201D8.cpp. The FBAL actor (spawner spawnActor_801F9484, __nw__
// 0x2B0, constructor fn_80220378). Not named: docs/names/actors.tsv does not
// tie it to one plain-word actor. Named after its vtable, 0x804A6828.

#include "game/act/tu_801D2C60.h"

class JPABaseEmitter;

class UnknownFBAL_804A6828 : public cls_804A48B0 {
public:
    // Out of line in act_unknown_fbal_802201D8 (0x80220378); the spawner calls it.
    UnknownFBAL_804A6828();
    virtual ~UnknownFBAL_804A6828();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x20 */ virtual void draw();

    /* 0x2AC */ JPABaseEmitter* mEmitter; // spawnEmitter_8013CC50 effect 0x290
}; // Size: 0x2B0

#endif
