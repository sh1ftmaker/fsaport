#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/ActorManager.h"
#include "game/ViewLayer.h"
#include "game/act/act_gohma.h"
#include "game/Camera.h"
#include "game/Effect.h"
#include "game/Player.h"
#include "JSystem/JMath/JMath.h"
#include "JSystem/JParticle/JPAEmitter.h"

static const char* lbl_804A3020[] = {
    "e_goma_yoko_0.bti",
    "e_goma_yoko_1.bti",
    "e_goma_yoko_2.bti",
    "e_goma_yoko_3.bti",
    "e_goma_yoko_0.bti",
    "e_goma_yoko_1.bti",
    "e_goma_yoko_2.bti",
    "e_goma_yoko_3.bti",
    "e_goma_mae_0.bti",
    "e_goma_mae_1.bti",
    "e_goma_mae_2.bti",
    "e_goma_mae_3.bti",
    "e_goma_ushiro_0.bti",
    "e_goma_ushiro_1.bti",
    "e_goma_ushiro_2.bti",
    "e_goma_ushiro_3.bti",
    "e_goma_ake_0.bti",
    "e_goma_ake_1.bti",
    "e_goma_ake_2.bti",
    "e_goma_toji_0.bti",
    "e_goma_toji_1.bti",
    "e_goma_toji_2.bti",
    "e_goma_toji_3.bti",
};

static s32 lbl_8053ABD8[2];

// Sets v to a vector of length speed along direction dir (0 +x, 1 -x, 2 +y, 3 -y). The same
// four-way set appears at six sites in fn_801AB6A8 / fn_801AAF44, written once here.
static inline void setMoveDir(JGeometry::TVec3f& v, s32 dir, f32 speed) {
    switch (dir) {
    case 0:
        v.x = speed;
        v.y = 0.0f;
        v.z = 0.0f;
        break;
    case 1:
        v.x = -speed;
        v.y = 0.0f;
        v.z = 0.0f;
        break;
    case 2:
        v.x = 0.0f;
        v.y = speed;
        v.z = 0.0f;
        break;
    case 3:
        v.x = 0.0f;
        v.y = -speed;
        v.z = 0.0f;
        break;
    }
}

void Gohma_804A307C::init() {
    mBox.i.x = -10.0f;
    mBox.i.y = -8.0f;
    mBox.f.x = 10.0f;
    mBox.f.y = 8.0f;
    mMapBox_0x80.i.x = -12.0f;
    mMapBox_0x80.i.y = -8.0f;
    mMapBox_0x80.f.x = 12.0f;
    mMapBox_0x80.f.y = 8.0f;
    mBox2.i.x = -16.0f;
    mBox2.i.y = -16.0f;
    mBox2.f.x = 16.0f;
    mBox2.f.y = 16.0f;
    field_0x254 = mSpawnParam_0x90 & 0xF;
    field_0x244 = 0;
    field_0x248 = 0;
    field_0x24c = 0;
    mTimers[0] = 0;
    mTimers[1] = 0;
    mTimers[2] = 0;
    mTimers[3] = 0;
    mEmitters[0] = NULL;
    mEmitters[1] = NULL;
    field_0xb8 = 1;
    mDir_0x98 = (s32)(4.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
    mDir_0x98 &= 3;
    s32 layer = mLayer_0x4;
    if (viewLayerFlag(layer)) {
        mDir_0x98 &= 1;
    }
    field_0x250 = (mDir_0x98 << 2) + field_0x244;
    field_0x264.z = 0.0f;
    field_0x264.y = 0.0f;
    field_0x264.x = 0.0f;
    mLife = 0x3EF;
    field_0x258 = 3;
    field_0x260 = mDir_0x98;
    field_0xb0 = 0x0A040010;
    if (field_0x254 == 0 || field_0x254 == 2) {
        field_0x258 = 2;
        field_0xbc = 2;
        field_0x270 = 1.0f;
        field_0x274 = 1.0f;
        field_0x278 = 1.0f;
        s32 layer = mLayer_0x4;
        if (viewLayerFlag(layer)) {
            mMapBox_0x80.i.x = -4.0f;
            mMapBox_0x80.i.y = -8.0f;
            mMapBox_0x80.f.x = 4.0f;
            mMapBox_0x80.f.y = 8.0f;
        }
        field_0x230 = 1;
        if (field_0x254 == 2) {
            if (lbl_8053ABD8[0] == 0) {
                gAudioMgr_8053AF80->stopBgm_80458F9C(0x3C);
                lbl_8053ABD8[0]++;
                field_0x230 = 0;
            }
        }
    } else {
        field_0xbc = 3;
        mLife = 0x3F2;
        field_0x260 = mDir_0x98 & 1;
        mDir_0x98 = 2;
        field_0xb0 = 0x0A040004;
        field_0x270 = 1.5f;
        field_0x274 = 1.2f;
        field_0x278 = 1.2f;
        mParamEfsz_0xc0 = 3;
        mDir_0x98 &= 1;
        field_0x250 = 0x13;
        field_0x230 = 8;
        if (field_0x254 == 3) {
            if (lbl_8053ABD8[0] == 0) {
                gAudioMgr_8053AF80->stopBgm_80458F9C(0x3C);
                lbl_8053ABD8[0]++;
                field_0x230 = 7;
            }
        }
    }
    field_0x25c = mLife;
}

Gohma_804A307C::~Gohma_804A307C() {
    for (int i = 0; i < 2; i++) {
        releaseEmitter_801EE434(mEmitters[i]);
    }
    if (field_0x254 == 2 || field_0x254 == 3) {
        if (--lbl_8053ABD8[0] == 0) {
            if (gGameRoot_8053AAF8->field_0x4c != 9) {
                gGameRoot_8053AAF8->startStageMusic_80138B7C();
            }
        }
    }
}

void Gohma_804A307C::vfunc_0x44() {
    if (field_0x25c == mLife) {
        return;
    }
    if (field_0x258 > 0) {
        field_0x258--;
    }
    if (field_0x258 <= 0) {
        field_0xb0 = 0;
    }
    field_0x25c = mLife;
    if (field_0x254 == 1 || field_0x254 == 3) {
        field_0x264.z = 0.0f;
        field_0x264.y = 0.0f;
        field_0x264.x = 0.0f;
        mVel_0x3c.z = 0.0f;
        mVel_0x3c.y = 0.0f;
        mVel_0x3c.x = 0.0f;
        field_0xb0 = 0x0A040004;
        field_0x248 = 1;
        field_0x230 = 12;
        return;
    }
    u16 angle = (u16)((u16)mDir_0x98 + 0x8000);
    mVel_0x3c.x = -10.0f * JMASinShort(dirToAngle_801F6874(angle));
    mVel_0x3c.y = -10.0f * JMACosShort(dirToAngle_801F6874(angle));
    field_0x230 = 5;
}

// FIXME: 98.59 %, missing/extra; behaviour drift.
// FIXME: emitter stops also store 1 at emitter+0x24 right after the status-bit store
// (JPAEmitter.h has no field there, same as act_force_gem_wooden).
void Gohma_804A307C::fn_801AB6A8() {
    JGeometry::TBox2f box(mBox);
    box.addPos(mPos.x, mPos.y);

    if (updateHitReaction_801F3D94(1)) {
        sweepMapBox_801F2618(NULL, false);
        return;
    }

    if (mLife < 1000) {
        die_0x24();
        return;
    }

    if (viewLayerFlag(mLayer_0x4)) {
        mPos.y = mPos.y - mPos.z;
        mPos.z = 0.0f;
        mVel_0x3c.z = 0.0f;
        field_0x264.y = 5.0f;
    }

    addCalc2_801F71A4(&mVel_0x3c.x, field_0x264.x, 0.3f, 1.0f);
    addCalc2_801F71A4(&mVel_0x3c.y, field_0x264.y, 0.3f, 1.0f);

    mPos += mVel_0x3c;

    if (field_0x230 != 6) {
        f32 centerX = box.f.x + 0.5f * (box.i.x - box.f.x);
        f32 centerY = box.f.y + 0.5f * (box.i.y - box.f.y);

        for (s32 i = 0; i < 4; i++) {
            s32 hitPlayer = findOverlappingPlayer_801F285C(NULL, NULL, false, i);
            if (hitPlayer >= 0) {
                u16 angle;
                s32 dir;
                findTargetPlayer_801EEC98(mSerial, &angle, &dir, NULL, NULL, hitPlayer, 2);
                if (dir >= 0) {
                    if (!viewLayerFlag(mLayer_0x4)) {
                        mDir_0x98 = dir;
                    } else if (!(dir & 2)) {
                        mDir_0x98 = dir;
                    } else if (angle < 0x8000) {
                        mDir_0x98 = 0;
                    } else {
                        mDir_0x98 = 1;
                    }
                    field_0x250 = mDir_0x98 * 4;
                    mVel_0x3c.x = -10.0f * JMASinShort(angle);
                    mVel_0x3c.y = -10.0f * JMACosShort(angle);
                }
                JGeometry::TVec3f hitPos;
                hitPos.x = centerX;
                hitPos.y = centerY;
                if (fn_802379E4(hitPlayer, &hitPos)) {
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x117, 0, 1, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x118, 0, 1, NULL, NULL, true, 1.0f);
                    startSound_801F0E34(0x400D1, 0);
                }
                field_0x230 = 5;
            }
        }
    }

    if (mTimers[2] == 0) {
        damagePlayersInBox_801F2B7C(-100, -1, 1);
        if (mEmitters[0] == NULL && mEmitters[1] == NULL) {
            if (findOverlappingActor_801F2718('FIRE') >= 0 || findOverlappingActor_801F2718('BLZE') >= 0) {
                mEmitters[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xBB, 0, -1, NULL, NULL, true, 1.0f);
                mEmitters[1] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xBC, 0, 1, NULL, NULL, true, 1.0f);
                mTimers[2] = 200;
            }
        }
    } else {
        damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 8);
        setEmitterPos_8013CB44(mEmitters[0], &mPos, 0, -1, true);
        setEmitterPos_8013CB44(mEmitters[1], &mPos, 0, 1, true);
        if (mTimers[2] == 1) {
            if (mEmitters[0] != NULL) {
                mEmitters[0]->stopCreateParticle();
                mEmitters[0] = NULL;
            }
            if (mEmitters[1] != NULL) {
                mEmitters[1]->stopCreateParticle();
                mEmitters[1] = NULL;
            }
            field_0x230 = 3;
        }
    }

    fn_801EF510();
    JGeometry::TVec3f vec(0.0f, 0.0f, 0.0f);
    u8 r31 = sweepMapBox_801F2618(&vec, false);

    switch (field_0x230) {
    case 0:
        gAudioMgr_8053AF80->startBgm_80458FF0(0x1000004, false, 0);
        field_0x230 = 1;
        // fallthrough
    case 1:
        mVel_0x3c.z = 0.0f;
        mVel_0x3c.y = 0.0f;
        mVel_0x3c.x = 0.0f;
        setMoveDir(field_0x264, mDir_0x98, 1.0f);
        mTimers[0] = 0;
        mTimers[1] = 50 + (s32)(100.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
        field_0x244 = 0;
        field_0x230 = 2;
        // fallthrough
    case 2:
        if (r31) {
            u8 flag = 0;
            if (!viewLayerFlag(mLayer_0x4)) {
                flag = 1;
            } else if (vec.y != 0.0f) {
                if ((vec.x > 0.0f && mVel_0x3c.x < 0.0f) || (vec.x < 0.0f && mVel_0x3c.x > 0.0f)) {
                    flag = 1;
                }
            }
            if (flag == 1) {
                if (mTimers[1] < 5) {
                    mTimers[1] = 5;
                }
                if (mTimers[3] == 0) {
                    if (!viewLayerFlag(mLayer_0x4)) {
                        mDir_0x98 ^= 2;
                        if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.5f) {
                            mDir_0x98 ^= 1;
                        }
                        f32 saveX = mPos.x;
                        f32 saveY = mPos.y;
                        f32 saveZ = mPos.z;
                        mVel_0x3c.z = 0.0f;
                        mVel_0x3c.y = 0.0f;
                        mVel_0x3c.x = 0.0f;
                        setMoveDir(field_0x264, mDir_0x98, 16.0f);
                        mPos += field_0x264;
                        if (sweepMapBox_801F2618(NULL, false)) {
                            mDir_0x98 ^= 1;
                        }
                        mPos.x = saveX;
                        mPos.y = saveY;
                        mPos.z = saveZ;
                    } else {
                        mTimers[3] = 30 + (s32)(30.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
                        mDir_0x98 ^= 1;
                    }
                    mTimers[0] = 0;
                    mVel_0x3c.z = 0.0f;
                    mVel_0x3c.y = 0.0f;
                    mVel_0x3c.x = 0.0f;
                    setMoveDir(field_0x264, mDir_0x98, 1.0f);
                }
            }
        }
        if (mTimers[0] == 0) {
            field_0x250 = mDir_0x98 * 4 + field_0x244;
            mTimers[0] = 5;
            field_0x244++;
            field_0x244 &= 3;
        }
        if (mTimers[1] != 0) {
            break;
        }
        field_0x230 = 3;
        // fallthrough
    case 3:
        field_0x264.z = 0.0f;
        field_0x264.y = 0.0f;
        field_0x264.x = 0.0f;
        mVel_0x3c.z = 0.0f;
        mVel_0x3c.y = 0.0f;
        mVel_0x3c.x = 0.0f;
        mTimers[1] = 30;
        field_0x230 = 4;
        // fallthrough
    case 4:
        if (mTimers[1] == 0) {
            mDir_0x98 = (s32)(4.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
            mDir_0x98 &= 3;
            if (!viewLayerFlag(mLayer_0x4)) {
                f32 saveX = mPos.x;
                f32 saveY = mPos.y;
                f32 saveZ = mPos.z;
                mVel_0x3c.z = 0.0f;
                mVel_0x3c.y = 0.0f;
                mVel_0x3c.x = 0.0f;
                setMoveDir(field_0x264, mDir_0x98, 16.0f);
                mPos += field_0x264;
                if (sweepMapBox_801F2618(NULL, false)) {
                    mDir_0x98 ^= 1;
                }
                mPos.x = saveX;
                mPos.y = saveY;
                mPos.z = saveZ;
            } else {
                mDir_0x98 &= 1;
            }
            field_0x230 = 1;
        }
        break;
    case 5:
        field_0x264.z = 0.0f;
        field_0x264.y = 0.0f;
        field_0x264.x = 0.0f;
        field_0x230 = 6;
        // fallthrough
    case 6:
        if (FABS(field_0x264.x - mVel_0x3c.x) < 0.1f && FABS(field_0x264.y - mVel_0x3c.y) < 0.1f) {
            field_0x230 = 1;
        }
        break;
    }
}

// FIXME: 91.51 %, missing/extra + reloc-name; behaviour drift.
// (missing/extra + reloc-name). Scheduling, ours has 2 fewer instructions: the 0.0f that zeroes
// mVel_0x3c is loaded earlier in the original (before the case-8/9 speed and timer arithmetic), the `li` argument
// setup of the findTargetPlayer_801EEC98 call and `li r30, 1` are ordered differently, and in case 11 the original stores
// field_0x244 and reloads it for the `> 2` test.
void Gohma_804A307C::fn_801AAF44() {
    gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&mPos);

    if (updateHitReaction_801F3D94(1)) {
        sweepMapBox_801F2618(NULL, false);
        return;
    }

    if (mLife < 1000) {
        die_0x24();
        field_0xb0 = 0;
        return;
    }

    f32 saveVelX = mVel_0x3c.x;
    f32 saveVelY = mVel_0x3c.y;
    f32 saveVelZ = mVel_0x3c.z;
    if (checkActorHit_801F2DB4('NULL', 'HBMB', NULL)) {
        mVel_0x3c.x = saveVelX;
        mVel_0x3c.y = saveVelY;
        mVel_0x3c.z = saveVelZ;
        mHitTimer_0xd8 = 0;
        mHitTimerStart_0xdc = 0;
    }

    addCalc2_801F71A4(&mVel_0x3c.x, field_0x264.x, 0.3f, 1.0f);
    addCalc2_801F71A4(&mVel_0x3c.y, field_0x264.y, 0.3f, 1.0f);

    mPos += mVel_0x3c;

    damagePlayersInBox_801F2B7C(-100, -1, 1);
    u8 r31 = sweepMapBox_801F2618(NULL, false);

    field_0x270 = 1.5f;
    field_0x274 = 1.2f;
    field_0x278 = 1.2f;

    mBox.i.x = -26.0f;
    mBox.i.y = -14.0f;
    mBox.f.x = 26.0f;
    mBox.f.y = 14.0f;

    mMapBox_0x80.i.x = -12.0f;
    mMapBox_0x80.i.y = 12.0f;
    mMapBox_0x80.f.x = 26.0f;
    mMapBox_0x80.f.y = 14.0f;

    mBox2.i.x = -16.0f;
    mBox2.i.y = -14.0f;
    mBox2.f.x = 16.0f;
    mBox2.f.y = 14.0f;

    switch (field_0x230) {
    case 7:
        gAudioMgr_8053AF80->startBgm_80458FF0(0x1000004, false, 0);
        field_0x230 = 8;
        // fallthrough
    case 8:
        mTimers[0] = 0;
        mTimers[2] = 200 + (s32)(200.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
        field_0x244 = 0;
        mDir_0x98 = field_0x260;
        mVel_0x3c.z = 0.0f;
        mVel_0x3c.y = 0.0f;
        mVel_0x3c.x = 0.0f;
        {
            f32 f = 0.7f + 0.35f * (3 - field_0x258);
            setMoveDir(field_0x264, mDir_0x98, f);
        }
        mDir_0x98 = 2;
        mTimers[1] = (s32)(100.0f - 20.0f * (3 - field_0x258));
        field_0x230 = 9;
        // fallthrough
    case 9:
        if (r31) {
            field_0x260 ^= 1;
            mDir_0x98 = field_0x260;
            mVel_0x3c.z = 0.0f;
            mVel_0x3c.y = 0.0f;
            mVel_0x3c.x = 0.0f;
            f32 f = 0.7f + 0.35f * (3 - field_0x258);
            setMoveDir(field_0x264, mDir_0x98, f);
            mDir_0x98 = 2;
        }
        if (mTimers[1] == 0) {
            f32 saveX = mPos.x;
            f32 saveY = mPos.y;
            f32 saveZ = mPos.z;
            mPos.y += 8.0f;
            u16 angle = 0;
            s32 count = 1;
            findTargetPlayer_801EEC98(mSerial, &angle, NULL, NULL, NULL, 0, 4);
            if (field_0x258 == 1) {
                count = 3;
                angle -= 0x2000;
            }
            for (s32 i = 0; i < count; i++) {
                s32 serial = getActorManager_801F856C()->spawnActor_801F9484('OKTM', mLayer_0x4, &mPos, 4, -1, -1);
                if (serial >= 0) {
                    ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                    if (actor != NULL) {
                        actor->mVel_0x3c = JGeometry::TVec3f(3.0f * JMASinShort(angle), 3.0f * JMACosShort(angle), 0.0f);
                        actor->mDir_0x98 = angleToDir_801F681C(angle);
                        mTimers[1] = (s32)(100.0f - 20.0f * (3 - field_0x258));
                    }
                }
                angle += 0x2000;
            }
            mPos.x = saveX;
            mPos.y = saveY;
            mPos.z = saveZ;
        }
        if (mTimers[0] == 0) {
            if (field_0x248 == 0) {
                field_0x250 = field_0x244 + 0x13;
            } else {
                field_0x250 = field_0x244 + 8;
            }
            mTimers[0] = 5;
            field_0x244++;
            field_0x244 &= 3;
        }
        if (mTimers[2] == 0) {
            field_0x264.z = 0.0f;
            field_0x264.y = 0.0f;
            field_0x264.x = 0.0f;
            mVel_0x3c.z = 0.0f;
            mVel_0x3c.y = 0.0f;
            mVel_0x3c.x = 0.0f;
            if (field_0x248 == 0) {
                field_0x230 = 10;
            } else {
                field_0x230 = 12;
            }
            field_0x248 ^= 1;
        }
        break;
    case 10:
        mTimers[0] = 0;
        field_0x244 = 0;
        field_0x230 = 11;
        // fallthrough
    case 11:
        if (mTimers[0] == 0) {
            mTimers[0] = 12;
            field_0x250 = field_0x244 + 0x10;
            field_0x244++;
            if (field_0x244 > 2) {
                field_0xb0 = 0x0A040010;
                field_0x248 = 1;
                field_0x230 = 8;
            }
        }
        break;
    case 12:
        field_0xb0 = 0x0A040004;
        mTimers[0] = 0;
        field_0x244 = 2;
        field_0x230 = 13;
        // fallthrough
    case 13:
        if (mTimers[0] == 0) {
            mTimers[0] = 12;
            field_0x250 = field_0x244 + 0x10;
            field_0x244--;
            if (field_0x244 < 0) {
                field_0x248 = 0;
                field_0x230 = 8;
            }
        }
        break;
    }
}

void Gohma_804A307C::update() {
    switch (field_0x254) {
    case 0:
    case 2:
        fn_801AB6A8();
        break;
    case 1:
    case 3:
        fn_801AAF44();
        break;
    }

    for (int i = 0; i < 4; i++) {
        if (mTimers[i] > 0) {
            mTimers[i]--;
        }
    }
}

void Gohma_804A307C::draw() {
    SpriteParams_801F5AE0 params;
    // Declaration order steers FPR allocation (f2 for scaleX, f1 for scaleY)
    f32 scaleY;
    f32 scaleX;
    scaleX = field_0x270;
    if (mDir_0x98 == 0) {
        scaleX *= -1.0f;
    }
    scaleY = field_0x274;
    Mtx mtx;
    mtx[0][0] = scaleX;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = scaleY;
    mtx[1][2] = 0.0f;
    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = scaleY;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    params.mMtx = mtx;

    s32 r30 = 4;
    f32 f31 = -5.0f;
    if (field_0x254 == 0 || field_0x254 == 2) {
        r30 = 3;
        f31 = -3.0f;
    }

    void* res = gGameRoot_8053AAF8->field_0xa4->getResource('TIMG', lbl_804A3020[field_0x250]);
    drawSpriteRes_801EFD34(res, &params, r30, 0.0f, f31, 0.0f);
}
