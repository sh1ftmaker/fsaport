#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/BgRewrite.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/PlayerBit.h"
#include "game/cls_804AE9B0.h"
#include "game/act/act_grass.h"

BgRewrite_804A7C98::BgRewrite_804A7C98() {
    gBgRewrite_8053AC98 = this;
    field_0x4 = new ActorSoundObj_804578F4(NULL, NULL, 8);
}

BgRewrite_804A7C98::~BgRewrite_804A7C98() {
    gBgRewrite_8053AC98 = NULL;
    delete field_0x4;
    field_0x4 = NULL;
}

void BgRewrite_804A7C98::fn_802A3D80() {
    field_0x8 = 10;
    field_0xc = 0;
    field_0x10 = 10;
    field_0x14 = 0;
    field_0x18 = 10;
    field_0x1c = 0;
}

static const u16 lbl_8046C4A8[16] = {
    0x0000, 0x0400, 0x0800, 0x0C00, 0x1000, 0x1400, 0x1800, 0x1C00,
    0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400, 0x3800, 0x3C00,
};

static const s32 lbl_8046C4C8[16] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
};

// Pos offsets of the four directions.
static const f32 lbl_8046C508[4][2] = {
    { 16.0f, 0.0f },
    { -16.0f, 0.0f },
    { 0.0f, 16.0f },
    { 0.0f, -16.0f },
};

// The 4 x 4 tile blocks (patterns of 20 u16: four rows of four tiles and padding).
static const u16 lbl_8046C528[3][20] = {
    {
        0x01B0, 0x01B0, 0x01B0, 0x01B0,
        0x01B0, 0x01B0, 0x01B0, 0x01B0,
        0x01B0, 0x01B0, 0x01B0, 0x01B0,
        0x01B0, 0x01B0, 0x01B0, 0x01B0,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    {
        0x01B0, 0x01E1, 0x01E2, 0x01B0,
        0x01B0, 0x01F1, 0x01F2, 0x01B0,
        0x01FC, 0x01FD, 0x01FE, 0x01FF,
        0x020C, 0x020D, 0x020E, 0x020F,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    {
        0x01B0, 0x004A, 0x004A, 0x01B0,
        0x01B0, 0x004A, 0x004A, 0x01B0,
        0x004B, 0x004A, 0x004A, 0x004C,
        0x01B0, 0x004D, 0x004D, 0x01B0,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
};

static const u16 lbl_8046C5A0[3][20] = {
    { 0 },
    {
        0x01D0, 0x01D1, 0x01D2, 0x01D3,
        0x01E0, 0x01E1, 0x01E2, 0x01E3,
        0x01F0, 0x01F1, 0x01F2, 0x01F3,
        0x0200, 0x0201, 0x0202, 0x0203,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    { 0 },
};

void BgRewrite_804A7C98::fn_802A3A0C() {
    field_0x8--;
    if (field_0x8 == 0) {
        field_0x8 = 10;
        field_0xc += 2;
        if (field_0xc >= 0x10) {
            field_0xc = 0;
        }
        if (getGameView_802D6A9C()->field_0x40 == 0x10) {
            u32 off = lbl_8046C4A8[field_0xc] + 0x4000;
            gGameRoot2_8053AB10->field_0x20->loadTileImages_802FA160(
                gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "bg_anim_b_sch.bin"), off, 0, 0x3800, 0x800);
        } else {
            u32 off = lbl_8046C4A8[field_0xc];
            gGameRoot2_8053AB10->field_0x20->loadTileImages_802FA160(
                gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "bg_anim_b_sch.bin"), off, 0, 0x3800, 0x800);
        }
    }
    field_0x18--;
    if (field_0x18 == 0) {
        field_0x18 = 10;
        field_0x1c++;
        if (field_0x1c >= 8) {
            field_0x1c = 0;
        }
        const char* name = getGameView_802D6A9C()->getStageBgName_802D9C4C();
        char c = *name;
        if (c == 'l') {
            u32 off = lbl_8046C4A8[field_0x1c] + 0x2000;
            gGameRoot2_8053AB10->field_0x20->loadTileImages_802FA160(
                gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "bg_anim_sch.bin"), off, 0, 0x5800, 0x400);
            field_0x18 = 0x14;
        } else if (c == 's' || c == 'w') {
            u32 off = lbl_8046C4A8[field_0x1c] + 0x6000;
            gGameRoot2_8053AB10->field_0x20->loadTileImages_802FA160(
                gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "bg_anim_sch.bin"), off, 0, 0x1c00, 0x400);
        } else if (c == 'k' || c == 'v') {
            if (field_0x1c >= 4) {
                field_0x1c = 0;
            }
            u32 off = lbl_8046C4A8[field_0x1c] + 0x1000;
            gGameRoot2_8053AB10->field_0x20->loadTileImages_802FA160(
                gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "bg_anim_sch.bin"), off, 0, 0x5c00, 0x400);
        } else if (getGameView_802D6A9C()->field_0x40 == 0x1b || *name == 'y') {
            if (field_0x1c >= 4) {
                field_0x1c = 0;
            }
            u32 off = lbl_8046C4A8[field_0x1c * 2] + 0x4000;
            gGameRoot2_8053AB10->field_0x20->loadTileImages_802FA160(
                gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "bg_anim_sch.bin"), off, 0, 0x5800, 0x800);
        }
    }
}

// Shared body of writeTileMarkView_802A3884 / fn_802A3948 (`set` is the bit value written to the screen mask).
inline void BgRewrite_804A7C98::writeTileMarkView(const JGeometry::TVec3f* pos, s32 layer, s16 tile, bool set) {
    s32 slot;
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer & 0xFE);
    s32 cell = field_0x20;
    getGameView_802D6A9C()->fn_802D782C(1);
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, tile);
    slot = getGameView_802D6A9C()->fn_802D4C10(layer, field_0x20);
    if (slot != -1) {
        getGameView_802D6A9C()->setScreenMaskBit_802D7D00(layer, slot, set);
    }
}

void BgRewrite_804A7C98::fn_802A3948(const JGeometry::TVec3f* pos, s32 player, s32 layer, u16 tile) {
    writeTileMarkView(pos, layer, tile, true);
}

void BgRewrite_804A7C98::writeTileMarkView_802A3884(const JGeometry::TVec3f* pos, s32 player, s32 layer, u16 tile) {
    writeTileMarkView(pos, layer, tile, false);
}

// FIXME: 97.69 %, different; behaviour near.
// As fn_802A3948: only the clrlwi r6, r31, 16 on tile is missing.
void BgRewrite_804A7C98::writeTile_802A381C(const JGeometry::TVec3f* pos, s32 layer, u16 tile) {
    JGeometry::TVec2f p(pos->x, pos->y);
    s32 cell = getCellIndex_80226BC4(&p, layer & 0xFE);
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, tile);
}

// FIXME(shotgun): 99.40 %, scheduling+regalloc; behaviour clean.
bool BgRewrite_804A7C98::fn_802A3164(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    bool result = true;
    s32 serial;
    ActorBase_804A5770* actor;
    if (field_0x24 >= 0x40 && field_0x24 <= 0x44) {
        if (field_0x24 == 0x41) {
            gBgRewrite_8053AC98->writeTileMarkView_802A3884(pos, 0, layer, 0x12);
            s32 count = getActorManager_801F856C()->mActorEnd;
            for (s32 i = 0; i < count; i++) {
                actor = getActorManager_801F856C()->mActors[i];
                if (actor != NULL && actor->mActorId == 'TBO2' && layer == actor->mLayer_0x4) {
                    JGeometry::TVec3f actorPos = actor->mPos;
                    JGeometry::TVec2f q(actorPos.x, actorPos.y);
                    if (field_0x20 == getCellIndex_80226BC4(&q, layer)) {
                        actor->markDead_801F3668();
                        break;
                    }
                }
            }
        } else if (field_0x24 == 0x43) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x50);
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x11);
        }
        serial = getActorManager_801F856C()->spawnActor_801F9484('TUBO', layer, pos, field_0x24 - 0x40, -1, -1);
        actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
    } else if (field_0x24 >= 0x45 && field_0x24 <= 0x49) {
        if (field_0x24 == 0x46) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x14);
        } else if (field_0x24 == 0x48) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x52);
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x15);
        }
        serial = getActorManager_801F856C()->spawnActor_801F9484('TUBO', layer, pos, field_0x24 - 0x45, -1, -1);
        actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
    } else if (field_0x24 == 0x4E) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x11);
        serial = getActorManager_801F856C()->spawnActor_801F9484('TUBO', layer, pos, 0x80, -1, -1);
        actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
    } else if (field_0x24 == 0x4F) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x11);
        serial = getActorManager_801F856C()->spawnActor_801F9484('TUBO', layer, pos, 0x100, -1, -1);
        actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
    } else {
        result = false;
    }
    return result;
}

// FIXME: 98.62 %, missing/extra; behaviour clean.
// Still differs: the original keeps layer in a second register (mr r31, r29; the spawn calls read the first).
void BgRewrite_804A7C98::fn_802A20F0(const JGeometry::TVec3f* pos, s32 player, s32 layer, s32 a, s32 b) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    s32 bit = getCellIndexVec3_80226AE0(pos, layer);
    if (field_0x24 == 0x69) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7A);
        s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a, -1, -1);
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
        gAudioMgr_8053AF80->playSystemSound_80458880(3, -1);
    } else if (field_0x24 == 0x68) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x12);
        s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a, -1, -1);
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
    } else if (field_0x24 == 0x6A) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x50);
        s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a, -1, -1);
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
    } else if (field_0x24 == 0x70 || field_0x24 == 0x75 || field_0x24 == 0x6B) {
        if (a == 0x10000) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x5A);
            if (!getGameView_802D6A9C()->fn_802D6AC4(bit)) {
                s32 serial;
                if (field_0x24 == 0x6B) {
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 3, -1, -1);
                } else {
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a, -1, -1);
                }
                Grass_804A92D0* grass =
                    (Grass_804A92D0*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (grass != NULL) {
                    grass->fn_802CEC10();
                }
                getGameView_802D6A9C()->fn_802D6B24(bit, true);
            }
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x79);
            s32 serial;
            if (field_0x24 == 0x70) {
                serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a, -1, -1);
            } else if (field_0x24 == 0x75) {
                serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 1, -1, -1);
            } else if (field_0x24 == 0x6B) {
                if (getGameView_802D6A9C()->fn_802D6AC4(bit)) {
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 4, -1, -1);
                } else {
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 3, -1, -1);
                    getGameView_802D6A9C()->fn_802D6B24(bit, true);
                }
            }
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->die_0x24();
            }
        }
    } else if (field_0x24 == 0x74) {
        if (a == 0x10000) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x5A);
            if (!getGameView_802D6A9C()->fn_802D6AC4(bit)) {
                s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 0x100000, -1, -1);
                Grass_804A92D0* grass =
                    (Grass_804A92D0*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (grass != NULL) {
                    grass->fn_802CEC10();
                }
                getGameView_802D6A9C()->fn_802D6B24(bit, true);
            }
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7D);
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 0x100000, -1, -1);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->die_0x24();
            }
        }
    } else if (field_0x24 == 0x146) {
        if (a == 0x10000) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x147);
            if (!getGameView_802D6A9C()->fn_802D6AC4(bit)) {
                s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 0x100000, -1, -1);
                Grass_804A92D0* grass =
                    (Grass_804A92D0*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (grass != NULL) {
                    grass->fn_802CEC10();
                }
                getGameView_802D6A9C()->fn_802D6B24(bit, true);
            }
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7D);
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 0x100000, -1, -1);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->die_0x24();
            }
        }
    } else if (field_0x24 == 0xFA) {
        if (a == 0x10000) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x5A);
            if (!getGameView_802D6A9C()->fn_802D6AC4(bit)) {
                s32 serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, pos, a + 0x200000, -1, -1);
                Grass_804A92D0* grass =
                    (Grass_804A92D0*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (grass != NULL) {
                    grass->fn_802CEC10();
                }
                getGameView_802D6A9C()->fn_802D6B24(bit, true);
            }
        } else if (!getGameView_802D6A9C()->fn_802D72BC(bit)) {
            getGameView_802D6A9C()->fn_802D7310(bit, true);
            JGeometry::TVec3f spawnPos;
            cellToPos_80226CA8(bit, layer, &spawnPos);
            spawnPos.x += 8.0f;
            spawnPos.y += 8.0f;
            getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &spawnPos, a + 0x200000, -1, -1);
        }
    } else if (field_0x24 == 0x71) {
        if (a == 0) {
            // both outcomes write 0x7B in the original; the result is compared with 1 (cmplwi)
            if (fn_802353AC(player) == true) {
                gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7B);
            } else {
                gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7B);
            }
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x5A);
        }
        s32 serial = getActorManager_801F856C()->spawnActor_801F9484('BORD', layer, pos, 0, -1, -1);
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            actor->die_0x24();
        }
        spawnEmitter_8013CC50(layer, pos, 0x24A, 1, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(layer, pos, 0x24B, 1, 0, NULL, NULL, true, 1.0f);
    }
    fn_802A3164(pos, player, layer);
    if (b != 0) {
        if (field_0x24 == 0xEA) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x252);
            fn_80230FD0(player, 0x30006);
        } else if (field_0x24 == 0xEB) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x253);
            fn_80230FD0(player, 0x30006);
        } else if (field_0x24 == 0xEC) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x254);
            fn_80230FD0(player, 0x30006);
        } else if (field_0x24 == 0xF9) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x25B);
            fn_80230FD0(player, 0x30006);
        } else if (field_0x24 == 0x159 && a == 0x10000) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x5A);
        }
    }
}

bool BgRewrite_804A7C98::fn_802A1D44(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    bool result = true;
    if (field_0x24 == 0x76) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x77);
        fn_80230FD0(player, 0x20026);
    } else if (field_0x24 == 0x3A) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x3B);
        fn_80230FD0(player, 0x20026);
    } else if (field_0x24 == 0x3F) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x3E);
        fn_80230FD0(player, 0x20026);
    } else if (field_0x24 == 0x90) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x91);
        fn_80230FD0(player, 0x50013);
    } else if (field_0x24 == 0x91 || field_0x24 == 0x22E) {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x92);
        fn_80230FD0(player, 0x50012);
    } else if (!fn_802A3164(pos, player, layer)) {
        result = false;
    }
    return result;
}

// FIXME(shotgun): 99.55 %, missing/extra; behaviour clean.
// 1 fewer insn (literal pool order)
s32 BgRewrite_804A7C98::fn_802A1844(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    if ((layer == 0 && field_0x24 >= 0x1B0 && field_0x24 <= 0x1CF) || field_0x24 == 0x231 || field_0x24 == 0x8A ||
        field_0x24 == 0x79 || field_0x24 == 0x7B || field_0x24 == 0x7D) {
        u16 tile = field_0x24;
        if (tile == 0x231) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7A);
        } else if ((tile >= 0x1B5 && tile <= 0x1BA) || (u16)(tile - 0x1BC) <= 1 || tile == 0x8A ||
                   (tile >= 0x1C5 && tile <= 0x1CD)) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x83);
        } else if (tile == 0x79 || tile == 0x7B || tile == 0x7D) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7C);
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7C);
        }
        if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.2f) {
            getActorManager_801F856C()->spawnActor_801F9484('HART', layer, pos, 0, -1, -1);
        }
        if (getGameView_802D6A9C()->field_0x40 == 0x14) {
            return 3;
        }
        if (getGameView_802D6A9C()->field_0x40 == 0x11 || getGameView_802D6A9C()->field_0x40 == 0x18) {
            return 4;
        }
        tile = field_0x24;
        if ((tile >= 0x1B5 && tile <= 0x1BA) || (u16)(tile - 0x1BC) <= 1 || (tile >= 0x1C5 && tile <= 0x1CD)) {
            return 2;
        }
        return 1;
    }
    if (getGameView_802D6A9C()->mStage_0x4 == 4 && getGameView_802D6A9C()->mLevel_0x8 == 3) {
        return 8;
    }
    if ((getGameView_802D6A9C()->mStage_0x4 == 1 && getGameView_802D6A9C()->mLevel_0x8 == 1 && field_0x24 == 0x270) ||
        field_0x24 == 0x271 || field_0x24 == 0x361) {
        return 8;
    }
    u16 tile = field_0x24;
    if ((u16)(tile - 0x11) <= 2 || tile == 0x38 || tile == 0x3B || (u16)(tile - 0x50) <= 1 || tile == 0x78 ||
        tile == 0x8F || (u16)(tile - 0x9D) <= 1 || tile == 0x1AE || tile == 0x1AF) {
        return 8;
    }
    if (tile == 0x7C || tile == 0x83) {
        return 9;
    }
    return 0;
}

bool BgRewrite_804A7C98::fn_802A1688(const JGeometry::TVec3f* pos, s32 layer, u32 tag) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    if (field_0x24 == 0x81) {
        if (tag != 'SBAL') {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x9D);
            if (layer >= 8) {
                getActorManager_801F856C()->spawnActor_801F9484('KMRI', layer, pos, 0, -1, -1);
            } else {
                spawnEmitter_8013CC50(layer, pos, 0xF9, 1, 0, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(layer, pos, 0xFA, 1, 0, NULL, NULL, true, 1.0f);
            }
        }
        field_0x4->startSound(0x50045);
    }
    return fn_802A3164(pos, -1, layer);
}

bool BgRewrite_804A7C98::fn_802A1554(const JGeometry::TVec3f* pos) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, 0);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(0, field_0x20);
    u16 tile = field_0x24;
    if (lbl_8046CB88[tile] == 0) {
        return true;
    }
    if (tile == 0x75 || tile == 0x68 || tile == 0x69 || tile == 0x6A || tile == 0x6B || tile == 0x82 ||
        tile == 0x5F || tile == 0x70 || tile == 0x71 || tile == 0x72 || tile == 0x40 || tile == 0x41 ||
        tile == 0x42 || tile == 0x43 || tile == 0x44 || tile == 0xE5 || tile == 0x58) {
        return true;
    }
    s32 code = getTileCollisionCode_8022461C(pos, 0);
    if (code == 0xE || code == 3 || (code == 0x22 && getGameView_802D6A9C()->field_0x170 == 0)) {
        return true;
    }
    return false;
}

bool BgRewrite_804A7C98::fn_802A1490(const JGeometry::TVec3f* pos) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, 0);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(0, field_0x20);
    if (lbl_8046CB88[field_0x24] == 0) {
        return true;
    }
    s32 code = getTileCollisionCode_8022461C(pos, 0);
    if (code == 0x22 && getGameView_802D6A9C()->field_0x170 == 0) {
        return true;
    }
    return false;
}

// Regalloc: inlined fn_802A3948 slot uses r29 instead of r28 in three cases.
// FIXME: 99.83 %, regalloc-only; behaviour clean.
void BgRewrite_804A7C98::fn_802A1050(const JGeometry::TVec3f* pos) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, 0);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(0, field_0x20);
    if (field_0x24 == 0x69 || field_0x24 == 0x68 || field_0x24 == 0x75 || field_0x24 == 0x6A || field_0x24 == 0x6B ||
        field_0x24 == 0x82 || field_0x24 == 0x5F || field_0x24 == 0x70 || field_0x24 == 0x71 || field_0x24 == 0x72 ||
        field_0x24 == 0x73 || field_0x24 == 0x40 || field_0x24 == 0x41 || field_0x24 == 0x42 || field_0x24 == 0x43 ||
        field_0x24 == 0x44 || field_0x24 == 0xE5) {
        switch (field_0x24) {
        case 0x72:
        case 0x73:
        case 0x82: {
            if (field_0x24 == 0x72 || field_0x24 == 0x73) {
                gBgRewrite_8053AC98->fn_802A3948(pos, 0, 0, 0x7C);
            } else {
                gBgRewrite_8053AC98->fn_802A3948(pos, 0, 0, 0x83);
            }
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484('STON', 0, pos, 0, -1, -1);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->die_0x24();
            }
            break;
        }
        case 0x5F: {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, 0, 0x83);
            s32 serial = getActorManager_801F856C()->spawnActor_801F9484('YASI', 0, pos, 0, -1, -1);
            ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (actor != NULL) {
                actor->die_0x24();
            }
            break;
        }
        case 0xE5:
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, 0, 0x9D);
            spawnEmitter_8013CC50(0, pos, 0xF9, 1, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(0, pos, 0xFA, 1, 0, NULL, NULL, true, 1.0f);
            field_0x4->startSound(0x50045);
            break;
        }
    }
}

// Regalloc: the original keeps tile and layer + layerOffset extended in one register each.
// FIXME: 90.64 %, missing/extra; behaviour drift.
bool BgRewrite_804A7C98::fn_802A0800(s32 player, s32 layer) {
    s32 leader = player;
    if (hasPlayerLeader_8022F514(player)) {
        leader = getPlayerLeader_8022F3DC(player);
    }
    JGeometry::TVec3f pos = *getPlayerPos_8023E724(player);
    u8 layerOffset = 8;
    if (layer < 8) {
        layerOffset = 1;
    }
    s32 item = fn_802373EC(leader);
    u16 tile = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer + layerOffset, field_0x20);
    if (tile == 0xE) {
        return false;
    }
    if (player != getRepresentativePlayer_8022EA0C(player)) {
        return false;
    }
    if (tile != 0 || item != 0) {
        if (gGameRoot_8053AAF8->field_0x7e == 1) {
            if (!fn_802308E0(leader)) {
                return false;
            }
        } else {
            if (!fn_8023077C()) {
                return false;
            }
            lbl_8053AD70->fn_8037AE30();
        }
    }
    switch (item) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        writeTilePair_8029EC84(field_0x20, layer, layerOffset, 0xE, 0xE);
        if (tile == 0) {
            getActorManager_801F856C()->spawnActor_801F9484(
                'ITMS', layer, &pos, (field_0x20 << 16) + (player << 12) + (layerOffset << 8) + item + 0x10, -1, -1);
        } else {
            getActorManager_801F856C()->spawnActor_801F9484(
                'ITMS', layer, &pos, (field_0x20 << 16) + (player << 12) + (layerOffset << 8) + item, -1, -1);
        }
        break;
    case 0:
        writeTilePair_8029EC84(field_0x20, layer, layerOffset, 0, 0);
        break;
    }
    JGeometry::TVec3f effectPos(1.0f + pos.x, pos.y - 12.0f, pos.z);
    s32 kind;
    s32 code;
    switch (tile) {
    case 0x28:
        kind = 1;
        code = 0x30;
        break;
    case 0x29:
        kind = 2;
        code = 0x31;
        break;
    case 0x2A:
        kind = 3;
        code = 0x32;
        break;
    case 0x2B:
        kind = 4;
        code = 0x33;
        break;
    case 0x2C:
        kind = 5;
        code = 0x34;
        break;
    case 0x2D:
        kind = 6;
        code = 0x35;
        break;
    case 0x2E:
        kind = 7;
        code = 0x36;
        break;
    case 0x2F:
        kind = 8;
        code = 0x37;
        break;
    case 0xB0:
        kind = 9;
        code = 0x38;
        break;
    case 0xB1:
        kind = 10;
        code = 0x39;
        break;
    case 0xB2:
        kind = 11;
        code = 0x1000A;
        break;
    case 0xB3:
        kind = 12;
        code = 0x1000B;
        break;
    case 0x100F:
        kind = 13;
        code = 0x1000C;
        break;
    case 0:
    default:
        kind = 0;
        break;
    }
    if (kind == 0) {
        if (item != 0) {
            fn_80230FD0(player, 0x2002C);
            fn_802372F8(player, 0, 1);
            if (gGameRoot_8053AAF8->field_0x7e == 1) {
                for (s32 i = 0; i < 4; i++) {
                    if (hasPlayerLeader_8022F514(i) == true && leader == getPlayerLeader_8022F3DC(i)) {
                        getActorManager_801F856C()->spawnActor_801F9484(
                            'DMYK', getPlayerLayer_8023DE58(i), &effectPos, (player << 16) + (i << 12) + 0xD5, -1, -1);
                    } else if (i == leader) {
                        getActorManager_801F856C()->spawnActor_801F9484(
                            'DMYK', getPlayerLayer_8023DE58(i), &effectPos, (player << 16) + (i << 12) + 0xD5, -1, -1);
                    }
                }
            } else {
                for (s32 i = 0; i < 4; i++) {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'DMYK', getPlayerLayer_8023DE58(i), &effectPos, (player << 16) + (i << 12) + 0xD5, -1, -1);
                }
            }
        } else {
            gAudioMgr_8053AF80->playSystemSound_80458880(6, -1);
        }
        return false;
    }
    if (gGameRoot_8053AAF8->field_0x7e == 1) {
        for (s32 i = 0; i < 4; i++) {
            pos = *getPlayerPos_8023E724(i);
            effectPos.set(1.0f + pos.x, pos.y - 12.0f, pos.z);
            if (hasPlayerLeader_8022F514(i) == true && leader == getPlayerLeader_8022F3DC(i)) {
                if (i == player) {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'IMUP', getPlayerLayer_8023DE58(i), &effectPos, code + ((i << 24) + (player << 26) + 0x10000000), -1, -1);
                } else {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'IMUP', getPlayerLayer_8023DE58(i), &effectPos, code + ((i << 24) + (player << 26) + 0x30000000), -1, -1);
                }
                fn_80230498(i, &pos, 0xE9, 0xFF, -0x80, -0x80, true);
            } else if (i == leader) {
                if (i == player) {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'IMUP', getPlayerLayer_8023DE58(i), &effectPos, code + ((i << 24) + (player << 26) + 0x10000000), -1, -1);
                } else {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'IMUP', getPlayerLayer_8023DE58(i), &effectPos, code + ((i << 24) + (player << 26) + 0x30000000), -1, -1);
                }
                fn_80230498(i, &pos, 0xE9, 0xFF, -0x80, -0x80, true);
            }
        }
    } else {
        for (s32 i = 0; i < 4; i++) {
            pos = *getPlayerPos_8023E724(i);
            effectPos.set(1.0f + pos.x, pos.y - 12.0f, pos.z);
            if (i == player) {
                getActorManager_801F856C()->spawnActor_801F9484(
                    'IMUP', getPlayerLayer_8023DE58(i), &effectPos, code + ((i << 24) + (player << 26) + 0x10000000), -1, -1);
            } else {
                getActorManager_801F856C()->spawnActor_801F9484(
                    'IMUP', getPlayerLayer_8023DE58(i), &effectPos, code + ((i << 24) + (player << 26) + 0x30000000), -1, -1);
            }
            fn_80230498(i, &pos, 0xE9, 0xFF, -0x80, -0x80, true);
        }
    }
    return true;
}

static inline void readTileAt(BgRewrite_804A7C98* self, const JGeometry::TVec3f* pos, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    self->field_0x20 = getCellIndex_80226BC4(&p, layer);
    self->field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, self->field_0x20);
}

// FIXME(shotgun): 99.05 %, regalloc-only; behaviour drift.
// layer/kind, slot and loop registers differ
s32 BgRewrite_804A7C98::fn_802A02A8(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    ActorBase_804A5770* actor;
    s32 count;
    readTileAt(this, pos, layer);
    s32 kind = -1;
    s32 code = 0x79;
    switch (field_0x24) {
    case 0x69:
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x7A);
        fn_80230FD0(player, 0x20007);
        gAudioMgr_8053AF80->playSystemSound_80458880(3, -1);
        return 0;
    case 0x16:
    case 0x17:
        if (fn_802A0800(player, layer) == true) {
            return 0x25;
        }
        return -1;
    case 0x68:
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x12);
        fn_80230FD0(player, 0x20007);
        return 0;
    case 0x70:
        kind = 0;
        break;
    case 0x75:
        kind = 1;
        break;
    case 0x6A:
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x50);
        fn_80230FD0(player, 0x20007);
        return 0;
    case 0x6B:
        kind = 3;
        break;
    case 0x72:
    case 0x82:
        kind = 5;
        if (field_0x24 == 0x72) {
            code = 0x7C;
        } else {
            code = 0x83;
        }
        break;
    case 0x5F:
        kind = 0x27;
        code = 0x83;
        break;
    case 0x71:
        kind = 7;
        code = 0x7B;
        break;
    case 0x40:
        kind = 0x18;
        code = 0x11;
        break;
    case 0x41:
        kind = 0x19;
        code = 0x12;
        break;
    case 0x42:
        kind = 0x1A;
        code = 0x11;
        break;
    case 0x43:
        kind = 0x1B;
        code = 0x50;
        break;
    case 0x44:
        kind = 0x1C;
        code = 0x11;
        break;
    case 0x45:
        kind = 0x1D;
        code = 0x15;
        break;
    case 0x46:
        kind = 0x1E;
        code = 0x14;
        break;
    case 0x47:
        kind = 0x1F;
        code = 0x15;
        break;
    case 0x48:
        kind = 0x20;
        code = 0x52;
        break;
    case 0x49:
        kind = 0x21;
        code = 0x15;
        break;
    case 0x4E:
        kind = 0x22;
        code = 0x11;
        break;
    case 0x4F:
        kind = 0x23;
        code = 0x11;
        break;
    }
    if (kind != -1 && kind != 0x25 && kind != 0xC) {
        if (kind == 0x19) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, code);
            fn_80230FD0(player, 0x20007);
            count = getActorManager_801F856C()->mActorEnd;
            for (s32 i = 0; i < count; i++) {
                actor = getActorManager_801F856C()->mActors[i];
                if (actor != NULL && actor->mActorId == 'TBO2' && layer == actor->mLayer_0x4) {
                    JGeometry::TVec3f actorPos = actor->mPos;
                    JGeometry::TVec2f q(actorPos.x, actorPos.y);
                    if (field_0x20 == getCellIndex_80226BC4(&q, layer)) {
                        actor->markDead_801F3668();
                        break;
                    }
                }
            }
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, code);
            fn_80230FD0(player, 0x20007);
        }
    }
    return kind;
}

bool BgRewrite_804A7C98::fn_802A01F0(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    bool result = false;
    switch (field_0x24) {
    case 0x16:
    case 0x17:
        result = fn_802A0800(player, layer);
        break;
    case 0x71:
        result = true;
        break;
    }
    return result;
}

// Register copies differ (the original keeps layer, cell and the spawned serial twice).
// FIXME: 98.56 %, missing/extra; behaviour clean.
u8 BgRewrite_804A7C98::fn_8029EDB8(const JGeometry::TVec3f* pos, s32 player, s32 layer, JGeometry::TVec3f* out) {
    u8 result = 0;
    if (pos->z < 19.0f) {
        GameView_804A9950* view = getGameView_802D6A9C();
        const JGeometry::TBox2f* src = layer < 8 ? &view->mVisibleBox : &view->field_0xdc;
        JGeometry::TBox2f box;
        box = *src;
        JGeometry::TVec3f p;
        p.y = pos->y - 16.0f;
        p.z = pos->z;
        for (int i = 0; i < 3; i++) {
            p.x = pos->x - 16.0f;
            for (int j = 0; j < 3; j++) {
                s32 x = (s32)(p.x - box.i.x);
                s32 y = (s32)(p.y - box.i.y);
                if (x < 0) {
                    x = 0;
                } else if (x >= 0x200) {
                    x = 0x1FF;
                }
                if (y < 0) {
                    y = 0;
                } else if (y >= 0x200) {
                    y = 0x1FF;
                }
                s32 cell = (x & 0x100) | ((y & 0x100) << 1) | (y & 0xF0) | ((x >> 4) & 0xF);
                u32 tile = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, cell);
                JGeometry::TVec3f cellPos;
                cellToPos_80226CA8(cell, layer, &cellPos);
                s32 serial;
                if (tile >= 0x40 && tile <= 0x44) {
                    if (tile == 0x41) {
                        gBgRewrite_8053AC98->writeTileMarkView_802A3884(&p, 0, layer, 0x12);
                        s32 count = getActorManager_801F856C()->mActorEnd;
                        for (s32 k = 0; k < count; k++) {
                            ActorBase_804A5770* actor = getActorManager_801F856C()->mActors[k];
                            if (actor != NULL && actor->mActorId == 'TBO2' && layer == actor->mLayer_0x4) {
                                JGeometry::TVec3f actorPos = actor->mPos;
                                JGeometry::TVec2f q(actorPos.x, actorPos.y);
                                if (cell == getCellIndex_80226BC4(&q, layer)) {
                                    actor->markDead_801F3668();
                                    break;
                                }
                            }
                        }
                    } else if (tile == 0x43) {
                        gBgRewrite_8053AC98->writeTileMarkView_802A3884(&p, 0, layer, 0x50);
                    } else {
                        gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x11);
                    }
                    serial = getActorManager_801F856C()->spawnActor_801F9484('TUBO', layer, &p, tile - 0x40, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    fn_80230FD0(player, 0x20025);
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile >= 0x45 && tile <= 0x49) {
                    if (tile == 0x46) {
                        gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x14);
                    } else if (tile == 0x48) {
                        gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x52);
                    } else {
                        gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x15);
                    }
                    serial = getActorManager_801F856C()->spawnActor_801F9484('TUBO', layer, &p, field_0x24 - 0x45, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    fn_80230FD0(player, 0x20025);
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x69) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x7A);
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 0, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    gAudioMgr_8053AF80->playSystemSound_80458880(3, -1);
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x68) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x12);
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 0, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x6A) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x50);
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 0, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x70 || tile == 0x75 || tile == 0x6B) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x79);
                    if (tile == 0x70) {
                        serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 0, -1, -1);
                    } else if (tile == 0x75) {
                        serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 1, -1, -1);
                    } else if (tile == 0x6B) {
                        if (getGameView_802D6A9C()->fn_802D6AC4(field_0x20)) {
                            serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 2, -1, -1);
                        } else {
                            serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 3, -1, -1);
                            getGameView_802D6A9C()->fn_802D6B24(field_0x20, true);
                        }
                    }
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x74 || tile == 0x146) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x7D);
                    serial = getActorManager_801F856C()->spawnActor_801F9484('GRAS', layer, &p, 0x100000, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x71) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x7B);
                    serial = getActorManager_801F856C()->spawnActor_801F9484('BORD', layer, &p, 0, -1, -1);
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->die_0x24();
                    }
                    getActorManager_801F856C()->spawnActor_801F9484('BLZE', layer, &p, 0xF, -1, -1);
                } else if (tile == 0x81) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x9D);
                    fn_80230FD0(player, 0x20025);
                    if (layer >= 8) {
                        JGeometry::TVec3f kmriPos(8.0f + cellPos.x, 8.0f + cellPos.y, 0.0f);
                        getActorManager_801F856C()->spawnActor_801F9484('KMRI', layer, &kmriPos, 0, -1, -1);
                    } else {
                        spawnEmitter_8013CC50(layer, pos, 0xF9, 1, 0, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(layer, pos, 0xFA, 1, 0, NULL, NULL, true, 1.0f);
                    }
                } else if (tile == 0x90) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x91);
                    fn_80230FD0(player, 0x20025);
                    spawnEmitter_8013CC50(layer, pos, 0x24A, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(layer, pos, 0x24B, 1, 0, NULL, NULL, true, 1.0f);
                } else if (tile == 0x91 || tile == 0x22E) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x92);
                    fn_80230FD0(player, 0x20025);
                    spawnEmitter_8013CC50(layer, pos, 0x24A, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(layer, pos, 0x24B, 1, 0, NULL, NULL, true, 1.0f);
                } else if (tile == 0x1) {
                    gBgRewrite_8053AC98->fn_802A3948(&p, 0, layer, 0x3E0);
                    fn_80230FD0(player, 0x20025);
                    if (layer >= 8) {
                        JGeometry::TVec3f kmriPos(8.0f + cellPos.x, 8.0f + cellPos.y, 0.0f);
                        getActorManager_801F856C()->spawnActor_801F9484('KMRI', layer, &kmriPos, 0, -1, -1);
                    } else {
                        spawnEmitter_8013CC50(layer, &cellPos, 0xF9, 1, 0, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(layer, &cellPos, 0xFA, 1, 0, NULL, NULL, true, 1.0f);
                    }
                } else if (tile == 0x9B || tile == 0x9C) {
                    JGeometry::TVec3f wallPos;
                    cellToPos_80226CA8(cell, layer, &wallPos);
                    *out = wallPos;
                    result = 1;
                    if (tile == 0x9B) {
                        wallPos.y -= 16.0f;
                    } else if (tile == 0x9C) {
                        wallPos.x -= 16.0f;
                        wallPos.y -= 16.0f;
                    }
                    fn_8029E050(&wallPos, layer, 0x6E, 0x6F, 0x7E, 0x7F);
                    JGeometry::TVec3f effPos(16.0f + wallPos.x, 32.0f + wallPos.y, 0.0f);
                    spawnEmitter_8013CC50(layer, &effPos, 0x60, 0, 0, NULL, NULL, true, 1.0f);
                    s32 offset = 8;
                    if (layer < 8) {
                        offset = 1;
                    }
                    s32 wallCell = field_0x20;
                    getGameView_802D6A9C()->fn_802D782C(1);
                    s32 upper = layer + offset;
                    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(upper, wallCell, 0x6C);
                    s32 slot = getGameView_802D6A9C()->fn_802D4C10(upper, wallCell);
                    if (slot != -1) {
                        getGameView_802D6A9C()->setScreenMaskBit_802D7D00(upper, slot, true);
                    }
                    wallCell++;
                    if ((wallCell & 0xF) == 0) {
                        wallCell += 0xF0;
                    }
                    getGameView_802D6A9C()->fn_802D782C(1);
                    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(upper, wallCell, 0x6D);
                    slot = getGameView_802D6A9C()->fn_802D4C10(upper, wallCell);
                    if (slot != -1) {
                        getGameView_802D6A9C()->setScreenMaskBit_802D7D00(upper, slot, true);
                    }
                    getActorManager_801F856C()->spawnActor_801F9484('SEN2', layer, &wallPos, 0x3C00, -1, -1);
                    spawnEmitter_8013CC50(layer, pos, 0x24A, 1, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(layer, pos, 0x24B, 1, 0, NULL, NULL, true, 1.0f);
                }
                p.x += 16.0f;
            }
            p.y += 16.0f;
        }
    }
    return result;
}

// FIXME(shotgun): 91.69 %, missing/extra; behaviour near.
// The original copies layer into a second register before the fn_802D4C10
// call and passes that copy in the default and 0xA0 cases.
void BgRewrite_804A7C98::writeTilePair_8029EC84(s32 cell, s32 layer, s32 layerOffset, u16 tileA, u16 tileB) {
    s32 layerPlus = layer + layerOffset;
    int index = cell;
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layerPlus, index, tileB);
    if ((index & 0x100) != ((index - 0x10) & 0x100)) {
        index -= 0x110;
    } else {
        index -= 0x10;
    }
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layerPlus, index, tileA);
    s32 slot = getGameView_802D6A9C()->fn_802D4C10(layer, cell);
    s32 value = lbl_8046C4C8[(u32)slot >> 2];
    u16 tile = tileA;
    switch (tile) {
    default:
    case 0:
        getGameView_802D6A9C()->fn_802D7BE8(layer, value, 1);
        break;
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
        getGameView_802D6A9C()->fn_802D7BE8(layer, value, tile - 0x16);
        break;
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xFFF:
        getGameView_802D6A9C()->fn_802D7BE8(layer, value, tile - 0x96);
        break;
    }
}

// FIXME(shotgun): 99.49 %, different; behaviour near.
// 2 opcodes differ
void BgRewrite_804A7C98::fn_8029E9EC(const JGeometry::TVec3f* pos, s32 layer, u16 tile, s32 arg4, u8 dir) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    if (arg4 == 0) {
        if ((tile >= 0x21 && tile <= 0x25) || tile == 0x26 || tile == 0x27 || tile == 0x36 || tile == 0x37) {
            JGeometry::TVec2f q(pos->x + lbl_8046C508[dir][0], pos->y + lbl_8046C508[dir][1]);
            field_0x20 = getCellIndex_80226BC4(&q, layer);
            field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, field_0x24);
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, tile);
        }
    } else {
        gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, tile);
    }
}

void BgRewrite_804A7C98::fn_8029E928(const JGeometry::TVec3f* pos, s32 layer, s32 tile) {
    gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, tile);
}

u8 BgRewrite_804A7C98::fn_8029E888(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    JGeometry::TVec3f playerPos = *getPlayerPos_8023E724(player);
    return field_0x24 == 0x30;
}

// FIXME(shotgun): 99.35 %, regalloc-only; behaviour clean.
// (the player-mask test: the original keeps the bool and compares it with 1)
bool BgRewrite_804A7C98::fn_8029E508(const JGeometry::TVec3f* pos, s32 player, s32 layer) {
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    JGeometry::TVec3f playerPos = *getPlayerPos_8023E724(player);
    if (field_0x24 == 0x19C || field_0x24 == 0x19D) {
        s32 blocked = false;
        if (gGameRoot_8053AAF8->field_0x7e == 1) {
            for (s32 i = 0; i < 4; i++) {
                if (isPlayerBitSet(gGameRoot_8053AAF8, i) == true && isPlayerActive_8023F264(i) == true && fn_80303750(i) == 1 && fn_8022C8EC(i) == 1) {
                    blocked = true;
                }
            }
        } else if (fn_80303750(0) == 1 && fn_8022C8EC(0) == 1) {
            blocked = true;
        }
        if (!blocked) {
            if (field_0x24 == 0x19D) {
                s32 cell = field_0x20;
                getGameView_802D6A9C()->fn_802D782C(1);
                gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, 0x19F);
                s32 slot = getGameView_802D6A9C()->fn_802D4C10(layer, cell);
                if (slot != -1) {
                    getGameView_802D6A9C()->setScreenMaskBit_802D7D00(layer, slot, true);
                }
                playerPos.x = pos->x - 16.0f;
                playerPos.y = pos->y;
                playerPos.z = 0.0f;
                gBgRewrite_8053AC98->fn_802A3948(&playerPos, 0, layer, 0x19E);
            } else {
                s32 cell = field_0x20;
                getGameView_802D6A9C()->fn_802D782C(1);
                gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, 0x19E);
                s32 slot = getGameView_802D6A9C()->fn_802D4C10(layer, cell);
                if (slot != -1) {
                    getGameView_802D6A9C()->setScreenMaskBit_802D7D00(layer, slot, true);
                }
                playerPos.x = 16.0f + pos->x;
                playerPos.y = pos->y;
                playerPos.z = 0.0f;
                gBgRewrite_8053AC98->fn_802A3948(&playerPos, 0, layer, 0x19F);
            }
            fn_80230FD0(player, 0x30007);
        } else {
            fn_80230FD0(player, 0x30017);
        }
        return true;
    }
    return false;
}

// FIXME(shotgun): 99.12 %, missing/extra; behaviour clean.
// 2 fewer insns (register copies of layer)
bool BgRewrite_804A7C98::fn_8029E248(const JGeometry::TVec3f* pos, s32 layer) {
    bool result = false;
    JGeometry::TVec2f p(pos->x, pos->y);
    field_0x20 = getCellIndex_80226BC4(&p, layer);
    field_0x24 = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(layer, field_0x20);
    if (field_0x24 == 0x19C || field_0x24 == 0x19D) {
        if (field_0x24 == 0x19D) {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x19F);
            JGeometry::TVec3f left(pos->x - 16.0f, pos->y, pos->z);
            gBgRewrite_8053AC98->fn_802A3948(&left, 0, layer, 0x19E);
        } else {
            gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, 0x19E);
            JGeometry::TVec3f right(pos->x + 16.0f, pos->y, pos->z);
            gBgRewrite_8053AC98->fn_802A3948(&right, 0, layer, 0x19F);
        }
        fn_80230FD0(0, 0x30007);
        result = true;
    }
    return result;
}

static inline void writeCellMarkView(int layer, s32 cell, s16 tile) {
    getGameView_802D6A9C()->fn_802D782C(1);
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, tile);
    s32 slot = getGameView_802D6A9C()->fn_802D4C10(layer, cell);
    if (slot != -1) {
        getGameView_802D6A9C()->setScreenMaskBit_802D7D00(layer, slot, true);
    }
}

// Not auto-inlined: fn_8029EDB8 calls fn_8029E050 out of line in the
// original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// FIXME(shotgun): 99.17 %, regalloc-only; behaviour clean.
// 4 opcodes differ
void BgRewrite_804A7C98::fn_8029E050(const JGeometry::TVec3f* pos, s32 layer, u16 a, u16 b, u16 c, u16 d) {
    gBgRewrite_8053AC98->fn_802A3948(pos, 0, layer, a);
    s32 cell = field_0x20 + 1;
    if ((cell & 0xF) == 0) {
        cell += 0xF0;
    }
    writeCellMarkView(layer, cell, b);
    cell = field_0x20;
    s32 below = cell + 0x10;
    if ((cell & 0x100) != (below & 0x100)) {
        below = cell + 0x110;
    }
    writeCellMarkView(layer, below, c);
    cell = below + 1;
    if ((cell & 0xF) == 0) {
        cell += 0xF0;
    }
    writeCellMarkView(layer, cell, d);
}
#pragma auto_inline reset

// FIXME: 98.46 %, regalloc-only; behaviour clean.
// Registers only: the original keeps cell/layer in r29/r30 and b/c/d in r26-r28;
// ours has cell/layer in r26/r27 and b/c/d in r28-r30.
void BgRewrite_804A7C98::fn_8029DF80(s32 cell, s32 layer, s32 a, s32 b, s32 c, s32 d) {
    int index = cell;
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, index, a);
    index++;
    if ((index & 0xF) == 0) {
        index += 0xF0;
    }
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, index, b);
    index = cell + 0x10;
    if ((cell & 0x100) != (index & 0x100)) {
        index = cell + 0x110;
    }
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, index, c);
    s32 next = index + 1;
    if ((next & 0xF) == 0) {
        next += 0xF0;
    }
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, next, d);
}

static inline void writeCell(int layer, s32 cell, u16 tile) {
    gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, tile);
}

void BgRewrite_804A7C98::writeTilePair_8029DEBC(s32 layer, s32 cell, s32 vertical, s32 a, s32 b) {
    if (vertical == 0) {
        writeCell(layer, cell, a);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        writeCell(layer, cell, b);
    } else {
        writeCell(layer, cell, a);
        s32 below = cell + 0x10;
        if ((cell & 0x100) != (below & 0x100)) {
            below = cell + 0x110;
        }
        writeCell(layer, below, b);
    }
}

// Registers only: cell, the layer copies and b/c/d sit in other callee-saved registers.
// FIXME: 97.09 %, regalloc-only; behaviour clean.
void BgRewrite_804A7C98::writeFourTiles_8029DD50(s32 layer, s32 cell, s32 vertical, s32 a, s32 b, s32 c, s32 d) {
    if (vertical == 0) {
        writeCell(layer, cell, a);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        writeCell(layer, cell, b);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        writeCell(layer, cell, c);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        writeCell(layer, cell, d);
    } else {
        writeCell(layer, cell, a);
        s32 below0 = cell + 0x10;
        if ((cell & 0x100) != (below0 & 0x100)) {
            below0 = cell + 0x110;
        }
        cell = below0;
        writeCell(layer, cell, b);
        s32 below1 = cell + 0x10;
        if ((cell & 0x100) != (below1 & 0x100)) {
            below1 = cell + 0x110;
        }
        cell = below1;
        writeCell(layer, cell, c);
        s32 below2 = cell + 0x10;
        if ((cell & 0x100) != (below2 & 0x100)) {
            below2 = cell + 0x110;
        }
        cell = below2;
        writeCell(layer, cell, d);
    }
}

// FIXME(shotgun): 92.26 %, missing/extra; behaviour drift.
// (register copies of layer / cell)
void BgRewrite_804A7C98::writeTileRun_8029DB2C(s32 layer, s32 cell, s32 vertical, u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    if (vertical == 0) {
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, a);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, b);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, c);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, d);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, e);
        cell++;
        if ((cell & 0xF) == 0) {
            cell += 0xF0;
        }
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, f);
    } else {
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, a);
        s32 below0 = cell + 0x10;
        if ((cell & 0x100) != (below0 & 0x100)) {
            below0 = cell + 0x110;
        }
        cell = below0;
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, b);
        s32 below1 = cell + 0x10;
        if ((cell & 0x100) != (below1 & 0x100)) {
            below1 = cell + 0x110;
        }
        cell = below1;
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, c);
        s32 below2 = cell + 0x10;
        if ((cell & 0x100) != (below2 & 0x100)) {
            below2 = cell + 0x110;
        }
        cell = below2;
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, d);
        s32 below3 = cell + 0x10;
        if ((cell & 0x100) != (below3 & 0x100)) {
            below3 = cell + 0x110;
        }
        cell = below3;
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, e);
        s32 below4 = cell + 0x10;
        if ((cell & 0x100) != (below4 & 0x100)) {
            below4 = cell + 0x110;
        }
        cell = below4;
        gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, f);
    }
}

static inline void writeTileBlock(s32 layer, s32 row, const u16* tiles) {
    for (int r = 0; r < 4; r++) {
        s32 cell = row;
        const u16* p = &tiles[r * 4];
        for (int i = 0; i < 4; i++) {
            u16 tile = p[i];
            getGameView_802D6A9C()->fn_802D782C(1);
            gGameRoot2_8053AB10->field_0x20->setTile_802F9974(layer, cell, tile);
            cell++;
            if ((cell & 0xF) == 0) {
                cell += 0xF0;
            }
        }
        s32 next = row + 0x10;
        if ((row & 0x100) != (next & 0x100)) {
            next = row + 0x110;
        }
        row = next;
    }
}

// FIXME(shotgun): 97.34 %, regalloc-only; behaviour clean.
// regalloc (cell kept in other registers)
void BgRewrite_804A7C98::fn_8029D8F0(const JGeometry::TVec3f* pos, s32 layer, s32 layerOffset, s32 pattern) {
    GameView_804A9950* view = getGameView_802D6A9C();
    const JGeometry::TBox2f* src = (layer & 0xFE) < 8 ? &view->mVisibleBox : &view->field_0xdc;
    JGeometry::TBox2f box;
    box = *src;
    s32 x = (s32)(pos->x - box.i.x);
    s32 y = (s32)(pos->y - box.i.y);
    if (x < 0) {
        x = 0;
    } else if (x >= 0x200) {
        x = 0x1FF;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= 0x200) {
        y = 0x1FF;
    }
    x -= 0x10;
    y -= 0x30;
    s32 start = ((y & 0x100) << 1) | (x & 0x100) | (y & 0xF0) | ((x >> 4) & 0xF);
    writeTileBlock(layer + layerOffset, start, lbl_8046C5A0[pattern]);
    s32 row2 = start + 0x10;
    if ((start & 0x100) != (row2 & 0x100)) {
        row2 = start + 0x110;
    }
    writeTileBlock(layer, row2, lbl_8046C528[pattern]);
}

void BgRewrite_804A7C98::fn_8029D898(s32 value) {
    s32 saved = getGameView_802D6A9C()->field_0x34;
    getGameView_802D6A9C()->fn_802D7970(value);
    gGameRoot_8053AAF8->startStageMusic_80138B7C();
    getGameView_802D6A9C()->fn_802D7970(saved);
}
