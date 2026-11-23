#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/act/act_enemy_count_wipe_out_check.h"

void EnemyCountWipeOutCheck_804A8250::init() {
    field_0x230 = 0;
    field_0x234 = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x238 = mSpawnParam_0x90 & 0xFF;
    field_0x244 = gAudioMgr_8053AF80->field_0x58;
    field_0x240 = (mSpawnParam_0x90 >> 21) & 1;
}

void EnemyCountWipeOutCheck_804A8250::update() {
    s32 layer;
    u32 count = 0;
    if (field_0x234 == 0) {
        for (s32 i = 0; i < 8; i++) {
            count += getGameView_802D6A9C()->field_0x6748[i];
        }
    } else {
        layer = mLayer_0x4;
        GameView_804A9950* view = getGameView_802D6A9C();
        count = view->field_0x6748[layer < 8 ? 0 : layer - 7];
    }
    if (count >= field_0x238) {
        setScreenFlag_0x30(0xFF, true);
        if (field_0x244 != gAudioMgr_8053AF80->field_0x58 && field_0x240 == 0) {
            gGameRoot_8053AAF8->startStageMusic_80138B7C();
        }
        mAlive = false;
    }
}

void EnemyCountWipeOutCheck_804A8250::draw() {}

EnemyCountWipeOutCheck_804A8250::~EnemyCountWipeOutCheck_804A8250() {}
