#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/act/act_unknown_bome_801688AC.h"
#include "game/act/act_arrow.h"
#include "game/act/act_octorok_rock.h"
#include "game/ActorId.h"
#include "game/Effect.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/ViewLayer.h"
#include "game/Player.h"
#include "std/math.h"

// 0x801A6160.
s32 ActorBase_804A5770::fn_801A6160() {
    s32 timer;
    s32 result = 0;
    JGeometry::TBox2f box = mBox2;
    JGeometry::TVec2f pos(mPos.x, mPos.y - mPos.z);
    timer = field_0xa8;
    if (timer != 0) {
        if (timer == 30) {
            f32 scale = getEffectScale_801EE4F0();
            if (scale < 1.0f) {
                scale = 1.0f;
            }
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x17C, 0, 1, NULL, NULL, true, scale);
        }
        if (timer - 1 <= 0) {
            die_0x24();
        }
        field_0xa8 = timer - 1;
        return 1;
    }
    if (mLife > 0 && fn_802242F4(this)) {
        return 1;
    }
    if (field_0xb0 == 0 || mLife <= 0) {
        return 0;
    }
    lbl_8053A100 = -1;
    box.addPos(pos);
    s32 hit;
    if ((hit = fn_801A5400(&box, &pos)) != 0) {
        result = 3;
    } else if ((hit = fn_801A4D74(&box, &pos)) != 0) {
        result = 3;
    } else if ((hit = fn_801A4754(&box, &pos)) != 0) {
        result = 1;
    } else if ((hit = fn_801A41D8(&box, &pos)) == 50) {
        result = 1;
    }
    if (hit == 0xFF) {
        return 0;
    }
    if (result <= 1 && fn_801A4FB4() != 0) {
        result = 2;
    }
    if (field_0xa4 != 0) {
        s32 count = field_0xa4;
        if (mPos.z > 0.0f) {
            mVel_0x3c = JGeometry::TVec3f(mVel_0x3c.x, mVel_0x3c.y, 0.0f);
            mPos = JGeometry::TVec3f(mPos.x, mPos.y, mPos.z - 1.0f);
        } else {
            mPos = JGeometry::TVec3f(mPos.x, mPos.y, 0.0f);
        }
        field_0xa4 = count - 1;
        result = 1;
        if (checkHits_801F305C()) {
            if (field_0xbc == 3) {
                mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, 0.0f);
            }
            result = 2;
            sweepMapBox_801F2618(NULL, false);
        }
    }
    if (result > 1) {
        field_0xa4 = 0;
    }
    return result;
}

extern "C" {

// 0x801A6108.
s32 fn_801A6108(s32 serial, const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    return getActorManager_801F856C()->findActorBySerial_801F666C(serial)->fn_801A5400(box, pos);
}

} // extern "C"

// 0x801A5400.
// Case 6 also stores 1 at this + 0x230 (past ActorBase_804A5770); that store is not written.
// FIXME: 96.02 %, missing/extra; behaviour drift.
s32 ActorBase_804A5770::fn_801A5400(const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    if ((field_0xb0 & 0xFF) == 0) {
        return 0;
    }
    s32 result = 0;
    s32 player = fn_8023AE34(box, mLayer_0x4, pos);
    if (getGameView_802D6A9C()->field_0x131df[getField_0x4()] == 0 && mPos.z > 32.0f) {
        return 0;
    }
    if (field_0xa0 < 0) {
        if (player < 0) {
            return 0;
        }
        s32 kind = field_0xb0 & 0xFF;
        ActorBase_804A5770* arrow;
        if (kind != 3) {
            field_0xa0 = fn_8023B2B0(player);
            arrow = getActorManager_801F856C()->findActorBySerial_801F666C(field_0xa0);
        } else {
            arrow = getActorManager_801F856C()->findActorBySerial_801F666C(fn_8023B2B0(player));
        }
        if (arrow == NULL) {
            field_0xa0 = -1;
            return 0;
        }
        if (arrow->mLife == 0) {
            field_0xa0 = -1;
            return 0;
        }
        mLastAttacker_0x10c = player;
        if (kind == 9) {
            kind = 4;
            f32 dx = mPos.x - getPlayerPos_8023E724(player)->x;
            s32 face = mDir_0x98;
            if (face != angleToDir_801F681C((s32)(JMath::atanTable_.atan2_(-dx, -(mPos.y - getPlayerPos_8023E724(player)->y)) *
                                          10430.378f))) {
                kind = 5;
            }
            if (field_0xa4 != 0) {
                kind = 5;
            }
        }
        if (kind == 16) {
            f32 dx = mPos.x - getPlayerPos_8023E724(player)->x;
            s32 face = mDir_0x98;
            if (face != angleToDir_801F681C((s32)(JMath::atanTable_.atan2_(-dx, -(mPos.y - getPlayerPos_8023E724(player)->y)) *
                                          10430.378f))) {
                kind = 4;
            }
        }
        if (arrow->field_0x94 != 0 && arrow->field_0x94 != 4) {
            if (kind == 1 || kind == 8) {
                kind = 2;
            }
            if (kind == 10 || kind == 17) {
                field_0xa0 = -1;
                return 0;
            }
        }
        if (kind == 11 && field_0xac >= 3) {
            kind = 4;
        }
        switch (kind) {
        case 1:
            static_cast<ArrowPlayer_804A05E0*>(arrow)->field_0x270++;
            mHitTimer_0xd8 = 0;
            mHitTimerStart_0xdc = 0;
            mLastAttacker_0x10c = player;
            // fall through
        case 8:
            if (field_0xc4) {
                field_0xa0 = -1;
                result = 0xFF;
                break;
            }
            if (kind == 8 && (mPos.z != 0.0f || mVel_0x3c.z != 0.0f)) {
                field_0xa0 = -1;
                result = 0xFF;
                break;
            }
            mSpawnPos = JGeometry::TVec3f(mPos.x - arrow->mPos.x, mPos.y - arrow->mPos.y, mPos.z - arrow->mPos.z);
            if (kind == 8) {
                break;
            }
            // fall through
        case 0:
        case 9:
        default:
            if (!(arrow->mDir_0x98 & 2)) {
                field_0x48 = JGeometry::TVec3f(0.5f, 1.4f, 0.0f);
            } else {
                field_0x48 = JGeometry::TVec3f(1.4f, 0.5f, 0.0f);
            }
            break;
        case 16:
            mLife = mLife - fn_8023B5B4(player);
            if (mLife < 1000) {
                switch (arrow->mDir_0x98) {
                case 0:
                    spawnEmitter_8013CC50(arrow->mLayer_0x4, &mPos, 0x177, 1, 1, NULL, NULL, true, getEffectScale_801EE4F0());
                    break;
                case 1:
                    spawnEmitter_8013CC50(arrow->mLayer_0x4, &mPos, 0x176, 1, 1, NULL, NULL, true, getEffectScale_801EE4F0());
                    break;
                case 2:
                    spawnEmitter_8013CC50(arrow->mLayer_0x4, &mPos, 0x175, 1, 1, NULL, NULL, true, getEffectScale_801EE4F0());
                    break;
                case 3:
                    spawnEmitter_8013CC50(arrow->mLayer_0x4, &mPos, 0x178, 1, 1, NULL, NULL, true, getEffectScale_801EE4F0());
                    break;
                }
            }
            // fall through
        case 2:
            if (field_0xc4) {
                result = 0xFF;
                break;
            }
            // fall through
        case 5:
            if (kind != 16) {
                s32 life = mLife - fn_8023B5B4(player);
                if (life <= 0) {
                    mLife = 0;
                    result = 20;
                    arrow->mAlive = false;
                    break;
                }
                mLife = life;
            }
            // fall through
        case 11:
        case 15:
        case 18:
            if (kind == 11) {
                field_0xac++;
            }
            arrow->mAlive = false;
            if (kind == 5 || kind == 16 || kind == 11 || kind == 18 || kind == 15) {
                s32 serial = getActorManager_801F856C()->spawnActor_801F9484('OKTM', arrow->mLayer_0x4, &arrow->mPos, 1, -1, -1);
                if (serial >= 0) {
                    OctorokRock_8049F5D8* rock =
                        static_cast<OctorokRock_8049F5D8*>(getActorManager_801F856C()->findActorBySerial_801F666C(serial));
                    if (rock != NULL && rock->field_0x230 != 4) {
                        u16 dir = dirToAngle_801F6874(mDir_0x98);
                        f32 dx = 0.8f * (mPos.x - rock->mPos.x);
                        f32 dy = 0.8f * (mPos.y - rock->mPos.y);
                        rock->field_0x254 = std::sqrtf(dx * dx + dy * dy);
                        u16 angle = JMath::atanTable_.atan2_(-dx, -dy) * 10430.378f;
                        u16 target = dirToAngle_801F6874(arrow->mDir_0x98) + 0x8000;
                        while (angleToDir_801F681C(angle) != angleToDir_801F681C(target)) {
                            addCalcAngle2_801F714C(&angle, target, 1, 0x1000);
                        }
                        if (kind == 15) {
                            rock->field_0x230 = 5;
                        } else if (kind == 16 || kind == 18) {
                            rock->field_0x230 = 6;
                        } else {
                            rock->field_0x230 = 4;
                        }
                        rock->field_0x258 = mSerial;
                        rock->field_0x25c = dir - angle;
                    }
                }
            }
            field_0xa0 = -1;
            result = 10;
            mHitTimer_0xd8 = 0;
            mHitTimerStart_0xdc = 0;
            if (kind == 15) {
                result = 0xFF;
            }
            break;
        case 10:
            result = 0xFF;
            break;
        case 17:
            result = 10;
            break;
        case 6:
            spawnEmitter_8013CC50(arrow->mLayer_0x4, &arrow->mPos, 0x117, 0, 1, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(arrow->mLayer_0x4, &arrow->mPos, 0x118, 0, 1, NULL, NULL, true, 1.0f);
            // fall through
        case 4:
            for (s32 i = 0; i < 4; i++) {
                s32 serial = fn_8023B2B0(i);
                if (serial >= 0) {
                    arrow = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (arrow != NULL) {
                        arrow->mLife = 0;
                    }
                }
            }
            field_0xa0 = -1;
            // fall through
        case 3:
            result = 0xFF;
            break;
        case 7:
            mAlive = false;
            // fall through
        case 14:
            field_0xa0 = -1;
            spawnEmitter_8013CC50(arrow->mLayer_0x4, &arrow->mPos, 0x24A, 0, 1, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(arrow->mLayer_0x4, &arrow->mPos, 0x24B, 0, 1, NULL, NULL, true, 1.0f);
            arrow->mAlive = false;
            result = 0xFF;
            break;
        case 12:
            if (arrow->field_0x94 == 1) {
                arrow->field_0x94 = 2;
            }
            if (arrow->field_0x94 == 0) {
                arrow->field_0x94 = 4;
            }
            field_0xa0 = -1;
            result = 0xFF;
            break;
        case 13:
            if (arrow->field_0x94 == 2 || arrow->field_0x94 == 4) {
                field_0x94 = 1;
            }
            field_0xa0 = -1;
            result = 0xFF;
            break;
        }
        if (player >= 0) {
            lbl_8053A100 = player;
        }
        if (kind != 4 && kind != 8) {
            vfunc_0x44();
        }
        if (result != 0) {
            if (kind == 1 || kind == 2 || kind == 5 || kind == 16) {
                JGeometry::TVec3f from = mPos;
                from -= arrow->mVel_0x3c;
                takeHit_801F37AC(0, &from, 0, 3, -1);
                if (field_0xbc == 3 || kind == 5) {
                    mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, 0.0f);
                }
            }
            return result;
        }
    }
    s32 serial = field_0xa0;
    if (serial >= 0) {
        ActorBase_804A5770* holder = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (holder != NULL) {
            if (holder->mActorId == ACTOR_ARRW) {
            result = 10;
            switch (field_0xb0 & 0xFF) {
            case 10:
            case 17: {
                JGeometry::TVec3f p = holder->mPos;
                if (player >= 0) {
                    lbl_8053A100 = player;
                }
                // Tests (!mSerial) & 0x10, always false (cntlzw; rlwinm. 27,27,27).
                if (!mSerial & 0x10) {
                    p.x += 3.5f - (s32)(mSerial & 7);
                    p.y += 3.5f - (s32)(mSerial & 7);
                } else {
                    p.x -= 3.5f - (s32)(mSerial & 7);
                    p.y -= 3.5f - (s32)(mSerial & 7);
                }
                mPos = p;
                mLayer_0x4 = holder->mLayer_0x4;
                vfunc_0x44();
                break;
            }
            case 8:
                if (field_0xc4 || mPos.z != 0.0f || mVel_0x3c.z != 0.0f) {
                    field_0xa0 = -1;
                    result = 0xFF;
                    break;
                }
                if (static_cast<ArrowPlayer_804A05E0*>(holder)->field_0x26c == 1) {
                    field_0xa0 = -1;
                    mLife = 0;
                    field_0xb0 = 0;
                }
                mPos = holder->mPos + mSpawnPos;
                mLayer_0x4 = holder->mLayer_0x4;
                vfunc_0x44();
                if (static_cast<ArrowPlayer_804A05E0*>(holder)->field_0x26c == 2) {
                    die_0x24();
                    result = 20;
                }
                break;
            case 1:
                mPos = holder->mPos + mSpawnPos;
                mLayer_0x4 = holder->mLayer_0x4;
                if (static_cast<ArrowPlayer_804A05E0*>(holder)->field_0x26c == 2) {
                    die_0x24();
                    result = 20;
                }
                break;
            }
            }
        } else {
            switch (field_0xb0 & 0xFF) {
            case 8:
                mLife = 0;
                field_0xb0 = 0;
                break;
            case 10:
            case 17:
                break;
            default:
                die_0x24();
                break;
            }
            field_0xa0 = -1;
            result = 20;
        }
    }
    return result;
}

static inline f32 getRandFX(f32 max) {
    return max - 2.0f * max * gGameRoot_8053AAF8->mRandom.get_ufloat_1();
}

// 0x801A4FB4.
s32 ActorBase_804A5770::fn_801A4FB4() {
    s32 kind = (field_0xb0 >> 12) & 0xF;
    if (kind == 0) {
        return 0;
    }
    if (field_0xd4 == 0) {
        f32 range = 0.0f;
        for (s32 i = 0; i < 4; i++) {
            if (isPlayerActive_8023F264(i) && getField_0x4() == getPlayerLayer_8023DE58(i) && fn_8023A51C(i) && !mPos.z) {
                if (fn_8023725C(i) == 1) {
                    range = 50.0f;
                } else if (fn_8023725C(i) == 2) {
                    range = 777.0f;
                }
                if (range > 0.0f) {
                    f32 dx = fn_8023A478(i)->x - mPos.x;
                    f32 dy = fn_8023A478(i)->y - mPos.y;
                    if (std::sqrtf(dx * dx + dy * dy) < range || 777.0f == range) {
                        field_0xd4 = 1;
                        if (i >= 0) {
                            lbl_8053A100 = i;
                        }
                        vfunc_0x44();
                    }
                }
            }
        }
    }
    f32 z = 0.0f;
    if (field_0xd4 == 1) {
        switch (kind) {
        case 1:
            z = 5.0f + getRandFX(1.0f);
            break;
        case 2:
            z = 2.0f + getRandFX(1.0f);
            break;
        case 3:
            if (mLife - 10 <= 0) {
                mLife = 0;
            }
            field_0xd4 = 0;
            break;
        }
        mVel_0x3c = JGeometry::TVec3f(mVel_0x3c.x, mVel_0x3c.y, z);
        field_0xd4 = 2;
    }
    if (field_0xd4 == 2) {
        mPos = JGeometry::TVec3f(mPos.x, mPos.y, mPos.z + mVel_0x3c.z);
        mVel_0x3c = JGeometry::TVec3f(mVel_0x3c.x, mVel_0x3c.y, mVel_0x3c.z - 0.5f);
        if (mPos.z <= 0.0f) {
            mVel_0x3c = JGeometry::TVec3f(mVel_0x3c.x, mVel_0x3c.y, 0.0f);
            mPos = JGeometry::TVec3f(mPos.x, mPos.y, 0.0f);
            field_0xd4 = 0;
            s32 type = (field_0xb0 >> 16) & 0xFF;
            if (type == 1) {
                fn_801F3780(300);
            }
        }
    }
    return field_0xd4;
}

extern "C" {

// 0x801A4F5C.
s32 fn_801A4F5C(s32 serial, const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    return getActorManager_801F856C()->findActorBySerial_801F666C(serial)->fn_801A4D74(box, pos);
}

} // extern "C"

// Not auto-inlined: fn_801A4F5C and fn_801A6160 call fn_801A4D74 out of line in the original
// (docs/INLINING.md, pragma rule 1).
#pragma auto_inline off
// 0x801A4D74.
// FIXME(shotgun): 97.95 %, scheduling+regalloc; behaviour clean.
s32 ActorBase_804A5770::fn_801A4D74(const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    s32 kind = (field_0xb0 >> 8) & 0xF;
    if (kind == 0) {
        return 0;
    }
    if (viewLayerFlag(mLayer_0x4) == 0 && mPos.z > 32.0f) {
        return 0;
    }
    s32 player = fn_8023AA18(box, mLayer_0x4, pos);
    if (player < 0) {
        return 0;
    }
    if (kind != 3) {
        if (mHitTimer_0xd8 == 0) {
            s32 life = mLife - fn_8023B520(player);
            mLastAttacker_0x10c = player;
            if (life <= 0) {
                life = 0;
                if (field_0xbc != 3) {
                    fn_801A3C50(player);
                    return 40;
                }
            }
            mLife = life;
            takeHit_801F37AC(0, getPlayerPos_8023E724(player), 0, 4, -1);
            if (field_0xbc == 3) {
                mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, 0.0f);
            }
            if (player >= 0) {
                lbl_8053A100 = player;
            }
            vfunc_0x44();
        }
    } else {
        return 0xFF;
    }
    startSound_801F0E34(0x4001E, 0);
    return 30;
}
#pragma auto_inline reset

extern "C" {

// 0x801A4D1C.
s32 fn_801A4D1C(s32 serial, const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    return getActorManager_801F856C()->findActorBySerial_801F666C(serial)->fn_801A4754(box, pos);
}

} // extern "C"

// 0x801A4754.
// FIXME: 99.23 %, scheduling+regalloc; behaviour drift.
s32 ActorBase_804A5770::fn_801A4754(const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    s32 kind = (field_0xb0 >> 16) & 0xFF;
    if (kind == 0) {
        return 0;
    }
    if (kind == 2 || kind == 7 || kind == 8) {
        s32 serial = field_0xa0;
        if (serial >= 0) {
        ActorBase_804A5770* holder = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (holder != NULL) {
            JGeometry::TVec3f p = holder->mPos;
            s32 hit = fn_8023AC74(box, mLayer_0x4, pos);
            if (hit >= 0) {
                lbl_8053A100 = hit;
            }
            vfunc_0x44();
            if (kind == 8 && holder->mLayer_0x4 != 0) {
                field_0xb0 = field_0xb4;
                field_0xa0 = -1;
                return 0;
            }
            if (!mSerial & 0x10) {
                p.x += 3.5f - (s32)(mSerial & 7);
                p.y += 3.5f - (s32)(mSerial & 7);
            } else {
                p.x -= 3.5f - (s32)(mSerial & 7);
                p.y -= 3.5f - (s32)(mSerial & 7);
            }
            mPos = p;
            mLayer_0x4 = holder->mLayer_0x4;
            if (kind == 2) {
                return 50;
            }
            return 0xFF;
        }
        if (kind == 7 || kind == 2 || kind == 8) {
            field_0xb0 = field_0xb4;
        }
        field_0xa0 = -1;
        return 0;
        }
    }
    if (getGameView_802D6A9C()->field_0x131df[getField_0x4()] == 0 && mPos.z > 32.0f) {
        return 0;
    }
    s32 player = fn_8023AC74(box, mLayer_0x4, pos);
    if (player < 0) {
        return 0;
    }
    s32 result = 0;
    ActorBase_804A5770* other = getActorManager_801F856C()->findActorBySerial_801F666C(fn_8023B22C(player));
    if (other == NULL) {
        return 0;
    }
    if (kind == 6) {
        bool turned = false;
        kind = 4;
        f32 rad = JMath::atanTable_.atan2_(-(mPos.x - other->mPos.x), -(mPos.y - other->mPos.y));
        s32 face = mDir_0x98;
        if ((face ^ 1) != angleToDir_801F681C((s32)(rad * 10430.378f))) {
            turned = true;
        }
        if (other != NULL && static_cast<UnknownBOME_804A0738*>(other)->field_0x254 != 0) {
            turned = true;
        }
        if (!turned) {
            kind = 1;
        }
        if (field_0xa4 != 0) {
            kind = 1;
        }
    }
    if (player >= 0) {
        lbl_8053A100 = player;
    }
    vfunc_0x44();
    switch (kind) {
    case 4:
        if (field_0x1dc.startTimer_801EAC20(player)) {
            startSound_801F0E34(0x400D1, 0);
        }
        // fall through
    case 1:
        if (other != NULL) {
            static_cast<UnknownBOME_804A0738*>(other)->field_0x254 = 1;
        }
        mHitTimer_0xd8 = 0;
        mHitTimerStart_0xdc = 0;
        // fall through
    case 5:
        result = 0xFF;
        if (kind == 1) {
            if (mPos.z == 0.0f) {
                mVel_0x3c = JGeometry::TVec3f(mVel_0x3c.x, mVel_0x3c.y,
                                               4.0f + (1.0f - 2.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()));
                mPos = JGeometry::TVec3f(mPos.x, mPos.y, mPos.z + mVel_0x3c.z);
            }
            fn_801F3780(300);
            result = 50;
            startSound_801F0E34(0x30021, 0);
        }
        break;
    case 2:
        result = 50;
        field_0xa0 = fn_8023B22C(player);
        field_0xb4 = field_0xb0;
        field_0xb0 = 0x20000;
        vfunc_0x40();
        break;
    case 7:
        result = 0xFF;
        if (!field_0xc4) {
            field_0xa0 = fn_8023B22C(player);
            field_0xb4 = field_0xb0;
            field_0xb0 = 0x70000;
            vfunc_0x40();
        }
        break;
    case 8:
        if (other != NULL) {
        s32 idx = field_0x94 - 1;
        s32 own = 0;
        if (hasPlayerLeader_8022F514(player)) {
            if (idx == getPlayerLeader_8022F3DC(player)) {
                own = 1;
            }
        } else if (idx == player) {
            own = 1;
        }
        if (field_0x94 == 0 || own == 1) {
            UnknownBOME_804A0738* bome = static_cast<UnknownBOME_804A0738*>(other);
            if (bome->field_0x254 != 2) {
                bome->field_0x254 = 2;
                field_0xa0 = fn_8023B22C(player);
                vfunc_0x40();
                field_0xb4 = field_0xb0;
                field_0xb0 = 0x80000;
            }
        }
        }
        result = 0xFF;
        break;
    case 3:
        mLastAttacker_0x10c = player;
        fn_801F4694();
        die_0x24();
        break;
    }
    return result;
}

extern "C" {

// 0x801A46FC.
s32 fn_801A46FC(s32 serial, const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    return getActorManager_801F856C()->findActorBySerial_801F666C(serial)->fn_801A41D8(box, pos);
}

} // extern "C"

// 0x801A41D8.
// FIXME(shotgun): 98.25 %, missing/extra; behaviour drift.
s32 ActorBase_804A5770::fn_801A41D8(const JGeometry::TBox2f* box, JGeometry::TVec2f* pos) {
    ActorBase_804A5770* weapon;
    s32 player = fn_8023AD54(box, mLayer_0x4, pos);
    if (player < 0) {
        return 0;
    }
    if (viewLayerFlag(mLayer_0x4) == 0 && mPos.z > 32.0f) {
        return 0;
    }
    weapon = getActorManager_801F856C()->findActorBySerial_801F666C(fn_8023B358(player));
    if (weapon == NULL || weapon->mLife == 0) {
        return 0;
    }
    s32 byPlayer = false;
    s32 kind;
    if (weapon->field_0x94 != 0) {
        kind = (field_0xb0 >> 16) & 0xFF;
    } else {
        byPlayer = true;
        kind = field_0xb0 & 0xFF;
    }
    if (kind == 0) {
        return 0;
    }
    s32 result = 0;
    if (player >= 0) {
        lbl_8053A100 = player;
    }
    vfunc_0x44();
    if (!byPlayer) {
        if (kind == 6) {
            bool turned = false;
            kind = 4;
            f32 angle = JMath::atanTable_.atan2_(-(mPos.x - weapon->mPos.x), -(mPos.y - weapon->mPos.y));
            s32 face = mDir_0x98;
            if ((face ^ 1) != angleToDir_801F681C((s32)(angle * 10430.378f))) {
                turned = true;
            }
            if (!turned) {
                kind = 1;
            }
            if (field_0xa4 != 0) {
                kind = 1;
            }
        }
        switch (kind) {
        case 0:
        case 4:
        case 6:
        default:
            for (s32 i = 0; i < 4; i++) {
                s32 serial = fn_8023B358(i);
                if (serial >= 0) {
                    ActorBase_804A5770* other = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (other != NULL) {
                        other->mLife = 0;
                    }
                }
            }
            // fall through
        case 2:
        case 5:
            result = 0xFF;
            break;
        case 1:
            if (mPos.z == 0.0f) {
                mVel_0x3c = JGeometry::TVec3f(mVel_0x3c.x, mVel_0x3c.y,
                                               4.0f + (1.0f - 2.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()));
                mPos = JGeometry::TVec3f(mPos.x, mPos.y, mPos.z + mVel_0x3c.z);
            }
            mHitTimer_0xd8 = 0;
            mHitTimerStart_0xdc = 0;
            weapon->mLife = 0;
            fn_801F3780(300);
            startSound_801F0E34(0x30021, 0);
            result = 50;
            break;
        case 7:
            weapon->mAlive = false;
            result = 0xFF;
            mLife = 0;
            break;
        case 3:
            weapon->mAlive = false;
            mLastAttacker_0x10c = player;
            fn_801F4694();
            die_0x24();
            break;
        }
    } else {
        if (kind == 9) {
            kind = 4;
            f32 dx = mPos.x - getPlayerPos_8023E724(player)->x;
            f32 angle = JMath::atanTable_.atan2_(-dx, -(mPos.y - getPlayerPos_8023E724(player)->y));
            s32 face = mDir_0x98;
            if (face != angleToDir_801F681C((s32)(angle * 10430.378f))) {
                kind = 5;
            }
        }
        switch (kind) {
        case 8:
            mLife = 0;
            result = 0xFF;
            break;
        case 1:
        case 2:
        case 5: {
            s32 life = mLife - fn_8023B5B4(player);
            weapon->mAlive = false;
            if (life <= 0) {
                mLife = 0;
                fn_80235B68(player, mSerial);
                mLastAttacker_0x10c = player;
                result = 20;
            } else {
                mLife = life;
                result = 10;
            }
            break;
        }
        case 4:
        case 6:
        case 11:
        case 16:
        case 17:
        case 18:
            for (s32 i = 0; i < 4; i++) {
                s32 serial = fn_8023B358(i);
                if (serial >= 0) {
                    weapon = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (weapon != NULL) {
                        weapon->mLife = 0;
                    }
                }
            }
            // fall through
        case 0:
        case 3:
        case 9:
        case 10:
        case 12:
        case 13:
        case 14:
        case 15:
        default:
            result = 0xFF;
            break;
        case 7:
            spawnEmitter_8013CC50(weapon->mLayer_0x4, &weapon->mPos, 0x24A, 0, 1, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(weapon->mLayer_0x4, &weapon->mPos, 0x24B, 0, 1, NULL, NULL, true, 1.0f);
            result = 0xFF;
            weapon->mAlive = false;
            mAlive = false;
            break;
        }
        if (result != 0 && (kind == 1 || kind == 2 || kind == 5)) {
            JGeometry::TVec3f from = mPos;
            from -= weapon->mVel_0x3c;
            takeHit_801F37AC(0, &from, 3, 0, -1);
        }
    }
    return result;
}

// 0x801A3CFC.
void ActorBase_804A5770::fn_801A3CFC() {
    SpriteParams_801F5AE0 params;
    static JGeometry::TSMtxf sMtx;
    mSpriteParams = params;
    if (field_0xa0 >= 0 && 1.0f != field_0x48.x) {
        params.field_0x30 = 8;
        PSMTXScale(sMtx, field_0x48.x, field_0x48.y, field_0x48.y);
        sMtx.mMtx[0][3] = 0.0f;
        sMtx.mMtx[1][3] = 0.0f;
        sMtx.mMtx[2][3] = 0.0f;
        params.mMtx = sMtx;
        mSpriteParams = params;
    } else if (field_0xa4 != 0) {
        PSMTXTrans(sMtx, 0.0f, 0.0f, 0.0f);
        if (field_0xa4 < 100) {
            if ((field_0xa4 & 2) == 0) {
                sMtx.mMtx[0][3] = 1.0f;
                sMtx.mMtx[1][3] = 0.0f;
                sMtx.mMtx[2][3] = 0.0f;
            } else {
                sMtx.mMtx[0][3] = -1.0f;
                sMtx.mMtx[1][3] = 0.0f;
                sMtx.mMtx[2][3] = 0.0f;
            }
        }
        params.mMtx = sMtx;
        mSpriteParams = params;
    }
}

// 0x801A3C50.
void ActorBase_804A5770::fn_801A3C50(s32 player) {
    mLife = 0;
    mLastAttacker_0x10c = player;
    field_0xa0 = player;
    field_0x48 = JGeometry::TVec3f(1.4f, 0.4f, 0.4f);
    mHitTimer_0xd8 = 0;
    mHitTimerStart_0xdc = 0;
    field_0xa4 = 0;
    fn_801F4694();
    if (mLayer_0x4 >= 8) {
        field_0xa8 = 30;
    } else {
        field_0xa8 = 35;
    }
    lbl_8053A100 = player;
    vfunc_0x44();
}
