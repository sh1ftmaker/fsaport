#ifndef GAME_ACT_ACT_SHADOW_LINK_THWOMP_MINOR_802233A4_H
#define GAME_ACT_ACT_SHADOW_LINK_THWOMP_MINOR_802233A4_H

// Code: game/act_shadow_link_thwomp_minor_802233A4.cpp.

#include "game/cls_804A6A00.h"

class JPABaseEmitter;

// Spawned for 'ZSLD' (a Shadow Link shield bearer, by its sub-base): named
// after its vtable (0x804A6D00); moved here from tu_801F9484.h in the shotgun
// pass. Field names come from the asm only.
class ShadowLinkThwompMinor_804A6D00 : public ShadowLinkBase_804A6A00 {
public:
    ShadowLinkThwompMinor_804A6D00();
    /* 0x08 */ virtual ~ShadowLinkThwompMinor_804A6D00();
    /* 0x0C */ virtual void init();
    /* 0x68 */ virtual u8 attackPlayers_0x68();
    /* 0x6C */ virtual void onHitByPlayer_0x6C();
    /* 0x88 */ virtual void calcVelocity_0x88();
    /* 0x90 */ virtual void updateState_0x90(u8 mapHit);
    /* 0x9C */ virtual void updateBehavior_0x9C(u8 mapHit);

    // 0x80223AD8: enters state `state` (5-8); 0x802238CC: burns the tiles in front.
    void fn_80223AD8(s32 state);
    void fn_802238CC();

    /* 0x2B8 */ JPABaseEmitter* field_0x2b8;
    /* 0x2BC */ JPABaseEmitter* field_0x2bc;
    /* 0x2C0 */ JPABaseEmitter* field_0x2c0;
}; // Size: 0x2C4

#endif
