#ifndef GAME_ACT_ACT_COLOURED_FLOOR_TOGGLE_8012DAC0_H
#define GAME_ACT_ACT_COLOURED_FLOOR_TOGGLE_8012DAC0_H

// Code: game/act_coloured_floor_toggle_8012DAC0.cpp.

#include "game/ActorBase.h"

// Named after its vtable, 0x8049E848.
class ColouredFloorToggle_8049E848 : public ActorBase_804A5770 {
public:
    ColouredFloorToggle_8049E848() : field_0x230(0), field_0x234(0), field_0x238(0), field_0x23c(0), field_0x240(0), field_0x244(0) {}
    /* 0x08 */ virtual ~ColouredFloorToggle_8049E848();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x230 */ s32 field_0x230; // state: 0 waiting, 1 held while getScreenFlagBit_0x3C() && !isScreenFlagSet_0x34()
    /* 0x234 */ s32 field_0x234; // (mSpawnParam_0x90 >> 7) & 0x1F, 0 if >= 4 (init only)
    /* 0x238 */ s32 field_0x238; // frame counter
    /* 0x23C */ s32 field_0x23c; // (mSpawnParam_0x90 & 0x7F) * 10, 0x78 if not positive
    /* 0x240 */ s32 field_0x240; // index 0..3 into the u16 tile table
    /* 0x244 */ s32 field_0x244; // (mSpawnParam_0x90 >> 22) & 0x1F: bit for GameView::isScreenFlagSet_802D800C
}; // Size: 0x248

#endif
