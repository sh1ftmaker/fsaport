#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/Effect.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Player.h"
#include "game/cls_804AE9B0.h"
#include "game/EnemyBase.h"
#include "game/act/act_warp_cannon.h"

// The animation: (cel, duration) pairs, ended by a cel of -1.
static const s16 lbl_80465120[] = {0, 3, 1, 2, 2, 2, 3, 3, 4, 3, 5, 3, -1, -1};
static const s16* lbl_80539F48[] = {lbl_80465120, NULL};

WarpCannon_8049EAE0::WarpCannon_8049EAE0() {
    field_0x268 = 0;
    field_0x26c = -1;
    field_0x270 = 0;
    field_0x274 = 0;
    field_0x278 = 0;
    field_0x27c = 0.0f;
    field_0x280 = 0.0f;
    field_0x284 = 0.0f;
    field_0x288 = 0.0f;
    field_0x28c = 0.0f;
    field_0x290.set(0.0f, 0.0f, 0.0f);
}

WarpCannon_8049EAE0::~WarpCannon_8049EAE0() {}

// FIXME: 99.83 %, reloc-name; behaviour clean.
void WarpCannon_8049EAE0::init() {
    field_0x274 = mSpawnParam_0x90 >> 8 & 0xFF;
    field_0x270 = mSpawnParam_0x90 >> 16 & 0xFF;
    mBox.set(-16.0f, -16.0f, 16.0f, -12.0f);
    mBox2.set(mBox);
    mMapBox_0x80.set(0.0f, 0.0f, 0.0f, 0.0f);
    mLife = 0;
    field_0xb0 = 0x50303;
    mAnim.setKeyTables_801D2608(lbl_80539F48, 1);
    mAnim.startAnim_801D22DC(0, 2, false);
    mAnim.setFrame_801D1F14(0.0f);
    field_0x284 = mPos.x;
    field_0x288 = mPos.y;
    field_0x28c = mPos.z;
}

// FIXME(shotgun): 99.76 %, operands + reloc-name; behaviour drift.
// a few instructions (state 0 probe loop, shared statics @NNN)
void WarpCannon_8049EAE0::update() {
    switch (field_0x268) {
    case 0: {
        s32 p = findPlayer();
        if (p >= 0) {
            field_0x26c = p;
            setState_80130FAC(1);
        }
        if (allPlayersIn()) {
            setState_80130FAC(2);
        }
        break;
    }
    case 1: {
        fn_802308E0(field_0x26c);
        if (recoil()) {
            field_0x26c = -1;
            setState_80130FAC(0);
        }
        break;
    }
    case 2: {
        movePlayers();
        if (fireRecoil()) {
            setState_80130FAC(3);
        }
        break;
    }
    case 3:
        field_0x278++;
        if (field_0x278 >= 0x3C) {
            setState_80130FAC(4);
        }
        break;
    case 4:
        pool_8053CCD0();
        if (getPlayerPos_8023E724(0)->z >= 90.0f) {
            setState_80130FAC(6);
        }
        break;
    case 5:
        pool_8053CCD0();
        if (fn_80119DD0()->field_0x14 >= 250.0f) {
            setState_80130FAC(6);
        }
        break;
    case 6:
        setState_80130FAC(7);
        break;
    case 7: {
        if (landPlayers()) {
            setState_80130FAC(8);
        }
        break;
    }
    case 8:
        break;
    }
    mAnim.update_801D1C18();
}

void WarpCannon_8049EAE0::draw() {
    SpriteParams_801F5AE0 params;
    params.field_0x1d = 1;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    mtx[0][0] = 2.0f;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = 2.0f;
    mtx[1][2] = 0.0f;
    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = 2.0f;
    params.mMtx = mtx;
    drawSpriteScaled_801F02BC("enemy_both2_sob.bin", mAnim.mCel + 0x14C, &params, 0x407, 1, false,
                1.0f, 1.0f);
}

// FIXME: 99.48 %, regalloc-only + reloc-name; behaviour drift.
// FIXME: regalloc-only + reloc-name; the switch's jump table is @NNN where the original names jumptable_8049EB34.
void WarpCannon_8049EAE0::setState_80130FAC(s32 state) {
    field_0x268 = state;
    switch (field_0x268) {
    case 1:
        fn_80230FD0(field_0x26c, 0x20035);
        break;
    case 2:
        field_0x27c = 13.0f;
        fn_80230FD0(0, 0x20036);
        gGameRoot_8053AAF8->startRumble_80134DE4(1, 0);
        lbl_8053AD70->fn_8037AE00();
        break;
    case 3: {
        field_0x290 = *getPlayerLastTvPos_8023E32C(0);
        resetIfAllPlayersReady();
        for (s32 i = 0; i < 4; i++) {
            fn_80230498(i, &field_0x290, 0xB0, 0, -0x80, -0x80, true);
        }
        break;
    }
    case 4:
        for (s32 i = 0; i < 4; i++) {
            s32 id = fn_8023CBC0(i);
            fn_80230498(i, getPlayerPos_8023E724(i), id, 0xFF, -0x80, -0x80, true);
        }
        for (s32 i = 0; i < 4; i++) {
            fn_80230FD0(i, 0x10003);
        }
        field_0x27c = 4.0f;
        gGameRoot2_8053AB10->field_0x20->startHeightGridEffect_802F601C(&field_0x290, 1, NULL, 1.0f, -2.0f);
        spawnEmitter_8013CC50(0, &field_0x290, 0x17D, 1, 0, NULL, NULL, true, 1.0f);
        break;
    case 6:
        fn_80230534(false);
        getGameView_802D6A9C()->fn_802D9FF4(field_0x270, field_0x274);
        gGameRoot_8053AAF8->fn_80136798(0xD, 0);
        lbl_8053AD70->fn_8037AE30();
        break;
    case 7: {
        fn_8023077C();
        field_0x278 = 0;
        liftPlayers();
        field_0x27c = 0.0f;
        break;
    }
    case 8:
        fn_80230534(false);
        mAlive = false;
        break;
    }
}

// From here down: plain functions that update and setState_80130FAC inline and whose own copies the
// linker dropped. The original's .sdata2 has their literals ahead of setState_80130FAC's, in the order
// of these functions from the bottom of the file up, so they were generated before it
// (tools/pool.py). One more is still missing: the original creates a 16.0f after flyPlayers'
// literals and before setState_80130FAC's 13.0f, which leaves init's literals one slot off.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 15-line definition): split out only where the .sdata2 order needs it.
s32 WarpCannon_8049EAE0::findPlayer() {
    JGeometry::TBox2f probe = mBox;
    f32 dy = mPos.y - mPos.z;
    probe.i.x += mPos.x;
    probe.i.y += dy;
    probe.f.x += mPos.x;
    probe.f.y += dy;
    for (s32 p = 0; p < 4; p++) {
        if ((gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << p)) && fn_8023B0F4(p, &probe, mLayer_0x4, false, 1000.0f) &&
            !fn_80230188(p) && field_0x26c < 0) {
            return p;
        }
    }
    return -1;
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 8-line definition): split out only where the .sdata2 order needs it.
bool WarpCannon_8049EAE0::allPlayersIn() {
    for (s32 p = 0; p < 4; p++) {
        if (!fn_80230188(p)) {
            return false;
        }
    }
    return true;
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 8-line definition): split out only where the .sdata2 order needs it.
void WarpCannon_8049EAE0::movePlayers() {
    moveToward_801CD950(&field_0x27c, -10.0f, -0.2f);
    for (s32 p = 0; p < 4; p++) {
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(p);
        pos.y -= field_0x27c;
        fn_80230498(p, &pos, 0xB0, 0xFF, -0x80, -0x80, true);
    }
}

// States 4 and 5: the players fly off, accelerating towards y = 1000.
// R2 pool helper (inlined at 2 call sites); was flyPlayers.
void WarpCannon_8049EAE0::pool_8053CCD0() {
    moveToward_801CD950(&field_0x27c, -10.0f, -0.002f);
    for (s32 p = 0; p < 4; p++) {
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(p);
        moveToward_801CD950(&pos.y, 1000.0f, field_0x27c);
        fn_80230498(p, &pos, 0xB0, 0xFF, -0x80, -0x80, true);
    }
}

// Entering state 3: reset field_0x278 if all players pass fn_80230360.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 8-line definition): split out only where the .sdata2 order needs it.
void WarpCannon_8049EAE0::resetIfAllPlayersReady() {
    for (s32 i = 0; i < 4; i++) {
        if (!fn_80230360(i)) {
            return;
        }
    }
    field_0x278 = 0;
}

// Entering state 7: the players at z = 90.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 7-line definition): split out only where the .sdata2 order needs it.
void WarpCannon_8049EAE0::liftPlayers() {
    for (s32 i = 0; i < 4; i++) {
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(i);
        pos.z = 90.0f;
        fn_80230498(i, &pos, 0xB0, 0xFF, -0x80, -0x80, true);
    }
}

// State 7: the players come down towards y = 0. True once one has arrived.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 12-line definition): split out only where the .sdata2 order needs it.
bool WarpCannon_8049EAE0::landPlayers() {
    bool all = false;
    moveToward_801CD950(&field_0x27c, -10.0f, -0.2f);
    for (s32 p = 0; p < 4; p++) {
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(p);
        if (moveToward_801CD950(&pos.y, 0.0f, field_0x27c) == 0) {
            all = true;
        }
        fn_80230498(p, &pos, 0xB0, 0xFF, -0x80, -0x80, true);
    }
    return all;
}

// State 1: the cannon's kick when a player gets in (field_0x280 runs 0 to 1). True at the end.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 13-line definition): split out only where the .sdata2 order needs it.
bool WarpCannon_8049EAE0::recoil() {
    field_0x280 += 0.05f;
    JGeometry::TVec3f step(0.0f, 2.0f * (0.5f - field_0x280), 0.0f);
    mPos.add(step);
    if (mPos.y < field_0x288) {
        mPos.y = field_0x288;
    }
    if (field_0x280 >= 1.0f) {
        field_0x280 = 1.0f;
        return true;
    }
    return false;
}

// State 2: the stronger kick of the shot, on its own static phase, held for 30 frames. True at
// the end.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 26-line definition): split out only where the .sdata2 order needs it.
bool WarpCannon_8049EAE0::fireRecoil() {
    static s32 counter = 0;
    static f32 phase = 0.0f;
    phase += 0.1f;
    if (phase >= 1.0f) {
        phase = 1.0f;
    }
    counter++;
    f32 t;
    if (phase <= 0.2f) {
        t = 12.0f * (0.5f - phase);
    } else {
        t = 4.0f * (0.2f - phase);
    }
    JGeometry::TVec3f step(0.0f, t, 0.0f);
    mPos.add(step);
    if (mPos.y < field_0x288) {
        mPos.y = field_0x288;
    }
    if (phase >= 1.0f && counter >= 30) {
        counter = 0;
        phase = 0.0f;
        return true;
    }
    return false;
}
