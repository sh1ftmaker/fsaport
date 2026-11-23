#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/BgRewrite.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/ViewLayer.h"
#include "game/act/act_panel_rewrite_npc.h"
#include "game/act/act_small_key.h"


// Per-type effects, read once per call from field_0x244; the KMRI actor takes
// the table entry as its fourth argument.
static const u32 lbl_8046D808[4] = {3, 0, 0, 0};

// Writes the panel's tile at its cell: the 0x10 / 0x30 types go to the layer's
// tile grid, other types to the BgRewrite animation (fn_802A3948) by the tile
// code under the actor.
static inline void writeCell(PanelRewriteNPC_804A8E40* p) {
    if (0x30 == p->field_0x234 || p->field_0x234 == 0x10) {
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(p->mLayer_0x4 + p->field_0x256, p->field_0x250, p->field_0x234);
    } else {
        u16 tile = getTileAt_80226998(&p->mPos, p->mLayer_0x4);
        if (tile == 2) {
            gBgRewrite_8053AC98->fn_802A3948(&p->mPos, 0, p->mLayer_0x4 + p->field_0x256, 0xB8);
        } else if (tile == 0xDC) {
            gBgRewrite_8053AC98->fn_802A3948(&p->mPos, 0, p->mLayer_0x4 + p->field_0x256, 0x9E);
        } else if (tile == 0xDD) {
            gBgRewrite_8053AC98->fn_802A3948(&p->mPos, 0, p->mLayer_0x4 + p->field_0x256, 0x20);
        } else {
            gBgRewrite_8053AC98->fn_802A3948(&p->mPos, 0, p->mLayer_0x4 + p->field_0x256, p->field_0x234);
        }
    }
}

static inline void finishCell(PanelRewriteNPC_804A8E40* p) {
    p->vfunc_0x54(1);
    p->vfunc_0x58();
    if (p->field_0x23c == 0) {
        p->mAlive = false;
    } else {
        p->field_0x230 = 1;
    }
}

void PanelRewriteNPC_804A8E40::init() {
    field_0x234 = mSpawnParam_0x90 & 0x3FF;
    field_0x238 = (mSpawnParam_0x90 >> 16) & 0xF;
    field_0x23c = (mSpawnParam_0x90 >> 20) & 7;
    field_0x240 = (mSpawnParam_0x90 >> 23) & 7;
    field_0x244 = (mSpawnParam_0x90 >> 12) & 0xF;
    field_0x248 = (mSpawnParam_0x90 >> 10) & 3;
}

void PanelRewriteNPC_804A8E40::vfunc_0x10() {
    u8 gameFlag = gGameRoot_8053AAF8->getPlayerCount_80138A30();
    s32 keep = 0;
    u64 bit;
    u64 mask;
    if (field_0x248 == 0) {
        keep = 1;
    } else if (field_0x248 == 1) {
        if (gameFlag >= 3) {
            keep = 1;
        }
    } else if (field_0x248 == 2) {
        if (gameFlag >= 4) {
            keep = 1;
        }
    } else if (field_0x248 == 3) {
        if (gameFlag >= 2) {
            keep = 1;
        }
    }

    JGeometry::TVec2f pos(mPos.x, mPos.y);
    field_0x250 = getCellIndex_80226BC4(&pos, mLayer_0x4);
    if (getGameView_802D6A9C()->fn_802D4ADC(mLayer_0x4, field_0x250) == true && field_0x234 != 0x30) {
        keep = 0;
    }
    if (keep == 1 || field_0x23c == 2) {
        if (field_0x238 != 0) {
            if (mLayer_0x4 < 8) {
                field_0x256 = 1;
            } else {
                field_0x256 = 8;
            }
        } else {
            field_0x256 = 0;
        }
        if (mSpawnParam_0x90 & 0x04000000) {
            field_0x24c = true;
        } else {
            field_0x24c = false;
        }
        field_0x254 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(mLayer_0x4, field_0x250);
        field_0x230 = 0;

        if (field_0x234 == 0x30) {
            field_0x240 = 2;
            getGameView_802D6A9C()->fn_802D4874(mLayer_0x4, field_0x250, &bit, &mask);
            if (getGameView_802D6A9C()->fn_802D4ADC(mLayer_0x4, field_0x250) == true) {
                field_0x234 = 0x10;
                gGameRoot2_8053AB10->field_0x20->setTile_802F9974(mLayer_0x4 + field_0x256, field_0x250, field_0x234);
                mAlive = false;
            }
        } else if (field_0x234 >= 0x8B && field_0x234 <= 0x8E) {
            if (getGameView_802D6A9C()->fn_802D49A8(mLayer_0x4, field_0x250) == true) {
                gGameRoot2_8053AB10->field_0x20->setTile_802F9974(mLayer_0x4 + field_0x256, field_0x250, field_0x234);
                field_0x230 = 1;
            }
        } else if (field_0x234 == 0x17 || field_0x234 == 0x16) {
            field_0x240 = 2;
        } else if (field_0x234 == 0x3EE || field_0x234 == 0x3EF) {
            field_0x240 = 3;
        }

        if ((mSpawnParam_0x90 >> 27) != 0) {
            if (isScreenFlagSet_0x34() == true) {
                field_0x24c = false;
            }
        }
        if (field_0x23c == 2 && !isScreenFlagSet_0x34()) {
            field_0x230 = 3;
            if (field_0x254 == field_0x234) {
                switch (field_0x234) {
                case 0x9E:
                    field_0x254 = 0xDC;
                    break;
                }
            }
            update();
        } else if (mAlive) {
            update();
        }
    } else {
        mAlive = false;
    }
}

PanelRewriteNPC_804A8E40::~PanelRewriteNPC_804A8E40() {
    if (field_0x23c == 2) {
        JGeometry::TVec2f pos(mPos.x, mPos.y);
        s32 cell = getCellIndex_80226BC4(&pos, mLayer_0x4);
        s32 slot = getGameView_802D6A9C()->fn_802D4C10(mLayer_0x4, cell);
        if (slot != -1) {
            getGameView_802D6A9C()->setScreenMaskBit_802D7D00(mLayer_0x4, slot, false);
        }
    }
}

void PanelRewriteNPC_804A8E40::update() {
    if ((mSpawnParam_0x90 >> 27) == 0) {
        mBox.i.x = -48.0f;
        mBox.i.y = -48.0f;
        mBox.f.x = 48.0f;
        mBox.f.y = 48.0f;
        JGeometry::TVec2f offset(mPos.x, mPos.y);
        JGeometry::TBox2f box(mBox);
        box.addPos(offset);
        s32 hit = fn_8023AF14(&box, mLayer_0x4, false, 0.0f);
        switch (field_0x230) {
        case 0:
            if (hit >= 0) {
                writeCell(this);
                finishCell(this);
            }
            break;
        case 1:
            if (hit < 0) {
                gBgRewrite_8053AC98->writeTileMarkView_802A3884(&mPos, 0, mLayer_0x4 + field_0x256, field_0x254);
                vfunc_0x54(0);
                field_0x230 = 0;
            }
            break;
        }
    } else {
        switch (field_0x230) {
        case 0:
            if (isScreenFlagSet_0x34() == true) {
                writeCell(this);
                finishCell(this);
            }
            break;
        case 1:
            if (!isScreenFlagSet_0x34()) {
                gBgRewrite_8053AC98->writeTileMarkView_802A3884(&mPos, 0, mLayer_0x4 + field_0x256, field_0x254);
                vfunc_0x54(0);
                field_0x230 = 0;
            }
            break;
        case 2:
            // Nothing sets state 2 here; the original's dispatch still tests it.
            break;
        case 3:
            gBgRewrite_8053AC98->writeTileMarkView_802A3884(&mPos, 0, mLayer_0x4 + field_0x256, field_0x254);
            field_0x230 = 0;
            break;
        }
    }
}

void PanelRewriteNPC_804A8E40::draw() {}

void PanelRewriteNPC_804A8E40::vfunc_0x54(u8 arg) {
    if (field_0x24c) {
        if (mLayer_0x4 >= 8) {
            JGeometry::TVec3f pos;
            pos.x = mPos.x + 8.0f;
            pos.y = mPos.y + 8.0f;
            pos.z = 0.0f;
            u32 entry = lbl_8046D808[field_0x244];
            getActorManager_801F856C()->spawnActor_801F9484('KMRI', mLayer_0x4, &pos, entry, -1, -1);
        } else {
            if (field_0x244 == 0) {
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x564, 0, 0, NULL, NULL, true, 0.5f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x503, 1, 0, NULL, NULL, true, 1.0f);
            } else if (field_0x244 == 1) {
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x59, 1, 0, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x5A, 1, 0, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x5B, 1, 0, NULL, NULL, true, 1.0f);
            } else if (field_0x244 == 2) {
                if (getGameView_802D6A9C()->field_0x40 == 0x11) {
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xAD, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xB1, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xB2, 1, 0, NULL, NULL, true, 1.0f);
                } else {
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xAD, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xAF, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xB0, 1, 0, NULL, NULL, true, 1.0f);
                }
            } else if (field_0x244 == 3) {
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x99, 1, 0, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x9A, 1, 0, NULL, NULL, true, 1.0f);
            }
        }
        if (arg == 1) {
            if (field_0x240 == 0) {
                startSound_801F0E34(0x30003, 0);
            }
            if (field_0x240 == 2) {
                playSe_801F0D20(0x42);
            }
            if (field_0x240 == 3) {
                playSe_801F0D20(3);
            }
        }
    }
}

// The original keeps `this` in r31 from entry and the layer temporary in r30;
// ours rotates the callee-saved registers for this, the temporary, and other.
// FIXME: 96.65 %, scheduling+regalloc; behaviour clean.
bool PanelRewriteNPC_804A8E40::vfunc_0x58() {
    ActorBase_804A5770* other;
    s32 serial = -1;
    if (viewLayerFlag(mLayer_0x4)) {
        return false;
    }
    while (true) {
        serial = getActorManager_801F856C()->findNextActorById_801F78F4('KEY0', serial);
        if (serial < 0) {
            return false;
        }
        if (serial != mSerial) {
            other = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (other != NULL) {
                JGeometry::TVec3f otherPos = other->mPos;
                if (mLayer_0x4 == other->mLayer_0x4 &&
                    vfunc_0x5C(mPos.x + 8.0f, otherPos.x, 8.0f) &&
                    vfunc_0x5C(mPos.y + 8.0f, otherPos.y, 8.0f) &&
                    vfunc_0x5C(mPos.z, otherPos.z, 8.0f)) {
                    SmallKey_804A8CD8* key = static_cast<SmallKey_804A8CD8*>(other);
                    if (key->fn_802C1970(mLayer_0x4, &mPos)) {
                        key->fn_802C2B64();
                        return true;
                    }
                }
            }
        }
    }
}

bool PanelRewriteNPC_804A8E40::vfunc_0x5C(f32 a, f32 b, f32 c) {
    f32 d = a - b;
    if (d >= -c && d <= c) {
        return true;
    }
    return false;
}
