#include "game/GameAudio.h"
#include "game/ActorManager.h"
#include "game/GameGlobals.h"
#include "game/GameRootSub20.h"
#include "game/GameState.h"
#include "game/act/act_ice_melted_by_four_players.h"
#include "game/act/tu_801F9484.h"

// Timer values of the melting steps, indexed by field_0x24c (lbl_804751A8).
static const s32 lbl_804751A8[15] = {
    30, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 1, 1,
};

void IceMeltedByFourPlayers_804AE888::init() {
    mBox.set(-92.0f, -16.0f, 92.0f, 16.0f);
    mBox2.set(mBox);
    mMapBox_0x80.set(mBox);
    field_0xb8 = 1;
    mKnockbackKind_0xf8 = 2;
    field_0xbc = 3;
    field_0x1a4 = 1;
    field_0xb0 = 0x0A040304;
    field_0x234 = 0;
    field_0x23c = 1.0f;
    field_0x238 = 1.0f;
    mEmitter230 = NULL;
    field_0x240 = 0;
    field_0x250 = 0;
    field_0x251 = 0;
    field_0x244 = 0;
    field_0x248 = -1;
    field_0x24c = 0;
}

IceMeltedByFourPlayers_804AE888::~IceMeltedByFourPlayers_804AE888() {
    if (mEmitter230 != NULL) {
        releaseEmitter_801EE434(mEmitter230);
        mEmitter230 = NULL;
    }
}

static inline void applyToAllPlayers() {
    for (s32 i = 0; i < 4; i++) {
        fn_80230498(i, getPlayerPos_8023E724(i), 0xE8, 0xFF, -0x80, -0x80, true);
    }
}

void IceMeltedByFourPlayers_804AE888::update() {
    if (fn_8037845C() == 1) {
        return;
    }
    JGeometry::TBox2f box = mBox;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    fn_80239914(&box, mLayer_0x4, mSerial);
    fn_8020D58C()->addBox(mSerial);
    fn_80378680();
    switch (field_0x234) {
    case 0:
        mBox.set(-96.0f, -16.0f, 96.0f, 16.0f);
        if (fn_80378880() == 1) {
            spawnMeltEmitter();
            field_0x238 -= 0.002f;
            if (field_0x238 < 0.5f) {
                field_0x234 = 1;
            }
            startSound_801F0E34(0x50087, 0);
        } else if (mEmitter230 != NULL) {
            releaseEmitter_801EE434(mEmitter230);
            mEmitter230 = NULL;
        }
        break;
    case 1:
        startSound_801F0E34(0x50087, 0);
        field_0x23c -= 0.005;
        if (field_0x23c < 0.01f) {
            field_0x23c = 0.01f;
        }
        field_0x238 -= 0.002f;
        if (field_0x238 < 0.002f) {
            field_0x238 = 0.01f;
            if (fn_8023077C()) {
                field_0x244 = 50;
                field_0x248 = 4;
                field_0x24c = 0;
                applyToAllPlayers();
            }
            if (mEmitter230 != NULL) {
                releaseEmitter_801EE434(mEmitter230);
                mEmitter230 = NULL;
            }
        }
        break;
    }
    field_0x54.set(field_0x23c, 1.0f, field_0x238);
    s32 percent = 100 - (s32)(100.0f * field_0x238);
    GameRootSub20_801902C0* root = fn_801902C0();
    if (root->mIceRequest.field_0x10 != NULL && root->mIceRequest.mActive == 0) {
        root->mIceRequest.mPos.set(mPos);
        root->mIceRequest.mPercent = percent;
        root->mIceRequest.mActive = 1;
    }
    field_0x251 = 1;
}

u8 IceMeltedByFourPlayers_804AE888::fn_80378880() {
    JGeometry::TBox2f ours;
    ours.set(mBox);
    ours.addPos(mPos.x, mPos.y);
    s32 serial = -1;
    u8 playerMask = 0;
    for (;;) {
        serial = getActorManager_801F856C()->findNextActorById_801F78F4('FIRE', serial);
        if (serial < 0) {
            break;
        }
        ActorBase_804A5770* other = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (other == NULL) {
            continue;
        }
        if (mLayer_0x4 != other->mLayer_0x4) {
            continue;
        }
        JGeometry::TBox2f theirs;
        theirs.set(other->mBox);
        theirs.addPos(other->mPos.x, other->mPos.y);
        if (!theirs.intersect(ours)) {
            continue;
        }
        if (static_cast<FireRodFire_804A89F0*>(other)->fn_802BA9F8() != 1) {
            continue;
        }
        playerMask |= 1 << static_cast<FireRodFire_804A89F0*>(other)->fn_802BA9F0();
    }
    return playerMask == 0xF;
}

bool IceMeltedByFourPlayers_804AE888::fn_80378680() {
    JGeometry::TBox2f ours;
    ours.set(mBox);
    ours.addPos(mPos.x, mPos.y);
    s32 serial = -1;
    for (;;) {
        serial = getActorManager_801F856C()->findNextActorById_801F78F4('HBMB', serial);
        if (serial < 0) {
            break;
        }
        ActorBase_804A5770* other = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (other == NULL) {
            continue;
        }
        if (mLayer_0x4 != other->mLayer_0x4) {
            continue;
        }
        JGeometry::TBox2f theirs;
        theirs.set(other->mBox);
        theirs.addPos(other->mPos.x, other->mPos.y);
        if (!theirs.intersect(ours)) {
            continue;
        }
        JGeometry::TVec3f offset = other->mVel_0x3c;
        if (offset.x || offset.y) {
            other->mLife = 0;
            continue;
        }
        JGeometry::TVec3f pos = other->mPos;
        if (pos.y > mPos.y) {
            pos.y = mPos.y + 16.0f;
        } else {
            pos.y = mPos.y - 16.0f;
        }
        other->mPos = pos;
    }
    return false;
}

// A plain function that update inlines and whose own copy the linker dropped: the original's
// .sdata2 starts with 1.0f, ahead of fn_80378680's 0.0f and 16.0f, so it was generated before it
// (tools/pool.py). Spawns the emitter of the melting ice unless it is there already.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 5-line definition): split out only where the .sdata2 order needs it.
void IceMeltedByFourPlayers_804AE888::spawnMeltEmitter() {
    if (mEmitter230 == NULL) {
        mEmitter230 = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x456, 0, 1, NULL, NULL, true, 1.0f);
    }
}

u8 IceMeltedByFourPlayers_804AE888::fn_8037845C() {
    s32 i;
    if (field_0x244 > 0) {
        field_0x244--;
    }
    if (field_0x248 < 0) {
        return false;
    }
    if (field_0x248 < 4) {
        if (fn_8023077C()) {
            field_0x248 = 4;
            field_0x244 = 50;
            field_0x24c = 0;
            for (i = 0; i < 4; i++) {
                fn_80230498(i, getPlayerPos_8023E724(i), 0xE8, 0xFF, -0x80, -0x80, true);
            }
        }
        return true;
    }
    if (field_0x24c == 0 && field_0x244 > 30) {
        for (i = 0; i < 4; i++) {
            fn_80230498(i, getPlayerPos_8023E724(i), 0xE8, 0xFF, -0x80, -0x80, true);
        }
        return true;
    }
    if (field_0x24c < 13) {
        if (field_0x244 == 0) {
            field_0x24c++;
            field_0x244 = lbl_804751A8[field_0x24c];
            for (i = 0; i < 4; i++) {
                fn_80230498(i, getPlayerPos_8023E724(i), field_0x24c + 0xDB, 0xFF, -0x80, -0x80, true);
                if (field_0x24c == 1) {
                    fn_80230FD0(i, 0x10001);
                }
            }
            if (field_0x24c == 13) {
                field_0x244 = 25;
            }
        }
    } else if (field_0x244 == 0 && fn_80230534(false)) {
        field_0x248 = -1;
        playSe_801F0D20(3);
        setScreenFlag_0x30(0xFF, true);
        markDead_801F3668();
    }
    return true;
}

void IceMeltedByFourPlayers_804AE888::draw() {
    if (field_0x251 == 1) {
        return;
    }
    // The frame lays later locals lower: params (0x38) above mtx (0x8).
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    if (!field_0x54.isZero()) {
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;
        f32 scale = field_0x238;
        mtx[0][0] = scale;
        mtx[0][1] = 0.0f;
        mtx[0][2] = 0.0f;
        mtx[1][0] = 0.0f;
        mtx[1][1] = scale;
        mtx[1][2] = 0.0f;
        mtx[2][0] = 0.0f;
        mtx[2][1] = 0.0f;
        mtx[2][2] = scale;
        params.mMtx = mtx;
    }
    void* res = getBossArchiveResource_804032E8('TIMG', "yukikabe.bti");
    if (mLayer_0x4 < 8) {
        gGameRoot2_8053AB10->field_0x20->drawTimgImage_802F745C(&mPos, res, &params);
    }
}
