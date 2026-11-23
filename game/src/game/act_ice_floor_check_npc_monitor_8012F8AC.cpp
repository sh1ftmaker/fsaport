#include "game/GameAudio.h"
#include "game/act/act_ice_floor_check_npc_monitor_8012F8AC.h"
#include "game/ice_floor_check_list_8012F3E4.h"

void IceFloorCheckNPCMonitor_8049E9A8::init() {
    ActorBase_804A5770::init();
}

void IceFloorCheckNPCMonitor_8049E9A8::update() {
    ActorBase_804A5770::update();
    switch (mState) {
    case 0:
        if (fn_8012F554()->fn_8012F3E4()) {
            setScreenFlag_0x30(0xFF, true);
            mState = 1;
        }
        break;
    case 1:
        break;
    }
}

IceFloorCheckNPCMonitor_8049E9A8::~IceFloorCheckNPCMonitor_8049E9A8() {}
