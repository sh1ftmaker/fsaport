#ifndef GAME_ACT_ACT_FOUR_SWORD_DEMO_H
#define GAME_ACT_ACT_FOUR_SWORD_DEMO_H

// Code: game/act_four_sword_demo.cpp.

#include "game/act/demo_actor_base_8010DF44.h"
#include "game/SpriteAnim.h"

// Named after its vtable, 0x8049DA0C, which derives from DemoActorBase_8049D140. Its
// constructor (0x801180D0, game/act_four_sword_demo.cpp) clears field_0x230,
// field_0x231 and field_0x234. The destructor (0x80118018) runs four rounds over the
// emitter arrays, releasing each slot and clearing it; mEmitter2a8 is
// released in every round. Then it destroys mAnim250 and runs
// DemoActorBase_8049D140's destructor.
class FourSwordCutscene_8049DA0C : public DemoActorBase_8049D140 {
public:
    FourSwordCutscene_8049DA0C();
    /* 0x08 */ virtual ~FourSwordCutscene_8049DA0C();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x20 */ virtual void draw();

    // Slot 0x54, overriding DemoActorBase_8049D140::vfunc_0x54 (0x801174C0, game/act_four_sword_demo.cpp).
    // Reads mLayer_0x4 and field_0x8 of fn_8011921C()'s object and, when the
    // first is 2, calls fn_80117074 with the index of the matching
    // lbl_80464A7C entry.
    /* 0x54 */ virtual void vfunc_0x54();

    // Mangled fn_80117074__26FourSwordCutscene_8049DA0CFl (0x80117074, game/act_four_sword_demo.cpp).
    // Takes the table index 2..12 found by vfunc_0x54.
    void fn_80117074(s32 index);

    // Slot 0x58, overriding DemoActorBase_8049D140::vfunc_0x58 (game/demo_actor_base_8010DF44.cpp).
    // Returns field_0x234.
    /* 0x58 */ virtual s32 vfunc_0x58();

    // Slot 0x5C, overriding DemoActorBase_8049D140::vfunc_0x5C (game/demo_actor_base_8010DF44.cpp).
    // 1 when field_0x234 is 12, else 0.
    /* 0x5C */ virtual s32 vfunc_0x5C();

    // Not in the binary: plain functions that fn_80117074 and update inline.
    void spawnPlayerEffects();
    void startState5();
    void releaseEmitters();
    void startState7();
    void startState8();
    void startState11();
    bool spreadPlayers();
    bool raisePlayers();
    bool checkPlayers();
    bool showMessage();

    void fn_80116E8C();

    /* 0x230 */ u8 field_0x230;
    /* 0x231 */ u8 field_0x231;
    /* 0x234 */ s32 field_0x234;
    /* 0x238 */ s32 field_0x238;
    /* 0x23C */ f32 field_0x23c;
    /* 0x240 */ f32 field_0x240;
    /* 0x244 */ JGeometry::TVec3f field_0x244;

    /* 0x250 */ SpriteAnim_801D267C mAnim250; // its J3DFrameCtrl at 0x274
    /* 0x288 */ JPABaseEmitter* mEmitters288[4];
    /* 0x298 */ JPABaseEmitter* mEmitters298[4];
    /* 0x2A8 */ JPABaseEmitter* mEmitter2a8;
}; // Size: at least 0x2AC

#endif
