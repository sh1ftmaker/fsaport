#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/act/act_story_demo_control_npc_8011A0F8.h"

StoryDemoControlNPC_8049DAD8::StoryDemoControlNPC_8049DAD8() : field_0x230(0), field_0x234(0), field_0x238(false), field_0x239(false) {}

StoryDemoControlNPC_8049DAD8::~StoryDemoControlNPC_8049DAD8() {}

void StoryDemoControlNPC_8049DAD8::init() {
    ActorBase_804A5770::init();
    field_0x230 = mSpawnParam_0x90 & 0x1F;
}

void StoryDemoControlNPC_8049DAD8::update() {
    ActorBase_804A5770::update();
    if (!field_0x239) {
        if (field_0x238) {
            field_0x234++;
            if (field_0x234 >= 30) {
                if (fn_8011921C()->field_0x0) {
                    return;
                }
                if (!fn_8011921C()->fn_801186B0(field_0x230)) {
                    return;
                }
                field_0x239 = true;
            }
        }
        if ((mSpawnParam_0x90 & 0xF8000000) && isScreenFlagSet_0x34()) {
            field_0x238 = true;
        }
    }
}
