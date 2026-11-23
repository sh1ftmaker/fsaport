#include "game/GameAudio.h"
#include "game/act/act_cannon.h"
#include "game/Effect.h"
#include "game/EnemyBase.h"
#include "game/GameGlobals.h"
#include "game/Map.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"


// The animation: (cel, duration) pairs, ended by a cel of -1.
static const s16 lbl_80463FD0[] = {0, 3, 1, 2, 2, 2, 3, 3, 4, 3, 5, 3, -1, -1};
static const s16* lbl_80539ED0[] = {lbl_80463FD0, NULL};

Cannon_8049CCF0::Cannon_8049CCF0() {
    field_0x268 = 1;
    field_0x269 = 0;
    field_0x26a = 0;
    field_0x26c = -1;
    field_0x270 = 0;
    field_0x274 = 0;
    field_0x278 = 0;
    field_0x27c = 0;
    field_0x280 = 0.0f;
    field_0x284 = 0.0f;
    mHomePos.x = 0.0f;
    mHomePos.y = 0.0f;
    mHomePos.z = 0.0f;
}

Cannon_8049CCF0::~Cannon_8049CCF0() {}

f32 Cannon_8049CCF0::fn_801080B8() const {
    if (mLayer_0x4 == 0) {
        return 8.0f;
    }
    if (field_0x26a != 0) {
        return 8.0f;
    }
    return 13.0f;
}

void pool_8053C300(Cannon_8049CCF0* self);

void Cannon_8049CCF0::init() {
    if ((mSpawnParam_0x90 & 0xF) == 1) {
        field_0x274 = 1;
    }
    field_0x27c = mSpawnParam_0x90 >> 4 & 0xF;
    pool_8053C300(this);
    if ((mSpawnParam_0x90 >> 16 & 1) == 1) {
        field_0x26a = 1;
    } else {
        field_0x26a = 0;
    }
    mBox.set(-8.0f, -8.0f, 8.0f, -6.0f);
    mBox2.set(mBox);
    mMapBox_0x80.set(0.0f, 0.0f, 0.0f, 0.0f);
    mLife = 0;
    field_0xb0 = 0x50303;
    mAnim.setKeyTables_801D2608(lbl_80539ED0, 1);
    mAnim.startAnim_801D22DC(0, 2, false);
    mAnim.setFrame_801D1F14(0.0f);
    mHomePos.set(mPos);
    if (getScreenFlagBit_0x3C()) {
        if (isScreenFlagSet_0x34()) {
            field_0x269 = 1;
        }
    }
}

void pool_8053C2F8(Cannon_8049CCF0* c);

s32 findOwner(Cannon_8049CCF0* c, const JGeometry::TBox2f* box);

void setBounceHeight(Cannon_8049CCF0* self, s16 a);

void updateRecoil(Cannon_8049CCF0* self);

void updateRise(Cannon_8049CCF0* self);

void pool_8053C2F4(Cannon_8049CCF0* self);

// FIXME: 95.41 %, missing/extra + reloc-name; behaviour drift.
void Cannon_8049CCF0::update() {
    if (getScreenFlagBit_0x3C()) {
        if (field_0x274 == 0 || field_0x274 == 1) {
            if (!isScreenFlagSet_0x34()) {
                field_0x269 = 0;
                return;
            }
            if (field_0x269 == 0) {
                pool_8053C2F4(this);
                startSound_801F0E34(0x30003, 0);
                playSe_801F0D20(0x42);
            }
            field_0x269 = 1;
        }
    }
    if (field_0x274 == 0 || field_0x274 == 1) {
        JGeometry::TBox2f box(mBox);
        JGeometry::TVec2f p(mPos.x, mPos.y - mPos.z);
        box.addPos(p);
        s32 owner = findOwner(this, &box);
        if (owner >= 0) {
            field_0x26c = owner;
            field_0x278 = field_0x274;
            field_0x274 = 2;
            // FIXME: 94.96 %. The owner search's `break` and the state dispatch
            // below differ only in branch layout and register choice (see
            // fn_80107360's FIXME); the box and the sin lookup are placed as
            // the original's, but the box corners are not added in its order.
            switch (field_0x274) {
            case 2:
                fn_80230FD0(field_0x26c, 0x20035);
                break;
            case 1:
                break;
            }
        }
    }
    switch (field_0x274) {
    case 0:
        pool_8053C2F8(this);
        break;
    case 2:
        pool_8053C2F8(this);
        updateRise(this);
        if (mPos.y < mHomePos.y) {
            mPos.y = mHomePos.y;
        }
        if (field_0x284 >= 1.0f) {
            field_0x284 = 1.0f;
        }
        field_0x270++;
        if (field_0x270 >= 180 && field_0x270 < 300) {
            s16 a = (field_0x270 - 180) * (field_0x270 - 180) / 6;
            setBounceHeight(this, a);
        } else if (field_0x270 >= 300) {
            fn_80231590(field_0x26c, mSerial);
        }
        if (findOverlappingPlayer_801F285C(NULL, NULL, true, -1) >= 0) {
            fn_80231590(field_0x26c, mSerial);
        } else {
            // Declared in this order: MWCC gives the later aggregate the lower
            // stack slot (docs/RULES_CARD.md, two stack structs).
            JGeometry::TVec3f vec;
            JGeometry::TBox2f box;
            JGeometry::TVec2f out;
            out.set(0.0f, 0.0f);
            box.set(mBox2);
            JGeometry::TVec2f p(mPos.x, mPos.y - mPos.z);
            box.addPos(p);
            bool hit = false;
            if (fn_8023AA18(&box, mLayer_0x4, &out) >= 0) {
                hit = true;
            } else {
                s32 player = fn_8023AE34(&box, mLayer_0x4, &out);
                if (player >= 0) {
                    fn_801CC820(player, NULL);
                    hit = true;
                } else {
                    player = fn_8023AC74(&box, mLayer_0x4, &out);
                    if (player >= 0) {
                        fn_801CC538(player, &out, &vec, NULL);
                        hit = true;
                    } else {
                        player = fn_8023AD54(&box, mLayer_0x4, &out);
                        if (player >= 0) {
                            fn_801CC640(player, NULL, NULL);
                            hit = true;
                        }
                    }
                }
            }
            if (hit) {
                fn_80231590(field_0x26c, mSerial);
            } else if (checkActorHit_801F2DB4(mActorId, 'NULL', NULL)) {
                fn_80231590(field_0x26c, mSerial);
            }
        }
        break;
    case 3:
        updateRecoil(this);
        break;
    }
    mAnim.update_801D1C18();
}

void Cannon_8049CCF0::draw() {
    if (getScreenFlagBit_0x3C()) {
        if (field_0x274 == 0 || field_0x274 == 1) {
            if (!isScreenFlagSet_0x34()) {
                return;
            }
        }
    }
    SpriteParams_801F5AE0 params;
    params.field_0x1d = 1;
    s32 frame = mAnim.mCel + 0x14C;
    JGeometry::TVec3f* pos = &mPos;
    if (mLayer_0x4 < 8) {
        void* res = gGameRoot_8053AAF8->field_0xa4->getResource('SOB ', "enemy_both2_sob.bin");
        gGameRoot2_8053AB10->field_0x20->drawSobSprite_802F86CC(pos, res, frame, &params, 1.0f, 1.0f);
    }
    showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, mAnim.mCel + 0x14C, false, 1, -1, 0, 0, 0);
}

// FIXME: 98.39 %, missing/extra; behaviour clean.
bool Cannon_8049CCF0::fn_80107360() {
    if (field_0x274 != 2) {
        return false;
    }
    mPos.y = mHomePos.y;
    JGeometry::TVec3f pos = mPos;
    pos.y -= 16.0f;
    if (mLayer_0x4 == 0) {
        spawnEmitter_8013CC50(mLayer_0x4, &pos, 0x5A, 0, 1, NULL, NULL, true, 1.0f);
    }
    fn_80230FD0(field_0x26c, 0x20036);
    gGameRoot_8053AAF8->startRumble_80134DE4(1, 0);
    field_0x278 = field_0x274;
    field_0x274 = 3;
    // FIXME: 98.15 %. Only the branch layout differs: the original has one
    // more unreachable `b` after the dispatch (beq, bge, b, b; ours beq,
    // bge, b). The empty case 1 is there to produce the bge/b pair; its
    // meaning is unknown (init() sets state 1 for some spawns).
    switch (field_0x274) {
    case 2:
        fn_80230FD0(field_0x26c, 0x20035);
        break;
    case 1:
        break;
    }
    field_0x284 = 0.0f;
    return true;
}

// From here down: plain functions that update and init inline and whose own copies the linker
// dropped. The original's .sdata2 has their literals ahead of fn_80107360's, in the order of these
// functions from the bottom of the file up, so they were generated before it (tools/pool.py).
// R2 pool helper (one-statement body); was readSwingRange.
void pool_8053C300(Cannon_8049CCF0* self) {
    self->field_0x280 = (f32)(self->mSpawnParam_0x90 >> 8 & 0x1F);
}

// The swing (state 0 and 2): moves mPos.x between mHomePos.x +- 8 * field_0x280
// with a speed that falls off near the ends.
// R2 pool helper (inlined at 2 call sites); was swing.
void pool_8053C2F8(Cannon_8049CCF0* c) {
    if (c->field_0x280 <= 0.0f) {
        return;
    }
    f32 speed;
    f32 rate;
    switch (c->field_0x27c) {
    case 1:
        speed = 1.0f;
        rate = 4.0f;
        break;
    default:
        speed = 0.5f;
        rate = 2.0f;
        break;
    }
    f32 dist = __fabsf((c->mPos.x - c->mHomePos.x) / (8.0f * c->field_0x280));
    if (dist >= 0.8f) {
        speed -= rate * (dist - 0.8f);
    }
    if (c->field_0x268 != 0) {
        c->mVel_0x3c.x = speed;
        c->mVel_0x3c.y = 0.0f;
        c->mVel_0x3c.z = 0.0f;
    } else {
        c->mVel_0x3c.x = -speed;
        c->mVel_0x3c.y = 0.0f;
        c->mVel_0x3c.z = 0.0f;
    }
    c->mPos += c->mVel_0x3c;
    if (c->mPos.x >= c->mHomePos.x + 8.0f * c->field_0x280) {
        c->mPos.x = c->mHomePos.x + 8.0f * c->field_0x280;
        c->field_0x268 = 0;
    } else if (c->mPos.x <= c->mHomePos.x - 8.0f * c->field_0x280) {
        c->mPos.x = c->mHomePos.x - 8.0f * c->field_0x280;
        c->field_0x268 = 1;
    }
}

// R2 pool helper (one-statement body); was spawnTakenEffect.
void pool_8053C2F4(Cannon_8049CCF0* self) {
    spawnEmitter_8013CC50(self->mLayer_0x4, &self->mPos, 0x503, 0, 0, NULL, NULL, true, 1.5f);
}

// Returns the first player that can take the cannon, or -1.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 15-line definition): split out only where the .sdata2 order needs it.
s32 findOwner(Cannon_8049CCF0* c, const JGeometry::TBox2f* box) {
    for (s32 i = 0; i < 4; i++) {
        u8 bit = 1 << i;
        if ((gGameRoot_8053AAF8->mPlayerMask & bit) != 0 || fn_8022CAF8(i) == 4) {
            if (fn_8023B0F4(i, box, c->mLayer_0x4, false, 1000.0f)) {
                if (c->field_0x26c < 0) {
                    if (fn_8023162C(i, c->mSerial)) {
                        return i;
                    }
                }
            }
        }
    }
    return -1;
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 8-line definition): split out only where the .sdata2 order needs it.
void updateRise(Cannon_8049CCF0* self) {
    self->field_0x284 += 0.05f;
    {
        f32 t = 2.0f * (0.5f - self->field_0x284);
        JGeometry::TVec3f v(0.0f, t, 0.0f);
        self->mPos += v;
    }
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 37-line definition): split out only where the .sdata2 order needs it.
void updateRecoil(Cannon_8049CCF0* self) {
    static s32 lbl_8053AA70 = 0;
    static f32 lbl_8053AA78 = 0.0f;
    lbl_8053AA78 = lbl_8053AA78 + 0.1f;
    if (lbl_8053AA78 >= 1.0f) {
        lbl_8053AA78 = 1.0f;
    }
    lbl_8053AA70++;
    {
        f32 tmp;
        if (lbl_8053AA78 <= 0.2f) {
            tmp = 12.0f * (0.5f - lbl_8053AA78);
        } else {
            tmp = 4.0f * (0.2f - lbl_8053AA78);
        }
        JGeometry::TVec3f v(0.0f, tmp, 0.0f);
        self->mPos += v;
    }
    if (self->mPos.y < self->mHomePos.y) {
        self->mPos.y = self->mHomePos.y;
    }
    {
        bool done = false;
        if (lbl_8053AA78 >= 1.0f) {
            if (lbl_8053AA70 >= 30) {
                lbl_8053AA70 = 0;
                lbl_8053AA78 = 0.0f;
                done = true;
            }
        }
        if (done) {
            self->field_0x270 = 0;
            self->field_0x26c = -1;
            self->field_0x274 = 0;
        }
    }
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 4-line definition): split out only where the .sdata2 order needs it.
void setBounceHeight(Cannon_8049CCF0* self, s16 a) {
    f32 sinv = JMath::sincosTable_.sinDegree(a);
    self->mPos.y = self->mHomePos.y + (f32)(12 * ((200 - (self->field_0x270 - 180)) / 200) + 2) * sinv;
}
