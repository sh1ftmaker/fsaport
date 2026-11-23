#ifndef GAME_ACT_ACT_HORSE_RACE_MAN_H
#define GAME_ACT_ACT_HORSE_RACE_MAN_H

// Code: game/act_horse_race_man.cpp.

#include "game/EnemyBase.h"
#include "game/stopwatch_801ED3DC.h"
#include "game/horse_race_table_80215A94.h"

// The horse-race man (HRMN in docs/names/actors.tsv). Named after its vtable,
// 0x804A5630; its overrides are in the unit's vtable at 0x90-0xA0.
class HorseRaceMan_804A5630 : public EnemyBase_804A4808 {
public:
    // 0x801ED144. Sets the player count, clears the race state and starts the
    // countdown and the entry table.
    HorseRaceMan_804A5630();
    // 0x801ED0AC (deleting destructor). Empty in source: the base and member
    // destructors are inline and the deleting form adds the __dl__ call.
    virtual ~HorseRaceMan_804A5630();
    /* 0x0C */ virtual void init();
    /* 0x20 */ virtual void draw();
    /* 0x90 */ virtual void updateState_0x90(u8 mapHit);
    /* 0x94 */ virtual void updateAnim_0x94();
    /* 0x9C */ virtual void updateBehavior_0x9C(u8 mapHit);

    void fn_801EC7B8();
    void setState_801ECB08(s32 state);

    /* 0x2A8 */ s32 field_0x2a8;   // getPlayerCount_80138A30(): the number of players taking part
    /* 0x2AC */ s32 field_0x2ac;   // zeroed by the constructor
    /* 0x2B0 */ s32 field_0x2b0;   // -1 from the constructor
    /* 0x2B4 */ u8 field_0x2b4;    // zeroed by the constructor
    /* 0x2B6 */ StartCountdown_801ED828 mCountdown;
    /* 0x2B8 */ HorseRaceTable_80215E48 mTable;
};

#endif
