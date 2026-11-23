#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/EnemyBase.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "std/math.h"
#include "game/RotMatrix.h"
#include "game/SpriteParams.h"
#include "game/act/act_fish.h"

// FIXME: 97.32 %, different + reloc-name; behaviour clean.
// FIXME(shotgun): different + reloc-name (register order and literal placement in the spawn loop)
void Fish_804AC658::init() {
    u32 param = mSpawnParam_0x90;
    mSpawnParam_0x90 = param & 0xFFF;
    mLife = 10000;
    s32 base = (param >> 12) & 0xFF;
    s32 spread = (param >> 20) & 0xFF;
    mBox.i.x = -4.0f;
    mBox.i.y = -4.0f;
    mBox.f.x = 4.0f;
    mBox.f.y = 4.0f;
    mMapBox_0x80.i.x = -4.0f;
    mMapBox_0x80.i.y = -4.0f;
    mMapBox_0x80.f.x = 4.0f;
    mMapBox_0x80.f.y = 4.0f;
    field_0x240 = 0;
    field_0x244 = 0;
    field_0x248 = 0;
    field_0x234 = 0;
    field_0x238 = 0;
    field_0x23c = 0;
    field_0x264 = 0.0f;
    field_0x268 = 0;
    field_0x26c = -1;
    field_0x270 = -1;
    isOutsideView_801F6758(&mPos, mLayer_0x4, 0.0f);
    if (mSpawnParam_0x90 != 0) {
        s32 count = mSpawnParam_0x90;
        u16 tiles[0x2C9];
        s32 layer = mLayer_0x4;
        GameView_804A9950* view = getGameView_802D6A9C();
        const JGeometry::TBox2f* box = layer < 8 ? &view->mVisibleBox : &view->field_0xdc;
        f32 x0 = box->i.x;
        f32 y0 = box->i.y;
        s32 width = (s32)(box->f.x - x0);
        s32 height = (s32)(box->f.y - y0);
        s32 n = 0;
        s32 xi = 1;
        for (s32 x = 16; x < width; x += 16) {
            s32 packed = xi + 0x20;
            for (s32 y = 16; y < height; y += 16) {
                JGeometry::TVec3f p;
                p.x = x + x0;
                p.y = y + y0;
                s32 tile = getTileCollisionCodeAlt_802265FC(&p, mLayer_0x4);
                if (tile == 0xD || tile == 0x3A) {
                    if (n >= 0x2C9) {
                        return;
                    }
                    tiles[n] = packed;
                    n++;
                }
                packed += 0x20;
            }
            xi++;
        }
        if (n == 0) {
            return;
        }
        {
            s32 spawned = 0;
            while (spawned < count) {
                s32 idx = (s32)(n * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                s32 size = base + (s32)((spread + 1) * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                u16 tile = tiles[idx];
                if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() > 0.8f && count - spawned > size) {
                } else {
                    size = 1;
                }
                f32 cx = (tile & 0x1F) * 16 + x0;
                f32 cy = (tile >> 5) * 16 + y0;
                for (s32 j = 0; j < size; j++) {
                    f32 ry = gGameRoot_8053AAF8->mRandom.get_ufloat_1();
                    f32 rx = gGameRoot_8053AAF8->mRandom.get_ufloat_1();
                    JGeometry::TVec3f pos(16.0f * rx + cx, 16.0f * ry + cy, 0.0f);
                    getActorManager_801F856C()->spawnActor_801F9484(ACTOR_FISH, mLayer_0x4, &pos, 0, -1, -1);
                    spawned++;
                }
            }
        }
    }
    field_0x250 = (s16)(65536.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
    field_0x252 = field_0x250 + 0x4000;
    field_0x24c = 0;
    field_0x250 = 0;
    field_0x25c = 0.0f;
    field_0x258 = 0.0f;
    field_0x254 = 0.0f;
    field_0x260 = 0.3f;
    field_0x230 = 0;
}

bool Fish_804AC658::isBlocked_0x4C(s32 layer, const JGeometry::TVec3f* pos) {
    s32 tile = (u16)getTileCollisionCode_8022461C(pos, layer);
    if (tile == 0xD || tile == 0x3A) {
        return false;
    }
    return true;
}

// FIXME(shotgun): 99.96 %, regalloc-only; behaviour clean.
s32 Fish_804AC658::fn_80346330() {
    field_0x264 = 0.0f;
    field_0x268 = 0;
    field_0x26c = -1;
    field_0x270 = -1;
    findTargetPlayer_801EEC98(mSerial, &field_0x268, &field_0x26c, &field_0x264, &field_0x270, 0, 0);
    if (field_0x26c >= 0 && field_0x264 < 32.0f && field_0x270 >= 0) {
        if (fn_80233E40(field_0x270) == true || fn_8022B7C4(field_0x270) == true) {
            return true;
        }
    }
    for (s32 p = 0; p < 4; p++) {
        if (fn_8022CAF8(p) != 4 && fn_8022CAF8(p) != 5 && field_0x270 != p && getField_0x4() == getPlayerLayer_8023DE58(p)) {
            if (fn_80233E40(p) == true || fn_8022B7C4(p) == true) {
                f32 dx = mPos.x - getPlayerPos_8023E724(p)->x;
                f32 dy = mPos.y - getPlayerPos_8023E724(p)->y;
                if (std::sqrtf(dx * dx + dy * dy) < 32.0f) {
                    field_0x264 = std::sqrtf(dx * dx + dy * dy);
                    field_0x268 = (s32)(JMath::atanTable_.atan2_(-dx, -dy) * 10430.378f);
                    field_0x270 = p;
                    field_0x26c = angleToDir_801F681C(field_0x268);
                    return true;
                }
            }
        }
    }
    return false;
}

// Heading wobble and speed draws shared by the swimming states.
static inline void swimVelocity(Fish_804AC658* fish, f32 speed) {
    fish->field_0x254 = speed * JMath::sincosTable_.sinShort(fish->field_0x250);
    fish->field_0x258 = speed * JMath::sincosTable_.cosShort(fish->field_0x250);
    fish->mVel_0x3c.x = fish->field_0x254;
    fish->mVel_0x3c.y = fish->field_0x258;
    fish->mVel_0x3c.z = fish->field_0x25c;
}

// FIXME(shotgun): 99.43 %, regalloc-only; behaviour clean.
// (states follow the asm's switch (0 falls into 1, 2 into 3); the hit-avoid branch of state 1 and the wall-bounce
// heading draws of state 5 are the asm's, written with plain field stores; the original's vector-add temporaries are not matched)
void Fish_804AC658::update() {
    addCalc2_801F71A4(&mVel_0x3c.x, field_0x254, 0.1f, field_0x260);
    addCalc2_801F71A4(&mVel_0x3c.y, field_0x258, 0.1f, field_0x260);
    mPos += mVel_0x3c;
    JGeometry::TVec3f hitDir;
    hitDir.zero();
    u8 hit = sweepMapBox_801F2618(&hitDir, false);
    switch (field_0x230) {
    case 0: {
        field_0x260 = 0.007f;
        field_0x250 += (s32)(8192.0f - 16384.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
        f32 speed = 0.4f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 0.4f;
        mVel_0x3c.x = speed * JMath::sincosTable_.sinShort(field_0x250);
        mVel_0x3c.y = speed * JMath::sincosTable_.cosShort(field_0x250);
        field_0x238 = (s32)(75.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x41;
        field_0x230 = 1;
    }
    case 1:
        if (hit) {
            s32 angle = field_0x250;
            mVel_0x3c.zero();
            f32 savedX = mPos.x;
            f32 savedY = mPos.y;
            f32 savedZ = mPos.z;
            angle += 0x4000;
            for (s32 i = 0; i < 15; i++) {
                mPos.x = savedX;
                mPos.y = savedY;
                mPos.z = savedZ;
                JGeometry::TVec3f step(0.0f, 0.0f, 0.0f);
                step.x = 16.0f * JMath::sincosTable_.sinShort(angle);
                step.y = 16.0f * JMath::sincosTable_.cosShort(angle);
                mPos += step;
                if (!fn_802265DC(&mPos, mLayer_0x4, false)) {
                    break;
                }
                angle += 0x1000;
            }
            field_0x250 = angle;
            field_0x252 = field_0x250 + 0x4000;
            mPos.x = savedX;
            mPos.y = savedY;
            mPos.z = savedZ;
            f32 speed = 0.4f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 0.4f;
            mVel_0x3c.x = speed * JMath::sincosTable_.sinShort(field_0x250);
            mVel_0x3c.y = speed * JMath::sincosTable_.cosShort(field_0x250);
        } else {
            field_0x252 = field_0x250 + 0x4000;
            if (fn_80346330()) {
                field_0x230 = 4;
            } else if (field_0x238 == 0) {
                if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.3f) {
                    field_0x230 = 2;
                } else if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.5f) {
                    field_0x230 = 0;
                }
            }
        }
        break;
    case 2:
        field_0x238 = (s32)(75.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x41;
        field_0x230 = 3;
    case 3:
        if (fn_80346330()) {
            field_0x230 = 4;
        } else if (field_0x238 == 0) {
            field_0x230 = 0;
        }
        break;
    case 4:
        if (fn_80346330()) {
            field_0x250 = field_0x268 + 0x8000;
            field_0x250 += (s32)(4096.0f - 8192.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
        }
        swimVelocity(this, 1.5f);
        field_0x234 = 0;
        field_0x238 = (s32)(20.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x14;
        field_0x240 = 0;
        field_0x230 = 5;
    case 5:
        if (hit && field_0x234 == 0) {
            field_0x250 = field_0x268 + 0x8000;
            if (hitDir.x != 0.0f) {
                field_0x234 = (s32)(8.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 8;
                if (field_0x244 == 0) {
                    field_0x240 = 1;
                    if (hitDir.x > 0.0f) {
                        if (field_0x250 >= 0x4000 && field_0x250 <= 0xC000) {
                            field_0x250 = (s32)(32768.0f - 8192.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                        } else {
                            field_0x250 = (s32)(8192.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 1.0f);
                        }
                    } else {
                        if (field_0x250 >= 0x4000 && field_0x250 <= 0xC000) {
                            field_0x250 = (s32)(8192.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 32768.0f);
                        } else {
                            field_0x250 = (s32)(1.0f - 8192.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                        }
                    }
                } else {
                    field_0x250 = field_0x268;
                    field_0x234 = (s32)(20.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x14;
                }
            } else if (hitDir.y != 0.0f) {
                field_0x234 = (s32)(8.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 8;
                if (field_0x240 == 0) {
                    field_0x244 = 1;
                    if (hitDir.y > 0.0f) {
                        if (field_0x250 & 0x8000) {
                            field_0x250 = (s32)(61440.0f - 4096.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                        } else {
                            field_0x250 = (s32)(4096.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 4096.0f);
                        }
                    } else {
                        if (field_0x250 & 0x8000) {
                            field_0x250 = (s32)(45056.0f - 4096.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                        } else {
                            field_0x250 = (s32)(4096.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1() + 20480.0f);
                        }
                    }
                } else {
                    field_0x250 = field_0x268;
                    field_0x234 = (s32)(20.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 0x14;
                }
            }
            swimVelocity(this, 1.5f);
        } else {
            field_0x252 = field_0x250 + 0x4000;
            if (!hit && field_0x234 == 0) {
                field_0x240 = 0;
                field_0x244 = 0;
            }
            if (field_0x238 == 0 && field_0x234 == 0) {
                if (!fn_80346330()) {
                    stopSwim();
                } else {
                    field_0x230 = 4;
                }
            }
        }
        break;
    }
    if (field_0x234 > 0) {
        field_0x234--;
    }
    if (field_0x238 > 0) {
        field_0x238--;
    }
    if (field_0x23c > 0) {
        field_0x23c--;
    }
}

// FIXME(shotgun): 96.79 %, scheduling+regalloc; behaviour clean.
// (the SpriteParams stores and the matrix are scheduled differently)
void Fish_804AC658::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    setRotScaleMtx(mtx, JGeometry::TVec3f(0.0f, (f32)field_0x252 / -10430.228f, 0.0f), 1.0f);
    params.mMtx = mtx;
    params.field_0x21 = 1;
    params.field_0x1c = -1;
    params.field_0x1d = -1;
    params.field_0x10.a = 200;
    drawSprite_801F06F0("enemy_gc_sob.bin", field_0x24c + 0x283, &params, -1, 1.0f, 1.0f);
}

// Not in the binary (update inlines it); the pool has its 0.0f ahead of every other function's.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 6-line definition): split out only where the .sdata2 order needs it.
void Fish_804AC658::stopSwim() {
    field_0x25c = 0.0f;
    field_0x258 = 0.0f;
    field_0x254 = 0.0f;
    field_0x230 = 0;
}

Fish_804AC658::~Fish_804AC658() {}
