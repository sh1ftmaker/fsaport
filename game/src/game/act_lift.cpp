#include "game/GameAudio.h"
#include "game/act/act_lift.h"
#include "game/GameGlobals.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "JSystem/ResTIMG.h"

// The "TIMG" texture names by mode (0x90 & 0x7F).
const char* lbl_804A6F48[9] = {
    "lift_00.bti", "lift_01.bti", "lift_01_ana.bti", "lift_01_edge.bti", "lift_02a.bti",
    "lift_03.bti", "lift_00.bti", "lift_00.bti",     "lift_00.bti",
};

// The rotation of game/RotMatrix.h's setRotScaleMtx, with the angles as s16
// read from JMath's sincos table.
static inline void setRotScaleMtxS16(Mtx m, s16 x, s16 y, s16 z, f32 scale) {
    f32 sx = JMath::sincosTable_.sinShort(x);
    f32 sy = JMath::sincosTable_.sinShort(y);
    f32 sz = JMath::sincosTable_.sinShort(z);
    f32 cx = JMath::sincosTable_.cosShort(x);
    f32 cy = JMath::sincosTable_.cosShort(y);
    f32 cz = JMath::sincosTable_.cosShort(z);
    m[0][0] = scale * (cy * cz);
    m[1][0] = scale * (cy * sz);
    m[2][0] = scale * -sy;
    m[0][1] = scale * (sx * sy * cz - cx * sz);
    m[1][1] = scale * (sx * sy * sz + cx * cz);
    m[2][1] = scale * (sx * cy);
    m[0][2] = scale * (cx * cz * sy + sx * sz);
    m[1][2] = scale * (cx * sz * sy - sx * cz);
    m[2][2] = scale * (cx * cy);
}

// r3/r4 swapped for res and res->mSizeX in mBox.set.
// FIXME: 99.74 %, regalloc-only; behaviour clean.
void Lift_804A6F6C::init() {
    if (((getField_0x90() >> 17) & 0x1F) == 0) {
        mSpawnParam_0x90 |= 0x20000;
    }
    field_0x23c = gGameRoot_8053AAF8->field_0xa4->getResource(0x3F3F3F3F /* "????": a char constant would be a trigraph */, lbl_804A6F48[getMode()]);
    ResTIMG* res = static_cast<ResTIMG*>(field_0x23c);
    mBox.set(-res->mSizeX / 2, -res->mSizeY / 2, res->mSizeX / 2, res->mSizeY / 2);
    mLife = 0;
    field_0x230 = 0;
    field_0x238 = 0;
    field_0x23a = 0;
    if (getMode() == 8) {
        mPos.x = mSpawnPos.x + 64.0f * JMath::sincosTable_.cosShort(field_0x23a);
        mPos.y = mSpawnPos.y + 64.0f * JMath::sincosTable_.sinShort(field_0x23a);
    }
    if (getMode() == 6 || getMode() == 7) {
        field_0x234 = 60;
    } else {
        field_0x234 = 0;
    }
    if (getMode() == 7) {
        mDir_0x98 = 3;
    } else {
        mDir_0x98 = 0;
    }
}

// FIXME(shotgun): 84.92 %, missing/extra; behaviour drift.
// the player-position vector arithmetic, the atan2 argument order and the tile-index math are scheduled differently
bool Lift_804A6F6C::fn_802565BC(s32 index) {
    if (mLayer_0x4 != getPlayerLayer_8023DE58(index)) {
        return false;
    }
    for (s32 i = 0; i < 8; i++) {
        JGeometry::TVec3f p;
        fn_8022E088(index, i, &p);
        p -= mPos;
        p.z = 0.0f;
        s32 angle = JMath::atanTable_.atan2_(p.y, p.x) * (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180());
        angle -= field_0x238;
        f32 len = JGeometry::TUtil<f32>::sqrt(p.x * p.x + p.y * p.y);
        p.set(len * JMath::sincosTable_.cosShort(angle), len * JMath::sincosTable_.sinShort(angle), 0.0f);
        bool inside = false;
        if (p.x >= mBox.i.x && p.y >= mBox.i.y && p.x < mBox.f.x && p.y < mBox.f.y) {
            inside = true;
        }
        if (inside) {
            ResTIMG* res = static_cast<ResTIMG*>(field_0x23c);
            s32 px = (s32)p.x + res->mSizeX / 2;
            s32 py = (s32)p.y + res->mSizeY / 2;
            bool hit = false;
            if (px >= 0 && px < res->getWidth() && py >= 0 && py < res->getHeight()) {
                switch (res->mTextureFormat) {
                case 8:
                    switch (res->mLutFormat) {
                    case 2: {
                        s32 tilesPerRow = ((res->getWidth() + 7) & 0xFFF8) >> 3;
                        s32 offset = (((px >> 3) + (py >> 3) * tilesPerRow) << 6) + ((py & 7) << 3) + (px & 7);
                        const u8* base = (const u8*)res;
                        const u8* image = base + res->mImageDataOffset;
                        u8 byte = image[offset >> 1];
                        u32 paletteIndex = (px & 1) ? (byte & 7) : ((byte >> 4) & 7);
                        const u16* palette = (const u16*)(base + res->mPaletteOffset);
                        u16 color = palette[paletteIndex & 0xFF];
                        if ((color & 0x8000) || (color & 0xF000) == 0x7000) {
                            hit = true;
                        }
                        break;
                    }
                    case 1:
                        hit = true;
                        break;
                    }
                    break;
                }
            }
            if (hit) {
                fn_8022DF48(index);
                return true;
            }
        }
    }
    return false;
}

void Lift_804A6F6C::fn_80256410() {
    if (field_0x234 != 0) {
        field_0x234--;
        return;
    }
    if (getMode() == 6 || getMode() == 7) {
        u32 limit = (mSpawnParam_0x90 & 0x3E0000) >> 12;
        if (field_0x230 < limit) {
            field_0x230++;
        } else {
            field_0x234 = 60;
            field_0x230 = 0;
            switch (mDir_0x98) {
            case 0:
                mDir_0x98 = 1;
                break;
            case 1:
                mDir_0x98 = 0;
                break;
            case 2:
                mDir_0x98 = 3;
                break;
            case 3:
                mDir_0x98 = 2;
                break;
            }
        }
    }
    if (getMode() < 6) {
        field_0x238 += 0x40;
    }
    if (getMode() == 6) {
        if (mDir_0x98 == 0) {
            mPos.x += 0.5f;
        } else {
            mPos.x -= 0.5f;
        }
    }
    if (getMode() == 7) {
        if (mDir_0x98 == 2) {
            mPos.y += 0.5f;
        } else {
            mPos.y -= 0.5f;
        }
    }
    if (getMode() != 8) {
        return;
    }
    field_0x23a += 0x40;
    mPos.x = mSpawnPos.x + 64.0f * JMath::sincosTable_.cosShort(field_0x23a);
    mPos.y = mSpawnPos.y + 64.0f * JMath::sincosTable_.sinShort(field_0x23a);
}

// Stack slot order and f4/f5 regalloc around sqrt/sincos still differ.
// FIXME: 99.17 %, operands; behaviour clean.
void Lift_804A6F6C::update() {
    if (mSpawnParam_0x90 >> 27 == 0 || isScreenFlagSet_0x34() == true) {
        fn_80256410();
    }
    for (s32 i = 0; i < 4; i++) {
        if (!fn_802565BC(i)) {
            continue;
        }
        if (mSpawnParam_0x90 >> 27 != 0 && isScreenFlagSet_0x34() != true) {
            continue;
        }
        fn_8023DF68(i, &(mPos - mPrevPos));
        if (getMode() >= 6) {
            continue;
        }
        JGeometry::TVec3f d = mPos - *getPlayerPos_8023E724(i);
        d.z = 0.0f;
        s32 angle = JMath::atanTable_.atan2_(d.y, d.x) * (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180());
        f32 len = JGeometry::TUtilf::sqrt(d.x * d.x + d.y * d.y);
        s16 a = angle - 0x40;
        JGeometry::TVec3f v(len * JMath::sincosTable_.cosShort(a), len * JMath::sincosTable_.sinShort(a), 0.0f);
        fn_8023DFF4(i, &(v - d));
    }
}

// Float regalloc in setRotScaleMtxS16 (f8-f12) still differs.
// FIXME: 99.23 %, regalloc-only; behaviour clean.
void Lift_804A6F6C::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    setRotScaleMtxS16(mtx, 0, field_0x238, 0, 1.0f);
    params.mMtx = mtx;
    params.field_0x21 = 1;
    params.field_0x1d = -2;
    const char* name = lbl_804A6F48[getMode()];
    if (mLayer_0x4 < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('TIMG', name);
        gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&mPos, res, &params);
    }
}

Lift_804A6F6C::~Lift_804A6F6C() {}
