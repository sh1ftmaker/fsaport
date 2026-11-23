#ifndef GAME_ACT_DEMO_ACTOR_BASE_8010DF44_H
#define GAME_ACT_DEMO_ACTOR_BASE_8010DF44_H

// Code: game/demo_actor_base_8010DF44.cpp.

#include "game/ActorBase.h"

// Named after its vtable, 0x8049D140. Adds three slots after ActorBase's 19.
class DemoActorBase_8049D140 : public ActorBase_804A5770 {
public:
    DemoActorBase_8049D140();
    /* 0x08 */ virtual ~DemoActorBase_8049D140();
    /* 0x0C */ virtual void init();
    /* 0x14 */ virtual void update();
    /* 0x20 */ virtual void draw();
    /* 0x54 */ virtual void vfunc_0x54();
    /* 0x58 */ virtual s32 vfunc_0x58();
    /* 0x5C */ virtual s32 vfunc_0x5C();
}; // Size: 0x230

#endif
