#include "game/GameAudio.h"
#include "game/act/act_unknown_htmk_8034A6B0.h"

void UnknownHTMK_804AC868::init() {
    mLife = 10000;
    field_0x234 = 0;
    field_0x230 = 2;
}

void UnknownHTMK_804AC868::update() {
    if (field_0x230 <= 0 && field_0x234 == 0) {
        field_0x234++;
        field_0x230 = 2;
    } else if (field_0x230 <= 0 && field_0x234 == 1) {
        markDead_801F3668();
        return;
    }
    field_0x230--;
}

void UnknownHTMK_804AC868::draw() {
    drawSprite_801F06F0("enemy_both2_sob.bin", field_0x234 + 0x2DD, NULL, -1, 1.0f, 1.0f);
    showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, field_0x234 + 0x2DD, false, 2, -1, 0, 0, 0);
}

UnknownHTMK_804AC868::~UnknownHTMK_804AC868() {}
