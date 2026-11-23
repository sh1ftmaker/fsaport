#ifndef GAME_ACT_ACT_ENEMY_GENERATOR_2_802E2074_H
#define GAME_ACT_ACT_ENEMY_GENERATOR_2_802E2074_H

// Code: game/act_enemy_generator_2_802E2074.cpp.

#include "game/ActorBase.h"

// Named after its vtable, 0x804A9BC8.
class EnemyGenerator2_804A9BC8 : public ActorBase_804A5770 {
public:
    EnemyGenerator2_804A9BC8() {}
    /* 0x08 */ virtual ~EnemyGenerator2_804A9BC8();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x20 */ virtual void draw();
    /* 0x230 */ s32 field_0x230;
    /* 0x234 */ s32 field_0x234; // grid column: mSpawnParam_0x90 bits 23-26
    /* 0x238 */ s32 field_0x238; // grid row: mSpawnParam_0x90 bits 19-22
    /* 0x23C */ s32 field_0x23c; // mSpawnParam_0x90 bits 16-18
    /* 0x240 */ s32 field_0x240;
    /* 0x244 */ u8* field_0x244; // resource from getBossArchiveResource_804032E8
    /* 0x248 */ s32 field_0x248;
    /* 0x24C */ u16 field_0x24c;
    /* 0x24E */ u16 field_0x24e;
}; // Size: 0x250

#endif
