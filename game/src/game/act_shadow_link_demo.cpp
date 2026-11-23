#include "game/GameAudio.h"
#include "game/Camera.h"
#include "game/GameGlobals.h"
#include "game/Map.h"
#include "game/SpriteParams.h"
#include "game/EnemyBase.h"
#include "game/Effect.h"
#include "game/GameRootSub20.h"
#include "game/Player.h"
#include "game/act/act_shadow_link_demo.h"


// Values that vfunc_0x54 compares the object's field_0x8 with, by the index it
// passes to setState_8011B878. Entries 0, 5, 13, 21-26, 30 and 34 are never compared.
static const s32 lbl_80464AF8[35] = {
    0x00000000, 0x0000083E, 0x0000087A, 0x000008DE, 0x00000901, 0x00000956, 0x00000A8C,
    0x00000AF0, 0x00000E1A, 0x00000E56, 0x00000E92, 0x00000ECE, 0x00000F0A, 0x00000000,
    0x0000005A, 0x000000D2, 0x0000014A, 0x00000384, 0x00000479, 0x000004D3, 0x0000050F,
    0x00000573, 0x00000573, 0x00000573, 0x00000573, 0x00000000, 0x00000000, 0x00000000,
    0x00000226, 0x00000280, 0x00000280, 0x000002F8, 0x00000320, 0x00000384, 0x00000000,
};

ShadowLinkCutscene_8049DB90::ShadowLinkCutscene_8049DB90() {
    field_0x230 = 1;
    field_0x231 = false;
    field_0x234 = 0;
    field_0x238 = 0;
    field_0x23c = 0;
    field_0x240 = 0;
    field_0x244 = 0.0f;
    field_0x248 = 0.0f;
    field_0x254 = 0.0f;
    field_0x250 = 0.0f;
    field_0x25c = 0.0f;
    field_0x258 = 0.0f;
}

void ShadowLinkCutscene_8049DB90::init() {
    DemoActorBase_8049D140::init();
    field_0x234 = mSpawnParam_0x90 & 0xF;
    mDir_0x98 = (s8)((mSpawnParam_0x90 >> 4) & 3);
    switch (field_0x234) {
    case 1:
        setState_8011B878(0xD);
        mDir_0x98 = 3;
        mSequencer.setState_800FEB78(0);
        break;
    case 2:
        setState_8011B878(0x16);
        mDir_0x98 = 3;
        mSequencer.setState_800FEB78(1);
        mPos.y += 80.0f;
        break;
    default:
        mSequencer.setState_800FEB78(0);
        break;
    }
    mSequencer.mDir = mDir_0x98;
}

// The hop that update's states share: mVel_0x3c gets (vx, vy, base - 0.2 * t);
// once t > 1 and the actor is back at z <= 0, it stops and the hop is over.
static inline bool hop(ShadowLinkCutscene_8049DB90* self, s32 t, f32 base, f32 vy, f32 vx) {
    self->mVel_0x3c.z = base - 0.2f * t;
    self->mVel_0x3c.y = vy;
    self->mVel_0x3c.x = vx;
    if (t > 1 && self->mPos.z <= 0.0f) {
        self->mPos.z = self->mVel_0x3c.x = self->mVel_0x3c.y = self->mVel_0x3c.z = 0.0f;
        return true;
    }
    return false;
}

// FIXME: 99.32 %, different; behaviour drift.
void ShadowLinkCutscene_8049DB90::update() {
    DemoActorBase_8049D140::update();
    if (field_0x238 >= 2 && field_0x238 <= 11) {
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);
        fn_8011B3F0();
    }
    switch (field_0x238) {
    case 0:
        mDrawEnabled = false;
        break;
    case 4:
        if (hop(this, fn_8011921C()->field_0x8 - lbl_80464AF8[4], 4.5f, 0.5f, 0.0f)) {
            setState_8011B878(5);
        }
        break;
    case 9:
        hopDown();
        break;
    case 11:
        // the original passes 0xFF as fn_802303FC's flag here and in states 20 and 22 (li r5, 0xff)
        for (s32 i = 0; i < 4; i++) {
            JGeometry::TVec3f pos(*getPlayerPos_8023E724(i));
            pos.add(field_0x26c);
            fn_802303FC(i, &pos, true);
        }
        break;
    case 16:
        mPos.y -= 0.3f;
        if (mPos.y <= mSpawnPos.y - 64.0f) {
            mPos.y = mSpawnPos.y - 64.0f;
        }
        break;
    case 19:
        hop(this, fn_8011921C()->field_0x8 - lbl_80464AF8[19], 2.5f, 0.0f, 0.0f);
        break;
    case 20:
        mPos.y -= 1.2f;
        if (mPos.y <= mSpawnPos.y - 282.0f) {
            mPos.y = mSpawnPos.y - 282.0f;
            setState_8011B878(23);
        } else if (fn_8011921C()->field_0x8 >= 0x52D) {
            JGeometry::TVec3f pos(*getPlayerPos_8023E724(0));
            pos.y -= 1.6f;
            for (s32 i = 0; i < 4; i++) {
                fn_802303FC(i, &pos, true);
            }
        }
        break;
    case 22:
        if (fn_8011921C()->field_0x0 == 0) {
            fn_8011921C()->fn_801186B0(0x12);
            fn_8011921C()->field_0x2 = 1;
        }
        mPos.y -= 1.2f;
        if (mPos.y <= mSpawnPos.y) {
            mPos.y = mSpawnPos.y;
            setState_8011B878(23);
        } else {
            JGeometry::TVec3f pos(mPos);
            pos.y += 32.0f;
            for (s32 i = 0; i < 4; i++) {
                fn_802303FC(i, &pos, true);
            }
        }
        break;
    case 23:
        field_0x23c++;
        if (hop(this, field_0x23c, 4.5f, -0.8f, 1.45f)) {
            setState_8011B878(24);
        }
        break;
    case 24:
        startSound_801F0E34(0x400C7, 0);
        setState_8011B878(25);
        break;
    case 25:
        field_0x23c++;
        if (hop(this, field_0x23c, 2.5f, 0.0f, 0.0f)) {
            field_0x23c = 0;
        }
        if (damagePlayersInBox_801F2B7C(0, -1, 1)) {
            setState_8011B878(26);
        }
        break;
    case 26: {
        field_0x23c++;
        f32 vx = -2.9f;
        if (!field_0x231) {
            vx = 2.9f;
        }
        if (hop(this, field_0x23c, 4.5f, 0.0f, vx)) {
            setState_8011B878(25);
            if (field_0x231) {
                mPos.x = mSpawnPos.x - 64.0f;
            } else {
                mPos.x = 64.0f + mSpawnPos.x;
            }
        }
        break;
    }
    case 28:
        if (field_0x240 < 3) {
            field_0x23c++;
            if (hop(this, field_0x23c, 2.5f, 0.0f, 0.0f)) {
                field_0x23c = 0;
                field_0x240++;
            }
        }
        break;
    case 29: {
        f32 vx = 1.45f;
        if (!field_0x231) {
            vx = -1.45f;
        }
        if (hop(this, fn_8011921C()->field_0x8 - lbl_80464AF8[29], 4.5f, 1.1f, vx)) {
            setState_8011B878(30);
        }
        break;
    }
    case 32:
        mPos.y += 1.2f;
        if (mPos.y >= 300.0f + mSpawnPos.y) {
            mPos.y = 300.0f + mSpawnPos.y;
        }
        break;
    case 34: // the original's jump table runs to state 34; nothing is done there
        break;
    }
    mPos.add(mVel_0x3c);
    mSequencer.mDir = mDir_0x98;
    if (field_0x238 == 16) {
        if (field_0x230) {
            mSequencer.fn_800FE8C0();
            field_0x230 = 0;
        } else {
            field_0x230 = 1;
        }
    } else {
        mSequencer.fn_800FE8C0();
    }
}

static inline void drawShadowSob(ActorBase_804A5770* actor, const char* name, s32 frame,
                                 SpriteParams_801F5AE0* params, s32 flags) {
    if (actor->mSpriteParams.mMtx == NULL) {
        drawShadow_801EF830(actor->mLayer_0x4, &actor->mPos, params, flags, name, frame, NULL);
    }
}

static inline void drawSob(s32 layer, const JGeometry::TVec3f* pos, const char* name, s32 frame) {
    if (layer < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', name);
        gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pos, res, frame, NULL, 1.0f, 1.0f);
    }
}

void ShadowLinkCutscene_8049DB90::draw() {
    DemoActorBase_8049D140::draw();
    SpriteParams_801F5AE0 params;
    params.field_0x2e = 0x40;
    drawShadowSob(this, "player_sob.bin", mSequencer.mFrame, &params, 7);
    drawSob(mLayer_0x4, getPos(), "player_sob.bin", mSequencer.mFrame);
    showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, mSequencer.mFrame, 0, 1, -1, false, 0, 0);
}

void ShadowLinkCutscene_8049DB90::vfunc_0x54() {
    s32 value = fn_8011921C()->field_0x8;
    switch (field_0x234) {
    case 0:
        if (value == lbl_80464AF8[1]) {
            setState_8011B878(1);
        } else if (value == lbl_80464AF8[2]) {
            setState_8011B878(2);
        } else if (value == lbl_80464AF8[3]) {
            setState_8011B878(3);
        } else if (value == lbl_80464AF8[4]) {
            setState_8011B878(4);
        } else if (value == lbl_80464AF8[6]) {
            setState_8011B878(6);
        } else if (value == lbl_80464AF8[7]) {
            setState_8011B878(7);
        } else if (value == lbl_80464AF8[8]) {
            setState_8011B878(8);
        } else if (value == lbl_80464AF8[9]) {
            setState_8011B878(9);
        } else if (value == lbl_80464AF8[10]) {
            setState_8011B878(10);
        } else if (value == lbl_80464AF8[11]) {
            setState_8011B878(11);
        } else if (value == lbl_80464AF8[12]) {
            setState_8011B878(12);
        }
        break;
    case 1:
        if (fn_8011921C()->field_0x4 == 0x11) {
            if (value == lbl_80464AF8[14]) {
                setState_8011B878(14);
            } else if (value == lbl_80464AF8[15]) {
                setState_8011B878(15);
            } else if (value == lbl_80464AF8[16]) {
                setState_8011B878(16);
            } else if (value == lbl_80464AF8[17]) {
                setState_8011B878(17);
            } else if (value == lbl_80464AF8[18]) {
                setState_8011B878(18);
            } else if (value == lbl_80464AF8[19]) {
                setState_8011B878(19);
            } else if (value == lbl_80464AF8[20]) {
                setState_8011B878(20);
            }
        } else if (fn_8011921C()->field_0x4 == 2) {
            if (value == lbl_80464AF8[27]) {
                setState_8011B878(27);
            } else if (value == lbl_80464AF8[28]) {
                setState_8011B878(28);
            } else if (value == lbl_80464AF8[29]) {
                setState_8011B878(29);
            } else if (value == lbl_80464AF8[31]) {
                setState_8011B878(31);
            } else if (value == lbl_80464AF8[32]) {
                setState_8011B878(32);
            } else if (value == lbl_80464AF8[33]) {
                setState_8011B878(33);
            }
        }
        break;
    case 2: // nothing sets state 2; the original's switch has an entry for it
        break;
    }
}

s32 ShadowLinkCutscene_8049DB90::vfunc_0x58() {
    return field_0x238;
}

s32 ShadowLinkCutscene_8049DB90::vfunc_0x5C() {
    switch (field_0x238) {
    case 12:
    case 21:
    case 24:
    case 33:
    case 34:
        return 1;
    }
    return 0;
}

static inline void setupMoveToPlayer(ShadowLinkCutscene_8049DB90* self) {
    for (s32 i = 0; i < 4; i++) {
        setPlayerFacing_80230BF4(i, 3);
    }
    self->field_0x260 = self->mSpawnPos;
    JGeometry::TVec3f d = self->mSpawnPos - *getPlayerPos_8023E724(0);
    self->field_0x26c = JGeometry::TVec3f(d.x / 60.0f, d.y / 60.0f, d.z / 60.0f);
}

static inline void spawnAppearEmitters(ShadowLinkCutscene_8049DB90* self) {
    JGeometry::TVec3f pos(self->mPos);
    spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x41D, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x41E, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x41F, 0, 0, NULL, NULL, true, 1.0f);
    spawnEmitter_8013CC50(self->mLayer_0x4, &pos, 0x420, 0, 0, NULL, NULL, true, 1.0f);
    self->startSound_801F0E34(0x30091, 0);
}

void ShadowLinkCutscene_8049DB90::setState_8011B878(s32 index) {
    switch (index) {
    case 0:
        mDrawEnabled = false;
        break;
    case 1:
        setScreenFlag_0x30(0xFF, true);
        break;
    case 4: {
        GameRootSub20_801902C0* sub = fn_801902C0();
        sub->field_0x7c0 = 1;
        sub->field_0x7bc = 1;
        startSound_801F0E34(0x40035, 0);
        mSequencer.setState_800FEB78(4);
        mDrawEnabled = true;
        break;
    }
    case 5:
        mSequencer.setState_800FEB78(0);
        break;
    case 6:
        startSound_801F0E34(0x400C7, 0);
        break;
    case 9:
        mSequencer.setState_800FEB78(4);
        break;
    case 10: {
        setScreenFlag_0x30(0xFF, false);
        mDrawEnabled = false;
        JGeometry::TVec3f pos(mSpawnPos);
        pos.y += 128.0f;
        for (s32 i = 0; i < 4; i++) {
            fn_80230498(i, &pos, 0x18, 0xFF, -0x80, -0x80, true);
        }
        break;
    }
    case 11:
        setupMoveToPlayer(this);
        break;
    case 13:
        mDrawEnabled = false;
        break;
    case 14:
        spawnAppearEmitters(this);
        break;
    case 15:
        mDrawEnabled = true;
        break;
    case 16:
        mDir_0x98 = 3;
        mSequencer.mState = 1;
        mSequencer.mDir = 3;
        break;
    case 17:
        mDir_0x98 = 2;
        mSequencer.mState = 0;
        mSequencer.mDir = 2;
        break;
    case 18:
        mDir_0x98 = 2;
        mSequencer.mState = 0;
        mSequencer.mDir = 2;
        spawnFlash();
        break;
    case 20:
        mDir_0x98 = 3;
        mSequencer.setState_800FEB78(1);
        break;
    case 21:
        mSequencer.setState_800FEB78(0);
        break;
    case 22:
        mDir_0x98 = 3;
        mSequencer.setState_800FEB78(1);
        mVel_0x3c.x = mVel_0x3c.y = mVel_0x3c.z = 0.0f;
        break;
    case 23:
        mSequencer.setState_800FEB78(4);
        field_0x23c = 0;
        for (s32 i = 0; i < 4; i++) {
            fn_80230498(i, getPlayerPos_8023E724(i), 0x3F, 0xFF, -0x80, -0x80, true);
        }
        break;
    case 24:
        mDir_0x98 = 2;
        mSequencer.setState_800FEB78(0);
        field_0x23c = 0;
        break;
    case 25:
        mSequencer.setState_800FEB78(0);
        field_0x23c = 0;
        break;
    case 26:
        mSequencer.setState_800FEB78(4);
        field_0x231 = !field_0x231;
        field_0x23c = 0;
        mPos.z = 0.0f;
        startSound_801F0E34(0x400C7, 0);
        break;
    case 27:
        mSequencer.setState_800FEB78(0);
        mPos.z = mVel_0x3c.x = mVel_0x3c.y = mVel_0x3c.z = 0.0f;
        break;
    case 28:
        startSound_801F0E34(0x400C7, 0);
        break;
    case 29:
        mSequencer.setState_800FEB78(4);
        mPos.z = 0.0f;
        break;
    case 30:
        mDir_0x98 = 3;
        mSequencer.setState_800FEB78(0);
        break;
    case 31:
        startSound_801F0E34(0x400C7, 0);
        break;
    case 32:
        mDir_0x98 = 2;
        mSequencer.setState_800FEB78(1);
        break;
    case 33:
    case 34:
        break;
    }
    field_0x238 = index;
}

// Not in the binary: setState_8011B878 and update inline these two, and the pool has their literals
// between fn_8011B3F0's and setState_8011B878's.

// Effect 0x33B at mPos with y offset by -8.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 5-line definition): split out only where the .sdata2 order needs it.
void ShadowLinkCutscene_8049DB90::spawnFlash() {
    JGeometry::TVec3f pos(mPos);
    pos.y += -8.0f;
    spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x33B, 0, 0, NULL, NULL, true, 1.0f);
}

// State 9 of update: the hop of state 4 with mVel_0x3c.y -0.5 and mVel_0x3c.x left alone.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 8-line definition): split out only where the .sdata2 order needs it.
void ShadowLinkCutscene_8049DB90::hopDown() {
    s32 t = fn_8011921C()->field_0x8 - lbl_80464AF8[9];
    mVel_0x3c.z = 4.5f - 0.2f * t;
    mVel_0x3c.y = -0.5f;
    if (t > 1 && mPos.z <= 0.0f) {
        mPos.z = mVel_0x3c.x = mVel_0x3c.y = mVel_0x3c.z = 0.0f;
    }
}

// Pushes the 4-float box at 0x250 to the scripted camera (mode 5).
static inline void setCameraBox(ShadowLinkCutscene_8049DB90* self) {
    Camera_804AA9E4* cam = gGameRoot2_8053AB10->field_0x24;
    if (cam->mShowWholeArea_0x7f5 == 0) {
        cam->mFocusOverride_0x7f4 = 1;
        cam->mFocusMode_0x7f8 = 5;
        cam->mFocusBox_0x808.i.x = self->field_0x250;
        cam->mFocusBox_0x808.i.y = self->field_0x254;
        cam->mFocusBox_0x808.f.x = self->field_0x258;
        cam->mFocusBox_0x808.f.y = self->field_0x25c;
        cam->mOverrideBlend_0x7f6 = 0;
    }
}

static inline void setCameraBoxPos(ShadowLinkCutscene_8049DB90* self, f32 x, f32 y) {
    self->field_0x250 = x - 64.0f;
    self->field_0x254 = y - 32.0f;
    self->field_0x258 = 64.0f + x;
    self->field_0x25c = 32.0f + y;
    setCameraBox(self);
}

// FIXME: 96.72 %, scheduling+regalloc; behaviour clean.
void ShadowLinkCutscene_8049DB90::fn_8011B3F0() {
    Camera_804AA9E4* cam = gGameRoot2_8053AB10->field_0x24;
    if (cam->field_0x179 == 0) {
        cam->field_0x17a = 1;
        cam->field_0x179 = 0;
    }
    switch (field_0x238) {
    case 7: {
        s32 diff = fn_8011921C()->field_0x8 - lbl_80464AF8[7];
        if (diff < 0xB4) {
            setCameraBox(this);
        } else {
            if (diff > 0x19A && diff != 0) {
                moveToward_801CD950(&field_0x244, 0.0f, -0.007f);
            } else {
                moveToward_801CD950(&field_0x244, 0.1f, 0.0035f);
            }
            moveToward_801CD950(&field_0x254, (mPos.y - 32.0f) - 24.0f, -field_0x244);
            moveToward_801CD950(&field_0x25c, (32.0f + mPos.y) + 24.0f, field_0x244);
            setCameraBox(this);
        }
        break;
    }
    case 1:
    case 2:
    case 3:
        setCameraBoxPos(this, mPos.x, mPos.y);
        break;
    case 4:
    case 5: {
        JGeometry::TVec3f p(mPos);
        p.y += field_0x248;
        moveToward_801CD950(&field_0x248, 32.0f, 0.8f);
        setCameraBoxPos(this, p.x, p.y);
        break;
    }
    case 6: {
        JGeometry::TVec3f p(mPos);
        p.y += field_0x248;
        moveToward_801CD950(&field_0x248, 0.0f, 0.8f);
        setCameraBoxPos(this, p.x, p.y);
        break;
    }
    case 8: {
        Camera_804AA9E4* c = gGameRoot2_8053AB10->field_0x24;
        if (c->mShowWholeArea_0x7f5 == 0) {
            c->mFocusOverride_0x7f4 = 1;
            c->mFocusMode_0x7f8 = 4;
            c->mFocusPos_0x7fc.set(mPos);
            c->mOverrideBlend_0x7f6 = 0;
        }
        break;
    }
    case 9:
    case 10:
    case 11: {
        Camera_804AA9E4* c = gGameRoot2_8053AB10->field_0x24;
        if (c->mBlendBoxOn_0x840 == 0) {
            c->mBlendBoxOn_0x840 = 1;
            c->mBlendBox_0x844.i.x = c->mBlendBox_0x844.f.x = mPos.x;
            c->mBlendBox_0x844.i.y = c->mBlendBox_0x844.f.y = mPos.y;
            c->mBlendWeight_0x854 = 1.0f;
            c->mMargin_0x1d4 = -1.0f;
            c->mMinHalfWidth_0x1d8 = -1.0f;
        }
        break;
    }
    default:
        setCameraBox(this);
        break;
    }
}

ShadowLinkCutscene_8049DB90::~ShadowLinkCutscene_8049DB90() {}
