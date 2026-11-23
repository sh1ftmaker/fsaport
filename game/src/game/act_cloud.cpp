#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "game/ViewLayer.h"
#include "JSystem/JMath/JMath.h"
#include "game/act/act_cloud.h"

// Name of each state's sprite, four per state (the state is field_0x36c).
static const char* lbl_8049CBB0[3][4] = {
    {"lift_s_a.bti", "lift_s_b.bti", "lift_s_c.bti", "lift_s_d.bti"},
    {"lift_l_a.bti", "lift_l_b.bti", "lift_l_c.bti", "lift_l_d.bti"},
    {"said_a.bti", "said_b.bti", "said_c.bti", "said_d.bti"},
};

// One animation: (cel, duration) pairs, ended by a cel of -1.
static const s16 lbl_80463FA0[] = {0, 10, 1, 10, 2, 10, 3, 10, -1, 0};
static const s16* lbl_80539EC0[] = {lbl_80463FA0, NULL};

static inline bool contains(const JGeometry::TBox2f& box, const JGeometry::TVec3f& p) {
    return p.x >= box.i.x && p.y >= box.i.y && p.x < box.f.x && p.y < box.f.y;
}

Cloud_8049CBE0::Cloud_8049CBE0()
    : field_0x344(0.0f), field_0x348(0.0f), field_0x34c(0.0f), field_0x350(0.0f), field_0x354(0),
      field_0x358(0), field_0x35c(-1), field_0x360(0), field_0x364(0), field_0x368(0),
      field_0x36c(0), field_0x370(0) {
    for (s32 i = 0; i < 4; i++) {
        field_0x2f8[i] = NULL;
    }
}

// 0x80105A34.
void Cloud_8049CBE0::init() {
    ActorBase_804A5770::init();
    field_0x354 = mSpawnParam_0x90 & 0xFF;
    field_0x358 = mSpawnParam_0x90 >> 12 & 0x1F;
    field_0x36c = mSpawnParam_0x90 >> 17 & 0x1F;
    field_0x364 = mSpawnParam_0x90 >> 22 & 0x1F;
    field_0x368 = mSpawnParam_0x90 >> 22 & 0x1F;
    field_0x35c = mSpawnParam_0x90 >> 27;
    for (s32 i = 0; i < 4; i++) {
        field_0x2f8[i] = gGameRoot_8053AAF8->field_0xa4->getResource('????', lbl_8049CBB0[field_0x36c][i]);
    }
    switch (field_0x36c) {
    case 1:
        mBox.i.x = -38.0f;
        mBox.i.y = -38.0f;
        mBox.f.x = 38.0f;
        mBox.f.y = 38.0f;
        break;
    case 2:
        mBox.i.x = -32.0f;
        mBox.i.y = -16.0f;
        mBox.f.x = 32.0f;
        mBox.f.y = 12.0f;
        break;
    case 0:
    default:
        mBox.i.x = -22.0f;
        mBox.i.y = -22.0f;
        mBox.f.x = 22.0f;
        mBox.f.y = 22.0f;
        break;
    }
    mBox2.set(mBox);
    mMapBox_0x80.set(mBox);
    mSprite.setKeyTables_801D2608(lbl_80539EC0, 1);
    mSprite.startAnim_801D22DC(0, 2, false);
    mSprite.setFrame_801D1F14(0.0f);
}

static bool pool_8053C228(s32 player);

// 0x801056E4.
// Remaining diffs: box offset scheduling/regalloc, case 0 ble target (field_0x360/field_0x370
// assignment is inside `if (field_0x354 > 0)` in the original), and stack slot of `at`
// (stepAlongRail_800FDEA0 took pos by value in the original; RailBase.h declares it as a pointer).
// FIXME: 94.02 %, scheduling+regalloc; behaviour clean.
void Cloud_8049CBE0::update() {
    ActorBase_804A5770::update();
    if (viewLayerFlag(mLayer_0x4) != 0) {
        JGeometry::TBox2f box(mBox);
        box.i.x += mPos.x;
        box.f.x += mPos.x;
        box.i.y += mPos.y;
        box.f.y += mPos.y;
        fn_80239914(&box, mLayer_0x4, mSerial);
    }
    s32 bit = field_0x368;
    if (bit > 0 && getGameView_802D6A9C()->isScreenFlagSet_802D800C(bit)) {
        mAlive = false;
        return;
    }
    switch (field_0x370) {
    case 0:
        if (field_0x354 > 0) {
            mRail.fn_800FE3EC(field_0x354, mPos, mLayer_0x4);
            if (field_0x354 > 0 && field_0x364 > 0) {
                u8 count = mRail.fn_800FD894(-1, NULL, 0);
                if (field_0x364 <= count) {
                    mRail.setIndex(field_0x364);
                }
            }
        }
        field_0x360 = 0;
        field_0x370 = 1;
        break;
    case 1:
        if (field_0x354 > 0) {
            JGeometry::TVec3f step(0.0f, 0.0f, 0.0f);
            s32 railLayer = mLayer_0x4;
            u16 angle;
            JGeometry::TVec3f at = mPos;
            f32 speed = 0.2f * field_0x358 + 0.5f;
            s32 result = mRail.stepAlongRail_800FDEA0(&at, speed, &step, &railLayer, &angle, 0x200);
            mVel_0x3c.x = step.x;
            mVel_0x3c.y = step.y;
            mVel_0x3c.z = step.z;
            mPos.add(mVel_0x3c);
            if (result != 0 && mRail.fn_800FE21C(0xFF) != 0) {
                field_0x360 = 0;
                mVel_0x3c.zero();
                field_0x370 = 2;
            }
        }
        break;
    case 2:
        field_0x360++;
        if (field_0x360 >= 60) {
            field_0x360 = 0;
            field_0x370 = 1;
        }
        break;
    }
    for (s32 i = 0; i < 4; i++) {
        if (fn_801053E8(i) && !fn_8022D75C(i) && !fn_80230188(i) && !pool_8053C228(i)) {
            const JGeometry::TVec3f& d = mPos - mPrevPos;
            fn_8023DF68(i, &d);
        }
    }
    mSprite.update_801D1C18();
}

void Cloud_8049CBE0::draw() {
    ActorBase_804A5770::draw();
    mParams.field_0x1d = -1;
    s32 key = mSprite.mKey;
    JGeometry::TVec3f pos = mPos;
    if (field_0x36c == 2) {
        pos.y -= 4.0f;
    }
    void* res = field_0x2f8[key];
    if (mLayer_0x4 < 8) {
        gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&pos, res, &mParams);
    }
}

// Whether `player` is in the cloud: on the cloud's layer, either its box
// (moved to the cloud's position) overlaps the player's, or one of eight
// points around the player falls in mBox.
bool Cloud_8049CBE0::fn_801053E8(s32 player) {
    if (player < 0) {
        return false;
    }
    if (mLayer_0x4 != getPlayerLayer_8023DE58(player)) {
        return false;
    }
    JGeometry::TBox2f box(mBox);
    if (viewLayerFlag(mLayer_0x4)) {
        box.i.x += mPos.x;
        box.i.y += mPos.y;
        box.f.x += mPos.x;
        box.f.y += mPos.y;
        box.i.y -= 2.0f;
        if (fn_8023B0F4(player, &box, mLayer_0x4, false, 0.0f)) {
            fn_8022DF48(player);
            return true;
        }
        return false;
    }
    for (s32 i = 0; i < 8; i++) {
        JGeometry::TVec3f v;
        fn_8022E088(player, i, &v);
        v -= mPos;
        v.z = 0.0f;
        // The angle is in s16 units (32768 = 180 degrees); JMASinShort and
        // JMACosShort index the sin/cos table with it.
        s32 angle = (s32)(JMath::atanTable_.atan2_(v.y, v.x) *
                          (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180()));
        f32 len = JGeometry::TUtil<f32>::sqrt(v.x * v.x + v.y * v.y);
        v.x = len * JMACosShort(angle);
        v.y = len * JMASinShort(angle);
        v.z = 0.0f;
        if (contains(box, v)) {
            fn_8022DF48(player);
            return true;
        }
    }
    return false;
}

// Not in the binary: a plain function that update inlines. The pool puts it below fn_801053E8:
// its 0.0f is the unit's first literal.
// R2 pool helper (one-statement body); was isAirborne.
static bool pool_8053C228(s32 player) {
    return getPlayerPos_8023E724(player)->z > 0.0f;
}

Cloud_8049CBE0::~Cloud_8049CBE0() {}
