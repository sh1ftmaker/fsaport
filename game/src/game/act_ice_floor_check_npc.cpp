// The IFCK actor ("Ice-floor check NPC", docs/names/actors.tsv, medium
// confidence). init() records the collision tile under it (getRawTile_80226850) and
// registers its serial in the list of ice_floor_check_list_8012F3E4; update() watches for the
// tile to change.

#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/Map.h"
#include "game/act/act_ice_floor_check_npc.h"
#include "game/ice_floor_check_list_8012F3E4.h"

IceFloorCheckNPC_8049E950::IceFloorCheckNPC_8049E950() : mState(0), mTile(0) {}

IceFloorCheckNPC_8049E950::~IceFloorCheckNPC_8049E950() {}

void IceFloorCheckNPC_8049E950::init() {
    ActorBase_804A5770::init();
    mTile = getRawTile_80226850(&mPos, mLayer_0x4);
    fn_8012F554()->fn_8012F530(this);
}

void IceFloorCheckNPC_8049E950::update() {
    ActorBase_804A5770::update();
    switch (mState) {
    case 0:
        if (mTile != getRawTile_80226850(&mPos, mLayer_0x4)) {
            if (getScreenFlagBit_0x3C() > 0) {
                setScreenFlag_0x30(0xFF, true);
            }
            mState = 1;
        }
        break;
    case 1:
        break;
    }
}

bool IceFloorCheckNPC_8049E950::fn_8012F008() {
    switch (mState) {
    case 0:
        break;
    case 1:
        return true;
    }
    return false;
}
