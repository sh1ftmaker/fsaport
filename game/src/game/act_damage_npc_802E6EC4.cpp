#include "game/GameAudio.h"
#include "game/act/act_damage_npc_802E6EC4.h"
#include "game/GameGlobals.h"

void DamageNPC_804A9E50::init() {
    mMode = mSpawnParam_0x90 & 0x7F;
    mFlag = (mSpawnParam_0x90 >> 7) & 0x1F;
    mStep = 0;
    mCount = 0;
    if (mMode == 0) {
        mTimer = 1;
    } else {
        mTimer = 2;
    }
}

void DamageNPC_804A9E50::update() {
    mTimer--;
    if (mTimer != 0) {
        return;
    }
    mTimer = 2;
    if (mMode == 0) {
        mStep++;
        mCount++;
        if (mStep > 6) {
            mStep = 5;
        }
        if (mCount == 8) {
            mAlive = false;
        }
    } else if (mStep == 0) {
        mStep = 2;
    } else if (mStep == 2) {
        mStep = 6;
    } else {
        mAlive = false;
    }
}

void DamageNPC_804A9E50::draw() {
    s32 flag = 0;
    if (mFlag != 0) {
        flag = 1;
    }
    showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, mStep + 0x26D, flag != 0, 2, -1, 1, 0, 0);
}

DamageNPC_804A9E50::~DamageNPC_804A9E50() {}
