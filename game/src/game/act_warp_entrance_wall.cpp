#include "game/GameAudio.h"
#include "game/ActorId.h"
#include "game/ActorManager.h"
#include "game/Player.h"
#include "game/act/act_warp_entrance_wall.h"

// Boxes (x0, y0, x1, y1) relative to the actor, one per mSpawnParam_0x90 & 3.
// lbl_804752C0 and lbl_80475300 are init()'s mBox for shape 0 and 1.
static const f32 lbl_804752C0[4][4] = {
    {-7.5f, -23.5f, 7.5f, 7.5f},
    {-7.5f, -7.5f, 7.5f, 23.5f},
    {-7.5f, -7.5f, 23.5f, 7.5f},
    {-23.5f, -7.5f, 7.5f, 7.5f},
};
static const f32 lbl_80475300[4][4] = {
    {-15.5f, -7.5f, 15.5f, 7.5f},
    {-15.5f, -7.5f, 15.5f, 7.5f},
    {-7.5f, -15.5f, 7.5f, 15.5f},
    {-7.5f, -15.5f, 7.5f, 15.5f},
};
// update()'s boxes: four per shape.
static const f32 lbl_80475340[8][4] = {
    {-7.5f, -19.5f, 7.5f, 7.5f},
    {-7.5f, -7.5f, 7.5f, 19.5f},
    {-7.5f, -7.5f, 19.5f, 7.5f},
    {-19.5f, -7.5f, 7.5f, 7.5f},
    {-15.5f, -12.5f, 15.5f, -3.5f},
    {-15.5f, 3.5f, 15.5f, 12.5f},
    {-12.5f, -15.5f, 3.5f, 15.5f},
    {3.5f, -15.5f, 12.5f, 15.5f},
};

// Whether the actor in slot i is a moving house or a log whose box overlaps box.
static inline bool overlapsHouseOrLog(s32 i, const JGeometry::TBox2f& box) {
    ActorBase_804A5770* other = getActorManager_801F856C()->mActors[i];
    if (other == NULL) {
        return false;
    }
    if (other->mActorId == ACTOR_HOUS || other->mActorId == ACTOR_MRTA) {
        JGeometry::TBox2f otherBox(other->mBox);
        otherBox.addPos(other->mPos.x, other->mPos.y);
        if (otherBox.intersect(box)) {
            return true;
        }
    }
    return false;
}

void WarpEntranceWall_804AE948::init() {
    u32 param = mSpawnParam_0x90;
    u8 shape = param & 3;
    field_0x230 = (param >> 22) & 0x1F;
    switch (field_0x230) {
    case 0: {
        mBox.set(lbl_804752C0[shape][0], lbl_804752C0[shape][1], lbl_804752C0[shape][2], lbl_804752C0[shape][3]);
        break;
    }
    case 1: {
        mBox.set(lbl_80475300[shape][0], lbl_80475300[shape][1], lbl_80475300[shape][2], lbl_80475300[shape][3]);
        break;
    }
    }
    mVel_0x3c.x = 0.0f;
    mVel_0x3c.y = 0.0f;
    mVel_0x3c.z = 0.0f;
    mMapBox_0x80.i.x = mBox.i.x;
    mMapBox_0x80.i.y = mBox.i.y;
    mMapBox_0x80.f.x = mBox.f.x;
    mMapBox_0x80.f.y = mBox.f.y;
    field_0x234 = mSpawnParam_0x90 >> 27;
    if (field_0x230 == 1) {
        mPos.x = (s32)mPos.x / 16 * 16 + 8;
        mPos.y = (s32)mPos.y / 16 * 16 + 8;
        if (shape < 2) {
            mPos.x += 8.0f;
        } else {
            mPos.y += 8.0f;
        }
    }
}

void WarpEntranceWall_804AE948::update() {
    if (field_0x234 == 0 || isScreenFlagSet_0x34()) {
        JGeometry::TBox2f box;
        switch (field_0x230) {
        case 0:
            if (!fn_803798D0()) {
                return;
            }
            break;
        case 1: {
            JGeometry::TVec3f pos(mPos);
            bool result = false;
            if (sweepMapBox_801F2618(NULL, false)) {
                result = true;
            }
            mPos = pos;
            if (!result) {
                return;
            }
            break;
        }
        }
        u8 index = (mSpawnParam_0x90 & 3) + field_0x230 * 4;
        box.set(lbl_80475340[index][0], lbl_80475340[index][1], lbl_80475340[index][2],
                lbl_80475340[index][3]);
        box.i.x += mPos.x;
        box.i.y += mPos.y;
        box.f.x += mPos.x;
        box.f.y += mPos.y;
        fn_80239914(&box, mLayer_0x4, mSerial);
    }
}

bool WarpEntranceWall_804AE948::fn_803798D0() {
    JGeometry::TBox2f box(mBox);
    box.addPos(mPos.x, mPos.y);
    s32 end = getActorManager_801F856C()->getActorEnd();
    for (s32 i = 0; i < end; i++) {
        if (i != mSerial && overlapsHouseOrLog(i, box)) {
            return true;
        }
    }
    return false;
}

WarpEntranceWall_804AE948::~WarpEntranceWall_804AE948() {}
