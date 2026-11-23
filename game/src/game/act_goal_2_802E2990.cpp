#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Unk_8045A080.h"
#include "game/act/tu_801F9484.h"
#include "game/Effect.h"
#include "game/ActorManager.h"
#include "game/Camera.h"
#include "game/Player.h"
#include "game/StageFlag.h"
#include "game/GameState.h"
#include "JSystem/JParticle/JPAEmitter.h"

struct GoalTables_8046EA10 {
    f32 x1[6][4];
    f32 y1[6][4];
    f32 x2[6][4];
    f32 y2[6][4];
    u16 id[6][4];
    f32 hurtDefault[4][4][4];
    f32 attackDefault[4][4][4];
    f32 hurtMode1[4][3][4];
    f32 attackMode1[4][3][4];
    f32 hurtMode4[3][3][4];
    f32 attackMode4[3][3][4];
    f32 focus[6][2];
};

static const GoalTables_8046EA10 lbl_8046EA10 = {
{
    { 0.0f, 64.0f, 0.0f, 64.0f },
    { 0.0f, -48.0f, 48.0f, 0.0f },
    { 0.0f, 32.0f, 64.0f, 96.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 48.0f, 112.0f, 160.0f },
    { 0.0f, 160.0f, 0.0f, 160.0f },
},
{
    { 0.0f, 0.0f, 64.0f, 64.0f },
    { 0.0f, 48.0f, 48.0f, 96.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 32.0f, 64.0f, 96.0f },
    { 0.0f, 48.0f, 48.0f, 0.0f },
    { 0.0f, 0.0f, 96.0f, 96.0f },
},
{
    { 32.0f, 0.0f, 0.0f, -32.0f },
    { -24.0f, 24.0f, -24.0f, 24.0f },
    { 16.0f, 16.0f, 16.0f, -16.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 24.0f, 32.0f, 24.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
},
{
    { 0.0f, 32.0f, -32.0f, 0.0f },
    { 24.0f, 24.0f, -24.0f, -24.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 16.0f, 16.0f, 16.0f, 0.0f },
    { 24.0f, 0.0f, -24.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
},
{
    { 0x10D, 0x10F, 0x10F, 0x10D },
    { 0x10C, 0x10B, 0x10B, 0x10C },
    { 0x10E, 0x10E, 0x10E, 0 },
    { 0x110, 0x110, 0x110, 0 },
    { 0x10B, 0x10D, 0x10C, 0 },
    { 0x106, 0x107, 0x108, 0x109 },
},
{
    {
        { -0.0f, -8.0f, 64.0f, 8.0f },
        { -8.0f, -0.0f, 8.0f, 64.0f },
        { -8.0f, -64.0f, 8.0f, 0.0f },
        { -64.0f, -8.0f, 0.0f, 4.0f },
    },
    {
        { -2.0f, -4.0f, 64.0f, 4.0f },
        { -4.0f, -2.0f, 4.0f, 64.0f },
        { -4.0f, -64.0f, 4.0f, 2.0f },
        { -64.0f, -4.0f, 2.0f, 4.0f },
    },
    {
        { -0.0f, -8.0f, 32.0f, 8.0f },
        { -0.0f, -8.0f, 32.0f, 8.0f },
        { -0.0f, -8.0f, 32.0f, 8.0f },
        { -0.0f, -8.0f, 32.0f, 8.0f },
    },
    {
        { -8.0f, -0.0f, 8.0f, 32.0f },
        { -8.0f, -0.0f, 8.0f, 32.0f },
        { -8.0f, -0.0f, 8.0f, 32.0f },
        { -8.0f, -0.0f, 8.0f, 32.0f },
    },
},
{
    {
        { -0.0f, -7.0f, 64.0f, 7.0f },
        { -7.0f, -0.0f, 7.0f, 64.0f },
        { -7.0f, -64.0f, 7.0f, 0.0f },
        { -64.0f, -7.0f, 0.0f, 7.0f },
    },
    {
        { -2.0f, -2.0f, 64.0f, 2.0f },
        { -2.0f, -2.0f, 2.0f, 64.0f },
        { -2.0f, -64.0f, 2.0f, 2.0f },
        { -64.0f, -2.0f, 2.0f, 2.0f },
    },
    {
        { -0.0f, -7.0f, 32.0f, 7.0f },
        { -0.0f, -7.0f, 32.0f, 7.0f },
        { -0.0f, -7.0f, 32.0f, 7.0f },
        { -0.0f, -7.0f, 32.0f, 7.0f },
    },
    {
        { -7.0f, -0.0f, 7.0f, 32.0f },
        { -7.0f, -0.0f, 7.0f, 32.0f },
        { -7.0f, -0.0f, 7.0f, 32.0f },
        { -7.0f, -0.0f, 7.0f, 32.0f },
    },
},
{
    {
        { -16.0f, 6.0f, -6.0f, 16.0f },
        { -26.0f, 18.0f, -16.0f, 28.0f },
        { -36.0f, 30.0f, -26.0f, 40.0f },
    },
    {
        { 10.0f, 0.0f, 20.0f, 10.0f },
        { 20.0f, 12.0f, 30.0f, 22.0f },
        { 32.0f, 24.0f, 42.0f, 34.0f },
    },
    {
        { -44.0f, -44.0f, -34.0f, -34.0f },
        { -36.0f, -32.0f, -26.0f, -22.0f },
        { -28.0f, -20.0f, -18.0f, -10.0f },
    },
    {
        { 8.0f, -26.0f, 18.0f, -16.0f },
        { 16.0f, -36.0f, 26.0f, -26.0f },
        { 26.0f, -46.0f, 36.0f, -36.0f },
    },
},
{
    {
        { -15.0f, 7.0f, -7.0f, 15.0f },
        { -25.0f, 19.0f, -17.0f, 27.0f },
        { -35.0f, 31.0f, -27.0f, 39.0f },
    },
    {
        { 11.0f, 1.0f, 19.0f, 9.0f },
        { 21.0f, 13.0f, 29.0f, 21.0f },
        { 33.0f, 23.0f, 41.0f, 33.0f },
    },
    {
        { -43.0f, -43.0f, -35.0f, -35.0f },
        { -35.0f, -31.0f, -27.0f, -23.0f },
        { -27.0f, -19.0f, -19.0f, -11.0f },
    },
    {
        { 9.0f, -25.0f, 17.0f, -17.0f },
        { 17.0f, -35.0f, 25.0f, -27.0f },
        { 27.0f, -45.0f, 35.0f, -37.0f },
    },
},
{
    {
        { 10.0f, 0.0f, 20.0f, 10.0f },
        { 20.0f, 12.0f, 30.0f, 22.0f },
        { 32.0f, 24.0f, 42.0f, 34.0f },
    },
    {
        { 8.0f, -26.0f, 18.0f, -16.0f },
        { 16.0f, -36.0f, 26.0f, -26.0f },
        { 26.0f, -46.0f, 36.0f, -36.0f },
    },
    {
        { 8.0f, -26.0f, 18.0f, -16.0f },
        { 16.0f, -36.0f, 26.0f, -26.0f },
        { 26.0f, -46.0f, 36.0f, -36.0f },
    },
},
{
    {
        { 11.0f, 1.0f, 19.0f, 9.0f },
        { 21.0f, 13.0f, 29.0f, 21.0f },
        { 33.0f, 23.0f, 41.0f, 33.0f },
    },
    {
        { 8.0f, -26.0f, 18.0f, -16.0f },
        { 16.0f, -36.0f, 26.0f, -26.0f },
        { 26.0f, -46.0f, 36.0f, -36.0f },
    },
    {
        { 9.0f, -25.0f, 17.0f, -17.0f },
        { 17.0f, -35.0f, 25.0f, -27.0f },
        { 27.0f, -45.0f, 35.0f, -37.0f },
    },
},
{
    { 32.0f, 32.0f },
    { 0.0f, 48.0f },
    { 48.0f, 0.0f },
    { 0.0f, 48.0f },
    { 80.0f, 24.0f },
    { 80.0f, 32.0f },
},
};

// 0x802E3B2C.
// FIXME(shotgun): 84.33 %, missing/extra; behaviour drift.
void Goal2_804A9C20::init() {
    field_0x238 = mSpawnParam_0x90 & 0x1F;
    field_0x240 = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x244 = (mSpawnParam_0x90 >> 7) & 1;
    field_0x23c = gGameRoot_8053AAF8->getPlayerCount_80138A30();
    for (s32 i = 0; i < 4; i++) {
        mEmitters2c8[i] = NULL;
        mEmitters2d8[i] = NULL;
    }
    mEmitter2e8 = NULL;
    mEmitter2ec = NULL;
    field_0x2f4 = 0xFF;
    if (!getGameView_802D6A9C()->isScreenFlagSet_802D800C(field_0x240)) {
        for (s32 i = 0; i < 4; i++) {
            field_0x258[i].x = mPos.x + lbl_8046EA10.x1[field_0x238][i];
            field_0x258[i].y = mPos.y + lbl_8046EA10.y1[field_0x238][i];
            field_0x258[i].z = 0.0f;
            field_0x298[i] = 0;
            field_0x2a8[i] = 0;
            field_0x288[i] = 1;
            mEmitters2c8[i] = spawnEmitter_8013CC50(mLayer_0x4, &field_0x258[i], 0x10A, 0, 0, NULL, NULL,
                                                    true, 1.0f);
        }
        if (field_0x238 != 5) {
            for (s32 i = 0; i < 4; i++) {
                field_0x2b8[i] = lbl_8046EA10.id[field_0x238][i];
                if (lbl_8046EA10.id[field_0x238][i] != 0) {
                    JGeometry::TVec3f pos(field_0x258[i].x + lbl_8046EA10.x2[field_0x238][i],
                                          field_0x258[i].y + lbl_8046EA10.y2[field_0x238][i],
                                          field_0x258[i].z);
                    mEmitters2d8[i] = spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_8046EA10.id[field_0x238][i],
                                                            1, 1, NULL, NULL, true, 1.0f);
                }
            }
        }
        field_0x230 = 0;
        field_0x234 = 0;
        field_0x24c[0] = mPos.x;
        field_0x24c[1] = mPos.y;
        field_0x24c[2] = mPos.z;
    } else {
        field_0x24c[0] = mPos.x;
        field_0x24c[1] = mPos.y;
        field_0x24c[2] = mPos.z;
        field_0x230 = 5;
    }
    if (field_0x238 == 5) {
        JGeometry::TVec3f pos(mPos.x + 80.0f, mPos.y + 48.0f, mPos.z);
        mEmitter2e8 = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0xC6, 0, 0, NULL, NULL, true, 1.0f);
        JGeometry::TVec3f pos2(mPos.x + 80.0f, mPos.y + 48.0f, mPos.z);
        mEmitter2ec = spawnEmitter_8013CC50(mLayer_0x4, &pos2, 0xC7, 0, 0, NULL, NULL, true, 1.0f);
    }
    field_0x248 = 0;
    if (field_0x238 != 5) {
        gAudioMgr_8053AF80->fn_80457E54(true);
    }
}

// 0x802E3A48.
Goal2_804A9C20::~Goal2_804A9C20() {
    for (s32 i = 0; i < 4; i++) {
        if (mEmitters2c8[i] != NULL) {
            releaseEmitter_801EE434(mEmitters2c8[i]);
        }
        if (mEmitters2d8[i] != NULL) {
            releaseEmitter_801EE434(mEmitters2d8[i]);
        }
    }
    if (mEmitter2e8 != NULL) {
        releaseEmitter_801EE434(mEmitter2e8);
    }
    if (mEmitter2ec != NULL) {
        releaseEmitter_801EE434(mEmitter2ec);
    }
    if (field_0x238 != 5) {
        gAudioMgr_8053AF80->fn_80457E54(false);
    }
}

// 0x802E2C28.
// FIXME(shotgun): 94.24 %, missing/extra; behaviour drift.
void Goal2_804A9C20::update() {
    if (field_0x230 < 1) {
        field_0x248++;
    }
    mPos.x = field_0x24c[0];
    mPos.y = field_0x24c[1];
    mPos.z = field_0x24c[2];
    f32 savedX = mPos.x;
    f32 savedY = mPos.y;
    f32 savedZ = mPos.z;
    switch (field_0x230) {
    case 2:
        field_0x2c4--;
        if (field_0x2c4 == 0) {
            lbl_8053AC00 = 1;
            field_0x230 = 0;
        }
        // fallthrough
    case 0:
        {
            s32 downCount = 0;
            s32 i;
            for (i = 0; i < 4; i++) {
                if (field_0x288[i] == 4) {
                    downCount++;
                }
            }
            for (i = 0; i < 4; i++) {
                mPos = field_0x258[i];
                mBox.set(-10.0f, -10.0f, 10.0f, 10.0f);
                JGeometry::TBox2f area;
                area.set(mBox);
                JGeometry::TVec2f xy(mPos.x, mPos.y);
                area.addPos(xy);
                s32 player = fn_8023B8E4(&area, mLayer_0x4, &xy, true);
                if (field_0x288[i] == 1) {
                    if (player >= 0 && fn_80237060(player) == 2) {
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x113, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x114, 1, 1, NULL, NULL, true, 1.0f);
                        field_0x288[i] = 2;
                        field_0x298[i] = 20;
                        field_0x2a8[i] = 0;
                        field_0x2c0[i] = 0xFF;
                        startSound_801F0E34(0x3003A, 0);
                        gGameRoot_8053AAF8->startRumble_80134DE4(1, 0);
                        if (downCount >= 3) {
                            field_0x288[i] = 3;
                            field_0x2c0[i] = 0;
                            mEmitters2c8[i]->setGlobalAlpha(field_0x2c0[i]);
                        }
                    } else if (player >= 0) {
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x7B, 0, 0, NULL, NULL, true, 1.0f);
                        JGeometry::TVec2f hit;
                        hit.set(mPos.x, mPos.y);
                        hitPlayerGroup_80238548(player, 0, &hit, 10);
                        startSound_801F0E34(0x3008B, 0);
                        field_0x230 = 2;
                        field_0x2c4 = 0x2D;
                    }
                } else if (field_0x288[i] == 2) {
                    field_0x2c0[i] -= 12;
                    field_0x298[i]--;
                    if (field_0x298[i] == 0) {
                        field_0x2c0[i] = 0;
                        field_0x288[i] = 3;
                    }
                    mEmitters2c8[i]->setGlobalAlpha(field_0x2c0[i]);
                } else if (field_0x288[i] == 3) {
                    if (mEmitters2c8[i] != NULL) {
                        releaseEmitter_801EE434(mEmitters2c8[i]);
                        mEmitters2c8[i] = NULL;
                    }
                    field_0x288[i] = 4;
                    if (downCount >= 3) {
                        field_0x298[i] = 1;
                    }
                } else if (field_0x288[i] == 4) {
                    if (field_0x2a8[i] != 0) {
                        field_0x2a8[i]--;
                    }
                    if (field_0x2a8[i] == 0 && player >= 0) {
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x113, 1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x114, 1, 1, NULL, NULL, true, 1.0f);
                        field_0x2a8[i] = 20;
                    }
                }
                mMapBox_0x80.set(-10.0f, -10.0f, 10.0f, 10.0f);
                area = mMapBox_0x80;
                xy.set(mPos.x, mPos.y);
                area.addPos(xy);
                fn_80239914(&area, mLayer_0x4, mSerial);
                if (field_0x2b8[i] != 0) {
                    switch (field_0x238) {
                    case 0:
                    case 2:
                    case 3:
                    default:
                        mBox.set(lbl_8046EA10.hurtDefault[field_0x238][i][0],
                                 lbl_8046EA10.hurtDefault[field_0x238][i][1],
                                 lbl_8046EA10.hurtDefault[field_0x238][i][2],
                                 lbl_8046EA10.hurtDefault[field_0x238][i][3]);
                        area.set(mBox);
                        xy.set(mPos.x, mPos.y);
                        area.addPos(xy);
                        if (!damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 0x1A)) {
                            mMapBox_0x80.set(lbl_8046EA10.attackDefault[field_0x238][i][0],
                                           lbl_8046EA10.attackDefault[field_0x238][i][1],
                                           lbl_8046EA10.attackDefault[field_0x238][i][2],
                                           lbl_8046EA10.attackDefault[field_0x238][i][3]);
                            area.set(mMapBox_0x80);
                            xy.set(mPos.x, mPos.y);
                            area.addPos(xy);
                            fn_80239914(&area, mLayer_0x4, mSerial);
                        }
                        break;
                    case 1:
                        for (s32 j = 0; j < 3; j++) {
                            mBox.set(lbl_8046EA10.hurtMode1[i][j][0],
                                     lbl_8046EA10.hurtMode1[i][j][1],
                                     lbl_8046EA10.hurtMode1[i][j][2],
                                     lbl_8046EA10.hurtMode1[i][j][3]);
                            area.set(mBox);
                            xy.set(mPos.x, mPos.y);
                            area.addPos(xy);
                            if (!damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 0x1A)) {
                                mMapBox_0x80.set(lbl_8046EA10.attackMode1[i][j][0],
                                               lbl_8046EA10.attackMode1[i][j][1],
                                               lbl_8046EA10.attackMode1[i][j][2],
                                               lbl_8046EA10.attackMode1[i][j][3]);
                                area.set(mMapBox_0x80);
                                xy.set(mPos.x, mPos.y);
                                area.addPos(xy);
                                fn_80239914(&area, mLayer_0x4, mSerial);
                            }
                        }
                        break;
                    case 4:
                        if (i < 3) {
                            if (i != 1) {
                                for (s32 j = 0; j < 3; j++) {
                                    mBox.set(lbl_8046EA10.hurtMode4[i][j][0],
                                             lbl_8046EA10.hurtMode4[i][j][1],
                                             lbl_8046EA10.hurtMode4[i][j][2],
                                             lbl_8046EA10.hurtMode4[i][j][3]);
                                    area.set(mBox);
                                    xy.set(mPos.x, mPos.y);
                                    area.addPos(xy);
                                    if (!damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 4)) {
                                        mMapBox_0x80.set(lbl_8046EA10.attackMode4[i][j][0],
                                                       lbl_8046EA10.attackMode4[i][j][1],
                                                       lbl_8046EA10.attackMode4[i][j][2],
                                                       lbl_8046EA10.attackMode4[i][j][3]);
                                        area.set(mMapBox_0x80);
                                        xy.set(mPos.x, mPos.y);
                                        area.addPos(xy);
                                        fn_80239914(&area, mLayer_0x4, mSerial);
                                    }
                                }
                            } else {
                                mBox.set(0.0f, -8.0f, 64.0f, 8.0f);
                                area.set(mBox);
                                xy.set(mPos.x, mPos.y);
                                area.addPos(xy);
                                if (!damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 4)) {
                                    mMapBox_0x80.set(0.0f, -7.0f, 64.0f, 7.0f);
                                    area.set(mMapBox_0x80);
                                    xy.set(mPos.x, mPos.y);
                                    area.addPos(xy);
                                    fn_80239914(&area, mLayer_0x4, mSerial);
                                }
                            }
                        }
                        break;
                    case 5:
                        break;
                    }
                }
            }
            s32 anyUp = 0;
            for (i = 0; i < 4; i++) {
                if (field_0x288[i] != 4) {
                    anyUp = 1;
                }
            }
            if (anyUp == 0) {
                for (i = 0; i < 4; i++) {
                    mPos = field_0x258[i];
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x7B, 0, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x7C, 0, 0, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x7D, 0, 0, NULL, NULL, true, 1.0f);
                    if (mEmitters2d8[i] != NULL) {
                        mEmitters2d8[i]->becomeInvalidEmitter();
                        mEmitters2d8[i] = NULL;
                    }
                }
                if (field_0x238 == 5) {
                    setScreenFlag_0x30(0xFF, true);
                } else if (field_0x244 != 0) {
                    setScreenFlag_0x30(0xFF, true);
                }
                if (fn_802DE7CC()) {
                    if (!isStageFlagSet(lbl_80529DEC.mStage_0x118, lbl_80529DEC.mLevel_0x11c)) {
                        lbl_8053AED0->requestMessage_8043D67C(0x341, 0, 0);
                    }
                }
                startSound_801F0E34(0x30039, 0);
                field_0x230 = 1;
                field_0x234 = 20;
            }
        }
        break;
    case 1:
        field_0x234--;
        if (field_0x234 == 0) {
            field_0x230 = 4;
            field_0x234 = 100;
        }
        break;
    case 4:
        field_0x234--;
        if (field_0x234 == 0) {
            if (field_0x238 != 5) {
                mPos.x = savedX;
                mPos.y = savedY;
                mPos.z = savedZ;
                s32 kind = 0;
                if (field_0x248 < 0x258) {
                    if (gGameRoot_8053AAF8->field_0x7e == 1) {
                        kind = 2;
                    } else {
                        kind = 1;
                    }
                }
                if (!getGameView_802D6A9C()->fn_802D75BC(field_0x16c)) {
                    fn_801F34DC();
                } else {
                    kind = 0;
                }
                if (!(getGameView_802D6A9C()->mStage_0x4 == 7 && getGameView_802D6A9C()->mLevel_0x8 == 3)) {
                    getActorManager_801F856C()->spawnActor_801F9484(
                        'GOAL', mLayer_0x4, &mPos, (kind << 17) + ((field_0x238 + 1) << 12) + 2, -1, -1);
                }
                mAlive = false;
            } else {
                field_0x230 = 5;
            }
        } else if (field_0x234 == 0x3C) {
            playSe_801F0D20(3);
        }
        break;
    case 3:
    case 5:
        break;
    default:
        break;
    }
    if (field_0x230 < 4) {
        startSound_801F0E34(0x30038, 0);
    }
    mPos.x = savedX;
    mPos.y = savedY;
    mPos.z = savedZ;
    JGeometry::TVec3f focus(mPos.x + lbl_8046EA10.focus[field_0x238][0],
                            mPos.y + lbl_8046EA10.focus[field_0x238][1], mPos.z);
    gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&focus);
    if (mEmitter2e8 != NULL) {
        mEmitter2e8->setGlobalAlpha(field_0x2f4);
    }
    if (mEmitter2ec != NULL) {
        mEmitter2ec->setGlobalAlpha(field_0x2f4);
    }
}

// 0x802E2998.
void Goal2_804A9C20::draw() {
    if (field_0x230 <= 3) {
        f32 savedX = mPos.x;
        f32 savedY = mPos.y;
        f32 savedZ = mPos.z;
        for (s32 i = 0; i < 4; i++) {
            mPos = field_0x258[i];
            if (field_0x288[i] == 1) {
                drawSprite_801F06F0("enemy_gc_sob.bin", 0x25A, NULL, 7, 1.0f, 1.0f);
            } else {
                drawSprite_801F06F0("enemy_gc_sob.bin", 0x25B, NULL, 7, 1.0f, 1.0f);
            }
        }
        mPos.x = savedX;
        mPos.y = savedY;
        mPos.z = savedZ;
    }
    if (field_0x238 == 5) {
        SpriteParams_801F5AE0 params;
        params.field_0x1d = -2;
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('TIMG', "shikimono.bti");
        JGeometry::TVec3f pos(mPos.x + 80.0f, mPos.y + 48.0f, mPos.z);
        if (mLayer_0x4 < 8) {
            gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&pos, res, &params);
        }
    }
}

// 0x802E2990.
void Goal2_804A9C20::fn_802E2990(u8 value) {
    field_0x2f4 = value;
}
