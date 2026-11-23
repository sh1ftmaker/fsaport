#ifndef GAME_ACT_ACT_SHADOW_LINK_FIRE_MINOR_80222CC4_H
#define GAME_ACT_ACT_SHADOW_LINK_FIRE_MINOR_80222CC4_H

#include "game/cls_804A6A00.h"

// A minor Shadow Link actor (sub-base ShadowLinkBase_804A6A00) that spawns a 'DDGO'
// actor and releases it again; code in game/act_shadow_link_fire_minor_80222CC4.cpp. Not tied to a
// spawner ID. Named after its vtable (0x804A6C38). The constructor is
// fn_8022313C.
class ShadowLinkFireMinor_804A6C38 : public ShadowLinkBase_804A6A00 {
public:
    ShadowLinkFireMinor_804A6C38();
    virtual ~ShadowLinkFireMinor_804A6C38();
    /* 0x0C */ virtual void init();
    // Releases the 'DDGO' actor after the base's work.
    /* 0x6C */ virtual void onHitByPlayer_0x6C();
    /* 0x88 */ virtual void calcVelocity_0x88();
    /* 0x90 */ virtual void updateState_0x90(u8 mapHit);

    // The 'DDGO' actor spawned in state 5 (-1 when none): its serial.
    /* 0x2B8 */ s32 mDDGOSerial;
}; // Size: 0x2BC

#endif
