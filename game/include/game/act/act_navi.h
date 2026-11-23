#ifndef GAME_ACT_ACT_NAVI_H
#define GAME_ACT_ACT_NAVI_H

#include "types.h"
#include "game/EnemyBase.h"

class JPABaseEmitter;

// Code: game/act_navi.cpp. The Navi actor ('NAVI', docs/names/actors.tsv),
// an EnemyBase_804A4808 named after its vtable, 0x804A6708.
class Navi_804A6708 : public EnemyBase_804A4808 {
public:
    // 0x8021EDC0.
    Navi_804A6708();
    // 0x8021ED04: releases emitters and unlists serial from navi_list_8021F064 list.
    /* 0x08 */ virtual ~Navi_804A6708();
    // 0x8021E8D0.
    /* 0x0C */ virtual void init();
    // 0x8021D7A8: draws SOB sprite "enemy_both2_sob.bin".
    /* 0x20 */ virtual void draw();
    // 0x8021DA20.
    /* 0x88 */ virtual void calcVelocity_0x88();
    // 0x8021DC44: state handler.
    /* 0x90 */ virtual void updateState_0x90(u8 mapHit);
    // 0x8021D9D4.
    /* 0x98 */ virtual void move_0x98();
    // 0x8021DB24.
    /* 0x9C */ virtual void updateBehavior_0x9C(u8 mapHit);
    // 0x8021D938: focus point / screen flash.
    /* 0xA0 */ virtual void vfunc_0xA0();

    // 0x8021D734: sets state to 8 if not already 8.
    void fn_8021D734();
    // 0x8021D764: sets state to 6 if not already 6.
    void fn_8021D764();
    // 0x8021D794: returns true if state == 5.
    bool fn_8021D794();

    // Inlined at each spawnEmitter_8013CC50 flag argument (cmpwi 9; beq; b over li 1).
    bool isState9() const {
        bool ret = false;
        switch (mState) {
        case 9:
            ret = true;
            break;
        }
        return ret;
    }

    bool isFollowing() const {
        bool ret = false;
        if (field_0x2b4 >= 0 && (mFlags & 0x30000)) {
            ret = true;
        }
        return ret;
    }

    // 0x8021DFAC: state transition helper.
    void setState_8021DFAC(s32 newState);
    // 0x8021E4F8.
    void fn_8021E4F8();

    /* 0x2A8 */ s32 field_0x2a8;
    /* 0x2AC */ u8 field_0x2ac;
    /* 0x2AD */ u8 field_0x2ad;
    /* 0x2AE */ u8 pad_0x2ae[2];
    /* 0x2B0 */ s32 field_0x2b0;
    /* 0x2B4 */ s32 field_0x2b4;
    /* 0x2B8 */ f32 field_0x2b8;
    /* 0x2BC */ f32 field_0x2bc;
    // fn_8021E4F8 passes &field_0x2c0 to fn_801CD754 as a TVec2f and copies both pairs as TVec2f.
    /* 0x2C0 */ JGeometry::TVec2f field_0x2c0;
    /* 0x2C8 */ JGeometry::TVec2f field_0x2c8;
    /* 0x2D0 */ JPABaseEmitter* mEmitter2D0;
    /* 0x2D4 */ JPABaseEmitter* mEmitter2D4;
}; // Size: 0x2D8

#endif
