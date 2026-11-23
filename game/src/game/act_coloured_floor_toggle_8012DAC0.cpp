#include "game/GameAudio.h"
#include "game/act/act_coloured_floor_toggle_8012DAC0.h"
#include "game/BgRewrite.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"

static const s32 lbl_804650F8[] = {0, 2, 1, 3, 4, 0};
static const u16 lbl_8053CC30[] = {0x8B, 0x8C, 0x8D, 0x8E};

void ColouredFloorToggle_8049E848::init() {
    field_0x23c = mSpawnParam_0x90 & 0x7F;
    field_0x23c *= 10;
    if (field_0x23c <= 0) {
        field_0x23c = 0x78;
    }
    field_0x234 = (mSpawnParam_0x90 >> 7) & 0x1F;
    if (field_0x234 >= 4) {
        field_0x234 = 0;
    }
    field_0x240 = field_0x234;
    field_0x244 = (mSpawnParam_0x90 >> 22) & 0x1F;
}

void ColouredFloorToggle_8049E848::update() {
    switch (field_0x230) {
    case 0: {
        if (getScreenFlagBit_0x3C() != 0 && !isScreenFlagSet_0x34()) {
            field_0x230 = 1;
            break;
        }
        s32 bit = field_0x244;
        if (bit != 0) {
            if (getGameView_802D6A9C()->isScreenFlagSet_802D800C(bit)) {
                field_0x238 = field_0x23c;
            }
        }
        field_0x238++;
        if (field_0x238 >= field_0x23c) {
            s32 idx;
            if (gGameRoot_8053AAF8->field_0x7f) {
                field_0x240--;
                if (field_0x240 < 0) {
                    field_0x240 = 3;
                }
                if (gGameRoot_8053AAF8->field_0x7e) {
                    idx = lbl_804650F8[field_0x240];
                } else {
                    idx = field_0x240;
                }
            } else {
                field_0x240++;
                if (field_0x240 >= 4) {
                    field_0x240 = 0;
                }
                idx = field_0x240;
            }
            gBgRewrite_8053AC98->writeTile_802A381C(&mPos, mLayer_0x4, lbl_8053CC30[idx]);
            field_0x238 = 0;
        }
        break;
    }
    case 1:
        if (getScreenFlagBit_0x3C() == 0) {
            field_0x230 = 0;
        } else if (isScreenFlagSet_0x34()) {
            field_0x230 = 0;
        }
        break;
    case 2: // nothing in this unit sets state 2
        break;
    }
}

ColouredFloorToggle_8049E848::~ColouredFloorToggle_8049E848() {}
