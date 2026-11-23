#include "game/GameAudio.h"
#include "game/EmitterDelete.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Player.h"
#include "game/act/tu_801F9484.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/EnemyBase.h"
#include "game/Camera.h"
#include "game/PlayerBit.h"
#include "game/GameState.h"

// lbl_8046C658: the x shake offsets of four steps per shake kind (s32, 16 bytes per kind).
const s32 lbl_8046C658[7][4] = {
    {-24, 40, -24, 40}, {0, 64, 0, 64}, {0, -48, 48, 0}, {0, 32, 64, 96},
    {0, 0, 0, 0}, {0, 48, 112, 160}, {80, -80, 80, -80},
};
// lbl_8046C6C8: the y shake offsets, then the camera focus offset of each kind, the drop offsets of
// state 1, the step timers of state 4 and the effect ids of the show of states 3 and 7.
struct GoalTables_8046C6C8 {
    s32 y[7][4];
    s32 focus[7][2];
    f32 dropX[6];
    f32 dropY[6];
    s32 timer[15];
    u16 showId[4][3];
    u16 endId[4][4];
};
const GoalTables_8046C6C8 lbl_8046C6C8 = {
    {{-24, -24, 40, 40}, {0, 0, 64, 64}, {0, 48, 48, 96}, {0, 0, 0, 0},
     {0, 32, 64, 96}, {0, 48, 48, 0}, {-32, -32, 80, -80}},
    {{8, 0}, {32, 32}, {0, 48}, {48, 0}, {0, 48}, {72, 0}, {0, 0}},
    {0.0f, 32.0f, 0.0f, 48.0f, 0.0f, 72.0f},
    {0.0f, 32.0f, 48.0f, 0.0f, 48.0f, 8.0f},
    {30, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 1, 1},
    {{0x209, 0x20A, 0x20B}, {0x20F, 0x210, 0x211}, {0x206, 0x207, 0x208}, {0x20C, 0x20D, 0x20E}},
    {{0x216, 0x217, 0x218, 0x219}, {0x21E, 0x21F, 0x220, 0x221}, {0x212, 0x213, 0x214, 0x215},
     {0x21A, 0x21B, 0x21C, 0x21D}},
};

static inline void invalidateEmitter(JPABaseEmitter*& emitter) {
    if (emitter != NULL) {
        emitter->becomeInvalidEmitter();
        emitter = NULL;
    }
}

// Ends the player-gathering state: the mourning effects at the four shake offsets, and the
// bombs and mirrors in the world are switched off.
static inline void beginShow(Goal_804A7EB0* self, s32 d0, s32 d1, s32 d2) {
    JGeometry::TVec3f pos;
    self->field_0x248 = 0;
    self->field_0x244 = 400;
    self->field_0x230 = 4;
    lbl_8053AED0->field_0xdc |= 0x10;
    getGameView_802D6A9C()->fn_802D7970(30);
    gAudioMgr_8053AF80->startBgm_80458FF0(0x1000001, true, 0);
    gGameRoot_8053AAF8->fn_801365C8(0);
    pos.z = self->mPos.z;
    for (s32 i = 0; i < 4; i++) {
        invalidateEmitter(self->mEmitters[0][i]);
        invalidateEmitter(self->mEmitters[1][i]);
        pos.x = self->mPos.x + (f32)lbl_8046C658[self->field_0x234][i];
        pos.y = self->mPos.y + (f32)lbl_8046C6C8.y[self->field_0x234][i];
        self->mEmitters[2][i] = spawnEmitter_8013CC50(self->mLayer_0x4, &pos, lbl_8046C6C8.showId[self->field_0x254[i]][0], 1, d0, NULL, NULL, true, 1.0f);
        self->mEmitters[3][i] = spawnEmitter_8013CC50(self->mLayer_0x4, &pos, lbl_8046C6C8.showId[self->field_0x254[i]][1], 1, d1, NULL, NULL, true, 1.0f);
        self->mEmitters[4][i] = spawnEmitter_8013CC50(self->mLayer_0x4, &pos, lbl_8046C6C8.showId[self->field_0x254[i]][2], 1, d2, NULL, NULL, true, 1.0f);
        fn_8023EE28(i);
    }
    s32 end = getActorManager_801F856C()->getActorEnd();
    for (s32 i = 0; i < end; i++) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->mActors[i];
        if (actor != NULL && (actor->mActorId == 'BOME' || actor->mActorId == 'SMRI')) {
            actor->mAlive = false;
        }
    }
    self->field_0x304 = 10;
    self->field_0x308 = 0xFF;
}

// The four burst effects at each shake offset; the actor then waits 10 frames in state 1.
static inline void burst(Goal_804A7EB0* self) {
    JGeometry::TVec3f pos;
    pos.z = self->mPos.z;
    for (s32 i = 0; i < 4; i++) {
        pos.x = self->mPos.x + (f32)lbl_8046C658[self->field_0x234][i];
        pos.y = self->mPos.y + (f32)lbl_8046C6C8.y[self->field_0x234][i];
        spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x202, 1, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x203, 1, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x204, 0, -4, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x205, 1, 0, NULL, NULL, true, 1.0f);
        self->field_0x30c = 60;
        self->field_0x310 = 0;
    }
    self->field_0x244 = 10;
    self->field_0x230 = 1;
}

// 0x802A6600.
// FIXME(shotgun): 71.57 %, missing/extra; behaviour drift.
// the player-mask test needs a bool form and the
// 1000-subtraction loop is unrolled here but not in the original
void Goal_804A7EB0::init() {
    mBox.set(-3.0f, -3.0f, 3.0f, 3.0f);
    field_0x230 = 0;
    mVel_0x3c.z = 0.0f;
    field_0x24c = 9;
    field_0x250 = 0;
    for (s32 i = 0; i < 4; i++) {
        field_0x2a4[i] = -1;
        field_0x254[i] = -1;
        for (s32 j = 0; j < 5; j++) {
            mEmitters[j][i] = NULL;
        }
        field_0x264[i].set(0.0f, 0.0f, 0.0f);
    }
    field_0x304 = 0;
    field_0x30c = 0;
    field_0x310 = 1;
    field_0x238 = mSpawnParam_0x90 & 0x1F;
    field_0x23c = (mSpawnParam_0x90 >> 17) & 0x1F;
    field_0x240 = (mSpawnParam_0x90 >> 22) & 0x1F;
    field_0x234 = (mSpawnParam_0x90 >> 12) & 0x1F;
    u32 flags = (mSpawnParam_0x90 >> 7) & 0x1F;
    if (flags & 1) {
        mPos.x += 8.0f;
    }
    if (flags & 2) {
        mPos.y += 8.0f;
    }
    getGameView_802D6A9C()->fn_802D752C(true);
    getGameView_802D6A9C()->fn_802D7440(false);
    fn_8022ADE4();
    s32 total = 0;
    for (s32 p = 0; p < 4; p++) {
        if ((gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << p)) != 0) {
            total += fn_80236228(p);
        }
    }
    field_0x314 = 0;
    field_0x318 = 0;
    field_0x31c = 1;
    while (total >= 1000) {
        total -= 1000;
        field_0x314++;
    }
}

// 0x802A64E8.
Goal_804A7EB0::~Goal_804A7EB0() {
    for (s32 i = 0; i < 4; i++) {
        deleteEmitterKeep(&mEmitters[0][i]);
        deleteEmitterKeep(&mEmitters[1][i]);
        deleteEmitterKeep(&mEmitters[2][i]);
        deleteEmitterKeep(&mEmitters[3][i]);
        deleteEmitterKeep(&mEmitters[4][i]);
    }
}

// 0x802A4A4C.
// FIXME(shotgun): 80.73 %, missing/extra + reloc-name; behaviour drift.
// (states 0-2 only), missing/extra + reloc-name
void Goal_804A7EB0::update() {
    switch (field_0x230) {
    case 0:
        switch (field_0x238) {
        case 0:
            if (getActorManager_801F856C()->mAllDefeated_0x305c == 1) {
                burst(this);
            }
            break;
        case 1:
            if (isScreenFlagSet_0x34()) {
                burst(this);
            }
            break;
        case 2:
            burst(this);
            break;
        }
        break;
    case 1:
        vfunc_0x54();
        field_0x244--;
        if (field_0x244 == 0) {
            if (field_0x23c != 0 && gGameRoot_8053AAF8->field_0x7e == 1) {
                s32 r = randInt_801CD664(100);
                u32 id = 'CRTL';
                u32 param = 0;
                if (r < 0x21) {
                    id = 'RUPY';
                    param = 0x1C0007;
                } else if (r < 0x42) {
                    param = 9;
                    id = 'HBMB';
                }
                JGeometry::TVec3f pos;
                pos.x = mPos.x + lbl_8046C6C8.dropX[field_0x234];
                pos.y = mPos.y + lbl_8046C6C8.dropY[field_0x234];
                pos.z = 300.0f + mPos.z;
                s32 serial = getActorManager_801F856C()->spawnActorSimple_801F8544(id, mLayer_0x4, &pos, param);
                if (id == 'HBMB') {
                    ActorBase_804A5770* bomb = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (bomb != NULL) {
                        bomb->mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, -0.1f);
                    }
                }
            }
            getGameView_802D6A9C()->setSwitchBit_802D8050(field_0x240, true, 0xFF);
            field_0x230 = 2;
            field_0x31c = 0xE6;
            field_0x244 = 300;
        }
        break;
    case 2:
        vfunc_0x54();
        field_0x244--;
        if (field_0x244 == 0) {
            field_0x230 = 3;
        }
        break;
    case 3: {
        vfunc_0x54();
        u8 count = 0;
        JGeometry::TVec3f pos;
        pos.z = mPos.z;
        if (gGameRoot_8053AAF8->field_0x7e == 1) {
            u8 n = gGameRoot_8053AAF8->getPlayerCount_80138A30();
            u8 found[4] = {0, 0, 0, 0};
            for (s32 i = 0; i < 4; i++) {
                field_0x254[i] = -1;
                f32 dx = mPos.x + (f32)lbl_8046C658[field_0x234][i];
                f32 dy = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
                JGeometry::TBox2f box;
                box.i.x = mBox.i.x + dx;
                box.i.y = mBox.i.y + dy;
                box.f.x = mBox.f.x + dx;
                box.f.y = mBox.f.y + dy;
                for (s32 p = 0; p < n; p++) {
                    if (fn_8023B0F4(getRepresentativePlayer_8022EA0C(p), &box, mLayer_0x4, false, 0.0f)) {
                        field_0x254[i] = getRepresentativePlayer_8022EA0C(p);
                        found[getRepresentativePlayer_8022EA0C(p)] = 1;
                        count++;
                        field_0x264[i].x = mPos.x + (f32)lbl_8046C658[field_0x234][i];
                        field_0x264[i].y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
                        field_0x264[i].z = 0.0f;
                        break;
                    }
                }
            }
            if (n == count && fn_8023077C()) {
                s32 slot = 0;
                for (s32 i = 0; i < 4; i++) {
                    if (found[i] == 0) {
                        while (slot < 4 && field_0x254[slot] >= 0) {
                            slot++;
                        }
                        field_0x254[slot] = i;
                        field_0x264[slot].x = mPos.x + (f32)lbl_8046C658[field_0x234][slot];
                        field_0x264[slot].y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][slot];
                        field_0x264[slot].z = mPos.z;
                        slot++;
                    }
                }
                for (s32 i = 0; i < 4; i++) {
                    if (isPlayerBitSet(gGameRoot_8053AAF8, field_0x254[i]) == 1) {
                        fn_802382D0(field_0x254[i], false);
                    }
                    fn_80230498(field_0x254[i], &field_0x264[i], 0x3E, 0xFF, -128, -128, true);
                }
                beginShow(this, 0, 0, 0);
            }
        } else {
            s32 i;
            for (i = 0; i < 4; i++) {
                f32 dx = mPos.x + (f32)lbl_8046C658[field_0x234][i];
                f32 dy = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
                JGeometry::TBox2f box;
                box.i.x = mBox.i.x + dx;
                box.i.y = mBox.i.y + dy;
                box.f.x = mBox.f.x + dx;
                box.f.y = mBox.f.y + dy;
                field_0x254[i] = fn_8023AF14(&box, mLayer_0x4, false, 0.0f);
                if (field_0x254[i] == getRepresentativePlayer_8022EA0C(0)) {
                    u8 slot = i;
                    field_0x264[i].x = mPos.x + (f32)lbl_8046C658[field_0x234][i];
                    field_0x264[i].y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
                    field_0x264[i].z = 0.0f;
                    fn_802382D0(field_0x254[i], false);
                    u8 alive = 0;
                    for (i = 0; i < 4; i++) {
                        if (fn_802300EC(i) == 1) {
                            alive++;
                        }
                    }
                    if (alive == 4) {
                        s32 next = 0;
                        for (s32 k = 0; k < 4; k++) {
                            if (next == field_0x254[slot]) {
                                next++;
                            }
                            if (k != slot) {
                                field_0x254[k] = next;
                                next++;
                                field_0x264[k].x = mPos.x + (f32)lbl_8046C658[field_0x234][k];
                                field_0x264[k].y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][k];
                                field_0x264[k].z = mPos.z;
                            }
                            fn_80230AD4(field_0x254[k]);
                            fn_80230498(field_0x254[k], &field_0x264[k], 0x3E, 0xFF, -128, -128, true);
                        }
                        beginShow(this, -1, -1, 1);
                    }
                }
            }
        }
        break;
    }
    case 4:
        if (field_0x304 == 0) {
            field_0x308--;
            if (field_0x308 != 0) {
                for (s32 i = 0; i < 4; i++) {
                    if (mEmitters[2][i] != NULL) {
                        mEmitters[2][i]->setGlobalAlpha(field_0x308);
                    }
                    if (mEmitters[3][i] != NULL) {
                        mEmitters[3][i]->setGlobalAlpha(field_0x308);
                    }
                    if (mEmitters[4][i] != NULL) {
                        mEmitters[4][i]->setGlobalAlpha(field_0x308);
                    }
                }
            } else {
                for (s32 i = 0; i < 4; i++) {
                    invalidateEmitter(mEmitters[2][i]);
                    invalidateEmitter(mEmitters[3][i]);
                    invalidateEmitter(mEmitters[4][i]);
                }
                field_0x230 = 5;
            }
        } else {
            field_0x304--;
        }
    case 5:
        field_0x244--;
        if (field_0x244 == 0) {
            field_0x248++;
            field_0x244 = lbl_8046C6C8.timer[field_0x248];
            for (s32 i = 0; i < 4; i++) {
                if (field_0x254[i] >= 0) {
                    fn_80230498(field_0x254[i], &field_0x264[i], field_0x248 + 0xDB, 0xFF, -128, -128, true);
                    if (field_0x248 == 1) {
                        fn_80230FD0(field_0x254[i], 0x10001);
                    }
                }
            }
            field_0x244 = 2;
            if (field_0x248 == 13) {
                for (s32 i = 0; i < 4; i++) {
                    if (field_0x254[i] >= 0) {
                        JGeometry::TVec3f a;
                        a.x = 7.0f + field_0x264[i].x;
                        a.y = field_0x264[i].y - 27.0f;
                        a.z = field_0x264[i].z;
                        spawnEmitter_8013CC50(mLayer_0x4, &a, 0xCB, 1, 0, NULL, NULL, true, 1.0f);
                        JGeometry::TVec3f b;
                        b.x = 7.0f + field_0x264[i].x;
                        b.y = field_0x264[i].y - 27.0f;
                        b.z = field_0x264[i].z;
                        spawnEmitter_8013CC50(mLayer_0x4, &b, 0xCC, 1, 0, NULL, NULL, true, 1.0f);
                        fn_80230FD0(field_0x254[i], 0x2000E);
                    }
                }
                field_0x244 = 0x82;
                field_0x230 = 6;
                fn_80133B58();
            }
        } else if (field_0x244 < 0x17C) {
            if (field_0x248 == 0) {
                for (s32 i = 0; i < 4; i++) {
                    if (!fn_80230188(i)) {
                        fn_80230AD4(i);
                    }
                }
            }
        }
        break;
    case 6:
        field_0x244--;
        if (field_0x244 == 0x46) {
            invalidateEmitter(mEmitters[0][0]);
            invalidateEmitter(mEmitters[0][1]);
            invalidateEmitter(mEmitters[0][2]);
            invalidateEmitter(mEmitters[0][3]);
            field_0x230 = 7;
        }
        break;
    case 7: {
        field_0x244--;
        JGeometry::TVec3f pos;
        pos.z = mPos.z;
        for (s32 i = 0; i < 4; i++) {
            pos.x = mPos.x + (f32)lbl_8046C658[field_0x234][i];
            pos.y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
            spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_8046C6C8.endId[field_0x254[i]][0], 1, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_8046C6C8.endId[field_0x254[i]][1], 1, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_8046C6C8.endId[field_0x254[i]][2], 1, 0, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &pos, lbl_8046C6C8.endId[field_0x254[i]][3], 1, 0, NULL, NULL, true, 1.0f);
        }
        field_0x304 = 30;
        fn_80230FD0(0, 0x20023);
        field_0x230 = 8;
        break;
    }
    case 8:
        field_0x304--;
        if (field_0x304 == 0) {
            for (s32 i = 0; i < 4; i++) {
                fn_80230498(field_0x254[i], &field_0x264[i], 0x121, 0xFF, -128, -128, true);
            }
            field_0x230 = 9;
        }
    case 9:
        field_0x244--;
        if (field_0x244 == 0) {
            field_0x230 = 10;
            field_0x244 = 120;
        }
        break;
    case 10:
        field_0x244--;
        if (field_0x244 == 0) {
            gGameRoot_8053AAF8->fn_801367E0(10);
        }
        break;
    }
    if (field_0x230 >= 2 && field_0x314 != 0) {
        field_0x31c--;
        if (field_0x31c == 0) {
            JGeometry::TVec3f pos;
            pos.x = mPos.x + (f32)lbl_8046C658[field_0x234][field_0x318];
            pos.y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][field_0x318];
            pos.z = 0.0f;
            getActorManager_801F856C()->spawnActorSimple_801F8544('FARY', mLayer_0x4, &pos, 0x30);
            field_0x314--;
            if (field_0x314 == 0) {
                lbl_8053AED0->requestMessage_8043D67C(0x33A, 0, lbl_80529DEC.field_0x24f);
            }
            field_0x318++;
            if (field_0x318 >= 4) {
                field_0x318 = 0;
            }
            field_0x31c = 20;
        }
    }
    field_0x24c--;
    if (field_0x24c == 0) {
        field_0x24c = 9;
        field_0x250++;
        if (field_0x250 > 2) {
            field_0x250 = 0;
        }
    }
    if (field_0x230 >= 4) {
        f32 x = mPos.x + (f32)lbl_8046C6C8.focus[field_0x234][0];
        f32 y = mPos.y + (f32)lbl_8046C6C8.focus[field_0x234][1];
        Camera_804AA9E4* camera = gGameRoot2_8053AB10->field_0x24;
        if (camera->mShowWholeArea_0x7f5 == 0) {
            camera->mFocusOverride_0x7f4 = 1;
            camera->mFocusMode_0x7f8 = 4;
            camera->mFocusPos_0x7fc.set(x, y, 0.0f);
            camera->mOverrideBlend_0x7f6 = 0;
        }
    }
    if (field_0x30c == 0 && field_0x310 == 0) {
        JGeometry::TVec3f pos;
        pos.z = mPos.z;
        for (s32 i = 0; i < 4; i++) {
            pos.x = mPos.x + (f32)lbl_8046C658[field_0x234][i];
            pos.y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
            mEmitters[0][i] = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x200, 1, 0, NULL, NULL, true, 1.0f);
            mEmitters[1][i] = spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x201, 1, 0, NULL, NULL, true, 1.0f);
        }
        field_0x310 = 1;
    } else {
        field_0x30c--;
    }
}

// 0x802A4780.
// FIXME(shotgun): 92.17 %, scheduling+regalloc; behaviour clean.
void Goal_804A7EB0::draw() {
    if (field_0x230 >= 2 && field_0x230 < 9) {
        SpriteParams_801F5AE0 params;
        for (s32 i = 0; i < 4; i++) {
            params.field_0x1d = -1;
            s32 frame = field_0x250 + 0x10D;
            JGeometry::TVec3f pos;
            pos.y = mPos.y + (f32)lbl_8046C6C8.y[field_0x234][i];
            pos.x = mPos.x + (f32)lbl_8046C658[field_0x234][i];
            pos.z = mPos.z;
            if (mLayer_0x4 < 8) {
                void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
                gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, frame, &params, 1.0f, 1.0f);
            }
        }
        for (s32 i = 0; i < 4; i++) {
            if (field_0x254[i] >= 0) {
                params.field_0x1d = -1;
                s32 frame = field_0x250 + 0x10A;
                JGeometry::TVec3f pos;
                pos.x = field_0x264[i].x;
                pos.y = field_0x264[i].y;
                pos.z = field_0x264[i].z;
                if (mLayer_0x4 < 8) {
                    void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_gc_sob.bin");
                    gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(&pos, res, frame, &params, 1.0f, 1.0f);
                }
            }
        }
    }
}

// 0x802A4654.
void Goal_804A7EB0::vfunc_0x54() {
    f32 x = mPos.x;
    f32 y = mPos.y;
    f32 z = mPos.z;
    for (s32 i = 0; i < 4; i++) {
        mPos.x = x + (f32)lbl_8046C658[field_0x234][i];
        mPos.y = y + (f32)lbl_8046C6C8.y[field_0x234][i];
        startSound_801F0E34(0x30084, 0);
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
    }
    mPos.x = x;
    mPos.y = y;
    mPos.z = z;
}
