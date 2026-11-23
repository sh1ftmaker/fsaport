#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/BgRewrite.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/ViewLayer.h"
#include "game/cls_804AE9B0.h"
#include "game/save_data_803B8C74.h"
#include "game/act/act_liftable_item.h"
#include "game/act/act_treasure_chest.h"
#include "game/StageFlag.h"

void TreasureChest_804AA178::init() {
    field_0x234 = mSpawnParam_0x90 & 0xFF;
    field_0x238 = (mSpawnParam_0x90 >> 8) & 0xF;
    field_0x244 = (mSpawnParam_0x90 >> 12) & 0xF;
    if (field_0x244 >= 4) {
        field_0x244 = -1;
    }
    if ((field_0x234 >= 0x1A && field_0x234 < 0x20) || (field_0x234 >= 0x26 && field_0x234 < 0x2A) ||
        (field_0x234 >= 0x3B && field_0x234 < 0x40) || field_0x234 >= 0x4B ||
        (field_0x234 == 0x2E && mLayer_0x4 >= 8)) {
        field_0x234 = 0;
    }

    field_0x25c = mPos;
    mPos.x += 8.0f;
    mPos.y += 12.0f;
    if (viewLayerFlag(mLayer_0x4)) {
        mBox.i.x = -10.0f;
        mBox.i.y = 4.0f;
        mBox.f.x = 10.0f;
        mBox.f.y = 5.0f;
    } else {
        mBox.i.x = -6.0f;
        mBox.i.y = 0.0f;
        mBox.f.x = 6.0f;
        mBox.f.y = 5.0f;
    }

    JGeometry::TVec2f cell(field_0x25c.x, field_0x25c.y);
    mCell = getCellIndex_80226BC4(&cell, mLayer_0x4);
    if (getGameView_802D6A9C()->fn_802D49A8(mLayer_0x4, (u16)mCell) == true) {
        mAlive = false;
    }

    if (field_0x238 == 0) {
        field_0x230 = 0;
    } else if (field_0x238 == 2) {
        field_0x230 = 3;
    } else {
        field_0x230 = 2;
    }
    field_0x248 = -1;
    field_0x24c[0] = -1;
    field_0x24c[1] = -1;
    field_0x24c[2] = -1;
    field_0x24c[3] = -1;
}

// FIXME: 96.79 %, scheduling+regalloc; behaviour clean.
// In state 2 the spawn code is built in other registers (the
// original compares field_0x244 with i as `cmpw r29, r0` and reloads
// field_0x234 off r28), and the literals are @NNN, not lbl_805413C0 onwards.
void TreasureChest_804AA178::update() {
    JGeometry::TVec2f pos(mPos.x, mPos.y);
    s32 layer = mLayer_0x4;
    if (viewLayerFlag(layer)) {
        mBox.i.x = -10.0f;
        mBox.i.y = 2.0f;
        mBox.f.x = 10.0f;
        mBox.f.y = 5.0f;
    } else {
        mBox.i.x = -6.0f;
        mBox.i.y = 0.0f;
        mBox.f.x = 6.0f;
        mBox.f.y = 5.0f;
    }
    JGeometry::TBox2f box(mBox);
    box.addPos(pos);
    field_0x240 = fn_8023AF14(&box, mLayer_0x4, false, 0.0f);

    switch (field_0x230) {
    case 0:
        if (gGameRoot2_8053AB10->field_0x20->getTile_802F9908(mLayer_0x4, mCell) == 0x30) {
            field_0x230 = 1;
        }
        break;
    case 1: {
        bool result = false;
        if (field_0x240 >= 0) {
            if (viewLayerFlag(mLayer_0x4)) {
                if (getPlayerFacing_80230CFC(field_0x240) == 3 || getPlayerFacing_80230CFC(field_0x240) == 0 || getPlayerFacing_80230CFC(field_0x240) == 1) {
                    result = true;
                }
            } else if (getPlayerFacing_80230CFC(field_0x240) == 3) {
                result = true;
            }
            if (result == true) {
                mBox.i.x = -20.0f;
                mBox.i.y = -18.0f;
                mBox.f.x = 20.0f;
                mBox.f.y = 18.0f;
                box.set(mBox);
                box.addPos(pos);
                fn_8022FEB8(&box, mLayer_0x4, mSerial, true, true);
            }
            field_0x248 = field_0x240;
            if (hasPlayerLeader_8022F514(field_0x240)) {
                field_0x248 = getPlayerLeader_8022F3DC(field_0x240);
            }
            if (result == true) {
                if (!fn_8023EFEC(field_0x240)) {
                    if (getRepresentativePlayer_8022EA0C(field_0x240) == field_0x240) {
                        if (lbl_8053AD30->fn_80309D6C(getPlayerInputSlot_8030AAF4(field_0x248), 0x100) != 0) {
                            if (vfunc_0x54()) {
                                mAlive = false;
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    case 2:
        field_0x240 = field_0x244;
        field_0x248 = field_0x240;
        if (hasPlayerLeader_8022F514(field_0x240)) {
            field_0x248 = getPlayerLeader_8022F3DC(field_0x240);
        }
        for (s32 i = 0; i < 4; i++) {
            JGeometry::TVec3f from = *getPlayerPos_8023E724(i);
            JGeometry::TVec3f at = from;
            at.x = from.x + 1.0f;
            at.y = from.y - 12.0f;
            at.z = from.z;
            if (hasPlayerLeader_8022F514(i) == true) {
                if (i == field_0x244) {
                    u32 code = (field_0x234 + (i << 24)) + ((field_0x244 << 26) + 0x80000000);
                    s32 spawnLayer = getPlayerLayer_8023DE58(i);
                    field_0x24c[i] = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, spawnLayer, &at, code, -1, -1);
                }
            } else {
                if (i == field_0x244) {
                    u32 code = (field_0x234 + (i << 24)) + ((field_0x244 << 26) + 0x80000000);
                    s32 spawnLayer = getPlayerLayer_8023DE58(i);
                    field_0x24c[i] = getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, spawnLayer, &at, code, -1, -1);
                }
            }
        }
        if (field_0x238 != 1) {
            mAlive = false;
        } else {
            field_0x230 = 4;
        }
        break;
    case 4:
        // Set by case 2 when the chest has no more spawns; nothing else reads it.
        break;
    case 3:
        field_0x240 = field_0x244;
        field_0x248 = field_0x240;
        if (hasPlayerLeader_8022F514(field_0x240)) {
            field_0x248 = getPlayerLeader_8022F3DC(field_0x240);
        }
        if (vfunc_0x54()) {
            mAlive = false;
        }
        break;
    }
}

void TreasureChest_804AA178::draw() {}

// Differs only in register numbers.
// FIXME: 99.40 %, scheduling+regalloc; behaviour clean.
bool TreasureChest_804AA178::vfunc_0x54() {
    bool ok = false;
    bool result = false;
    if (gGameRoot_8053AAF8->field_0x7e == 1) {
        if (fn_802308E0(field_0x240) == true) {
            ok = true;
        }
    } else if (fn_8023077C() == true) {
        lbl_8053AD70->fn_8037AE30();
        ok = true;
    }
    if (ok == true) {
        if (gGameRoot_8053AAF8->field_0x7e == 1) {
            BOOL open = isStageFlagSet(lbl_80529DEC.mStage_0x118, lbl_80529DEC.mLevel_0x11c);
            if (open && (field_0x234 <= 2 || field_0x234 == 0x2F)) {
                s32 count = gGameRoot_8053AAF8->getPlayerCount_80138A30();
                if (getActorManager_801F856C()->fn_801F6058(count - 1) == field_0x240) {
                    s32 limit = 20;
                    if (gGameRoot_8053AAF8->getPlayerCount_80138A30() == 3) {
                        limit = 30;
                    } else if (gGameRoot_8053AAF8->getPlayerCount_80138A30() == 4) {
                        limit = 50;
                    }
                    if (randInt_801CD664(100) <= limit) {
                        field_0x234 = 0x20;
                    }
                }
            }
        }

        for (s32 i = 0; i < 4; i++) {
            JGeometry::TVec3f pos = *getPlayerPos_8023E724(i);
            JGeometry::TVec3f at = pos;
            at.x = pos.x + 1.0f;
            at.y = pos.y - 12.0f;
            at.z = pos.z;
            if (0x30 <= field_0x234) {
                if (gGameRoot_8053AAF8->field_0x7e == 1) {
                    if (hasPlayerLeader_8022F514(i) == true) {
                        if (field_0x248 == getPlayerLeader_8022F3DC(i)) {
                            if (i == field_0x240) {
                                u32 code = field_0x234 + (i << 24) + (field_0x240 << 26);
                                s32 layer = getPlayerLayer_8023DE58(i);
                                getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                            } else {
                                u32 code = field_0x234 + (i << 24) + (field_0x240 << 26) + 0x10000000;
                                s32 layer = getPlayerLayer_8023DE58(i);
                                getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                            }
                        }
                    } else if (i == field_0x248) {
                        if (i == field_0x240) {
                            u32 code = field_0x234 + (i << 24) + (field_0x240 << 26);
                            s32 layer = getPlayerLayer_8023DE58(i);
                            getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                        } else {
                            u32 code = field_0x234 + (i << 24) + (field_0x240 << 26) + 0x10000000;
                            s32 layer = getPlayerLayer_8023DE58(i);
                            getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                        }
                    }
                } else {
                    if (i == field_0x240) {
                        u32 code = field_0x234 + (i << 24) + (field_0x240 << 26);
                        s32 layer = getPlayerLayer_8023DE58(i);
                        getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                    } else {
                        u32 code = field_0x234 + (i << 24) + (field_0x240 << 26) + 0x10000000;
                        s32 layer = getPlayerLayer_8023DE58(i);
                        getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                    }
                }
            } else {
                if (hasPlayerLeader_8022F514(i) == true && getPlayerLeader_8022F3DC(i) == field_0x248 && i == field_0x240) {
                    u32 code = field_0x234 + (i << 24) + (field_0x240 << 26) + 0x00800000;
                    s32 layer = getPlayerLayer_8023DE58(i);
                    getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                } else if (i == field_0x248 && i == field_0x240) {
                    u32 code = field_0x234 + (i << 24) + (field_0x240 << 26) + 0x00800000;
                    s32 layer = getPlayerLayer_8023DE58(i);
                    getActorManager_801F856C()->spawnActor_801F9484(ACTOR_IMUP, layer, &at, code, -1, -1);
                }
            }
        }

        if (field_0x238 == 0) {
            fn_80230FD0(field_0x240, 0x20013);
            gBgRewrite_8053AC98->fn_802A3948(&field_0x25c, 0, mLayer_0x4, 0x10);
            s32 bit = getGameView_802D6A9C()->fn_802D4C10(mLayer_0x4, (u16)mCell);
            if (bit != -1) {
                getGameView_802D6A9C()->setScreenMaskBit_802D7D00(mLayer_0x4, bit, true);
            }
            JGeometry::TVec3f fxA(field_0x25c.x + 8.0f, field_0x25c.y + 8.0f, 0.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &fxA, 0xD0, 0, 0, NULL, NULL, true, 1.0f);
            JGeometry::TVec3f fxB(field_0x25c.x + 8.0f, field_0x25c.y + 8.0f, 0.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &fxB, 0xD1, 0, 1, NULL, NULL, true, 1.0f);
        }
        result = true;
    }
    return result;
}

bool TreasureChest_804AA178::fn_802EB27C() {
    s32 serial;
    bool result = false;
    for (s32 i = 0; i < 4; i++) {
        serial = field_0x24c[i];
        if (serial >= 0) {
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            LiftableItem_804A01D0* item = static_cast<LiftableItem_804A01D0*>(actor);
            if (item != NULL && item->fn_801597B8() == 3) {
                item->fn_801597C0();
                result = true;
            }
        }
    }
    return result;
}

TreasureChest_804AA178::~TreasureChest_804AA178() {}
