#include "game/GameAudio.h"
#include "game/act/act_extending_lift.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "JSystem/ResTIMG.h"

static const char* lbl_8053A198[] = {"lift_00.bti", NULL};

static inline void setRotScaleMtxS16(Mtx m, s16 x, s16 y, s16 z, f32 scaleX, f32 scaleY, f32 scaleZ) {
    f32 sx = JMath::sincosTable_.sinShort(x);
    f32 sy = JMath::sincosTable_.sinShort(y);
    f32 sz = JMath::sincosTable_.sinShort(z);
    f32 cx = JMath::sincosTable_.cosShort(x);
    f32 cy = JMath::sincosTable_.cosShort(y);
    f32 cz = JMath::sincosTable_.cosShort(z);
    m[0][0] = scaleX * (cy * cz);
    m[1][0] = scaleX * (cy * sz);
    m[2][0] = scaleX * -sy;
    m[0][1] = scaleY * (sx * sy * cz - cx * sz);
    m[1][1] = scaleY * (sx * sy * sz + cx * cz);
    m[2][1] = scaleY * (sx * cy);
    m[0][2] = scaleZ * (cx * cz * sy + sx * sz);
    m[1][2] = scaleZ * (cx * sz * sy - sx * cz);
    m[2][2] = scaleZ * (cx * cy);
}

static inline bool contains(const JGeometry::TBox2f& b, const JGeometry::TVec3f& p) {
    return p.x >= b.i.x && p.y >= b.i.y && p.x < b.f.x && p.y < b.f.y;
}

inline bool ExtendingLift_804A78B0::isPlayerOn(s32 player) {
    if (getPlayerLayer_8023DE58(player) != getField_0x4()) {
        return false;
    }
    for (s32 j = 0; j < 8; j++) {
        JGeometry::TVec3f pt;
        fn_8022E088(player, j, &pt);
        pt -= mPos;
        pt.z = 0.0f;
        if (contains(mBox, pt)) {
            fn_8022DF48(player);
            return true;
        }
    }
    return false;
}

// FIXME: 97.85 %, missing/extra; behaviour near.
// FIXME: The flags test loads 0x90 into r3 and reuses it; the original loads it into r0 for the test and reloads 0x90(r31) for ori/oris.
void ExtendingLift_804A78B0::init() {
    if (!((mSpawnParam_0x90 >> 7) & 0x1F)) {
        mSpawnParam_0x90 |= 0x80;
    }
    if (!((mSpawnParam_0x90 >> 17) & 0x1F)) {
        mSpawnParam_0x90 |= 0x20000;
    }

    field_0x240 = static_cast<const ResTIMG*>(gGameRoot_8053AAF8->field_0xa4->getResource('????', lbl_8053A198[0]));
    mLife = 0;

    switch ((mSpawnParam_0x90 >> 12) & 0x1F) {
    case 0:
        break;
    case 1:
        mPos.x += 16.0f;
        break;
    case 2:
        break;
    case 3:
        mPos.y += 16.0f;
        break;
    }

    if ((s32)((mSpawnParam_0x90 >> 12) & 0x1F) < 2) {
        field_0x238.set(0.0f, 16.0f * (s32)((mSpawnParam_0x90 >> 7) & 0x1F));
    } else {
        field_0x238.set(16.0f * (s32)((mSpawnParam_0x90 >> 7) & 0x1F), 0.0f);
    }

    field_0x230.set(field_0x238.x / field_0x240->mSizeX, field_0x238.y / field_0x240->mSizeY);
    field_0x244 = 0;
}

// FIXME: 92.98 %, missing/extra; behaviour clean.
// FIXME: missing/extra (2 extra instrs): stack frame is 16 bytes larger, mBox bounds in contains() are loaded lazily instead of upfront, and the field_0x230/0x234 divides are scheduled differently.
void ExtendingLift_804A78B0::update() {
    if (isScreenFlagSet_0x34()) {
        if (!field_0x244) {
            startSound_801F0E34(0x30041, 0);
        }
        field_0x244 = 1;
    } else {
        if (!(mSpawnParam_0x90 & 0x7F)) {
            if (field_0x244 == 1) {
                startSound_801F0E34(0x30041, 0);
            }
            field_0x244 = 0;
        }
    }

    if (field_0x244) {
        if ((s32)((mSpawnParam_0x90 >> 12) & 0x1F) < 2) {
            field_0x238.x += 1.0f;
            if (field_0x238.x >= 16 * (s32)((mSpawnParam_0x90 >> 17) & 0x1F)) {
                field_0x238.x = 16 * (s32)((mSpawnParam_0x90 >> 17) & 0x1F);
            }
        } else {
            field_0x238.y += 1.0f;
            if (field_0x238.y >= 16 * (s32)((mSpawnParam_0x90 >> 17) & 0x1F)) {
                field_0x238.y = 16 * (s32)((mSpawnParam_0x90 >> 17) & 0x1F);
            }
        }
    } else {
        if ((s32)((mSpawnParam_0x90 >> 12) & 0x1F) < 2) {
            field_0x238.x -= 1.0f;
            if (field_0x238.x <= 0.0f) {
                field_0x238.x = 0.0f;
            }
        } else {
            field_0x238.y -= 1.0f;
            if (field_0x238.y <= 0.0f) {
                field_0x238.y = 0.0f;
            }
        }
    }

    JGeometry::TVec3f oldCenter;
    oldCenter.x = (mBox.i.x - mBox.f.x) * 0.5f + mBox.f.x;
    oldCenter.y = (mBox.i.y - mBox.f.y) * 0.5f + mBox.f.y;
    oldCenter.z = 0.0f;

    switch ((mSpawnParam_0x90 >> 12) & 0x1F) {
    case 0:
        mBox.set(0.0f, 3.0f, field_0x238.x, field_0x238.y - 3.0f);
        break;
    case 1:
        mBox.set(-field_0x238.x, 3.0f, 0.0f, field_0x238.y - 3.0f);
        break;
    case 2:
        mBox.set(3.0f, 0.0f, field_0x238.x - 3.0f, field_0x238.y);
        break;
    case 3:
        mBox.set(3.0f, -field_0x238.y, field_0x238.x - 3.0f, 0.0f);
        break;
    }

    field_0x230.set(field_0x238.x / field_0x240->mSizeX, field_0x238.y / field_0x240->mSizeY);

    JGeometry::TVec3f newCenter;
    newCenter.x = (mBox.i.x - mBox.f.x) * 0.5f + mBox.f.x;
    newCenter.y = (mBox.i.y - mBox.f.y) * 0.5f + mBox.f.y;
    newCenter.z = 0.0f;

    for (s32 i = 0; i < 4; i++) {
        if (isPlayerOn(i)) {
            if (!(newCenter - oldCenter).isZero()) {
                JGeometry::TVec3f delta = newCenter - oldCenter;
                fn_8023DF68(i, &delta);
            }
        }
    }
}

// FIXME: 96.53 %, scheduling+regalloc; behaviour clean.
// instruction scheduling and register allocation in setRotScaleMtxS16 and sprite setup.
void ExtendingLift_804A78B0::draw() {
    SpriteParams_801F5AE0 params;
    params.field_0x1d = -1;

    Mtx mtx;
    switch ((mSpawnParam_0x90 >> 12) & 0x1F) {
    case 0:
        PSMTXTrans(mtx,
                   (field_0x240->mSizeX * field_0x230.x) / 2.0f,
                   0.0f,
                   (-(s32)field_0x240->mSizeY * field_0x230.y) / 2.0f);
        break;
    case 1:
        PSMTXTrans(mtx,
                   (-(s32)field_0x240->mSizeX * field_0x230.x) / 2.0f,
                   0.0f,
                   (-(s32)field_0x240->mSizeY * field_0x230.y) / 2.0f);
        break;
    case 2:
        PSMTXTrans(mtx,
                   (field_0x240->mSizeX * field_0x230.x) / 2.0f,
                   0.0f,
                   (-(s32)field_0x240->mSizeY * field_0x230.y) / 2.0f);
        break;
    case 3:
        PSMTXTrans(mtx,
                   (field_0x240->mSizeX * field_0x230.x) / 2.0f,
                   0.0f,
                   (field_0x240->mSizeY * field_0x230.y) / 2.0f);
        break;
    default:
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;
        break;
    }

    setRotScaleMtxS16(mtx, 0, 0, 0, field_0x230.x, 1.0f, field_0x230.y);

    params.mMtx = mtx;
    params.field_0x21 = 1;
    params.field_0x1d = -2;

    const char* name = lbl_8053A198[0];
    if (mLayer_0x4 < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('TIMG', name);
        gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&mPos, res, &params);
    }
}

ExtendingLift_804A78B0::~ExtendingLift_804A78B0() {}
