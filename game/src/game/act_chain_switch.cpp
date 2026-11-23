#include "game/GameAudio.h"
#include "game/act/act_chain_switch.h"
#include "game/GameView.h"

void ChainSwitch_804AA3E8::init() {
    mDelay = mSpawnParam_0x90 & 0x1FFFF;
    mCallWhenDone = (mSpawnParam_0x90 >> 17) & 1;
    mSwitchMode = (mSpawnParam_0x90 >> 18) & 3;
    mSoundMode = (mSpawnParam_0x90 >> 20) & 3;
    mSwitchBit = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x242 = mSpawnParam_0x90 >> 27;
    mTimer = 0;
    mWasOn = isScreenFlagSet_0x34();
}

void ChainSwitch_804AA3E8::update() {
    if (mTimer != 0) {
        mTimer--;
        if (mTimer == 0) {
            if (mSoundMode == 1) {
                setScreenFlag_0x30(0xFF, true);
            } else if (mSoundMode == 2) {
                setScreenFlag_0x30(0xFF, false);
            }
            if (mSwitchMode == 0) {
                getGameView_802D6A9C()->setSwitchBit_802D8050(mSwitchBit, true, 0xFF);
            } else if (mSwitchMode == 1) {
                getGameView_802D6A9C()->setSwitchBit_802D8050(mSwitchBit, false, 0xFF);
            }
            if (mCallWhenDone == 1) {
                die_0x24();
            }
        }
    } else {
        if (!mWasOn) {
            if (isScreenFlagSet_0x34() == 1) {
                mTimer = mDelay;
            }
        }
        mWasOn = isScreenFlagSet_0x34();
    }
}

void ChainSwitch_804AA3E8::draw() {}

ChainSwitch_804AA3E8::~ChainSwitch_804AA3E8() {}
