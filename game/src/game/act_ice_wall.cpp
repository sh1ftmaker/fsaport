#include "game/GameAudio.h"
#include "game/act/act_ice_wall.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/EmitterDelete.h"
#include "game/EnemyBase.h"
#include "game/GameGlobals.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/SpriteParams.h"
#include "JSystem/JKernel/JKRArchive.h"

// One animation: (cel, duration) pairs, ended by a cel of -1.
static const s16 lbl_8053C8B0[] = {0, 2, -1, -1};
static const s16* lbl_80539F28[] = {lbl_8053C8B0, NULL};

// Actor spawned when the wall melts, indexed by field_0x23c: the actor id (spawnActor_801F9484's first
// argument) and, in lbl_80464C0C, its spawn argument.
static const u32 lbl_80464BE8[] = {
    'NULL', 'WIZR', 'RUPY', 'SLR1', 'FAGN', 'FARY', 'KEY0', 'GOLD', 'HART'
};
static const s32 lbl_80464C0C[] = {
    0, 5, 0x00260004, 2, 0, 0, 4, 0x00030000, 0
};

IceWall_8049DE18::IceWall_8049DE18()
    : field_0x230(0), field_0x234(0), field_0x238(0), field_0x23c(0), field_0x240(1.0f), mSprite(),
      field_0x27c(NULL) {}

IceWall_8049DE18::~IceWall_8049DE18() {
    releaseEmitter_801EE434(field_0x27c);
    field_0x27c = NULL;
}

void IceWall_8049DE18::init() {
    ActorBase_804A5770::init();
    field_0x230 = mSpawnParam_0x90 & 0xF;
    field_0x238 = mSpawnParam_0x90 >> 4 & 0xF;
    field_0x23c = mSpawnParam_0x90 >> 8 & 0xFF;
    switch (field_0x230) {
    case 1:
        field_0x240 = 0.5f;
        field_0x54.setAll(field_0x240);
        mBox.i.x = -8.0f;
        mBox.i.y = -8.0f;
        mBox.f.x = 8.0f;
        mBox.f.y = 8.0f;
        mMapBox_0x80.i.x = -4.0f;
        mMapBox_0x80.i.y = -4.0f;
        mMapBox_0x80.f.x = 4.0f;
        mMapBox_0x80.f.y = 4.0f;
        break;
    // Case 0 is written out with the default: the original's dispatch sends
    // both sides of the test against 1 to this block.
    case 0:
    default:
        mBox.i.x = -16.0f;
        mBox.i.y = -16.0f;
        mBox.f.x = 16.0f;
        mBox.f.y = 16.0f;
        mMapBox_0x80.i.x = -8.0f;
        mMapBox_0x80.i.y = -8.0f;
        mMapBox_0x80.f.x = 8.0f;
        mMapBox_0x80.f.y = 8.0f;
        break;
    }
    mBox2.i.x = mBox.i.x;
    mBox2.i.y = mBox.i.y;
    mBox2.f.x = mBox.f.x;
    mBox2.f.y = mBox.f.y;
    field_0xb8 = 1;
    field_0xb0 = 0x06040304;
    mParamAtcn_0x118 = 0;
    field_0xbc = 3;
    mSprite.setKeyTables_801D2608(lbl_80539F28, 1);
    mSprite.startAnim_801D22DC(0, 2, false);
    mSprite.setFrame_801D1F14(0.0f);
}

// FIXME: 99.90 %, regalloc-only; behaviour clean.
// FIXME: regalloc-only: saved registers r29/r30 are swapped in cases 0 and 1.
void IceWall_8049DE18::update() {
    ActorBase_804A5770::update();
    JGeometry::TBox2f box;
    JGeometry::TVec2f pos;

    box.set(mBox);
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;

    switch (field_0x234) {
    case 0: {
        fn_80239914(&box, mLayer_0x4, mSerial);
        s32 player = findOverlappingPlayer_801F285C(NULL, NULL, true, -1);
        bool hit; // the original tests the result of an inlined bool helper (li 1 / li 0, clrlwi.)
        if (player >= 0) {
            startSound_801F0E34(0x400D1, 0);
            // fn_802379E4 reads only x / y; the original stores just those two (as act_snow_wall).
            pos.set(mPos.x, mPos.y);
            fn_802379E4(player, (const JGeometry::TVec3f*)&pos);
            hit = true;
        } else {
            hit = false;
        }
        if (hit) {
            break;
        }

        if (hitByWeapon()) {
            break;
        }

        if (hitByFire()) {
            startMelting();
        } else {
            fn_8020D58C()->addBox(mSerial);
        }
        break;
    }
    case 1:
        if (moveToward_801CD950(&field_0x240, 0.1f, -0.02f) == 0) {
            if (field_0x27c != NULL) {
                field_0x27c->becomeInvalidEmitter();
            }
            switch (field_0x238) {
            case 0:
                break;
            case 1:
                playSe_801F0D20(3);
                break;
            case 2:
                playSe_801F0D20(0x42);
                break;
            case 3:
                playSe_801F0D20(6);
                break;
            }
            if (field_0x23c != 0) {
                s32 arg = lbl_80464C0C[field_0x23c];
                u32 actorId = lbl_80464BE8[field_0x23c];
                getActorManager_801F856C()->spawnActor_801F9484(actorId, mLayer_0x4, &mPos, arg, -1, -1);
            }
            markDead_801F3668();
            field_0x234 = 2;
        }
        break;
    case 2:
        break;
    }
    field_0x54.z = field_0x54.y = field_0x54.x = field_0x240;
    mSprite.update_801D1C18();
}

static inline void setScaleMtx(Mtx mtx, f32 x, f32 y, f32 z) {
    mtx[0][0] = x;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = y;
    mtx[1][2] = 0.0f;
    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = z;
}

void IceWall_8049DE18::draw() {
    ActorBase_804A5770::draw();
    SpriteParams_801F5AE0 params1;
    SpriteParams_801F5AE0 params2;
    Mtx mtx;
    if (!field_0x54.isZero()) {
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;
        setScaleMtx(mtx, field_0x54.x, field_0x54.y, field_0x54.z);
        params1.mMtx = mtx;
    }

    if ((field_0x230 == 0 && field_0x54.x >= 0.8f) || (field_0x230 == 1 && field_0x54.x >= 0.4f)) {
        const JGeometry::TVec3f* pPos = &mPos;
        JGeometry::TVec3f pos = *pPos;
        switch (field_0x23c) {
        case 1:
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pPos, res, 0xE9, NULL, 1.0f, 1.0f);
            }
            break;
        case 2:
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pPos, res, 0x11, NULL, 1.0f, 1.0f);
            }
            break;
        case 3:
            params2.field_0x0[2] = 4;
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pPos, res, 0x200, &params2, 1.0f, 1.0f);
            }
            break;
        case 4:
            pos.y += 5.0f;
            params2.field_0x1d = -1;
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, 0x14C, &params2, 1.0f, 1.0f);
            }
            break;
        case 5:
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, 0x5E, &params2, 1.0f, 1.0f);
            }
            break;
        case 6:
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, 0x31, &params2, 1.0f, 1.0f);
            }
            break;
        case 8:
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, 0x2C, &params2, 1.0f, 1.0f);
            }
            break;
        }
    }

    drawSpriteScaled_801F02BC("enemy_both2_sob.bin", mSprite.mCel + 0x1CA, &params1, 7, 0, false, 1.0f, 1.0f);
    if (field_0x230 == 0) {
        params1.field_0x1c = 1;
    }
    drawSpriteScaled_801F02BC("enemy_both2_sob.bin", mSprite.mCel + 0x1CB, &params1, 7, 0, false, 1.0f, 1.0f);
}

// The three functions below are plain functions that update inlines (state 0) and whose own copies
// the linker dropped: the original's .sdata2 has 0.0f, 1.0f and 0.5f ahead of draw's 32.0f, so the
// code that creates them was generated before draw (tools/pool.py). The pool proves the place and
// the order (the 0.0f of the two tests before startMelting's 1.0f and 0.5f); the boundaries are
// the bool results update tests (li 1 / li 0, clrlwi.) and inlining (the whole switch as one
// function is not inlined).

// The wall starts to melt: the smoke emitters (on layer 0) and the sound.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 15-line definition): split out only where the .sdata2 order needs it.
void IceWall_8049DE18::startMelting() {
    deleteEmitter(&mEmitter100);
    deleteEmitter(&mEmitter104);
    s32 layer = mLayer_0x4;
    if (layer == 0) {
        f32 scale = 1.0f;
        if (field_0x230 == 1) {
            scale = 0.5f;
        }
        field_0x27c = spawnEmitter_8013CC50(layer, &mPos, 0x28D, 1, 0, NULL, NULL, true, scale);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x28E, 1, 0, NULL, NULL, true, scale);
    }
    startSound_801F0E34(0x30061, 0);
    field_0x234 = 1;
}

// Whether the player probe fn_8023AB94 hits the wall's box or a 'FIRE' actor touches the wall.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 14-line definition): split out only where the .sdata2 order needs it.
bool IceWall_8049DE18::hitByFire() {
    JGeometry::TBox2f box;
    JGeometry::TVec2f hitPos;
    box.set(mBox);
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    hitPos.set(0.0f, 0.0f);
    if (fn_8023AB94(&box, mLayer_0x4, &hitPos) >= 0 || findOverlappingActor_801F2718('FIRE') >= 0) {
        return true;
    }
    return false;
}

// Whether one of the four player probes hits the wall's box (at its height on the screen); the
// arrow, bomb or 'PCDM' actor that hit is told so (fn_801CC820 / fn_801CC538 / fn_801CC640).
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 30-line definition): split out only where the .sdata2 order needs it.
bool IceWall_8049DE18::hitByWeapon() {
    JGeometry::TBox2f box;
    JGeometry::TVec3f stack40;
    JGeometry::TVec2f hitPos;
    hitPos.set(0.0f, 0.0f);
    JGeometry::TVec2f screenPos(mPos.x, mPos.y - mPos.z);
    box.set(mBox2);
    box.addPos(screenPos);

    s32 hitCode = fn_8023AA18(&box, mLayer_0x4, &hitPos);
    if (hitCode >= 0) {
        return true;
    }
    hitCode = fn_8023AE34(&box, mLayer_0x4, &hitPos);
    if (hitCode >= 0) {
        fn_801CC820(hitCode, NULL);
        return true;
    }
    hitCode = fn_8023AC74(&box, mLayer_0x4, &hitPos);
    if (hitCode >= 0) {
        fn_801CC538(hitCode, &hitPos, &stack40, NULL);
        return true;
    }
    hitCode = fn_8023AD54(&box, mLayer_0x4, &hitPos);
    if (hitCode >= 0) {
        fn_801CC640(hitCode, NULL, NULL);
        return true;
    }
    return false;
}
