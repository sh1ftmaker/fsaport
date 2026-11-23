#ifndef GAME_ACT_ACT_STORY_DEMO_CONTROL_NPC_8011A0F8_H
#define GAME_ACT_ACT_STORY_DEMO_CONTROL_NPC_8011A0F8_H

// Code: game/act_story_demo_control_npc_8011A0F8.cpp.

#include "game/ActorBase.h"

// Named after its vtable, 0x8049DAD8.
class StoryDemoControlNPC_8049DAD8 : public ActorBase_804A5770 {
public:
    StoryDemoControlNPC_8049DAD8();
    /* 0x08 */ virtual ~StoryDemoControlNPC_8049DAD8();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x230 */ u32 field_0x230; // init(): the top-5-bit-stripped low 5 bits of mSpawnParam_0x90
    /* 0x234 */ s32 field_0x234; // frames counted while field_0x238 is set
    /* 0x238 */ bool field_0x238;
    /* 0x239 */ bool field_0x239;
}; // Size: 0x23C

#endif
