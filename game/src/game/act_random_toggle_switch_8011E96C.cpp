#include "game/GameAudio.h"
#include "game/act/act_random_toggle_switch_8011E96C.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Player.h"

// Whether the actor's layer is 0 or any player's layer (getPlayerLayer_8023DE58).
static inline bool isOnPlayerLayer(const ActorBase_804A5770* actor) {
    if (actor->mLayer_0x4 == 0) {
        return true;
    }
    for (s32 i = 0; i < 4; i++) {
        if (actor->mLayer_0x4 == getPlayerLayer_8023DE58(i)) {
            return true;
        }
    }
    return false;
}

void RandomToggleSwitch_8049DE90::init() {
    mDuration = mSpawnParam_0x90 & 0x3F;
    mDuration *= 10;
    mCycle = (mSpawnParam_0x90 >> 6) & 1;
    mTable[0] = (mSpawnParam_0x90 >> 7) & 0x1F;
    mTable[1] = (mSpawnParam_0x90 >> 12) & 0x1F;
    mTable[2] = (mSpawnParam_0x90 >> 17) & 0x1F;
    mTable[3] = (mSpawnParam_0x90 >> 22) & 0x1F;
    bool empty = true;
    for (s32 i = 0; i < 4; i++) {
        if (mTable[i] != 0) {
            empty = false;
            break;
        }
    }
    if (empty) {
        mState = 2;
        markDead_801F3668();
    }
}

// Picks the next table entry (random, or in turn when mCycle is 1) and shows it alone. An inline
// of its own: its values get their registers before update's (docs/REGALLOC.md, rule 4).
static inline void showPickedEntry(RandomToggleSwitch_8049DE90* self) {
    s32 pick = randInt_801CD664(4);
    if (self->mCycle == 1) {
        pick = self->mNext;
        self->mNext++;
        if (self->mNext >= 4) {
            self->mNext = 0;
        }
    }
    if (self->mTable[pick] != 0) {
        for (s32 i = 0; i < 4; i++) {
            if (i != pick && self->mTable[i] != self->mTable[pick]) {
                getGameView_802D6A9C()->setSwitchBit_802D8050(self->mTable[i], false, 0xFF);
            }
        }
        getGameView_802D6A9C()->setSwitchBit_802D8050(self->mTable[pick], true, 0xFF);
    }
}

void RandomToggleSwitch_8049DE90::update() {
    switch (mState) {
    case 0:
        if (getScreenFlagBit_0x3C() != 0 && !isScreenFlagSet_0x34()) {
            mState = 1;
            break;
        }
        mTimer++;
        if (mTimer >= mDuration) {
            showPickedEntry(this);
            mTimer = 0;
        }
        if (mDuration == 0) {
            mState = 2;
            markDead_801F3668();
        }
        break;
    case 1: {
        if (mDuration == 0 && isOnPlayerLayer(this)) {
            mState = 0;
        } else if (getScreenFlagBit_0x3C() == 0) {
            mState = 0;
        } else if (isScreenFlagSet_0x34()) {
            mState = 0;
        }
        break;
    }
    case 2: // finished; nothing leaves this state
        break;
    }
}

RandomToggleSwitch_8049DE90::~RandomToggleSwitch_8049DE90() {}
