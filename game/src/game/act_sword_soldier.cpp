// The SLR1 actor (sword soldier, docs/names/actors.tsv; vtable 0x8049FCA0,
// constructor 0x80203CA4 in the spawner). Only the small helpers (and the
// actor-manager wrappers) are written so far.

#include "game/GameAudio.h" // its JAudio pools give the unit its __sinit (0x8015478C)
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/Map.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/ViewLayer.h"
#include "game/Player.h"
#include "game/RotMatrix.h"
#include "game/act/act_chief_soldier.h"
#include "game/act/act_sword_soldier.h"
#include "JSystem/JMath/JMath.h"
#include "std/math.h"

extern const char lbl_80465DBC[]; // "enemy_both2_sob.bin"
extern const char lbl_80465DD0[]; // "enemy_gc_sob.bin"
extern s32 lbl_80529DEC[];
s32 lbl_804BF850[4]; // serials of the soldiers that took a hit
s32 lbl_8053AB40;
s32 lbl_8053AB44;
s32 lbl_8053AB48;
extern s32 lbl_80539F68;

// Life and sprite mode per kind.
// The four facing directions in turning order.
static s32 lbl_8049FA30[4] = {0, 2, 1, 3};

static struct {
    s32 life;
    s32 mode;
} lbl_8049FA40[7] = {{12, 2}, {12, 3}, {1, 4}, {12, 3}, {16, 6}, {8, 2}, {20, 8}};

// Walk-cycle frames per direction (8 steps each).
static s32 lbl_8049FA78[32] = {
    0,  1,  0,  2,  0,  1,  0,  3,  6,  7,  6,  8,  6,  7,  6,  9,
    12, 13, 12, 14, 12, 13, 12, 15, 18, 19, 18, 20, 18, 19, 18, 21,
};
// Frame duration of each walk step.
static s32 lbl_8049FAF8[8] = {6, 8, 6, 8, 6, 8, 6, 8};
static s32 lbl_8049FB18[4] = {12, 18, 6, 0};
static s32 lbl_8049FB28[4] = {12, 8, 0, 4};
static s32 lbl_8049FB38[16] = {0, 4, 0, 5, 6, 10, 6, 11, 12, 16, 12, 17, 18, 22, 18, 23};
static s32 lbl_8049FB78[16] = {0, 3, 0, 2, 1, 3, 1, 2, 2, 1, 2, 0, 3, 1, 3, 0};
// Position offset (x, y) per sword pose.
static f32 lbl_8049FBB8[16] = {
    8.0f, 8.0f, -8.0f, 8.0f, -12.0f, 6.0f, 12.0f, -4.0f, 34.0f, 8.0f, -16.0f, 8.0f, -12.0f, 32.0f, 12.0f, -4.0f,
};
// Hit box (x0, y0, x1, y1) per sword pose.
static f32 lbl_8049FBF8[24] = {
    -3.0f, -3.0f, 3.0f, 3.0f, -3.0f, -3.0f, 3.0f, 3.0f, -4.0f, -1.0f, 4.0f, 1.0f,
    -1.0f, -2.0f, 1.0f, 4.0f, -12.0f, -6.0f, -10.0f, -4.0f, -2.0f, -12.0f, 2.0f, -10.0f,
};
static s32 lbl_8049FC58[9] = {16, 20, 21, 22, 23, 24, 25, 26, 8};
static s32 lbl_8049FC7C[9] = {4, 4, 4, 20, 2, 2, 2, 2, 4};

// A random float in [0, 1) from the game's RNG. Called from 10 units.
// The instructions match; its 1.0f only pairs with lbl_8053D25C once the
// unit's earlier functions put 0.0f (lbl_8053D258) first in .sdata2.
f32 randFloat_80154744() {
    return gGameRoot_8053AAF8->mRandom.get_ufloat_1();
}

// Keeps pos in the camera's view (3 units call it; they ignore r3).
void fn_80154718(const JGeometry::TVec3f* pos) {
    gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(pos);
}

void copyBox_801546F4(JGeometry::TBox2f* dst, const JGeometry::TBox2f* src) {
    dst->set(*src);
}

JGeometry::TVec2f* fn_801546E8(JGeometry::TVec2f* v, f32 x, f32 y) {
    v->set(x, y);
    return v;
}

void fn_801546D0(JGeometry::TVec3f* dst, const JGeometry::TVec3f* src) {
    *dst = *src;
}

void zeroVec_801546BC(JGeometry::TVec3f* v) {
    v->zero();
}

u8 fn_80154698() {
    return getGameView_802D6A9C()->isInTransition_802DCD0C();
}

// inline: its 10430.378f is pooled right after update()'s literals (first
// use), while its out-of-line copy sits at the end of the unit.
inline s32 atan2Short_8015465C(f32 y, f32 x) {
    return JMath::atanTable_.atan2_(y, x) * (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180());
}

u8 fn_80154624(s32 layer) {
    return viewLayerFlag(layer);
}

s32 ActorBase_804A5770::getLayer_8015461C() const {
    return mLayer_0x4;
}

void ActorBase_804A5770::fn_80154614(s32 value) {
    field_0xb8 = value;
}

void fn_801545F0(JGeometry::TVec3f* a, const JGeometry::TVec3f* b) {
    a->add(*b);
}

s32 ActorBase_804A5770::getSerial_801545E8() const {
    return mSerial;
}

s32 ActorBase_804A5770::fn_801545E0() const {
    return field_0x9c;
}

u8 ActorBase_804A5770::fn_801545C0(JGeometry::TVec3f* out, bool flag) {
    return sweepMapBox_801F2618(out, flag);
}

void fn_801545A4(JGeometry::TVec3f* dst, const JGeometry::TVec3f* src) {
    dst->set(*src);
}

void setBox_80154590(JGeometry::TBox2f* box, f32 x0, f32 y0, f32 x1, f32 y1) {
    box->set(x0, y0, x1, y1);
}

void fn_8015457C(JGeometry::TVec2f* dst, const JGeometry::TVec2f* src) {
    dst->set(*src);
}

void addVec2_80154558(JGeometry::TVec2f* a, const JGeometry::TVec2f* b) {
    a->add(*b);
}

void fn_80154528(const JGeometry::TBox2f* box, JGeometry::TVec2f* out) {
    out->x = box->f.x + (box->i.x - box->f.x) * 0.5f;
    out->y = box->f.y + (box->i.y - box->f.y) * 0.5f;
}

void setVec_80154518(JGeometry::TVec3f* v, f32 x, f32 y, f32 z) {
    v->set(x, y, z);
}

// The EnemyParam "atcp" damage (ActorBase's 0x114), passed to damagePlayersInBox_801F2B7C.
s32 ActorBase_804A5770::getParamAtcp_80154510() const {
    return mParamAtcp_0x114;
}

f32 sinShort_801544FC(s16 angle) {
    return JMASinShort(angle);
}

f32 cosShort_801544E4(s16 angle) {
    return JMACosShort(angle);
}

// An actor's position (its mPos). Out of line in the original, called from
// 4 other units on actors they got from findActorBySerial_801F666C / findActorBySerial_8015445C.
JGeometry::TVec3f* ActorBase_804A5770::getPos_801544DC() {
    return &mPos;
}

void ActorBase_804A5770::fn_801544D0(s32 value) {
    mHitTimer_0xd8 = value;
    mHitTimerStart_0xdc = value;
}

u32 ActorBase_804A5770::getSpawnParam_801544C8() const {
    return mSpawnParam_0x90;
}

s32 ChiefSoldier_804A0B90::fn_801544C0() {
    return field_0x288;
}

void ChiefSoldier_804A0B90::fn_801544B4() {
    field_0x290 = -1;
}

s32 ChiefSoldier_804A0B90::fn_801544AC() const {
    return field_0x28c;
}

void ChiefSoldier_804A0B90::fn_8015449C() {
    field_0x290 = field_0x290 + 1;
}

// fn_80154490 to fn_801544C0: the sword soldier (SLR1) calls them on the
// SLR4 (Chief Soldier) actor that fn_801542AC finds, so they are that class's
// members; they sit here because this is the first unit that needs them.
void ChiefSoldier_804A0B90::fn_80154490() {
    field_0x2a5 = 1;
}

ActorBase_804A5770* findActorBySerial_8015445C(s32 serial) {
    return getActorManager_801F856C()->findActorBySerial_801F666C(serial);
}

// std::sqrtf out of line; 13 calls from 7 units. inline: its constants are
// pooled right after update()'s literals (first use).
inline f32 sqrtf_80154378(f32 x) {
    return std::sqrtf(x);
}

// fabs narrowed to f32, out of line.
f32 fabsf_8015436C(f32 x) {
    return fabs(x);
}

s32 spawnActor_80154310(u32 id, s32 layer, const JGeometry::TVec3f* pos, u32 arg, s32 a, s32 b) {
    return getActorManager_801F856C()->spawnActor_801F9484(id, layer, pos, arg, a, b);
}

s32 fn_801542AC(u32 id, s32 layer, ActorBase_804A5770** out, s32 excludeSerial) {
    return getActorManager_801F856C()->collectActorsById_801F7818(id, layer, out, excludeSerial);
}

s32 fn_80154248(u32 id, s32 layer, const JGeometry::TVec3f* pos, u32 arg) {
    return getActorManager_801F856C()->spawnActorSimple_801F8544(id, layer, pos, arg);
}

// FIXME(shotgun): 99.80 %, scheduling+regalloc; behaviour clean.
void SwordSoldier_8049FCA0::init() {
    mBox.set(-8.0f, -4.0f, 8.0f, 8.0f);
    mBox2.set(-8.0f, -8.0f, 8.0f, 8.0f);
    mMapBox_0x80.set(-8.0f, -8.0f, 8.0f, 8.0f);
    field_0x94 = (mSpawnParam_0x90 >> 16) & 0xF;
    field_0x288 = (mSpawnParam_0x90 >> 20) & 0xF;
    field_0x280 = 0;
    if (mSpawnParam_0x90 & 0x100) {
        field_0x280 = 1;
    }
    lbl_804BF850[0] = -1;
    lbl_80539F68 = 200;
    lbl_804BF850[1] = -1;
    lbl_804BF850[2] = -1;
    lbl_804BF850[3] = -1;
    lbl_8053AB48 = 0;
    field_0x284 = 0;
    if (!(mSpawnParam_0x90 & 0x1000)) {
        field_0x284 = 1;
    }
    field_0x2a4 = mSpawnParam_0x90 >> 24;
    field_0x2a8 = 0;
    if (field_0x2a4 == 0) {
        field_0x2a4 = -1;
    }
    field_0x258 = 0;
    field_0x25c = 0;
    field_0x260 = 0;
    field_0x264 = 0;
    field_0x268 = 0;
    field_0x26c = 0;
    field_0x270 = 0;
    field_0x238 = 0;
    field_0x23c = 0;
    field_0x240 = 0;
    field_0x244 = 0;
    field_0x248 = 0;
    field_0x24c = 0;
    field_0x250 = 0;
    field_0x254 = 0;
    field_0x234 = 5;
    field_0x274 = 0;
    field_0x294 = 0.0f;
    field_0x298 = 1.0f;
    field_0x290 = 0.7f;
    field_0x27c = 0;
    field_0x29e = 0;
    if (field_0x94 == 1) {
        field_0x290 = 1.2f;
    }
    field_0x28c = 1.5f * field_0x290 + 0.2f * gGameRoot_8053AAF8->mRandom.get_ufloat_1();
    field_0x2ac.zero();
    mDir_0x98 = (s32)(3.99f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) & 3;
    if (viewLayerFlag(mLayer_0x4)) {
        mDir_0x98 &= 1;
    }
    field_0x238 = 8;
    field_0x278 = lbl_8053AB40;
    lbl_8053AB40++;
    field_0x29c = -1;
    field_0xb0 = 0x12205;
    field_0xbc = 2;
    mSpawnParam_0x90 &= 7;
    field_0x2a0 = 0;
    mParamVoic_0x190 = 4;
    mLife = lbl_8049FA40[mSpawnParam_0x90].life;
    field_0x234 = lbl_8049FA40[mSpawnParam_0x90].mode;
    field_0xb8 = 1;
    if (field_0x94 == 2 || field_0x94 == 5 || field_0x94 == 3) {
        field_0xb0 = 0;
        field_0xb8 = 0;
        mDir_0x98 = 2;
        field_0x274 = lbl_8049FA78[mDir_0x98 * 8];
    }
    if (mSpawnParam_0x90 == 2 || mSpawnParam_0x90 == 3 || mSpawnParam_0x90 == 1) {
        mParamVoic_0x190 = 5;
    }
    switch (mSpawnParam_0x90) {
    case 6:
        field_0x28c = 0.2f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + field_0x290;
        field_0xbc = 3;
        mParamEfsz_0xc0 = 3;
        field_0x284 = 0;
        field_0x54.set(1.5f, 1.5f, 1.5f);
        mParamVoic_0x190 = 6;
        field_0xb0 = 0x62209;
        mBox2.set(-10.0f, -8.0f, 10.0f, 10.0f);
        mState = 11;
        break;
    case 1:
        field_0xb8 = 0;
        mDir_0x98 = 2;
        field_0x274 = 12;
        lbl_8053AB44 = 0;
        mState = 12;
        break;
    case 0:
    case 4:
    case 5:
        field_0xb0 = 0x62209;
        // fall through
    default:
        mState = 1;
        if (lbl_80529DEC[0x46] == 0 && lbl_80529DEC[0x47] == 0 && getGameView_802D6A9C()->mScreenY_0x18 == 2 &&
            getGameView_802D6A9C()->mScreenX_0x14 == 2) {
            if ((s32)(0.1f * mPos.x) == 0x68 && (s32)(0.1f * mPos.y) == 0x57) {
                field_0x23c = (s32)(40.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x6E;
                field_0x290 = 1.4f;
                mDir_0x98 = 0;
                switch (mDir_0x98) {
                case 0:
                    field_0x2ac.set(field_0x290, 0.0f, 0.0f);
                    break;
                case 1:
                    field_0x2ac.set(-field_0x290, 0.0f, 0.0f);
                    break;
                case 2:
                    field_0x2ac.set(0.0f, field_0x290, 0.0f);
                    break;
                case 3:
                    field_0x2ac.set(0.0f, -field_0x290, 0.0f);
                    break;
                }
                mPos.y -= 32.0f;
                mState = 0;
            } else {
                field_0x270 = 1;
            }
        }
        break;
    }
    if (field_0x288 == 1 || field_0x288 == 3) {
        mBox2.set(-8.0f, -8.0f, 8.0f, 8.0f);
        if (field_0x288 == 3) {
            mBox2.set(-10.0f, -8.0f, 10.0f, 10.0f);
            field_0x54.set(1.5f, 1.5f, 1.5f);
        }
        field_0xb0 = 0xA042004;
        field_0x234 = 2;
        field_0xb8 = 0;
        mParamDasu_0x18c = 0;
        field_0x238 = (s32)(60.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x3C;
        field_0x274 = 0x10;
        mState = 0x11;
    }
    if (field_0x2a4 >= 0) {
        if (field_0x94 >= 6 && field_0x94 <= 9) {
            field_0xb0 = 0xA040004;
            mDir_0x98 = field_0x94 - 6;
            field_0x25c = mDir_0x98;
            field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
            }
            mMapBox_0x80.set(-6.0f, -6.0f, 6.0f, 6.0f);
            field_0xb8 = 0;
        }
        mState = 0xF;
    }
}

void SwordSoldier_8049FCA0::vfunc_0x44() {
    lbl_8053AB44 = 1;
    field_0xb8 = 1;
    if (mLife <= 0) {
        s32 serial = field_0x2a4;
        if (serial >= 0) {
            ChiefSoldier_804A0B90* leader =
                (ChiefSoldier_804A0B90*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (leader != NULL) {
                leader->field_0x290 = -1;
            }
        }
    }
    if (field_0x94 >= 6 && field_0x94 <= 9) {
        s32 serial = field_0x2a4;
        if (serial >= 0) {
            ChiefSoldier_804A0B90* leader =
                (ChiefSoldier_804A0B90*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
            if (leader != NULL) {
                leader->field_0x290 = -1;
            }
        }
    }
    for (s32 i = 0; i < 4; i++) {
        if (lbl_804BF850[i] == getSerial()) {
            lbl_804BF850[i] = -1;
            field_0xbc = 2;
        }
    }
    if (field_0x284 != 0) {
        if (lbl_8053AB48 == 0) {
            lbl_80539F68 = 0;
        }
        lbl_8053AB48 += 2;
        if (lbl_8053AB48 > 200) {
            lbl_8053AB48 = 200;
        }
    }
}

SwordSoldier_8049FCA0::~SwordSoldier_8049FCA0() {
    if (mLife <= 0) {
        if (field_0x288 == 1 || field_0x288 == 3) {
            if (field_0x288 == 1) {
                getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &mPos, 0x30004);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3D9, 0, 1, NULL, NULL, true, 1.0f);
            } else {
                getActorManager_801F856C()->spawnActorSimple_801F8544(ACTOR_RUPY, mLayer_0x4, &mPos, 0x30008);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3D9, 0, 1, NULL, NULL, true, 1.5f);
            }
        }
    }
    s32 serial = field_0x2a4;
    if (serial >= 0) {
        ChiefSoldier_804A0B90* leader =
            (ChiefSoldier_804A0B90*)getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (leader != NULL) {
            leader->field_0x290 = -1;
        }
    }
    for (s32 i = 0; i < 4; i++) {
        if (lbl_804BF850[i] == getSerial()) {
            lbl_804BF850[i] = -1;
        }
    }
}

BOOL SwordSoldier_8049FCA0::vfunc_0x54() {
    if (viewLayerFlag(mLayer_0x4)) {
        return FALSE;
    }
    if (mState == 0xE) {
        return TRUE;
    }
    if (mPos.z > 2.0f) {
        return FALSE;
    }
    field_0x2b8.x = mPos.x;
    field_0x2b8.y = mPos.y;
    field_0x2b8.z = mPos.z;
    field_0x2b8.x = (f32)((s32)field_0x2b8.x & 0xFFF8);
    field_0x2b8.y = (f32)((s32)field_0x2b8.y & 0xFFF8);
    field_0x2b8.x += -12.0f * JMath::sincosTable_.sinShort((s16)dirToAngle_801F6874(mDir_0x98));
    field_0x2b8.y += -12.0f * JMath::sincosTable_.cosShort((s16)dirToAngle_801F6874(mDir_0x98));
    s32 tile = getTileCollisionCode_8022461C(&field_0x2b8, mLayer_0x4);
    if ((u32)(tile - 2) <= 1 || tile == 0xE) {
        field_0x250 = 10000;
        mLife = 200;
        field_0x280 = 0;
        if (tile == 0xE && fn_802DE9A0() == 1) {
            field_0x280 = 1;
        }
        mVel_0x3c.zero();
        field_0x2ac.zero();
        mHitTimer_0xd8 = 0;
        mHitTimerStart_0xdc = 0;
        mState = 0xD;
        return TRUE;
    }
    return FALSE;
}

// FIXME(shotgun): 98.64 %, missing/extra + reloc-name; behaviour drift.
void SwordSoldier_8049FCA0::update() {
    JGeometry::TBox2f boxD8;
    copyBox_801546F4(&boxD8, &mMapBox_0x80);
    JGeometry::TVec2f pos70;
    fn_801546E8(&pos70, mPos.x, mPos.y);
    JGeometry::TVec2f vec68;
    JGeometry::TVec3f oldPos;
    fn_801546D0(&oldPos, &mPos);
    u16 angle = 0;
    u8 moved = 0;
    f32 dist = 0.0f;
    bool flag = true;
    s32 dir = 0;
    s32 playerIdx = -1;
    JGeometry::TVec3f moveOut;
    zeroVec_801546BC(&moveOut);

    if (field_0x94 == 2) {
        if (field_0x29c == 0xFFFF) {
            if (fn_80154698()) {
                return;
            }
            field_0x29c = getTileAt_80226998(&mPos, getLayer_8015461C());
            return;
        }
        if (field_0x29c == getTileAt_80226998(&mPos, getLayer_8015461C())) {
            return;
        }
        field_0x250 = 40;
        fn_80154614(1);
        field_0x94 = 0;
    }

    if (mState != 0xF && field_0x2a4 >= 0) {
        ChiefSoldier_804A0B90* leader = (ChiefSoldier_804A0B90*)findActorBySerial_8015445C(field_0x2a4);
        if (leader != NULL) {
            field_0x27c = leader->fn_801544C0();
        } else {
            field_0x2a4 = -2;
            if (field_0x2a8 == 1) {
                die_0x24();
                return;
            }
        }
    }

    if (field_0x94 == 5 || field_0x94 == 3) {
        if (mPos.z > 0.0f) {
            if (field_0x94 == 5) {
                if (0.0f != mVel_0x3c.z) {
                    mVel_0x3c.z -= 1.0f;
                    if (mVel_0x3c.z < -10.0f) {
                        mVel_0x3c.z = -10.0f;
                    }
                }
            } else {
                if (0.0f != mVel_0x3c.z) {
                    mVel_0x3c.z -= 1.0f;
                    if (mVel_0x3c.z < -8.0f) {
                        mVel_0x3c.z = -8.0f;
                    }
                }
            }
            fn_801545F0(&mPos, &mVel_0x3c);
            return;
        }
        field_0xb0 = 0x12205;
        mPos.z = 0.0f;
        zeroVec_801546BC(&mVel_0x3c);
        field_0x94 = 0;
    }

    if (field_0x2a4 == -2 && mState != 8 && mState != 9 && mState != 10 && mState != 17 &&
        mState != 18 && mState != 19 && mState != 20) {
        for (s32 i = 0; i < 4; i++) {
            if (lbl_804BF850[i] == getSerial_801545E8()) {
                lbl_804BF850[i] = -1;
                field_0xbc = 2;
            }
        }
        field_0x28c = 1.5f * field_0x290 + 0.2f * randFloat_80154744();
        mState = 7;
    }

    if (mSpawnParam_0x90 == 6 && mLife > 0) {
        fn_80154718(&mPos);
        flag = false;
    }

    if (updateHitReaction_801F3D94(flag) != 0) {
        zeroVec_801546BC(&field_0x2ac);
        if (mState != 12 && field_0x288 != 1 && field_0x288 != 3 && fn_801545E0() >= 0) {
            startSound_801F0E34(0x40010, 0);
        }
        if (!vfunc_0x54()) {
            fn_801545C0(NULL, false);
        }
        if (mSpawnParam_0x90 != 6) {
            field_0xbc = 2;
        }
        if (mSpawnParam_0x90 != 6 || mLife <= 0) {
            return;
        }
    }

    if (mState != 16 && mState != 9 && mState != 10 && mState != 17 && mState != 18 &&
        mState != 19 && mState != 20) {
        if (field_0x250 == 0 && checkHits_801F305C()) {
            if (mSpawnParam_0x90 == 6) {
                if (mLife > 0) {
                    zeroVec_801546BC(&mVel_0x3c);
                }
            } else if (mState != 4 && mState != 5 && mState != 6 && mState != 16) {
                mState = 4;
            }
            zeroVec_801546BC(&field_0x2ac);
            for (s32 i = 0; i < 4; i++) {
                if (lbl_804BF850[i] == getSerial_801545E8()) {
                    lbl_804BF850[i] = -1;
                    field_0xbc = 2;
                }
            }
            if (field_0x284 != 0) {
                if (lbl_8053AB48 == 0) {
                    lbl_80539F68 = 0;
                }
                lbl_8053AB48 += 2;
                if (lbl_8053AB48 > 200) {
                    lbl_8053AB48 = 200;
                }
            }
            fn_801545C0(NULL, false);
            if (mSpawnParam_0x90 != 6 || mLife <= 0) {
                return;
            }
        }

        if (mHitTimer_0xd8 == 0 && mSpawnParam_0x90 != 2 && mSpawnParam_0x90 != 3 && mSpawnParam_0x90 != 1 &&
            field_0x254 == 0) {
            s32 idx1 = mDir_0x98;
            s32 idx2 = (mDir_0x98 >> 1) & 1;
            if (field_0x288 != 1 && field_0x288 != 3) {
                if (mSpawnParam_0x90 == 5) {
                    idx2 += 2;
                }
                if (mSpawnParam_0x90 == 6) {
                    idx2 += 4;
                    idx1 += 4;
                }
            }
            fn_801545A4(&oldPos, &mPos);
            f32* offsetTbl = lbl_8049FBB8;
            f32* boxTbl = lbl_8049FBF8;
            mPos.x += offsetTbl[idx1 * 2];
            mPos.y += offsetTbl[idx1 * 2 + 1];
            setBox_80154590(&mBox, boxTbl[idx2 * 4], boxTbl[idx2 * 4 + 1], boxTbl[idx2 * 4 + 2],
                        boxTbl[idx2 * 4 + 3]);
            for (s32 i = 0; i < 4; i++) {
                playerIdx = findOverlappingPlayer_801F285C(NULL, NULL, true, i);
                if (playerIdx >= 0) {
                    JGeometry::TVec2f tmp28;
                    JGeometry::TVec2f tmp60;
                    fn_8015457C(&tmp60, fn_801546E8(&tmp28, mPos.x, mPos.y));
                    JGeometry::TVec2f center58;
                    JGeometry::TBox2f boxB0;
                    copyBox_801546F4(&boxB0, &mBox);
                    addVec2_80154558(&boxB0.i, &tmp60);
                    addVec2_80154558(&boxB0.f, &tmp60);
                    fn_80154528(&boxB0, &center58);
                    if (fn_80237948(playerIdx, &center58)) {
                        fn_80230FD0(playerIdx, 0x20002);
                    }
                    if (mSpawnParam_0x90 != 6 && (field_0x94 < 6 || field_0x94 > 9)) {
                        f32 speed = -5.0f;
                        switch (mDir_0x98) {
                        case 0:
                            setVec_80154518(&mVel_0x3c, speed, 0.0f, 0.0f);
                            break;
                        case 1:
                            setVec_80154518(&mVel_0x3c, -speed, 0.0f, 0.0f);
                            break;
                        case 2:
                            setVec_80154518(&mVel_0x3c, 0.0f, speed, 0.0f);
                            break;
                        case 3:
                            setVec_80154518(&mVel_0x3c, 0.0f, -speed, 0.0f);
                            break;
                        }
                        zeroVec_801546BC(&field_0x2ac);
                        field_0x254 = 40;
                    }
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x473, 0, 1, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x474, 0, 1, NULL, NULL, true, 1.0f);
                    break;
                }
            }
            if (mSpawnParam_0x90 == 6) {
                damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 3);
            } else {
                damagePlayersInBox_801F2B7C(-100, -1, 1);
            }
            fn_801545A4(&mPos, &oldPos);
        }
    }

    if (mSpawnParam_0x90 == 6 || field_0x288 == 3) {
        setBox_80154590(&mBox, -6.0f, -14.0f, 6.0f, 4.0f);
    } else if (mState != 16) {
        setBox_80154590(&mBox, -4.0f, -6.0f, 4.0f, 2.0f);
    } else {
        setBox_80154590(&mBox, -8.0f, -8.0f, 8.0f, 8.0f);
    }

    if (mState == 16 || mState == 17 || mState == 18 || mState == 19 || mState == 20) {
        u32 ignoreIds[17] = {'SWBM', 'FIRE', 'BLZE', 'HBMB', 'FLME', 'THCL', 'GRAS', 'SSTN', 'STON',
                             'ROCK', 'RCK2', 'BORD', 'TUBO', 'KEY0', 'YASI', 'SMRI', 'NULL'};
        if (checkActorHit_801F2DB4('NULL', 'NULL', ignoreIds)) {
            if (field_0x94 >= 6 && field_0x94 <= 9 && field_0x2a4 >= 0) {
                ChiefSoldier_804A0B90* leader = (ChiefSoldier_804A0B90*)findActorBySerial_8015445C(field_0x2a4);
                if (leader != NULL) {
                    leader->fn_801544B4();
                }
            }
        }
        for (s32 i = 0; i < 4; i++) {
            playerIdx = findOverlappingPlayer_801F285C(NULL, NULL, true, i);
            if (playerIdx >= 0) {
                JGeometry::TVec2f tmp20;
                JGeometry::TVec2f tmp50;
                fn_8015457C(&tmp50, fn_801546E8(&tmp20, mPos.x, mPos.y));
                JGeometry::TVec2f center48;
                JGeometry::TBox2f boxA0;
                copyBox_801546F4(&boxA0, &mBox);
                addVec2_80154558(&boxA0.i, &tmp50);
                addVec2_80154558(&boxA0.f, &tmp50);
                fn_80154528(&boxA0, &center48);
                if (fn_80237948(playerIdx, &center48)) {
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x473, 0, 1, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x474, 0, 1, NULL, NULL, true, 1.0f);
                    startSound_801F0E34(0x400D1, 0);
                    if (field_0x94 >= 6 && field_0x94 <= 9 && field_0x2a4 >= 0) {
                        ChiefSoldier_804A0B90* leader = (ChiefSoldier_804A0B90*)findActorBySerial_8015445C(field_0x2a4);
                        if (leader != NULL) {
                            leader->fn_801544B4();
                            break;
                        }
                    }
                }
            }
        }
    }

    if (mSpawnParam_0x90 == 6 && field_0x288 != 1 && field_0x288 != 3) {
        damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 3);
        for (s32 i = 0; i < 4; i++) {
            playerIdx = findOverlappingPlayer_801F285C(NULL, NULL, true, i);
            if (playerIdx >= 0 && !fn_80235448(playerIdx)) {
                f32 dx = mPos.x - getPlayerPos_8023E724(playerIdx)->x;
                f32 dy = mPos.y - getPlayerPos_8023E724(playerIdx)->y;
                u16 ang = (u16)atan2Short_8015465C(-dx, -dy);
                if (angleToDir_801F681C(ang) == (s32)mDir_0x98) {
                    field_0x250 = 2;
                    break;
                }
            }
        }
    } else {
        damagePlayersInBox_801F2B7C(-100, -1, 1);
    }

    if (mSpawnParam_0x90 != 6 && field_0x250 == 1) {
        field_0xb0 = 0x12205;
    }

    if (mState != 13 && mState != 14) {
        findTargetPlayer_801EEC98(getSerial_801545E8(), &angle, &dir, &dist, &playerIdx, 0, 0);
        if (dir < 0 && (mState == 4 || mState == 5 || mState == 6)) {
            for (s32 i = 0; i < 4; i++) {
                if (lbl_804BF850[i] == getSerial_801545E8()) {
                    lbl_804BF850[i] = -1;
                    field_0xbc = 2;
                }
            }
            mState = 1;
        }
        if (fn_80154624(getLayer_8015461C())) {
            mPos.y -= mPos.z;
            mPos.z = 0.0f;
            mVel_0x3c.z = 0.0f;
            field_0x2ac.y = 3.0f;
        }
        addCalc2_801F71A4(&mVel_0x3c.x, field_0x2ac.x, 0.3f, 1.0f);
        addCalc2_801F71A4(&mVel_0x3c.y, field_0x2ac.y, 0.3f, 1.0f);
        if (field_0x284 != 0 && playerIdx >= 0 && dist < 56.0f &&
            lbl_804BF850[playerIdx] != getSerial_801545E8()) {
            if (lbl_804BF850[playerIdx] >= 0) {
                if (mState == 6) {
                    field_0x2ac.x = field_0x28c * sinShort_801544FC((s16)(angle + 0x7000));
                    field_0x2ac.y = field_0x28c * cosShort_801544E4((s16)(angle + 0x7000));
                }
            } else if (mState == 6 && fn_801545E0() < 0) {
                lbl_80539F68 -= lbl_8053AB48;
                if (lbl_80539F68 > 0) {
                    field_0x2ac.x = field_0x28c * sinShort_801544FC((s16)(angle + 0x7000));
                    field_0x2ac.y = field_0x28c * cosShort_801544E4((s16)(angle + 0x7000));
                } else {
                    if (lbl_804BF850[playerIdx] != getSerial_801545E8()) {
                        if (mSpawnParam_0x90 != 2 && mSpawnParam_0x90 != 3 && mSpawnParam_0x90 != 1) {
                            startSound_801F0E34(0x40065, 0);
                        } else {
                            startSound_801F0E34(0x40066, 0);
                        }
                    }
                    lbl_80539F68 = 200;
                    lbl_804BF850[playerIdx] = getSerial_801545E8();
                    field_0xbc = 3;
                }
            }
        }
        fn_801545F0(&mPos, &mVel_0x3c);
        if (mState != 12 && field_0x288 != 1 && field_0x288 != 3 && fn_801545E0() >= 0) {
            startSound_801F0E34(0x40010, 0);
        }
        zeroVec_801546BC(&moveOut);
        moved = fn_801545C0(&moveOut, false);
    }

    if (field_0x254 != 0) {
        vfunc_0x54();
    }

    switch (mState) {
    case 0:
        fn_80154718(&mPos);
        if (field_0x23c == 0) {
            field_0x280 = 0;
            field_0x23c = (s32)(100.0f * randFloat_80154744()) + 150;
            field_0x240 = (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 20;
            field_0x290 = 0.7f;
            field_0x28c = 1.5f * field_0x290 + 0.2f * randFloat_80154744();
            mState = 6;
            break;
        }
        {
            f32 sightRange = 100.0f;
            if (mSpawnParam_0x90 == 2 || mSpawnParam_0x90 == 3 || mSpawnParam_0x90 == 1) {
                sightRange = 1000.0f;
            }
            if (dir == (s32)mDir_0x98 && dist <= sightRange) {
                JGeometry::TVec3f v94;
                JGeometry::TVec3f v88;
                if (!fn_801F23BC(mLayer_0x4, &mPos, getPlayerPos_8023E724(playerIdx), &mMapBox_0x80,
                                 &v94, &v88, false)) {
                    field_0x290 = 0.7f;
                    mState = 4;
                }
            }
        }
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 7;
            field_0x238 = lbl_8049FAF8[field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                if ((field_0x258 & 1) == 0) {
                    field_0x238 = 4;
                } else {
                    field_0x238 = 6;
                }
            }
        }
        field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
        if (field_0x288 == 1 || field_0x288 == 3) {
            field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
        }
        break;

    case 1:
        if (moved && field_0x244 == 0 && (!fn_80154624(getLayer_8015461C()) || moveOut.x != 0.0f)) {
            zeroVec_801546BC(&mVel_0x3c);
            zeroVec_801546BC(&field_0x2ac);
            field_0x238 = 8;
            mDir_0x98 ^= 1;
            field_0x274 = lbl_8049FB18[mDir_0x98];
            if (field_0x288 == 1 || field_0x288 == 3) {
                field_0x274 = lbl_8049FB28[mDir_0x98];
            }
            mState = 2;
            break;
        }
        if (dir >= 0 && (field_0x280 != 0 || field_0x270 != 0)) {
            field_0x23c = (s32)(100.0f * randFloat_80154744()) + 150;
            field_0x240 = (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 20;
            field_0x28c = 1.5f * field_0x290 + 0.2f * randFloat_80154744();
            mState = 6;
            field_0x280 = 0;
            break;
        }
        if (field_0x23c == 0) {
            zeroVec_801546BC(&field_0x2ac);
            field_0x258 = 0;
            field_0x25c = (s32)(1.99f * randFloat_80154744()) + 1;
            field_0x238 = 8;
            if (field_0x288 == 1 || field_0x288 == 3) {
                field_0x238 = (s32)(20.0f * randFloat_80154744()) + 20;
                field_0x25c = 0;
            }
            mState = 3;
            break;
        }
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 7;
            field_0x238 = lbl_8049FAF8[field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                if ((field_0x258 & 1) == 0) {
                    field_0x238 = 4;
                } else {
                    field_0x238 = 6;
                }
            }
        }
        field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
        if (field_0x288 == 1 || field_0x288 == 3) {
            field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
        }
        {
            f32 sightRange = 100.0f;
            if (mSpawnParam_0x90 == 2 || mSpawnParam_0x90 == 3 || mSpawnParam_0x90 == 1) {
                sightRange = 1000.0f;
            }
            if (field_0x2a4 >= 0 && field_0x27c == 1) {
                mState = 4;
            }
            if (field_0x240 == 0 && dir == (s32)mDir_0x98 && dist <= sightRange) {
                mState = 4;
            }
        }
        break;

    case 2:
        if (field_0x238 == 0) {
            switch (mDir_0x98) {
            case 0:
                setVec_80154518(&field_0x2ac, field_0x290, 0.0f, 0.0f);
                break;
            case 1:
                setVec_80154518(&field_0x2ac, -field_0x290, 0.0f, 0.0f);
                break;
            case 2:
                setVec_80154518(&field_0x2ac, 0.0f, field_0x290, 0.0f);
                break;
            case 3:
                setVec_80154518(&field_0x2ac, 0.0f, -field_0x290, 0.0f);
                break;
            }
            field_0x258 = 0;
            field_0x238 = 20;
            if (field_0x23c < 10) {
                field_0x23c = 10;
            }
            field_0x244 = (s32)(20.0f * randFloat_80154744() + 20.0f);
            mState = 1;
        }
        break;

    case 3:
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 3;
            field_0x238 = 15;
            if (field_0x25c <= 0) {
                if (!fn_80154624(getLayer_8015461C())) {
                    if ((mDir_0x98 & 2) == 0) {
                        if (randFloat_80154744() < 0.5f) {
                            mDir_0x98 = 2;
                        } else {
                            mDir_0x98 = 3;
                        }
                    } else {
                        if (randFloat_80154744() < 0.5f) {
                            mDir_0x98 = 0;
                        } else {
                            mDir_0x98 = 1;
                        }
                    }
                } else {
                    mDir_0x98 &= 1;
                    mDir_0x98 ^= 1;
                }
                switch (mDir_0x98) {
                case 0:
                    setVec_80154518(&field_0x2ac, field_0x290, 0.0f, 0.0f);
                    break;
                case 1:
                    setVec_80154518(&field_0x2ac, -field_0x290, 0.0f, 0.0f);
                    break;
                case 2:
                    setVec_80154518(&field_0x2ac, 0.0f, field_0x290, 0.0f);
                    break;
                case 3:
                    setVec_80154518(&field_0x2ac, 0.0f, -field_0x290, 0.0f);
                    break;
                }
                field_0x23c = (s32)(50.0f * randFloat_80154744()) + 50;
                field_0x258 = 0;
                field_0x238 = 8;
                mState = 1;
                break;
            }
            if (field_0x258 == 0) {
                field_0x25c--;
            }
            break;
        }
        if (field_0x240 == 0 && dist <= 100.0f) {
            if (field_0x288 != 1 && field_0x288 != 3) {
                if (dir == lbl_8049FB78[mDir_0x98 * 4 + field_0x258]) {
                    mState = 4;
                }
                field_0x274 = lbl_8049FB38[mDir_0x98 * 4 + field_0x258];
            } else {
                field_0x274 = mDir_0x98 * 4;
            }
        }
        break;

    case 4:
        fn_801545A4(&oldPos, &mPos);
        oldPos.y -= 16.0f;
        spawnEmitter_8013CC50(mLayer_0x4, &oldPos, 0x33B, 0, 1, NULL, NULL, true, 1.0f);
        if (mSpawnParam_0x90 != 2 && mSpawnParam_0x90 != 3 && mSpawnParam_0x90 != 1) {
            startSound_801F0E34(0x40063, 0);
        } else {
            startSound_801F0E34(0x40064, 0);
        }
        lbl_8053AB44 = 1;
        zeroVec_801546BC(&field_0x2ac);
        field_0x238 = 0;
        field_0x23c = 20;
        startSound_801F0E34(0x40009, 0);
        mState = 5;
        if (field_0x2a4 >= 0 && field_0x27c == 1) {
            field_0x23c = 0;
        }
    case 5:
        if (dir != -1) {
            if (!fn_80154624(getLayer_8015461C())) {
                mDir_0x98 = dir;
            } else if ((dir & 2) == 0) {
                mDir_0x98 = dir;
            } else if (angle < 0x8000) {
                mDir_0x98 = 0;
            } else {
                mDir_0x98 = 1;
            }
        }
        if (field_0x23c == 0) {
            field_0x23c = (s32)(200.0f * randFloat_80154744()) + 50;
            field_0x240 = (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 20;
            if (field_0x284 == 0) {
                if (mSpawnParam_0x90 != 2 && mSpawnParam_0x90 != 3 && mSpawnParam_0x90 != 1) {
                    startSound_801F0E34(0x40065, 0);
                } else {
                    startSound_801F0E34(0x40066, 0);
                }
            }
            field_0x28c = 1.5f * field_0x290 + 0.2f * randFloat_80154744();
            mState = 6;
        }
        break;

    case 6:
        if (field_0x2a4 >= 0) {
            if (field_0x27c == 1 ||
                (playerIdx >= 0 && !fn_801F23BC(mLayer_0x4, &mPos, getPlayerPos_8023E724(playerIdx),
                                                &mMapBox_0x80, NULL, NULL, false))) {
                field_0x23c = 20;
            }
        }
        if (field_0x270 != 0) {
            field_0x23c = 10;
        }
        if (field_0x23c == 0) {
            for (s32 i = 0; i < 4; i++) {
                if (lbl_804BF850[i] == getSerial_801545E8()) {
                    lbl_804BF850[i] = -1;
                    field_0xbc = 2;
                }
            }
            if (mSpawnParam_0x90 == 6) {
                mState = 11;
            } else {
                setVec_80154518(&field_0x2ac, 0.0f, 0.0f, 0.0f);
                if (field_0x288 == 1 || field_0x288 == 3) {
                    mState = 19;
                } else {
                    field_0x258 = 0;
                    field_0x25c = 0;
                    field_0x238 = 20;
                    field_0x23c = (s32)(50.0f * randFloat_80154744()) + 50;
                    field_0x240 = (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 30;
                    mState = 1;
                }
            }
            break;
        }
        if (field_0x240 == 0) {
            field_0x240 = (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 13;
            if (playerIdx >= 0) {
                field_0x29e = (s16)((f32)angle + (4096.0f - 8192.0f * randFloat_80154744()));
            }
            if (dir >= 0) {
                if (!fn_80154624(getLayer_8015461C())) {
                    mDir_0x98 = dir;
                } else if ((dir & 2) == 0) {
                    mDir_0x98 = dir;
                } else if (angle < 0x8000) {
                    mDir_0x98 = 0;
                } else {
                    mDir_0x98 = 1;
                }
            }
            if (field_0x2a4 >= 0) {
                ActorBase_804A5770* leader = findActorBySerial_8015445C(field_0x2a4);
                if (leader != NULL) {
                    if (field_0x27c == 1) {
                        field_0x2a0 = 0;
                        f32 dx = leader->getPos_801544DC()->x - mPos.x;
                        f32 dy = leader->getPos_801544DC()->y - mPos.y;
                        if (sqrtf_80154378(dx * dx + dy * dy) > 40.0f) {
                            field_0x29e = (s16)atan2Short_8015465C(dx, dy);
                            field_0x29e += (s16)(4096.0f - 8192.0f * randFloat_80154744());
                        }
                    } else {
                        field_0x240 += (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 10;
                    }
                }
            }
            if ((field_0x2a4 < 0 || field_0x27c != 1) && field_0x94 == 4 && playerIdx >= 0) {
                zeroVec_801546BC(&field_0x2ac);
                if (field_0x2a0 == 0) {
                    field_0x2a0 = angle + 0x8000;
                }
                if ((field_0x278 & 1) == 0) {
                    field_0x2a0 += 0x100;
                } else {
                    field_0x2a0 -= 0x100;
                }
                u16 pAng = (u16)(dirToAngle_801F6874(getPlayerFacing_80230CFC(playerIdx)) + 0x8000);
                if (fabsf_8015436C(field_0x2a0 - pAng) < 4096.0f) {
                    dist = 0.0f;
                    field_0x2a0 = angle + 0x8000;
                }
                f32 tx = getPlayerPos_8023E724(playerIdx)->x + dist * sinShort_801544FC((s16)field_0x2a0);
                addCalc2_801F71A4(&mPos.x, tx, 0.2f, 1.0f);
                f32 ty = getPlayerPos_8023E724(playerIdx)->y + dist * cosShort_801544E4((s16)field_0x2a0);
                addCalc2_801F71A4(&mPos.y, ty, 0.2f, 1.0f);
                field_0x240 = 0;
                break;
            }
            if (field_0x254 == 0) {
                field_0x2ac.x = field_0x28c * sinShort_801544FC(field_0x29e);
                field_0x2ac.y = field_0x28c * cosShort_801544E4(field_0x29e);
            }
            if (field_0x2a4 != -2 &&
                (mSpawnParam_0x90 == 6 || (u32)(mSpawnParam_0x90 - 2) <= 1 || mSpawnParam_0x90 == 1) &&
                field_0x2a4 <= 0) {
                if ((mDir_0x98 & 2) == 0) {
                    field_0x2ac.y *= 0.5f;
                } else {
                    field_0x2ac.x *= 0.5f;
                }
            }
        }
        break;

    case 7:
        field_0x248 = 300;
        field_0x24c = 0;
        mState = 8;
    case 8:
        if (moved) {
            zeroVec_801546BC(&mVel_0x3c);
            field_0x29e = dirToAngle_801F6874(mDir_0x98);
            field_0x24c = 40;
            if (field_0x264 == 0) {
                if (randFloat_80154744() < 0.5f) {
                    field_0x29e -= 0x4000;
                } else {
                    field_0x29e += 0x4000;
                }
                field_0x264++;
            } else {
                field_0x264 = 0;
                field_0x29e += 0x8000;
            }
            field_0x238 = 8;
        }
        if (field_0x24c == 0) {
            field_0x29e = angle + 0x8000;
            field_0x264 = 0;
        }
        mDir_0x98 = angleToDir_801F681C(field_0x29e);
        field_0x2ac.x = 1.5f * sinShort_801544FC(field_0x29e);
        field_0x2ac.y = 1.5f * cosShort_801544E4(field_0x29e);
        addVec2_80154558(&boxD8.i, &pos70);
        addVec2_80154558(&boxD8.f, &pos70);
        fn_80154528(&boxD8, &vec68);
        if (field_0x248 == 0 || isOutsideView_801F6758(&mPos, getLayer_8015461C(), -24.0f)) {
            zeroVec_801546BC(&field_0x2ac);
            zeroVec_801546BC(&mVel_0x3c);
            field_0x238 = 2;
            field_0x23c = 30;
            if (field_0x248 == 0) {
                mState = 10;
            } else {
                mState = 9;
            }
            for (s32 i = 0; i < 4; i++) {
                if ((s32)mDir_0x98 == lbl_8049FA30[i]) {
                    field_0x258 = i;
                    break;
                }
            }
        }
        break;

    case 9:
        if (field_0x238 == 0) {
            field_0x238 = 2;
            mDir_0x98 = lbl_8049FA30[field_0x258];
            field_0x258++;
            field_0x258 &= 3;
            field_0x274 = lbl_8049FA78[mDir_0x98 * 8];
            if (field_0x288 == 1 || field_0x288 == 3) {
                field_0x274 = mDir_0x98 * 4;
            }
        }
        if (field_0x23c == 0) {
            addCalc2_801F71A4(&field_0x54.y, 5.0f, 0.3f, 3.0f);
            addCalc2_801F71A4(&field_0x54.x, 0.0f, 0.3f, 0.1f);
            if (field_0x54.x < 0.1f) {
                markDead_801F3668();
            }
        }
        break;

    case 10:
        if (field_0x238 == 0) {
            field_0x238 = 2;
            mDir_0x98 = lbl_8049FA30[field_0x258];
            field_0x258++;
            field_0x258 &= 3;
            field_0x274 = lbl_8049FA78[mDir_0x98 * 8];
            if (field_0x288 == 1 || field_0x288 == 3) {
                field_0x274 = mDir_0x98 * 4;
            }
        }
        if (field_0x23c == 0) {
            spawnActor_80154310('HBMB', getLayer_8015461C(), &mPos, 7, -1, -1);
            markDead_801F3668();
        }
        break;

    case 11:
        if (field_0x2a4 >= 0 && field_0x27c == 1) {
            mState = 4;
        }
        if (moved || field_0x23c == 0) {
            u8 changeDir = moved;
            zeroVec_801546BC(&mVel_0x3c);
            zeroVec_801546BC(&field_0x2ac);
            field_0x23c = (s32)(100.0f * randFloat_80154744()) + 50;
            if (!moved) {
                if (randFloat_80154744() < 0.7f) {
                    if (dir >= 0) {
                        if ((s32)mDir_0x98 == dir) {
                            break;
                        }
                        if (!fn_80154624(getLayer_8015461C())) {
                            mDir_0x98 = dir;
                        } else if ((dir & 2) == 0) {
                            mDir_0x98 = dir;
                        } else if (angle < 0x8000) {
                            mDir_0x98 = 0;
                        } else {
                            mDir_0x98 = 1;
                        }
                    }
                } else {
                    changeDir = 1;
                }
            }
            if (changeDir) {
                if (!fn_80154624(getLayer_8015461C())) {
                    if ((mDir_0x98 & 2) == 0) {
                        if (randFloat_80154744() < 0.5f) {
                            mDir_0x98 = 2;
                        } else {
                            mDir_0x98 = 3;
                        }
                    } else {
                        if (randFloat_80154744() < 0.5f) {
                            mDir_0x98 = 0;
                        } else {
                            mDir_0x98 = 1;
                        }
                    }
                } else {
                    mDir_0x98 &= 1;
                    mDir_0x98 ^= 1;
                }
                field_0x238 = 8;
            }
        }
        switch (mDir_0x98) {
        case 0:
            setVec_80154518(&field_0x2ac, field_0x290, 0.0f, 0.0f);
            break;
        case 1:
            setVec_80154518(&field_0x2ac, -field_0x290, 0.0f, 0.0f);
            break;
        case 2:
            setVec_80154518(&field_0x2ac, 0.0f, field_0x290, 0.0f);
            break;
        case 3:
            setVec_80154518(&field_0x2ac, 0.0f, -field_0x290, 0.0f);
            break;
        }
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 7;
            field_0x238 = lbl_8049FAF8[field_0x258];
            field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                if ((field_0x258 & 1) == 0) {
                    field_0x238 = 4;
                } else {
                    field_0x238 = 6;
                }
                field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
            }
        }
        break;

    case 12:
        if ((playerIdx >= 0 && dist <= 40.0f) || lbl_8053AB44 != 0) {
            lbl_8053AB44 = 1;
            fn_80154614(1);
            mState = 4;
        }
        break;

    case 13:
        zeroVec_801546BC(&field_0x2ac);
        zeroVec_801546BC(&mVel_0x3c);
        field_0x294 = 0.0f;
        field_0x238 = 0;
        field_0x23c = 30;
        field_0xb0 = 0;
        mLife = 10000;
        mState = 14;
    case 14:
        fn_801544D0(0);
        addCalc2_801F71A4(&mPos.x, field_0x2b8.x, 0.1f, 0.5f);
        addCalc2_801F71A4(&mPos.y, field_0x2b8.y, 0.1f, 0.5f);
        {
            f32 dx = mPos.x - field_0x2b8.x;
            f32 dy = mPos.y - field_0x2b8.y;
            if (field_0x23c == 0 && sqrtf_80154378(dx * dx + dy * dy) < 2.0f) {
                if (field_0x274 != 24) {
                    startSound_801F0E34(0x4001A, 0);
                }
                field_0x274 = 24;
                field_0x294 += 0.1f;
                if (fn_802DEB58() == 1) {
                    if (field_0x294 > 1.0f) {
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x2A9, -1, 1, NULL, NULL, true, 1.0f);
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x2AA, -1, 0, NULL, NULL, true, 1.0f);
                        startSound_801F0E34(0x30047, 0);
                        markDead_801F3668();
                    }
                    return;
                }
                field_0x298 -= 0.02f;
                if (field_0x298 < 0.1f) {
                    if (field_0x280 == 1) {
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x2A9, -1, 1, NULL, NULL, true,
                                              1.0f);
                        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x2AA, -1, 0, NULL, NULL, true,
                                              1.0f);
                        startSound_801F0E34(0x5005F, 0);
                    }
                    markDead_801F3668();
                }
                break;
            }
        }
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 7;
            field_0x238 = (s32)(0.5f * (f32)lbl_8049FAF8[field_0x258]);
            field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                if ((field_0x258 & 1) == 0) {
                    field_0x238 = 4;
                } else {
                    field_0x238 = 6;
                }
                field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
            }
        }
        break;

    case 15:
        {
            ActorBase_804A5770* actors[1024];
            s32 count = fn_801542AC('SLR4', getLayer_8015461C(), actors, -1);
            for (s32 i = 0; i < count; i++) {
                ChiefSoldier_804A0B90* chief = (ChiefSoldier_804A0B90*)actors[i];
                if (chief != NULL && field_0x2a4 == chief->fn_801544AC()) {
                    chief->fn_8015449C();
                    field_0x2a4 = chief->getSerial_801545E8();
                    if (chief->getSpawnParam_801544C8() == 1 || chief->getSpawnParam_801544C8() == 3) {
                        field_0x2a8 = 1;
                    }
                    mState = 1;
                    if (mSpawnParam_0x90 == 6) {
                        mState = 11;
                    }
                    if (field_0x94 >= 6 && field_0x94 <= 9) {
                        mState = 16;
                        chief->fn_80154490();
                    }
                    if (field_0x288 == 1 || field_0x288 == 3) {
                        field_0x238 = (s32)(60.0f * randFloat_80154744()) + 60;
                        mState = 17;
                    }
                    if (mSpawnParam_0x90 == 1) {
                        mState = 12;
                    }
                    return;
                }
            }
            field_0x26c++;
            if (field_0x26c >= 3) {
                field_0x25c = 0;
                field_0x2a4 = -1;
                field_0x94 = 0;
                mState = 1;
                if (mSpawnParam_0x90 == 6) {
                    mState = 11;
                }
            }
        }
        break;

    case 16:
        startSound_801F0E34(0x4006A, 0);
        if (field_0x238 == 0 && field_0x27c != 3) {
            field_0x258++;
            field_0x258 &= 7;
            field_0x238 = lbl_8049FAF8[field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                if ((field_0x258 & 1) == 0) {
                    field_0x238 = 4;
                } else {
                    field_0x238 = 6;
                }
            }
        }
        field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
        if (field_0x288 == 1 || field_0x288 == 3) {
            field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
        }
        if (field_0x2a4 == -2 || field_0x27c == 2) {
            field_0x258 = 0;
            field_0x25c = 0;
            field_0x260 = 0;
            field_0x238 = 0;
            field_0x23c = 0;
            field_0x240 = 0;
            field_0x280 = 1;
            if (mSpawnParam_0x90 == 6) {
                field_0xb0 = 0x62209;
                mState = 11;
            } else if (mSpawnParam_0x90 == 1) {
                field_0xb0 = 0x12205;
                mState = 12;
            } else {
                field_0xb0 = 0x12205;
                mState = 1;
            }
            field_0x94 = 0;
            fn_80154614(1);
            break;
        }
        {
            JGeometry::TBox2f box78;
            copyBox_801546F4(&box78, &mMapBox_0x80);
            JGeometry::TVec2f tmp18;
            JGeometry::TVec2f tmp40;
            fn_8015457C(&tmp40, fn_801546E8(&tmp18, mPos.x, mPos.y));
            addVec2_80154558(&box78.i, &tmp40);
            addVec2_80154558(&box78.f, &tmp40);
            fn_80239914(&box78, getLayer_8015461C(), getSerial_801545E8());
        }
        if (field_0x27c == 0 && field_0x25c != (s32)mDir_0x98) {
            zeroVec_801546BC(&field_0x2ac);
            zeroVec_801546BC(&mVel_0x3c);
            if (field_0x268 == 0) {
                field_0x268 = 1;
                field_0x23c = 30;
            }
            if (field_0x23c == 0) {
                if (field_0x260 == 0) {
                    mDir_0x98 ^= 2;
                    field_0x260 = 1;
                } else {
                    mDir_0x98 = field_0x25c;
                    field_0x260 = 0;
                }
                field_0x23c = 10;
            }
            break;
        }
        if (field_0x27c == 1 && (field_0x25c ^ 1) != (s32)mDir_0x98) {
            zeroVec_801546BC(&field_0x2ac);
            zeroVec_801546BC(&mVel_0x3c);
            if (field_0x268 != 0) {
                field_0x268 = 0;
                field_0x23c = 30;
            }
            if (field_0x23c == 0) {
                if (field_0x260 == 0) {
                    mDir_0x98 ^= 2;
                    field_0x260 = 1;
                } else {
                    mDir_0x98 = field_0x25c ^ 1;
                    field_0x260 = 0;
                }
                field_0x23c = 10;
            }
            break;
        }
        if (field_0x27c == 3) {
            zeroVec_801546BC(&field_0x2ac);
            zeroVec_801546BC(&mVel_0x3c);
            break;
        }
        switch (mDir_0x98) {
        case 0:
            setVec_80154518(&field_0x2ac, field_0x290, 0.0f, 0.0f);
            break;
        case 1:
            setVec_80154518(&field_0x2ac, -field_0x290, 0.0f, 0.0f);
            break;
        case 2:
            setVec_80154518(&field_0x2ac, 0.0f, field_0x290, 0.0f);
            break;
        case 3:
            setVec_80154518(&field_0x2ac, 0.0f, -field_0x290, 0.0f);
            break;
        }
        break;

    case 17:
        for (s32 i = 0; i < 4; i++) {
            if (isPlayerActive_8023F264(i) && !fn_80230188(i) && !fn_8022DCD8(i) &&
                getLayer_8015461C() == getPlayerLayer_8023DE58(i) && !fn_8022D534(i)) {
                f32 dx = mPos.x - getPlayerPos_8023E724(i)->x;
                f32 dy = mPos.y - getPlayerPos_8023E724(i)->y;
                if (sqrtf_80154378(dx * dx + dy * dy) < 48.0f) {
                    field_0x258 = 0;
                    field_0x238 = lbl_8049FC7C[field_0x258];
                    field_0x274 = lbl_8049FC58[field_0x258];
                    mState = 18;
                    break;
                }
            }
        }
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 3;
            if (field_0x258 == 0) {
                field_0x238 = (s32)(60.0f * randFloat_80154744()) + 60;
            } else {
                field_0x238 = 2;
            }
            field_0x274 = field_0x258 + 16;
        }
        break;

    case 18:
        if (field_0x238 == 0) {
            field_0x258++;
            if (field_0x258 > 8) {
                field_0xb0 = 0x12205;
                field_0x258 = 0;
                field_0x238 = 0;
                fn_80154614(1);
                if (mSpawnParam_0x90 == 6) {
                    field_0xb0 = 0x62209;
                    mState = 11;
                } else {
                    field_0xb0 = 0x12205;
                    field_0x23c = 300;
                    field_0x23c += (s32)(100.0f * randFloat_80154744());
                    field_0x240 = (s32)((f32)(field_0x278 & 7) * randFloat_80154744()) + 20;
                    field_0x28c = 1.5f * field_0x290 + 0.2f * randFloat_80154744();
                    mState = 6;
                }
                if (field_0x94 >= 6 && field_0x94 <= 9) {
                    mState = 16;
                }
            } else {
                field_0x238 = lbl_8049FC7C[field_0x258];
                field_0x274 = lbl_8049FC58[field_0x258];
            }
        }
        break;

    case 19:
        field_0x258 = 8;
        field_0x238 = lbl_8049FC7C[field_0x258];
        field_0x274 = lbl_8049FC58[field_0x258];
        fn_80154614(0);
        field_0xb0 = 0xA042004;
        mState = 20;
    case 20:
        if (field_0x238 == 0) {
            field_0x258--;
            if (field_0x258 < 0) {
                field_0x238 = 0;
                field_0x258 = 0;
                spawnActor_80154310('HBMB', getLayer_8015461C(), &mPos, 7, -1, -1);
                markDead_801F3668();
                if (field_0x288 == 1) {
                    fn_80154248('RUPY', getLayer_8015461C(), &mPos, 0x30000);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3D9, 0, 1, NULL, NULL, true, 1.0f);
                } else {
                    fn_80154248('RUPY', getLayer_8015461C(), &mPos, 0x30004);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x3D9, 0, 1, NULL, NULL, true, 1.5f);
                }
                field_0x288 = 0;
            } else {
                field_0x238 = lbl_8049FC7C[field_0x258];
                field_0x274 = lbl_8049FC58[field_0x258];
            }
        }
        break;
    }

    if (field_0x238 > 0) {
        field_0x238--;
    }
    if (field_0x23c > 0) {
        field_0x23c--;
    }
    if (field_0x240 > 0) {
        field_0x240--;
    }
    if (field_0x244 > 0) {
        field_0x244--;
    }
    if (field_0x248 > 0) {
        field_0x248--;
    }
    if (field_0x24c > 0) {
        field_0x24c--;
    }
    if (field_0x250 > 0) {
        field_0x250--;
    }
    if (field_0x254 > 0) {
        field_0x254--;
    }

    if (mState == 14) {
        return;
    }
    if (mState == 4 || mState == 5 || mState == 6 || mState == 8) {
        if (moved && mState == 8) {
            return;
        }
        if (field_0x238 == 0) {
            field_0x258++;
            field_0x258 &= 7;
            field_0x238 = (s32)(0.5f * (f32)lbl_8049FAF8[field_0x258]);
            field_0x274 = lbl_8049FA78[mDir_0x98 * 8 + field_0x258];
            if (field_0x288 == 1 || field_0x288 == 3) {
                if ((field_0x258 & 1) == 0) {
                    field_0x238 = 4;
                } else {
                    field_0x238 = 6;
                }
                field_0x274 = mDir_0x98 * 4 + (field_0x258 & 3);
            }
        }
    }
}

// FIXME(shotgun): 96.05 %, missing/extra + reloc-name; behaviour near.
void SwordSoldier_8049FCA0::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    s32 flags = 0;
    if (field_0x94 == 2) {
        return;
    }
    params.field_0x0[2] = field_0x234;
    if (field_0x288 != 1 && field_0x288 != 3) {
        if (mSpawnParam_0x90 == 2 || mSpawnParam_0x90 == 3 || mSpawnParam_0x90 == 1) {
            params.field_0x0[8] = field_0x234;
        }
        if (field_0x94 >= 6 && field_0x94 <= 9) {
            params.field_0x0[2] = 11;
        }
        if (mState == 0xE) {
            JGeometry::TVec3f rot(0.0f, field_0x294, 0.0f);
            setRotScaleMtx(mtx, rot, field_0x298);
            params.mMtx = mtx;
            if (mSpawnParam_0x90 == 2 || mSpawnParam_0x90 == 3 || mSpawnParam_0x90 == 1) {
                drawSpriteScaled_801F02BC(lbl_80465DBC, field_0x274 + 0x1F4, &params, 0, 0, false, 1.0f, 1.0f);
            } else {
                drawSpriteScaled_801F02BC(lbl_80465DD0, field_0x274 + 0x8F, &params, 0, 0, false, 1.0f, 1.0f);
            }
            return;
        }
    }
    if (mSpawnParam_0x90 == 6 || field_0x288 == 3) {
        mtx[0][0] = field_0x54.x;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = field_0x54.y;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = field_0x54.z;
        params.mMtx = mtx;
        flags = 3;
    }
    if (mState == 8) {
        bool blink = false;
        s32 t = field_0x248;
        if (t < 100) {
            if (t < 50) {
                if ((t & 4) == 0) {
                    blink = true;
                }
            } else {
                if ((t & 8) == 0) {
                    blink = true;
                }
            }
        }
        if (blink) {
            params.field_0x14.r = 0x50;
        }
    }
    if (mState == 9) {
        mtx[0][0] = field_0x54.x;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = field_0x54.y;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = field_0x54.z;
        params.mMtx = mtx;
    }
    s32 far = 0;
    if (mLayer_0x4 >= 8 && (mSpawnParam_0x90 == 6 || field_0x288 == 3)) {
        far = 1;
    }
    if (field_0x288 == 1 || field_0x288 == 3) {
        drawSpriteScaled_801F02BC(lbl_80465DD0, field_0x274 + 0xA8, &params, flags, far, false, 1.0f, 1.0f);
    } else if (mSpawnParam_0x90 == 2 || mSpawnParam_0x90 == 3 || mSpawnParam_0x90 == 1) {
        drawSpriteScaled_801F02BC(lbl_80465DBC, field_0x274 + 0x1F4, &params, flags, far, false, 1.0f, 1.0f);
    } else {
        drawSpriteScaled_801F02BC(lbl_80465DD0, field_0x274 + 0x8F, &params, flags, far, false, 1.0f, 1.0f);
    }
}

