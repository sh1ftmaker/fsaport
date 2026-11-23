#ifndef GAME_ACT_ACT_FOUR_PERSON_SWITCH_8034DFF0_H
#define GAME_ACT_ACT_FOUR_PERSON_SWITCH_8034DFF0_H

// Code: game/act_four_person_switch_8034DFF0.cpp.

#include "game/ActorBase.h"

// Named after its vtable, 0x804AC9A8.
class FourPersonSwitch_804AC9A8 : public ActorBase_804A5770 {
public:
    FourPersonSwitch_804AC9A8() {}
    /* 0x08 */ virtual ~FourPersonSwitch_804AC9A8();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x20 */ virtual void draw();
    // fn_8034E050 (game/act_four_person_switch_8034DFF0.cpp): not yet named after its role.
    void fn_8034E050();
    /* 0x230 */ s32 field_0x230; // 4 once init() has seen field_0x234 == 1 and isScreenFlagSet_0x34() true
    /* 0x234 */ s32 field_0x234; // mSpawnParam_0x90 & 0xF, set by init()
    /* 0x238 */ s32 field_0x238; // 2 when init() takes its second path
    /* 0x23C */ s32 field_0x23c; // copy of field_0x238 made in init()
    /* 0x240 */ s32 field_0x240;
    /* 0x244 */ s32 field_0x244;
    /* 0x248 */ s32 field_0x248;
    /* 0x24C */ u8 field_0x24c[4]; // per player: inside the box this frame
    /* 0x250 */ u8 field_0x250[4]; // ... the frame before
    /* 0x254 */ JGeometry::TVec3f field_0x254[4]; // zeroed by init()
}; // Size: 0x284

#endif
