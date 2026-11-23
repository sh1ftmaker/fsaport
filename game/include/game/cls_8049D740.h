#ifndef GAME_CLS_8049D740_H
#define GAME_CLS_8049D740_H

// Code: game/act_maiden_demo.cpp. The Maiden (DMMD) actor, named after its
// vtable, 0x8049D740. Its constructor is not written yet; the destructor
// (0x8011342C) releases three emitters, then the mAnim member and DemoActorBase_8049D140's
// destructor run. Only the field PrincessZeldaDemo reads is known.

#include "game/act/demo_actor_base_8010DF44.h"
#include "game/SpriteAnim.h"

class MaidenCutscene_8049D740 : public DemoActorBase_8049D140 {
public:
    MaidenCutscene_8049D740();
    /* 0x08 */ virtual ~MaidenCutscene_8049D740();
    // Calls DemoActorBase_8049D140::init(), then sets mMode and the Maiden's animation
    // (key table, animation 5 in mode 2, frame 0).
    /* 0x0C */ virtual void init();
    /* 0x10 */ virtual void vfunc_0x10(); // copies the Goal (DMGL) actor's position
    /* 0x14 */ virtual void update();
    /* 0x20 */ virtual void draw();
    void fn_80112AE8();
    void fn_80112654(s32 kind);
    // No address (dead-stripped; update inlines both in cases 12 and 13).
    bool moveToGoal();
    void pool_8053C628();
    /* 0x54 */ virtual void vfunc_0x54(); // checks the Maiden's state against the compare table and calls fn_80112654
    /* 0x58 */ virtual s32 vfunc_0x58();
    /* 0x5C */ virtual s32 vfunc_0x5C();

    /* 0x230 */ u8 field_0x230;
    /* 0x231 */ u8 field_0x231[3];
    /* 0x234 */ s32 mMode; // mSpawnParam_0x90 & 0x1F, set by init
    /* 0x238 */ s32 field_0x238; // returned by vfunc_0x58
    /* 0x23C */ s32 field_0x23c;
    /* 0x240 */ s32 field_0x240;
    /* 0x244 */ s32 field_0x244;
    /* 0x248 */ f32 field_0x248;
    /* 0x24C */ f32 field_0x24c;
    /* 0x250 */ f32 field_0x250;
    // The position of the Goal (DMGL) actor, copied by vfunc_0x10.
    /* 0x254 */ JGeometry::TVec3f mGoalPos;
    /* 0x260 */ SpriteAnim_801D267C mAnim;
    /* 0x298 */ JPABaseEmitter* mEmitter298; // released by the destructor
    /* 0x29C */ JPABaseEmitter* mEmitter29c; // released by the destructor
    /* 0x2A0 */ JPABaseEmitter* mEmitter2a0; // released by the destructor
}; // Size: 0x2A4

u16 fn_801123C8(s32 kind, s32 mode);

#endif
