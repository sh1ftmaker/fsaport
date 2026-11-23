#include "game/ActorId.h"
#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Camera.h"
#include "game/Player.h"
#include "game/Effect.h"
#include "game/EnemyBase.h"
#include "game/act/act_after_goal_opening_8011CEC8.h"
#include "game/Unk_8045A080.h"
#include <float.h>

// 'DMGA' (AfterGoalOpening_8049DD68): the demo camera. States 1-9 are set by setState_8011D1B8
// from vfunc_0x54, which compares the manager's counter with lbl_80464BBC.

// The SpriteAnim frame tables (lbl_80464B98: anim 24-31, then the terminator).
static const s16 lbl_80464B98[18] = {24, 6, 25, 6, 26, 6, 27, 6, 28, 6, 29, 6, 30, 6, 31, 6, -1, -1};
const s16* lbl_80539F20[2] = {lbl_80464B98, NULL};

// Counter values that vfunc_0x54 matches, entry n selecting state n.
static const s32 lbl_80464BBC[11] = {0, 0, 360, 800, 830, 950, 1040, 1160, 1325, 1550, 0};

s32 fn_801365A8();

void AfterGoalOpening_8049DD68::init() {
    DemoActorBase_8049D140::init();
    field_0x274.setKeyTables_801D2608(lbl_80539F20, 1);
    field_0x274.startAnim_801D22DC(0, 2, false);
    field_0x274.setFrame_801D1F14(0.0f);
    field_0x240 = mPos;
    field_0x258 = 284.0f;
    getGameView_802D6A9C()->fn_802D7440(0);
}

void AfterGoalOpening_8049DD68::update() {
    DemoActorBase_8049D140::update();
    fn_8011CF48();
    static s32 lbl_8053AA98 = 0;
    switch (field_0x234) {
    case 0:
        setState_8011D1B8(1);
        break;
    case 1:
    case 2:
        break;
    case 3:
        moveToward_801CD950(&field_0x238, 255.0f, 10.0f);
        break;
    case 4:
        break;
    case 5:
        moveToward_801CD950(&field_0x238, 0.0f, -10.0f);
        break;
    case 6:
    case 7:
    case 8:
    case 9:
        break;
    }
    if (field_0x230) {
        setOverlayRect();
    }
    field_0x274.update_801D1C18();
}

void AfterGoalOpening_8049DD68::draw() {
    DemoActorBase_8049D140::draw();
}

void AfterGoalOpening_8049DD68::vfunc_0x54() {
    if (fn_8011921C()->field_0x4 == 0x11) {
        s32 v = fn_8011921C()->field_0x8;
        if (v == lbl_80464BBC[2]) {
            setState_8011D1B8(2);
        } else if (v == lbl_80464BBC[3]) {
            setState_8011D1B8(3);
        } else if (v == lbl_80464BBC[4]) {
            setState_8011D1B8(4);
        } else if (v == lbl_80464BBC[5]) {
            setState_8011D1B8(5);
        } else if (v == lbl_80464BBC[6]) {
            setState_8011D1B8(6);
        } else if (v == lbl_80464BBC[7]) {
            setState_8011D1B8(7);
        } else if (v == lbl_80464BBC[8]) {
            setState_8011D1B8(8);
        } else if (v == lbl_80464BBC[9]) {
            setState_8011D1B8(9);
        }
    }
}

s32 AfterGoalOpening_8049DD68::vfunc_0x58() {
    return field_0x234;
}

s32 AfterGoalOpening_8049DD68::vfunc_0x5C() {
    return field_0x234 == 9;
}

// Not auto-inlined: update and vfunc_0x54 call setState_8011D1B8 / fn_8011CF48 out of line in the
// original (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// FIXME(shotgun): 96.99 %, different; behaviour drift.
// jump table is not yet a named jumptable_8049DDF0; stack layout still differs
void AfterGoalOpening_8049DD68::setState_8011D1B8(s32 state) {
    switch (state) {
    case 1:
        fn_8011921C()->fn_801186B0(0x11);
        for (s32 i = 0; i < 4; i++) {
            setPlayerFacing_80230BF4(i, 3);
            fn_80230498(i, getPlayerPos_8023E724(i), 0x3F, 0, -0x80, -0x80, true);
        }
        fn_8011921C()->field_0x2 = 1;
        gAudioMgr_8053AF80->startBgm_80458FF0(0x100001B, false, 0);
        break;
    case 2:
        startScroll();
        break;
    case 3:
        field_0x238 = 0.0f;
        field_0x230 = 1;
        break;
    case 4:
        field_0x238 = 255.0f;
        break;
    case 6: {
        for (s32 i = 0; i < 4; i++) {
            cls_801192AC* area = fn_8011921C();
            ActorBase_804A5770** it = area->fn_8011871C(area->mActors.mBegin, ACTOR_DMLK);
            JGeometry::TVec3f zero;
            const JGeometry::TVec3f* pos;
            if (it == fn_8011921C()->mActors.mEnd) {
                zero.set(0.0f, 0.0f, 0.0f);
                pos = &zero;
            } else {
                pos = &(*it)->mSpawnPos;
            }
            fn_80230498(i, pos, 0x3F, 0, -0x80, -0x80, true);
        }
        cls_801192AC* area = fn_8011921C();
        ActorBase_804A5770** it = area->fn_8011871C(area->mActors.mBegin, ACTOR_DMLK);
        JGeometry::TVec3f zero;
        const JGeometry::TVec3f* pos;
        if (it == fn_8011921C()->mActors.mEnd) {
            zero.set(0.0f, 0.0f, 0.0f);
            pos = &zero;
        } else {
            pos = &(*it)->mSpawnPos;
        }
        JGeometry::TVec3f spawnPos = *pos;
        spawnEmitter_8013CC50(mLayer_0x4, &spawnPos, 0x33F, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &spawnPos, 0x340, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &spawnPos, 0x341, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &spawnPos, 0x342, 0, 0, NULL, NULL, true, 1.0f);
        startSound_801F0E34(0x30079, 0);
        break;
    }
    case 7:
        for (s32 i = 0; i < 4; i++) {
            setPlayerFacing_80230BF4(i, 3);
            fn_80230498(i, getPlayerPos_8023E724(i), 0x3F, 0xFF, -0x80, -0x80, true);
        }
        break;
    case 9:
        getGameView_802D6A9C()->fn_802D9CB0();
        fn_8011921C()->fn_80118384();
        markDead_801F3668();
        break;
    }
    field_0x234 = state;
}

#pragma auto_inline reset

// A plain function that update inlines and whose own copy the linker dropped: the original's
// .sdata2 has 373.0f, 500.0f, 0.5f, 20.0f and 40.0f between fn_8011CF48's literals and
// setState_8011D1B8's, so it was generated here (tools/pool.py). Gives lbl_8053AED0 its rect, 40 high
// and centred on the camera's position on the screen, with field_0x238 as the alpha.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 21-line definition): split out only where the .sdata2 order needs it.
void AfterGoalOpening_8049DD68::setOverlayRect() {
    Camera_804AA9E4* cam = gGameRoot2_8053AB10->field_0x24;
    Vec out;
    Vec in;
    in.x = -cam->mViewMtx_0x4[0][3];
    in.y = -cam->mViewMtx_0x4[1][3];
    in.z = 0.0f;
    cam->worldToScreen_8030C5F0(&in, false, false, &out);
    f32 size;
    switch (fn_801365A8()) {
    case 2:
        size = 373.0f;
        break;
    default:
        size = 500.0f;
        break;
    }
    out.x -= size / 2.0f;
    out.y -= 20.0f;
    lbl_8053AED0->fn_8043A208((u8)field_0x238, out.x, out.y, size, 40.0f);
}

#pragma auto_inline off
// FIXME(shotgun): 97.92 %, missing/extra; behaviour drift.
void AfterGoalOpening_8049DD68::fn_8011CF48() {
    switch (field_0x234) {
    case 0:
    case 9:
        break;
    case 2:
        if (field_0x240.y - (mSpawnPos.y - 440.0f) >= 116.0f) {
            moveToward_801CD950(&field_0x23c, -1.4f, -0.01f);
        } else {
            moveToward_801CD950(&field_0x23c, -0.1f, 0.006f);
        }
        moveToward_801CD950(&field_0x240.y, mSpawnPos.y - 440.0f, field_0x23c);
        {
            JGeometry::TVec2f c(field_0x240.x, field_0x240.y);
            gGameRoot2_8053AB10->field_0x24->requestBox_8030C7C4(&c, field_0x258);
        }
        break;
    case 1:
    case 3:
    case 4:
    case 5: {
        JGeometry::TVec2f c(field_0x240.x, field_0x240.y);
        gGameRoot2_8053AB10->field_0x24->requestBox_8030C7C4(&c, field_0x258);
        break;
    }
    case 6: {
        if (fn_8011921C()->field_0x8 >= lbl_80464BBC[6] + 20) {
            f32 d = (f32)(lbl_80464BBC[7] - 75 - fn_8011921C()->field_0x8);
            f32 spawnY = mSpawnPos.y;
            if (d < FLT_EPSILON) {
                field_0x240.y = spawnY;
            } else {
                field_0x240.y += (1.0f / d) * (spawnY - field_0x240.y);
            }
        }
        JGeometry::TVec2f c(field_0x240.x, field_0x240.y);
        gGameRoot2_8053AB10->field_0x24->requestBox_8030C7C4(&c, field_0x258);
        break;
    }
    case 7: {
        JGeometry::TVec2f c(field_0x240.x, field_0x240.y);
        gGameRoot2_8053AB10->field_0x24->requestBox_8030C7C4(&c, field_0x258);
        break;
    }
    case 8: {
        cls_801192AC* list = fn_8011921C();
        ActorBase_804A5770** it = list->fn_8011871C(list->mActors.mBegin, 'DMSW');
        const JGeometry::TVec3f* pos;
        if (it == fn_8011921C()->mActors.mEnd) {
            JGeometry::TVec3f zero(0.0f, 0.0f, 0.0f);
            pos = &zero;
        } else {
            pos = &(*it)->mPos;
        }
        moveToward_801CD950(&field_0x240.y, 40.0f + pos->y, 1.8f);
        JGeometry::TVec2f c(field_0x240.x, field_0x240.y);
        gGameRoot2_8053AB10->field_0x24->requestBox_8030C7C4(&c, field_0x258);
        break;
    }
    }
}
#pragma auto_inline reset

// A plain function that setState_8011D1B8 inlines and whose own copy the linker dropped: the original's
// .sdata2 starts with 0.0f, ahead of fn_8011CF48's literals, so it was generated before it
// (tools/pool.py). Entering state 2: the scroll speed and the overlay's alpha start at zero.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 9-line definition): split out only where the .sdata2 order needs it.
void AfterGoalOpening_8049DD68::startScroll() {
    field_0x23c = 0.0f;
    field_0x238 = 0.0f;
    field_0x230 = 0;
    s32 dir[2];
    dir[0] = 0;
    dir[1] = -1;
    getGameView_802D6A9C()->stepScreen_802DCD1C(dir);
}

AfterGoalOpening_8049DD68::~AfterGoalOpening_8049DD68() {}
