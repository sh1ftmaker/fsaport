#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/act/act_panel_rewrite_npc_2.h"

void PanelRewriteNPC2_804A9F58::init() {
    field_0x230 = mSpawnParam_0x90 & 0xFFF;
    field_0x234 = (mSpawnParam_0x90 >> 12) & 0xFFF;
    field_0x238 = (mSpawnParam_0x90 >> 24) & 1;
    field_0x23c = (mSpawnParam_0x90 >> 25) & 1;
    if (getGameView_802D6A9C()->fn_802D7544(field_0x16c)) {
        field_0x23c = 0;
        field_0x238 = 0;
    }
    update();
}

// Scans the 32 x 32 tile cells of this actor's layer (mLayer_0x4) for tiles
// equal to field_0x230 and rewrites them to field_0x234, with an effect (or,
// on layer 8 and up, a KMRI actor) and a sound when the flags ask for them.
void PanelRewriteNPC2_804A9F58::update() {
    s32 layer;
    JGeometry::TVec3f pos;

    if (isScreenFlagSet_0x34() == true) {
        for (s32 i = 0; i < 0x400; i++) {
            u16 tile = gGameRoot2_8053AB10->field_0x20->getTile_802F9908(mLayer_0x4, i);
            if (field_0x230 == tile) {
                gGameRoot2_8053AB10->field_0x20->setTile_802F9974(mLayer_0x4, i, field_0x234);
                if (field_0x23c != 0) {
                    cellToPos_80226CA8(i, mLayer_0x4, &pos);
                    layer = mLayer_0x4;
                    if (layer >= 8) {
                        getActorManager_801F856C()->spawnActor_801F9484(ACTOR_KMRI, layer, &pos, 0, -1, -1);
                    } else {
                        spawnEmitter_8013CC50(layer, &pos, 0x35C, 1, 0, NULL, NULL, true, 1.0f);
                    }
                }
                if (field_0x238 != 0) {
                    startSound_801F0E34(0x30003, 0);
                }
            }
        }
        getGameView_802D6A9C()->fn_802D7584(field_0x16c, 1);
        mAlive = false;
    }
}

void PanelRewriteNPC2_804A9F58::draw() {}

PanelRewriteNPC2_804A9F58::~PanelRewriteNPC2_804A9F58() {}
