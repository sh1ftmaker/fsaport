#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/cls_8016ED88.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/GameMode.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/J3D/J3DModel.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "JSystem/JUtility/JUTVideo.h"
#include "game/Map.h"
#include "game/Camera.h"
#include "dolphin/gx.h"
#include "JSystem/J3D/J3DModelLoader.h"
#include "JSystem/J3D/J3DAnmLoader.h"
#include "JSystem/J3D/J3DAnmTextureSRTKey.h"
#include "JSystem/J3D/J3DAnmTevRegKey.h"
#include "JSystem/J3D/J3DMaterialAnm.h"
#include "JSystem/J3D/J3DSys.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/Player.h"

// The colours of the backdrop GX setups (the user of each in the comment;
// none of the three users is written yet). File-scope objects: they come
// before the unit's literal pool in .sdata2, in declaration order (a function's own constants are pooled in
// first-use order), and equal colours are separate objects.
static const GXColor lbl_8053D910 = {0x00, 0x50, 0x78, 0x00}; // fn_8016CB14
static const GXColor lbl_8053D914 = {0x38, 0x60, 0xA8, 0xFF}; // fn_8016CB14
static const GXColor lbl_8053D918 = {0x00, 0x00, 0x00, 0x2F}; // fn_8016C2FC
static const GXColor lbl_8053D91C = {0xAF, 0xCF, 0xCF, 0xFF}; // fn_8016C2FC
static const GXColor lbl_8053D920 = {0xCF, 0xE7, 0xE7, 0xFF}; // fn_8016C2FC
static const GXColor lbl_8053D924 = {0x00, 0x00, 0x00, 0x2F}; // fn_8016B910
static const GXColor lbl_8053D928 = {0x36, 0x61, 0x82, 0xFF}; // fn_8016B910
static const GXColor lbl_8053D92C = {0xAF, 0xCF, 0xCF, 0xFF}; // fn_8016B910
static const GXColor lbl_8053D930 = {0x36, 0x61, 0x82, 0xFF}; // fn_8016B910
static const GXColor lbl_8053D934 = {0xCF, 0xE7, 0xE7, 0xFF}; // fn_8016B910

// Forward declarations.
void pool_8053D938(JGeometry::TVec3f& center, JGeometry::TVec3f& half);

cls_8016FDE0::cls_8016FDE0() {}

// The six cloud models: bmd, btk and brk of each.
static const char* lbl_804A0790[6][3] = {
    {"w_kumo.bmd", "w_kumo.btk", NULL},
    {"w_kumo1.bmd", "w_kumo1.btk", "w_kumo1.brk"},
    {"w_kumo2.bmd", "w_kumo2.btk", "w_kumo2.brk"},
    {"w_kumo3.bmd", "w_kumo3.btk", "w_kumo3.brk"},
    {"w_kumo4.bmd", "w_kumo4.btk", "w_kumo4.brk"},
    {"w_kumo5.bmd", NULL, NULL},
};

// A texture from the TIMG resource `name` of `archive`, or NULL when there is none.
static inline JUTTexture* loadTexture(JKRArchive* archive, const char* name, JKRHeap* heap) {
    JUTTexture* texture = NULL;
    if (archive != NULL && archive->getResource('TIMG', name) != NULL) {
        texture = new (heap, 0) JUTTexture(static_cast<const ResTIMG*>(archive->getResource('TIMG', name)), 0);
    }
    return texture;
}

// FIXME: 97.43 %, scheduling+regalloc + reloc-name; behaviour clean.
GameRootSub24_8016A3AC::GameRootSub24_8016A3AC(JKRHeap* heap) {
    mSolidHeap_0x0 = JKRSolidHeap::create(-1, heap, false);
    mSolidHeap_0x0->becomeCurrentHeap();
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "yougan.bmd", "yougan.btk", NULL, NULL, NULL, 0x240000, mSolidHeap_0x0,
                &mYouganModel_0x4, &mYouganBtk_0x8, NULL, NULL, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "doku_yougan.bmd", "doku_yougan.btk", NULL, NULL, NULL, 0x240000,
                mSolidHeap_0x0, &mDokuYouganModel_0xc, &mDokuYouganBtk_0x10, NULL, NULL, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "yami_yougan.bmd", "yami_yougan.btk", NULL, NULL, NULL, 0x240000,
                mSolidHeap_0x0, &mYamiYouganModel_0x14, &mYamiYouganBtk_0x18, NULL, NULL, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "moya.bmd", "moya.btk", NULL, "moya.brk", NULL, 0x240000, mSolidHeap_0x0,
                &mMoyaModel_0x1c, &mMoyaBtk_0x20, NULL, &mMoyaBrk_0x24, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "darkFilter00.bmd", "darkFilter00.btk", NULL, "darkFilter00.brk", NULL,
                0x11240000, mSolidHeap_0x0, &mDarkFilterModel_0x28, &mDarkFilterBtk_0x2c, NULL, &mDarkFilterBrk_0x30, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "fogFilter00.bmd", "fogFilter00.btk", NULL, "fogFilter00.brk", NULL,
                0x11240000, mSolidHeap_0x0, &mFogFilterModel_0x90, &mFogFilterBtk_0x94, NULL, &mFogFilterBrk_0x98, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "cloudShadowFilter00.bmd", "cloudShadowFilter00.btk", NULL,
                "cloudShadowFilter00.brk", NULL, 0x11240000, mSolidHeap_0x0, &mCloudShadowFilterModel_0x9c, &mCloudShadowFilterBtk_0xa0, NULL,
                &mCloudShadowFilterBrk_0xa4, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "poisonSwamp00.bmd", "poisonSwamp00.btk", NULL, "poisonSwamp00.brk",
                NULL, 0x11240000, mSolidHeap_0x0, &mPoisonSwampModel_0xa8, &mPoisonSwampBtk_0xac, NULL, &mPoisonSwampBrk_0xb0, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "blackCloudFilter.bmd", "blackCloudFilter.btk", NULL,
                "blackCloudFilter.brk", NULL, 0x11240000, mSolidHeap_0x0, &mBlackCloudFilterModel_0xb4, &mBlackCloudFilterBtk_0xb8, NULL, &mBlackCloudFilterBrk_0xbc,
                NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "mekakusi.bmd", "mekakusi.btk", NULL, NULL, NULL, 0x1240000, mSolidHeap_0x0,
                &mMekakusiModel_0x34, &mMekakusiBtk_0x38, NULL, NULL, NULL);

    loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "yuka_2.bmd", NULL, NULL, NULL, NULL, 0x220000, mSolidHeap_0x0, &mYukaModel_0xec,
                NULL, NULL, NULL, NULL);
    mYukaBtk_0xf0 = NULL;
    if (mYukaModel_0xec == NULL) {
        loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "yuka_4.bmd", NULL, NULL, NULL, NULL, 0x220000, mSolidHeap_0x0,
                    &mYukaModel_0xec, NULL, NULL, NULL, NULL);
    }
    if (mYukaModel_0xec == NULL) {
        loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "yuka_6.bmd", "yuka_6.btk", NULL, NULL, NULL, 0x220000, mSolidHeap_0x0,
                    &mYukaModel_0xec, &mYukaBtk_0xf0, NULL, NULL, NULL);
    }
    if (mYukaModel_0xec == NULL) {
        loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "yuka_7.bmd", NULL, NULL, NULL, NULL, 0x220000, mSolidHeap_0x0,
                    &mYukaModel_0xec, NULL, NULL, NULL, NULL);
    }
    if (mYukaModel_0xec == NULL) {
        loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "yuka_8.bmd", NULL, NULL, NULL, NULL, 0x220000, mSolidHeap_0x0,
                    &mYukaModel_0xec, NULL, NULL, NULL, NULL);
    }

    for (int i = 0; i < 6; i++) {
        loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, lbl_804A0790[i][0], lbl_804A0790[i][1], NULL, lbl_804A0790[i][2],
                    NULL, 0x10240000, mSolidHeap_0x0, &mKumoModels_0x3c[i], &mKumoBtks_0x54[i], NULL, &mKumoBrks_0x6c[i], NULL);
    }
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "forestFilter00.bmd", "forestFilter00.btk", NULL, "forestFilter00.brk",
                NULL, 0x11240000, mSolidHeap_0x0, &mForestFilterModel_0x84, &mForestFilterBtk_0x88, NULL, &mForestFilterBrk_0x8c, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "morningLight00.bmd", "morningLight00.btk", NULL, "morningLight00.brk",
                NULL, 0x240000, mSolidHeap_0x0, &mMorningLightModel_0xc0, &mMorningLightBtk_0xc4, NULL, &mMorningLightBrk_0xc8, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "nagarekumo.bmd", "nagarekumo.btk", NULL, NULL, NULL, 0x240000,
                mSolidHeap_0x0, &mNagarekumoModel_0xcc, &mNagarekumoBtk_0xd0, NULL, NULL, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mBossArchive_0x10, "darkFog00.bmd", "darkFog00.btk", NULL, "darkFog00.brk", NULL,
                0x11240000, mSolidHeap_0x0, &mDarkFogModel_0xd4, &mDarkFogBtk_0xd8, NULL, &mDarkFogBrk_0xdc, NULL);
    loadModelWithAnims_8016A3BC(lbl_80529DEC.mDataArchive_0x14, "moya_red.bmd", "moya_red.btk", NULL, "moya_red.brk", NULL, 0x240000,
                mSolidHeap_0x0, &mMoyaRedModel_0xe0, &mMoyaRedBtk_0xe4, NULL, &mMoyaRedBrk_0xe8, NULL);
    mSolidHeap_0x0->adjustSize();
    heap->becomeCurrentHeap();

    mKumoEnvTexture_0xf4 = new (heap, 0) JUTTexture(
        static_cast<const ResTIMG*>(gGameRoot_8053AAF8->field_0xa4->getResource('?\?\?\?', "kumo_env.bti")), 0);
    mSmokeTexture_0xf8 = new (heap, 0) JUTTexture(
        static_cast<const ResTIMG*>(gGameRoot_8053AAF8->field_0xa4->getResource('?\?\?\?', "smoke.bti")), 0);
    mKawazokoTexture_0xfc = new (heap, 0) JUTTexture(
        static_cast<const ResTIMG*>(gGameRoot_8053AAF8->field_0xa4->getResource('?\?\?\?', "kawazoko.bti")), 0);
    mNoizeKawazokoTexture_0x100 = new (heap, 0) JUTTexture(static_cast<const ResTIMG*>(
        gGameRoot_8053AAF8->field_0xa4->getResource('?\?\?\?', "noize_kawazoko_1.bti")), 0);
    mBgMoriTexture_0x104 = loadTexture(lbl_80529DEC.mBossArchive_0x10, "bg_mori.bti", heap);
    mEnkeiTexture_0x108 = loadTexture(lbl_80529DEC.mBossArchive_0x10, "enkei.bti", heap);
    mEnkeiDarkTexture_0x10c = loadTexture(lbl_80529DEC.mBossArchive_0x10, "enkei_dark.bti", heap);
    mWallTexture_0x110 = loadTexture(lbl_80529DEC.mBossArchive_0x10, "wall.bti", heap);
    mSkyTexture_0x114 = loadTexture(lbl_80529DEC.mBossArchive_0x10, "sky.bti", heap);
    mWallKowareTexture_0x118 = loadTexture(lbl_80529DEC.mBossArchive_0x10, "wall_koware.bti", heap);
    mSkyKowareTexture_0x11c = loadTexture(lbl_80529DEC.mBossArchive_0x10, "sky_koware.bti", heap);
    mBgPiramidTexture_0x120 = loadTexture(lbl_80529DEC.mBossArchive_0x10, "bg_piramid.bti", heap);

    for (int i = 0; i < 4; i++) {
        field_0x140.mEntries[i].field_0x18 = new ActorSoundObj_804578F4(NULL, NULL, 8);
    }
}

GameRootSub24_8016A3AC::~GameRootSub24_8016A3AC() {
    for (int i = 0; i < 4; i++) {
        JPABaseEmitter* emitter = field_0x140.mEntries[i].field_0x0;
        if (emitter != NULL) {
            getEffectSystem_8013C824()->mEmitterManager->forceDeleteEmitter(emitter);
        }
        if (field_0x140.mEntries[i].field_0x18 != NULL) {
            delete field_0x140.mEntries[i].field_0x18;
        }
    }
    if (field_0x138 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x138);
    }
    if (field_0x13c != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x13c);
    }
    if (field_0x130 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x130);
    }
    if (field_0x134 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x134);
    }
    if (field_0x124 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x124);
    }
    if (field_0x128 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x128);
    }
    if (field_0x12c != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x12c);
    }
    if (mBgPiramidTexture_0x120 != NULL) {
        delete mBgPiramidTexture_0x120;
    }
    if (mSkyKowareTexture_0x11c != NULL) {
        delete mSkyKowareTexture_0x11c;
    }
    if (mWallKowareTexture_0x118 != NULL) {
        delete mWallKowareTexture_0x118;
    }
    if (mSkyTexture_0x114 != NULL) {
        delete mSkyTexture_0x114;
    }
    if (mWallTexture_0x110 != NULL) {
        delete mWallTexture_0x110;
    }
    if (mEnkeiDarkTexture_0x10c != NULL) {
        delete mEnkeiDarkTexture_0x10c;
    }
    if (mEnkeiTexture_0x108 != NULL) {
        delete mEnkeiTexture_0x108;
    }
    if (mBgMoriTexture_0x104 != NULL) {
        delete mBgMoriTexture_0x104;
    }
    delete mNoizeKawazokoTexture_0x100;
    delete mKawazokoTexture_0xfc;
    delete mSmokeTexture_0xf8;
    delete mKumoEnvTexture_0xf4;
    mSolidHeap_0x0->destroy();
}

void GameRootSub24_8016A3AC::fn_8016EDB0() {
    fn_8016A330();
    field_0x130 = 0;
    field_0x134 = 0;
    field_0x124 = NULL;
    field_0x128 = NULL;
    field_0x12c = NULL;
    field_0x13c = 0;
    field_0x138 = 0;
    field_0x1b0 = 0x78;
    field_0x1b8 = 0x3c;
    field_0x1b4 = 0;
    field_0x140.fn_8016ED88();
    field_0x1bc = 0;
    field_0x1c0 = 0;
}

void cls_8016ED88::fn_8016ED88() {
    for (int i = 0; i < 4; i++) {
        mEntries[i].field_0x0 = NULL;
        mEntries[i].field_0x14 = 0;
    }
}

// Still differs: the stack slots of the first two zero vectors are swapped.
// FIXME: 99.87 %, scheduling+regalloc; behaviour clean.
void GameRootSub24_8016A3AC::fn_8016DE30() {
    if (isGameModeSet()) {
        if (field_0x1d2) {
            if (field_0x128 == NULL) {
                JGeometry::TVec3f pos(0.0f, 0.0f, 0.0f);
                field_0x128 = spawnEmitter_8013CC50(0, &pos, 0x543, 0, 0, NULL, NULL, true, 1.0f);
            }
            if (field_0x12c == NULL) {
                JGeometry::TVec3f pos(0.0f, 0.0f, 0.0f);
                field_0x12c = spawnEmitter_8013CC50(0, &pos, 0x544, 0, 0, NULL, NULL, true, 1.0f);
            }
        } else {
            if (field_0x128 != NULL) {
                JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
                manager->forceDeleteEmitter(field_0x128);
                field_0x128 = NULL;
            }
            if (field_0x12c != NULL) {
                JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
                manager->forceDeleteEmitter(field_0x12c);
                field_0x12c = NULL;
            }
        }

        if (field_0x1d1) {
            TileMap_804AA858* map = gGameRoot2_8053AB10->field_0x20;
            JGeometry::TVec3f pos = map->mLayerOrigin_0x2368;
            pos.x += 0.5f * (8.0f * map->mHeightGridCellsX_0x2380);
            pos.y += 0.5f * (8.0f * map->mHeightGridCellsY_0x2384);
            if (field_0x130 != NULL) {
                field_0x130->setGlobalTranslation(pos);
                field_0x134->setGlobalTranslation(pos);
            } else {
                field_0x130 = spawnEmitter_8013CC50(0, &pos, 0x222, 0, 0, NULL, NULL, true, 1.0f);
                field_0x134 = spawnEmitter_8013CC50(0, &pos, 0x223, 0, 0, NULL, NULL, true, 1.0f);
            }
        } else {
            if (field_0x130 != NULL) {
                field_0x130->becomeInvalidEmitter();
                field_0x130 = NULL;
            }
            if (field_0x134 != NULL) {
                field_0x134->becomeInvalidEmitter();
                field_0x134 = NULL;
            }
        }

        if (field_0x1c4 && (getGameView_802D6A9C()->isInTransition_802DCD0C() || !field_0x1bc)) {
            TileMap_804AA858* map = gGameRoot2_8053AB10->field_0x20;
            Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
            f32 camX = -camera->mViewMtx_0x4[0][3];
            f32 camY = -camera->mViewMtx_0x4[1][3];
            f32 halfX = 0.5f * camX;
            f32 halfY = 0.5f * camY;
            f32 width = 8.0f * map->mHeightGridCellsX_0x2380;
            f32 height = 8.0f * map->mHeightGridCellsY_0x2384;
            JGeometry::TVec3f pos(width * (s32)(halfX / width) + (camX - halfX),
                                  height * (s32)(halfY / height) + (camY - halfY), 0.0f);
            if (field_0x138 != NULL) {
                setEmitterPos_8013CB44(field_0x138, &pos, -1, 0, true);
            } else {
                field_0x138 = spawnEmitter_8013CC50(0, &pos, 0x2a8, -1, 0, NULL, NULL, true, 1.0f);
            }
        } else if (field_0x138 != NULL) {
            field_0x138->becomeInvalidEmitter();
            field_0x138 = NULL;
        }

        if (field_0x1d3) {
            TileMap_804AA858* map = gGameRoot2_8053AB10->field_0x20;
            JGeometry::TVec3f pos = map->mLayerOrigin_0x2368;
            pos.x += 0.5f * (8.0f * map->mHeightGridCellsX_0x2380);
            pos.y += 0.5f * (8.0f * map->mHeightGridCellsY_0x2384);
            if (field_0x13c != NULL) {
                setEmitterPos_8013CB44(field_0x13c, &pos, -1, 0, true);
            } else {
                field_0x13c = spawnEmitter_8013CC50(0, &pos, 0x385, -1, 0, NULL, NULL, true, 1.0f);
            }
        } else if (field_0x13c != NULL) {
            field_0x13c->becomeInvalidEmitter();
            field_0x13c = NULL;
        }

        field_0x1c0++;
    }

    if ((!isGameModeSet() || !getGameView_802D6A9C()->isInTransition_802DCD0C()) && gGameRoot_8053AAF8->field_0x94 == 0 &&
        gGameRoot_8053AAF8->field_0x96 == 0 &&
        (!gGameRoot_8053AAF8->field_0x7f ||
         (!fn_80239D38() && !(gGameRoot_8053AAF8->field_0x4c == 11 ? true : gGameRoot_8053AAF8->field_0x88 == 2))) &&
        lbl_80529DEC.field_0x240 == 0 && lbl_8053AEC8->field_0x5d == 0) {
        field_0x1c4 = 0;
        field_0x1c5 = 0;
        field_0x1c6 = 0;
        field_0x1c7 = 0;
        field_0x1c8 = 0;
        field_0x1c9 = 0;
        field_0x1ca = 0;
        field_0x1cb[0] = 0;
        field_0x1cb[1] = 0;
        field_0x1cb[2] = 0;
        field_0x1cb[3] = 0;
        field_0x1cf = 0;
        field_0x1d1 = 0;
        field_0x1d2 = 0;
        field_0x1d3 = 0;
        field_0x1dc = 0;
        field_0x1dd = 0;
    }

    field_0x1bc = getGameView_802D6A9C()->isInTransition_802DCD0C();

    if (isGameModeSet()) {
        s32 frame = field_0x1c0;
        if (!field_0x1ee) {
            if (field_0x124 != NULL) {
                field_0x124->becomeInvalidEmitter();
                field_0x124 = NULL;
            }
        } else if (field_0x124 == NULL) {
            JGeometry::TVec3f pos(0.0f, 0.0f, 0.0f);
            field_0x124 = spawnEmitter_8013CC50(0, &pos, 0x54b, 0, 0, NULL, NULL, true, 1.0f);
        }

        if (mYouganBtk_0x8 != NULL) {
            mYouganBtk_0x8->setFrame((f32)(frame % mYouganBtk_0x8->getTotalFrameCount()));
        }
        if (mDokuYouganBtk_0x10 != NULL) {
            mDokuYouganBtk_0x10->setFrame((f32)(frame % mDokuYouganBtk_0x10->getTotalFrameCount()));
        }
        if (mYamiYouganBtk_0x18 != NULL) {
            mYamiYouganBtk_0x18->setFrame((f32)(frame % mYamiYouganBtk_0x18->getTotalFrameCount()));
        }
        if (mMoyaBtk_0x20 != NULL) {
            mMoyaBtk_0x20->setFrame((f32)(frame % mMoyaBtk_0x20->getTotalFrameCount()));
            mMoyaBrk_0x24->setFrame((f32)(field_0x1ed * 120 / 255 + 1));
        }
        if (mDarkFilterBtk_0x2c != NULL) {
            mDarkFilterBtk_0x2c->setFrame((f32)(frame % mDarkFilterBtk_0x2c->getTotalFrameCount()));
            mDarkFilterBrk_0x30->setFrame((f32)(field_0x1ef * 100 / 255));
        }
        if (mMekakusiBtk_0x38 != NULL) {
            mMekakusiBtk_0x38->setFrame((f32)(frame % mMekakusiBtk_0x38->getTotalFrameCount()));
        }
        if (mForestFilterBtk_0x88 != NULL) {
            mForestFilterBtk_0x88->setFrame((f32)(frame % mForestFilterBtk_0x88->getTotalFrameCount()));
            mForestFilterBrk_0x8c->setFrame(100.0f);
        }
        if (mFogFilterBtk_0x94 != NULL) {
            mFogFilterBtk_0x94->setFrame((f32)(frame % mFogFilterBtk_0x94->getTotalFrameCount()));
            mFogFilterBrk_0x98->setFrame((f32)(field_0x1f1 * 100 / 255));
        }
        if (mCloudShadowFilterBtk_0xa0 != NULL) {
            mCloudShadowFilterBtk_0xa0->setFrame((f32)(frame % mCloudShadowFilterBtk_0xa0->getTotalFrameCount()));
            mCloudShadowFilterBrk_0xa4->setFrame((f32)(field_0x1f2 * 100 / 255));
        }
        if (mPoisonSwampBtk_0xac != NULL) {
            mPoisonSwampBtk_0xac->setFrame((f32)(frame % mPoisonSwampBtk_0xac->getTotalFrameCount()));
            mPoisonSwampBrk_0xb0->setFrame((f32)(frame % mPoisonSwampBrk_0xb0->getTotalFrameCount()));
        }
        if (mBlackCloudFilterBtk_0xb8 != NULL) {
            mBlackCloudFilterBtk_0xb8->setFrame((f32)(frame % mBlackCloudFilterBtk_0xb8->getTotalFrameCount()));
            mBlackCloudFilterBrk_0xbc->setFrame((f32)(field_0x1f3 * 100 / 255));
        }
        if (mMorningLightBtk_0xc4 != NULL) {
            s32 minX = 0x200;
            s32 alpha = 100 - gGameRoot2_8053AB10->field_0x2c->field_0xc * 100 / 255;
            for (s32 i = 0; i < 4; i++) {
                if (isPlayerActive_8023F264(i)) {
                    s32 x = (s32)getPlayerPos_8023E724(i)->x & 0x1ff;
                    if (minX > x) {
                        minX = x;
                    }
                }
            }
            if (field_0x1d8 > minX) {
                field_0x1d8 = field_0x1d8 - 4 >= minX ? field_0x1d8 - 4 : minX;
            }
            if (field_0x1d8 > 0) {
                s32 v = (256 - field_0x1d8) * 100 / 256;
                alpha = v >= alpha ? v : alpha;
            }
            mMorningLightBtk_0xc4->setFrame((f32)(frame % mMorningLightBtk_0xc4->getTotalFrameCount()));
            mMorningLightBrk_0xc8->setFrame((f32)alpha);
        }
        if (mNagarekumoBtk_0xd0 != NULL) {
            mNagarekumoBtk_0xd0->setFrame((f32)(frame % mNagarekumoBtk_0xd0->getTotalFrameCount()));
        }
        if (mDarkFogBtk_0xd8 != NULL) {
            mDarkFogBtk_0xd8->setFrame((f32)(frame % mDarkFogBtk_0xd8->getTotalFrameCount()));
            mDarkFogBrk_0xdc->setFrame((f32)(field_0x1f4 * 100 / 255));
        }
        if (mMoyaRedBtk_0xe4 != NULL) {
            mMoyaRedBtk_0xe4->setFrame((f32)(frame % mMoyaRedBtk_0xe4->getTotalFrameCount()));
            mMoyaRedBrk_0xe8->setFrame((f32)(field_0x1f5 * 100 / 255));
        }
        if (mYukaBtk_0xf0 != NULL) {
            mYukaBtk_0xf0->setFrame((f32)(frame % mYukaBtk_0xf0->getTotalFrameCount()));
        }
    }
}

void GameRootSub24_8016A3AC::fn_8016DCEC() {
    if (isGameModeSet() && field_0x1cf && gGameRoot_8053AAF8->field_0x4c != 0xB) {
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        fn_8016D398(camera->mViewMtx_0x4, field_0x1c0, false, !field_0x1d0, &field_0x140, &field_0x1b0,
                    &field_0x1b8, &field_0x1b4, mKumoBtks_0x54, mKumoBrks_0x6c, 6.0f / camera->mZoom_0x858);
    }
}

void GameRootSub24_8016A3AC::fn_8016DB60() {
    if (isGameModeSet()) {
        fn_8016D33C();
        fn_8016D2E0();
        fn_8016D284();
        fn_8016CB14();
        fn_8016C2FC();
        fn_8016B910();
        fn_8016B8AC();
        fn_8016B4D4();
        fn_8016B248();
        fn_8016B0B0();
        fn_8016ACCC();
        fn_8016AD28();
        fn_8016AC70();
        fn_8016AC14();
        fn_8016B054();
        fn_8016ABB8();
        fn_8016AB5C();
        fn_8016AAA4();
        fn_8016A9EC();
        fn_8016AB00();
        fn_8016AA48();
    }
}

// 100 % since the draw passes below are kept out of line (pragma region at fn_8016D33C).
void GameRootSub24_8016A3AC::fn_8016DA6C() {
    if (isGameModeSet()) {
        fn_8016ADE0();
        fn_8016AD84();
    }
}

// FIXME: 99.52 %, scheduling+regalloc; behaviour clean.
void fn_8016D398(Mtx view, s32 frame, bool arg2, bool arg3, cls_8016ED88* entries, s32* timer1,
                 s32* timer2, u8* flag, J3DAnmTextureSRTKey** btk, J3DAnmTevRegKey** brk, f32 scale) {
    JGeometry::TVec3f offset(-view[0][3], -view[1][3], 0.0f);
    JGeometry::TVec3f pos;

    if (arg3) {
        if (*timer1 > 0) {
            (*timer1)--;
        } else {
            *timer1 = 310.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1();
            for (s32 i = 0; i < 4; i++) {
                cls_8016FDE0* entry = &entries->mEntries[i];
                if (entry->field_0x14 == 0) {
                    entry->field_0x14 = 60;
                    entry->field_0x10 = 0x288;
                    entry->field_0x4.set(300.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() - 150.0f,
                                         48.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 96.0f, 0.0f);
                    JGeometry::TVec3f pos(0.0f, 0.0f, 0.0f);
                    entry->field_0x0 =
                        spawnEmitter_8013CC50(0, &pos, entry->field_0x10, 0, 0, NULL, NULL, true, 0.15f);
                    if (entry->field_0x0 != NULL) {
                        entry->field_0x0->setStatus(JPAEMIT_Immortal);
                    }
                    if (entry->field_0x18 != NULL) {
                        if (*flag != 0) {
                            if (arg2) {
                                entry->field_0x18->startSound(0x30078);
                            } else {
                                entry->field_0x18->startSound(0x30031);
                            }
                            *flag = 0;
                        } else {
                            *flag = 1;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (*timer2 > 0) {
        (*timer2)--;
    } else {
        *timer2 = 180.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1();
        for (s32 i = 0; i < 4; i++) {
            cls_8016FDE0* entry = &entries->mEntries[i];
            if (entry->field_0x14 == 0) {
                entry->field_0x14 = 60;
                entry->field_0x10 = 0x287;
                f32 y;
                f32 sign = 1.0f;
                y = 64.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() - 64.0f;
                if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() > 0.5f) {
                    sign = -1.0f;
                }
                entry->field_0x4.set(sign * (72.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 48.0f), y, 0.0f);
                JGeometry::TVec3f pos(0.0f, 0.0f, 0.0f);
                entry->field_0x0 = spawnEmitter_8013CC50(0, &pos, entry->field_0x10, 0, 0, NULL, NULL, true, 0.15f);
                if (entry->field_0x0 != NULL) {
                    entry->field_0x0->setStatus(JPAEMIT_Immortal);
                }
                break;
            }
        }
    }

    s32 maxA = 0;
    s32 maxB = 0;
    for (s32 i = 0; i < 4; i++) {
        cls_8016FDE0* entry = &entries->mEntries[i];
        if (entry->field_0x10 == 0x287) {
            maxB = maxB >= entry->field_0x14 ? maxB : entry->field_0x14;
        } else {
            maxA = maxA >= entry->field_0x14 ? maxA : entry->field_0x14;
        }
        if (entry->field_0x14 > 0) {
            entry->field_0x14--;
        }
        if (entry->field_0x0 != NULL) {
            if (entry->field_0x0->isEnableDeleteEmitter()) {
                JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
                manager->forceDeleteEmitter(entry->field_0x0);
                entry->field_0x0 = NULL;
            } else {
                entry->field_0x0->setGlobalScale(JGeometry::TVec3f(scale, scale, scale));
                pos.add(entry->field_0x4, offset);
                if (entry->field_0x10 == 0x287) {
                    if (!arg2) {
                        pos.y += -880.0f;
                        pos.z += -31.0f;
                    }
                    pos.z += -849.0f;
                } else {
                    if (!arg2) {
                        pos.y += -688.0f;
                        pos.z += -31.0f;
                    }
                    pos.z += -657.0f;
                }
                if (arg2) {
                    pos.x += 304.0f;
                    pos.y += 224.0f;
                }
                entry->field_0x0->setGlobalTranslation(pos);
            }
        }
    }
    if (maxB < maxA) {
        maxB = maxA;
    }

    for (s32 i = 0; i < 6; i++) {
        J3DAnmTextureSRTKey* srt = btk[i];
        J3DAnmTevRegKey* tev = brk[i];
        if (srt != NULL) {
            srt->setFrame(frame % srt->getTotalFrameCount());
        }
        if (tev != NULL) {
            s32 total = tev->getTotalFrameCount();
            tev->setFrame(((i < 4 ? maxA : maxB) * total) / 60);
        }
    }
}

// Not auto-inlined: fn_8016DB60 calls fn_8016D33C, fn_8016D2E0, fn_8016D284 and fn_8016B8AC out of
// line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
void GameRootSub24_8016A3AC::fn_8016D33C() {
    if (field_0x1c4) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mYouganModel_0x4, pos, -1, true, true, 0.5f, 10.24f);
    }
}

void GameRootSub24_8016A3AC::fn_8016D2E0() {
    if (field_0x1c5) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mDokuYouganModel_0xc, pos, -1, true, true, 0.5f, 10.24f);
    }
}

void GameRootSub24_8016A3AC::fn_8016D284() {
    if (field_0x1c6) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mYamiYouganModel_0x14, pos, -1, true, true, 0.5f, 10.24f);
    }
}

// FIXME: 81.17 %, missing/extra + reloc-name; behaviour clean.
void GameRootSub24_8016A3AC::fn_8016CB14() {
    if (field_0x1c7) {
        f32 width = (s32)JUTVideo::getManager()->getFbWidth();
        f32 height = (s32)JUTVideo::getManager()->getEfbHeight();
        f32 scrollB = 0.00024414063f * (f32)(field_0x1c0 & 0xFFF);
        f32 scrollC = 0.0009765625f * (f32)(field_0x1c0 & 0x3FF);
        JGeometry::TVec3f center;
        JGeometry::TVec3f half;
        pool_8053D938(center, half);
        JGeometry::TVec3f centerB(center);
        JGeometry::TVec3f centerC(center);
        JGeometry::TVec3f halfB(half);
        JGeometry::TVec3f halfC(half);
        centerB.scale(0.5f);
        centerB.x -= scrollB;
        centerB.y -= scrollB;
        halfB.scale(2.0f);
        centerC.mul(JGeometry::TVec3f(2.0f, 1.5f, 1.0f));
        centerC.x -= scrollC;
        halfC.mul(JGeometry::TVec3f(2.0f, 1.5f, 1.0f));
        center.mul(JGeometry::TVec3f(4.0f, 3.0f, 1.0f));
        half.mul(JGeometry::TVec3f(4.0f, 3.0f, 1.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        GXClearVtxDesc();
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);
        GXSetVtxDesc(GX_VA_TEX2, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
        GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
        GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
        GXSetTexCoordGen2(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX2, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
        GXSetNumTexGens(3);
        mKawazokoTexture_0xfc->load(GX_TEXMAP0);
        mKumoEnvTexture_0xf4->load(GX_TEXMAP1);
        mNoizeKawazokoTexture_0x100->load(GX_TEXMAP2);
        GXSetNumIndStages(2);
        GXSetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD2, GX_TEXMAP2);
        GXSetIndTexOrder(GX_INDTEXSTAGE1, GX_TEXCOORD2, GX_TEXMAP2);
        GXSetTevIndWarp(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_TRUE, GX_FALSE, GX_ITM_0);
        GXSetTevIndWarp(GX_TEVSTAGE1, GX_INDTEXSTAGE1, GX_TRUE, GX_FALSE, GX_ITM_0);
        f32 indMtx[2][3];
        indMtx[0][0] = 0.02f;
        indMtx[0][1] = 0.0f;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = 0.0f;
        indMtx[1][1] = 0.02f;
        indMtx[1][2] = 0.0f;
        GXSetIndTexMtx(GX_ITM_0, indMtx, 0);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_A0, GX_CC_TEXC, GX_CC_CPREV);
        GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
        GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
        GXSetNumTevStages(2);
        GXColor color1 = lbl_8053D910;
        u8 alpha = field_0x1ec * 120 / 47;
        color1.a = alpha > 120 ? 120 : alpha;
        GXSetTevColor(GX_TEVREG0, color1);
        GXColor color2 = lbl_8053D914;
        GXSetTevColor(GX_TEVREG1, color2);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXSetZCompLoc(GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXTexCoord2f32(centerB.x - halfB.x, centerB.y - halfB.y);
        GXTexCoord2f32(centerC.x - halfC.x, centerC.y - halfC.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXTexCoord2f32(centerB.x + halfB.x, centerB.y - halfB.y);
        GXTexCoord2f32(centerC.x + halfC.x, centerC.y - halfC.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXTexCoord2f32(centerB.x + halfB.x, centerB.y + halfB.y);
        GXTexCoord2f32(centerC.x + halfC.x, centerC.y + halfC.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXTexCoord2f32(centerB.x - halfB.x, centerB.y + halfB.y);
        GXTexCoord2f32(centerC.x - halfC.x, centerC.y + halfC.y);
        GXEnd();
    }
}

// FIXME: 69.69 %, scheduling+regalloc + reloc-name; behaviour clean.
void GameRootSub24_8016A3AC::fn_8016C2FC() {
    if (field_0x1c8 && mBgMoriTexture_0x104 != NULL) {
        f32 width = (s32)JUTVideo::getManager()->getFbWidth();
        f32 height = (s32)JUTVideo::getManager()->getEfbHeight();
        JGeometry::TVec3f center;
        JGeometry::TVec3f half;
        pool_8053D938(center, half);
        half.mul(JGeometry::TVec3f(2.0f, 1.5f, 2.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        mBgMoriTexture_0x104->load(GX_TEXMAP0);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();

        f32 scroll = 0.00024414063f * (f32)(field_0x1c0 & 0xFFF);
        pool_8053D938(center, half);
        center.x -= scroll;
        center.y -= scroll;
        half.mul(JGeometry::TVec3f(3.0f, 2.25f, 3.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        mKumoEnvTexture_0xf4->load(GX_TEXMAP0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
        GXSetTevColor(GX_TEVREG0, lbl_8053D918);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXSetZCompLoc(GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();

        f32 scrollX = 0.00024414063f * (f32)(field_0x1c0 & 0xFFF);
        f32 scrollY = 0.00024414063f * (f32)((field_0x1c0 + 1300) & 0xFFF);
        pool_8053D938(center, half);
        center.x -= scrollX;
        center.y -= scrollY;
        half.mul(JGeometry::TVec3f(2.5f, 1.875f, 2.5f));
        GXSetTevColor(GX_TEVREG0, lbl_8053D91C);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();

        scrollX = 0.00024414063f * (f32)((field_0x1c0 + 3100) & 0xFFF);
        scrollY = 0.00024414063f * (f32)((field_0x1c0 + 1300) & 0xFFF);
        pool_8053D938(center, half);
        center.x -= scrollX;
        center.y -= scrollY;
        half.mul(JGeometry::TVec3f(2.0f, 1.5f, 2.0f));
        GXSetTevColor(GX_TEVREG0, lbl_8053D920);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();
    }
}

// FIXME: 62.89 %, scheduling+regalloc + reloc-name; behaviour clean.
void GameRootSub24_8016A3AC::fn_8016B910() {
    if ((field_0x1c9 && mEnkeiTexture_0x108 != NULL) || (field_0x1ca && mEnkeiDarkTexture_0x10c != NULL)) {
        f32 width = (s32)JUTVideo::getManager()->getFbWidth();
        f32 height = (s32)JUTVideo::getManager()->getEfbHeight();
        JGeometry::TVec3f center;
        JGeometry::TVec3f half;
        pool_8053D938(center, half);
        center.mul(JGeometry::TVec3f(0.1f, 0.1f, 1.0f));
        center.add(JGeometry::TVec3f(0.2f, 0.2f, 0.0f));
        half.mul(JGeometry::TVec3f(0.7f, 0.525f, 1.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        if (field_0x1ca) {
            mEnkeiDarkTexture_0x10c->load(GX_TEXMAP0);
        } else {
            mEnkeiTexture_0x108->load(GX_TEXMAP0);
        }
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();

        f32 scroll = 0.00024414063f * (f32)(field_0x1c0 & 0xFFF);
        pool_8053D938(center, half);
        center.mul(JGeometry::TVec3f(0.1f, 0.1f, 1.0f));
        center.x -= scroll;
        center.y -= scroll;
        half.mul(JGeometry::TVec3f(3.0f, 2.25f, 3.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        mKumoEnvTexture_0xf4->load(GX_TEXMAP0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
        GXSetTevColor(GX_TEVREG0, lbl_8053D924);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXSetZCompLoc(GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();

        f32 scrollX = 0.00024414063f * (f32)(field_0x1c0 & 0xFFF);
        f32 scrollY = 0.00024414063f * (f32)((field_0x1c0 + 1300) & 0xFFF);
        pool_8053D938(center, half);
        center.mul(JGeometry::TVec3f(0.2f, 0.2f, 1.0f));
        center.x -= scrollX;
        center.y -= scrollY;
        half.mul(JGeometry::TVec3f(2.5f, 1.875f, 2.5f));
        if (field_0x1ca) {
            GXSetTevColor(GX_TEVREG0, lbl_8053D928);
        } else {
            GXSetTevColor(GX_TEVREG0, lbl_8053D92C);
        }
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();

        scrollX = 0.00024414063f * (f32)((field_0x1c0 + 3100) & 0xFFF);
        scrollY = 0.00024414063f * (f32)((field_0x1c0 + 1300) & 0xFFF);
        pool_8053D938(center, half);
        center.mul(JGeometry::TVec3f(0.3f, 0.3f, 1.0f));
        center.x -= scrollX;
        center.y -= scrollY;
        half.mul(JGeometry::TVec3f(2.0f, 1.5f, 2.0f));
        if (field_0x1ca) {
            GXSetTevColor(GX_TEVREG0, lbl_8053D930);
        } else {
            GXSetTevColor(GX_TEVREG0, lbl_8053D934);
        }
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();
    }
}

void GameRootSub24_8016A3AC::fn_8016B8AC() {
    if (field_0x1cb[0]) {
        JGeometry::TVec3f pos(256.0f, 256.0f + field_0x1e4, 0.0f);
        drawParallaxModel_8016A7DC(mMekakusiModel_0x34, pos, 1, true, false, 1.0f, 102.4f);
    }
}

// FIXME: 82.12 %, scheduling+regalloc + reloc-name; behaviour clean.
void GameRootSub24_8016A3AC::fn_8016B4D4() {
    if ((field_0x1cb[1] && mWallTexture_0x110 != NULL) || (field_0x1cb[2] && mWallKowareTexture_0x118 != NULL)) {
        f32 width = (s32)JUTVideo::getManager()->getFbWidth();
        f32 height = (s32)JUTVideo::getManager()->getEfbHeight();
        f32 scroll = 0.000244140625f * (f32)(field_0x1c0 & 0xFFF);
        JGeometry::TVec3f center1;
        JGeometry::TVec3f center2;
        JGeometry::TVec3f half1;
        JGeometry::TVec3f half2;
        pool_8053D938(center1, half1);
        center2.set(center1);
        half2.set(half1);
        center1.mul(JGeometry::TVec3f(1.0f, 3.0f, 1.0f));
        center1.x -= scroll;
        half1.mul(JGeometry::TVec3f(1.0f, 3.0f, 1.0f));
        center2.mul(JGeometry::TVec3f(1.0f, 0.75f, 1.0f));
        half2.mul(JGeometry::TVec3f(1.0f, 1.5f, 1.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        if (field_0x1cb[1]) {
            mSkyTexture_0x114->load(GX_TEXMAP0);
        } else {
            mSkyKowareTexture_0x11c->load(GX_TEXMAP0);
        }
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center1.x - half1.x, center1.y - half1.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center1.x + half1.x, center1.y - half1.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center1.x + half1.x, center1.y + half1.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center1.x - half1.x, center1.y + half1.y);
        GXEnd();
        if (field_0x1cb[1]) {
            mWallTexture_0x110->load(GX_TEXMAP0);
        } else {
            mWallKowareTexture_0x118->load(GX_TEXMAP0);
        }
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center2.x - half2.x, center2.y - half2.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center2.x + half2.x, center2.y - half2.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center2.x + half2.x, center2.y + half2.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center2.x - half2.x, center2.y + half2.y);
        GXEnd();
    }
}

#pragma auto_inline reset

// FIXME: 73.96 %, scheduling+regalloc + reloc-name; behaviour clean.
// scheduling + regalloc only (same instructions): the original
// interleaves the loads of getMapTexView differently (camera before the
// second map count) and stores the zero z components earlier.
// Draws mBgPiramidTexture_0x120 over the screen, its view shifted and halved in x and y.
void GameRootSub24_8016A3AC::fn_8016B248() {
    if (mBgPiramidTexture_0x120 != NULL && field_0x1cb[3]) {
        f32 width = (s32)JUTVideo::getManager()->getFbWidth();
        f32 height = (s32)JUTVideo::getManager()->getEfbHeight();
        JGeometry::TVec3f center;
        JGeometry::TVec3f half;
        pool_8053D938(center, half);
        center.x -= 1.5f;
        center.y -= 0.5f;
        center.mul(JGeometry::TVec3f(0.5f, 0.5f, 1.0f));
        center.x += 0.5f;
        center.y += 0.375f;
        half.mul(JGeometry::TVec3f(1.0f, 0.75f, 1.0f));
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        mBgPiramidTexture_0x120->load(GX_TEXMAP0);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();
    }
}

void GameRootSub24_8016A3AC::fn_8016B0B0() {
    if (field_0x1cf) {
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        f32 scale = 6.0f / camera->mZoom_0x858;
        for (s32 i = 0; i < 6; i++) {
            if (mKumoModels_0x3c[i] == NULL) {
                continue;
            }
            if (i == 3 && field_0x1d0 == 0) {
                continue;
            }
            if ((i == 1 || i == 2) && field_0x1d0 != 0) {
                continue;
            }
            Mtx mtx;
            f32 x = -camera->mViewMtx_0x4[0][3];
            f32 z = -545.0f - 96.0f * (f32)i;
            f32 y = 32.0f + (z - camera->mViewMtx_0x4[1][3]);
            mtx[0][0] = 1.0f;
            mtx[0][1] = 0.0f;
            mtx[0][2] = 0.0f;
            mtx[0][3] = x;
            mtx[1][0] = 0.0f;
            mtx[1][1] = 0.0f;
            mtx[1][2] = 1.0f;
            mtx[1][3] = y;
            mtx[2][0] = 0.0f;
            mtx[2][1] = 1.0f;
            mtx[2][2] = 0.0f;
            mtx[2][3] = z;
            mKumoModels_0x3c[i]->setBaseScale(scale, scale, scale);
            PSMTXCopy(mtx, mKumoModels_0x3c[i]->mPosMtx);
            mKumoModels_0x3c[i]->calc();
            mKumoModels_0x3c[i]->entry();
            mKumoModels_0x3c[i]->viewCalc();
        }
    }
}

// Not auto-inlined: fn_8016DB60 calls fn_8016B054 out of line in the original (docs/INLINING.md,
// pragma rule 1).
#pragma auto_inline off
void GameRootSub24_8016A3AC::fn_8016B054() {
    if (field_0x1d3) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mPoisonSwampModel_0xa8, pos, -1, true, true, 1.0f, 10.0f);
    }
}
#pragma auto_inline reset

// TEV register 0 = black with the alpha field_0x1ec. An inline of its own: the colour temporary is
// created at inlining (docs/REGALLOC.md, rule 4).
static inline void setTevAlpha(GameRootSub24_8016A3AC* self) {
    GXColor color = {0, 0, 0, self->field_0x1ec};
    GXSetTevColor(GX_TEVREG0, color);
}

void GameRootSub24_8016A3AC::fn_8016ADE0() {
    if (field_0x1ec) {
        f32 width = (s32)JUTVideo::getManager()->getFbWidth();
        f32 height = (s32)JUTVideo::getManager()->getEfbHeight();
        f32 scroll = 0.00048828125f * (f32)(field_0x1c0 & 0x7FF);
        JGeometry::TVec3f center;
        JGeometry::TVec3f half;
        pool_8053D938(center, half);
        center.x -= scroll;
        center.y -= scroll;
        gGameRoot2_8053AB10->fn_8013B460();
        gGameRoot2_8053AB10->fn_8013B1E8();
        mKumoEnvTexture_0xf4->load(GX_TEXMAP0);
        setTevAlpha(this);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXSetZCompLoc(GX_FALSE);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y - half.y);
        GXPosition3f32(width, 0.0f, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y - half.y);
        GXPosition3f32(width, height, 0.0f);
        GXTexCoord2f32(center.x + half.x, center.y + half.y);
        GXPosition3f32(0.0f, height, 0.0f);
        GXTexCoord2f32(center.x - half.x, center.y + half.y);
        GXEnd();
    }
}

// Not auto-inlined: fn_8016DA6C calls fn_8016AD84, and fn_8016DB60 calls fn_8016AD28 down to fn_8016A9EC,
// out of line in the original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
void GameRootSub24_8016A3AC::fn_8016AD84() {
    if (field_0x1ed) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mMoyaModel_0x1c, pos, 1, true, true, 1.5f, 10.24f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AD28() {
    if (field_0x1ef) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mDarkFilterModel_0x28, pos, 1, true, true, 1.0f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016ACCC() {
    if (field_0x1f0) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mForestFilterModel_0x84, pos, 1, true, true, 1.0f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AC70() {
    if (field_0x1f1) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mFogFilterModel_0x90, pos, 1, true, true, 1.5f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AC14() {
    if (field_0x1f2) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mCloudShadowFilterModel_0x9c, pos, 1, true, true, 1.0f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016ABB8() {
    if (field_0x1f3) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mBlackCloudFilterModel_0xb4, pos, 1, true, true, 1.0f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AB5C() {
    if (field_0x1d8 > 0) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mMorningLightModel_0xc0, pos, 1, true, true, 1.0f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AB00() {
    if (field_0x1f4) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mDarkFogModel_0xd4, pos, 1, true, true, 1.0f, 10.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AAA4() {
    if (field_0x1dc) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mNagarekumoModel_0xcc, pos, 1, true, true, 1.5f, 1.0f);
    }
}

void GameRootSub24_8016A3AC::fn_8016AA48() {
    if (field_0x1f5) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mMoyaRedModel_0xe0, pos, 1, true, true, 1.5f, 10.24f);
    }
}

void GameRootSub24_8016A3AC::fn_8016A9EC() {
    if (field_0x1dd) {
        JGeometry::TVec3f pos(256.0f, 256.0f, 0.0f);
        drawParallaxModel_8016A7DC(mYukaModel_0xec, pos, -1, true, true, 0.5f, 10.0f);
    }
}
#pragma auto_inline reset

void GameRootSub24_8016A3AC::drawParallaxModel_8016A7DC(J3DModel* model, const JGeometry::TVec3f& pos, s32 dir, bool followX, bool followY, f32 parallax, f32 scale) {
    if (dir == 1) {
        bool skip = gGameRoot_8053AAF8->field_0x4c == 11 ? true : gGameRoot_8053AAF8->field_0x88 == 2;
        if (skip) {
            return;
        }
    }
    Mtx mtx;
    JGeometry::TVec3f off;
    Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
    f32 z = 1.5f * (f32)(dir << 10);
    if (followX) {
        f32 c = -camera->mViewMtx_0x4[0][3];
        f32 d = c * parallax;
        f32 e = c - d;
        off.x = e + (f32)((s32)(d / 512.0f) << 9);
    } else {
        off.x = 0.0f;
    }
    if (followY) {
        f32 c = -camera->mViewMtx_0x4[1][3];
        f32 d = c * parallax;
        f32 e = c - d;
        off.y = e + (f32)((s32)(d / 512.0f) << 9);
    } else {
        off.y = 0.0f;
    }
    off.y += z;
    off.z = z;
    off += pos;
    mtx[0][0] = 1.0f;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[0][3] = off.x;
    mtx[1][0] = 0.0f;
    mtx[1][1] = 0.0f;
    mtx[1][2] = 1.0f;
    mtx[1][3] = off.y;
    mtx[2][0] = 0.0f;
    mtx[2][1] = 1.0f;
    mtx[2][2] = 0.0f;
    mtx[2][3] = off.z;
    model->setBaseScale(scale, 1.0f, scale);
    PSMTXCopy(mtx, model->mPosMtx);
    model->calc();
    model->entry();
    model->viewCalc();
}

// The visible part of the map in texture coordinates (centre and half size;
// a map cell is 8 units). Dead-stripped: fn_8016ADE0, fn_8016B248, fn_8016B4D4,
// fn_8016B910, fn_8016C2FC and fn_8016CB14 inline it. Its own copy created the
// unit's first pool literals (0.125, 1, 0.5, 0, then the int-to-float double),
// which come before drawParallaxModel_8016A7DC's 1.5 in the original .sdata2.
// R2 pool helper (inlined at 12 call sites); was getMapTexView.
void pool_8053D938(JGeometry::TVec3f& center, JGeometry::TVec3f& half) {
    Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
    TileMap_804AA858* map = gGameRoot2_8053AB10->field_0x20;
    f32 sx = 0.125f / (f32)map->mHeightGridCellsX_0x2380;
    f32 sy = 0.125f / (f32)map->mHeightGridCellsY_0x2384;
    f32 inv = 1.0f / camera->mZoom_0x858;
    JUTVideo* video = JUTVideo::getManager();
    half.x = inv * (0.5f * (sx * (s32)video->getFbWidth()));
    half.y = inv * (0.5f * (sy * (s32)video->getEfbHeight()));
    half.z = 0.0f;
    center.x = -camera->mViewMtx_0x4[0][3] * sx;
    center.y = -camera->mViewMtx_0x4[1][3] * sy;
    center.z = 0.0f;
}

// FIXME: 99.79 %, regalloc-only; behaviour clean.
// regalloc only: the original keeps heap in r29 and btk1Out in r30
// (ours swaps them).
void loadModelWithAnims_8016A3BC(JKRArchive* archive, const char* bmd, const char* btk1, const char* btk2,
                 const char* brk1, const char* brk2, u32 flags, JKRHeap* heap, J3DModel** model,
                 J3DAnmTextureSRTKey** btk1Out, J3DAnmTextureSRTKey** btk2Out,
                 J3DAnmTevRegKey** brk1Out, J3DAnmTevRegKey** brk2Out) {
    void* res = NULL;
    if (archive != NULL) {
        res = archive->getResource('J3D ', bmd);
    }
    if (res != NULL) {
        J3DModelData* data = J3DModelLoaderDataBase::load(res, flags);
        if (btk1 != NULL) {
            *btk1Out = (J3DAnmTextureSRTKey*)J3DAnmLoaderDataBase::load(archive->getResource('J3D ', btk1), J3DLOADER_UNK_FLAG0);
            (*btk1Out)->searchUpdateMaterialID(data);
        } else if (btk1Out != NULL) {
            *btk1Out = NULL;
        }
        if (btk2 != NULL) {
            *btk2Out = (J3DAnmTextureSRTKey*)J3DAnmLoaderDataBase::load(archive->getResource('J3D ', btk2), J3DLOADER_UNK_FLAG0);
            (*btk2Out)->searchUpdateMaterialID(data);
        } else if (btk2Out != NULL) {
            *btk2Out = NULL;
        }
        if (brk1 != NULL) {
            *brk1Out = (J3DAnmTevRegKey*)J3DAnmLoaderDataBase::load(archive->getResource('J3D ', brk1), J3DLOADER_UNK_FLAG0);
            (*brk1Out)->searchUpdateMaterialID(data);
        } else if (brk1Out != NULL) {
            *brk1Out = NULL;
        }
        if (brk2 != NULL) {
            *brk2Out = (J3DAnmTevRegKey*)J3DAnmLoaderDataBase::load(archive->getResource('J3D ', brk2), J3DLOADER_UNK_FLAG0);
            (*brk2Out)->searchUpdateMaterialID(data);
        } else if (brk2Out != NULL) {
            *brk2Out = NULL;
        }
        for (u16 i = 0; i < data->getMaterialNum(); i++) {
            J3DMaterialAnm* anm = new (heap, 0) J3DMaterialAnm();
            data->getMaterialNodePointer(i)->change();
            data->getMaterialNodePointer(i)->setMaterialAnm(anm);
        }
        if (btk1 != NULL) {
            j3dSys.ErrorReport(data->getMaterialTable().entryTexMtxAnimator(*btk1Out));
        }
        if (btk2 != NULL) {
            j3dSys.ErrorReport(data->getMaterialTable().entryTexMtxAnimator(*btk2Out));
        }
        if (brk1 != NULL) {
            j3dSys.ErrorReport(data->getMaterialTable().entryTevRegAnimator(*brk1Out));
        }
        if (brk2 != NULL) {
            j3dSys.ErrorReport(data->getMaterialTable().entryTevRegAnimator(*brk2Out));
        }
        *model = new (heap, 0) J3DModel(data, 0, 1);
    } else {
        if (btk1Out != NULL) {
            *btk1Out = NULL;
        }
        if (btk2Out != NULL) {
            *btk2Out = NULL;
        }
        if (brk1Out != NULL) {
            *brk1Out = NULL;
        }
        if (brk2Out != NULL) {
            *brk2Out = NULL;
        }
        *model = NULL;
    }
}

// Returns gGameRoot_8053AAF8->field_0x4->field_0x24 (the object act_environment_control_2
// writes flags into).
GameRootSub24_8016A3AC* fn_8016A3AC() {
    return gGameRoot_8053AAF8->field_0x4->field_0x24;
}

void GameRootSub24_8016A3AC::fn_8016A330() {
    field_0x1ec = 0;
    field_0x1ed = 0;
    field_0x1ee = 0;
    field_0x1ef = 0;
    field_0x1f0 = 0;
    field_0x1f1 = 0;
    field_0x1f2 = 0;
    field_0x1f3 = 0;
    field_0x1f4 = 0;
    field_0x1f5 = 0;
    field_0x1c4 = 0;
    field_0x1c5 = 0;
    field_0x1c6 = 0;
    field_0x1c7 = 0;
    field_0x1c8 = 0;
    field_0x1c9 = 0;
    field_0x1ca = 0;
    field_0x1cb[0] = 0;
    field_0x1cb[1] = 0;
    field_0x1cb[2] = 0;
    field_0x1cb[3] = 0;
    field_0x1cf = 0;
    field_0x1d1 = 0;
    field_0x1d2 = 0;
    field_0x1d3 = 0;
    field_0x1d8 = -1;
    field_0x1dc = 0;
    field_0x1dd = 0;
}

void GameRootSub24_8016A3AC::fn_8016A2B4() {
    if (field_0x124 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x124);
    }
    if (field_0x128 != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x128);
    }
    if (field_0x12c != NULL) {
        JPAEmitterManager* manager = getEffectSystem_8013C824()->mEmitterManager;
        manager->forceDeleteEmitter(field_0x12c);
    }
}
