#include "game/ActorManager.h"
#include "game/GameAudio.h"
#include "JSystem/JMath/JMath.h"
#include "game/Player.h"
#include "game/act/act_vaati_boss.h"
#include "game/GameMath.h"
#include "game/cls_801EB8CC.h"

static inline bool hasBombEntry4(const VaatiBoss_804A4958* boss, const ActorBase_804A5770* actor) {
    bool found = false;
    if (actor->mActorId == 'HBMB') {
        for (s32 i = 0; i < 0x10; i++) {
            if (boss->field_0x308[i].field_0x0 == actor->mSerial && boss->field_0x308[i].field_0x4 != 0) {
                found = true;
                break;
            }
        }
    }
    return found;
}

static inline bool hasBombEntry5(const VaatiBoss_804A4958* boss, const ActorBase_804A5770* actor) {
    bool found = false;
    if (actor->mActorId == 'HBMB') {
        for (s32 i = 0; i < 0x10; i++) {
            if (boss->field_0x308[i].field_0x0 == actor->mSerial && boss->field_0x308[i].field_0x5 != 0) {
                found = true;
                break;
            }
        }
    }
    return found;
}

void VaatiBoss_804A4958::fn_801E5BA8(const ActorBase_804A5770* actor, const JGeometry::TVec3f* dir, bool flag) {
    for (s32 i = 0; i < 0x10; i++) {
        if (field_0x308[i].field_0x4 != 0 && field_0x308[i].field_0x0 == actor->mSerial) {
            JGeometry::TVec2f v;
            if (!flag) {
                v.set(dir->x, dir->y);
                v.normalize();
                v.scale(2.0f);
                field_0x308[i].vel.x = v.x;
                field_0x308[i].vel.y = v.y;
                field_0x308[i].vel.z = 1.5f;
                field_0x308[i].field_0x5 = 1;
                return;
            } else {
                s32 player = findNearestPlayer_801D05F8(&mPos, 160.0f);
                if (player >= 0 && field_0x2a8[player].field_0x0 == 0) {
                    s32 angle = getAngleToPlayer_801D0280(player);
                    v.x = JMath::sincosTable_.mTable[static_cast<u16>(angle) >> 5].first;
                    v.y = JMath::sincosTable_.mTable[static_cast<u16>(angle) >> 5].second;
                } else {
                    v.set(dir->x, dir->y);
                    v.normalize();
                }
                v.scale(4.0f);
                field_0x308[i].vel.x = v.x;
                field_0x308[i].vel.y = v.y;
                field_0x308[i].vel.z = 3.0f;
            }
            field_0x308[i].field_0x5 = 1;
            return;
        }
    }
}

// FIXME(shotgun): 99.53 %, regalloc-only; behaviour clean.
// Registers only: the first loop's counter and element pointer and the dx/dy FPRs take other registers.
void VaatiBoss_804A4958::fn_801E5754() {
    for (s32 i = 0; i < 4; i++) {
        if (field_0x2a8[i].field_0x0 != 0) {
            if (isPlayerActive_8023F264(i) && !isPlayerDead_80236B50(i) && !fn_8022D75C(i)) {
                if (getPlayerLayer_8023DE58(i) == getField_0x4()) {
                    continue;
                }
            }
            cls_801D6A88& el = field_0x2a8[i];
            el.fn_801D789C();
            el.field_0x0 = 0;
            if (fn_80230188(i)) {
                fn_80230224(i, false);
            }
        }
    }

    for (s32 i = 0; i < 4; i++) {
        if (field_0x2a8[i].isField_0x4Positive()) {
            field_0x2a8[i].decField_0x4();
            continue;
        }
        if (field_0x2a8[i].field_0x0 == 0 && !fn_801D08B4(i, &mPos, field_0x464.field_0xc.field_0xc)) {
            continue;
        }
        // Address-taken locals get stack slots in reverse declaration order.
        JGeometry::TVec3f playerPos;
        JGeometry::TVec3f sum;
        JGeometry::TVec3f out;
        JGeometry::TVec2f v;
        f32 dy = mPos.y - mPrevPos.y;
        f32 dx = mPos.x - mPrevPos.x;
        // Copied field by field, as the original loads three scalars.
        JGeometry::TVec3f* pos = getPlayerPos_8023E724(i);
        playerPos.x = pos->x;
        playerPos.y = pos->y;
        playerPos.z = pos->z;
        if (field_0x2a8[i].field_0x0 != 0) {
            playerPos.x += dx;
            playerPos.y += dy;
            if (mFlags & 0x4000) {
                field_0x2a8[i].field_0x8++;
                if (field_0x2a8[i].field_0x8 % 40 == 0) {
                    dropPlayerForceGems_80236074(i, 10, 0);
                }
            }
        }
        field_0x464.fn_801EB00C(&playerPos, &out);
        if (out.z > 0.0f) {
            if ((mFlags & 0x4000) && !fn_80230188(i) && fn_802300EC(i) && !fn_80236BEC(i)) {
                fn_80230C6C(i);
                fn_80230AD4(i);
                field_0x2a8[i].field_0x0 = 1;
                field_0x2a8[i].field_0xc.fn_8021BD18(getPlayerFacing_80230CFC(i));
            }
            if (!fn_80230188(i)) {
                if (mFlags & 0x4000) {
                    if (!fn_80236BEC(i)) {
                        out.x += dx;
                        out.y += dy;
                        fn_8023DFF4(i, &out);
                        fn_8022CD48(i);
                    }
                }
            } else {
                sum.x = playerPos.x;
                sum.y = playerPos.y;
                sum.z = playerPos.z;
                sum += out;
                field_0x2a8[i].field_0xc.fn_8021BCBC();
                fn_80230498(i, &sum, field_0x2a8[i].field_0xc.field_0x0, 0xff, -0x80, -0x80, false);
                fn_8022CB94(i);
                v.set(playerPos.x, playerPos.y);
                v.x -= out.x;
                v.y -= out.y;
                bool started = false;
                bool queued = false;
                bool hit = false;
                if (!(mFlags & 0x4000)) {
                    started = true;
                    hit = hitPlayerGroup_80238548(i, 0, &v, 0x19);
                } else if (sum.z > field_0x464.field_0xc.field_0x8) {
                    queued = true;
                    hit = hitPlayerGroup_80238548(i, mParamAtcp_0x114, &v, 0x17);
                }
                if (started || queued) {
                    fn_80230224(i, false);
                    field_0x2a8[i].fn_801D789C();
                    field_0x2a8[i].field_0x0 = 0;
                    if (hit) {
                        field_0x2a8[i].field_0x4 = 0x78;
                    } else {
                        field_0x2a8[i].field_0x4 = 0x1e;
                    }
                }
            }
        } else if (mFlags & 0x4000) {
            if (getPlayerPos_8023E724(i)->z >= field_0x464.field_0xc.field_0x8) {
                continue;
            }
            if (!fn_80236BEC(i)) {
                fn_8023DFF4(i, &out);
                fn_8022CD48(i);
            }
        }
    }
}

// FIXME: 98.80 %, missing/extra; behaviour near.
// The original materialises the isZero() result (mfcr, extrwi.) before testing it; ours branches on the compare.
// FIXME(shotgun): written from asm
void VaatiBoss_804A4958::fn_801E4D88() {
    ActorBase_804A5770* actors[ACTOR_MANAGER_MAX_ACTORS];
    s32 count = getActorManager_801F856C()->collectActorsById_801F7818('HBMB', mLayer_0x4, actors, -1);

    for (s32 i = 0; i < 0x10; i++) {
        if (field_0x308[i].field_0x4 != 0) {
            s32 serial = field_0x308[i].field_0x0;
            if (serial < 0) {
                clearEntry(i);
            } else {
                ActorBase_804A5770* act = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (act == NULL) {
                    clearEntry(i);
                } else if (!act->mAlive || act->mLayer_0x4 != mLayer_0x4 || act->field_0x94 == 1 || act->mSpawnParam_0x90 != 0) {
                    clearEntry(i);
                } else {
                    s32 j;
                    for (j = 0; j < count; j++) {
                        if (act->mSerial == actors[j]->mSerial) {
                            break;
                        }
                    }
                    if (j >= count) {
                        clearEntry(i);
                    }
                }
            }
        }
    }

    for (s32 i = 0; i < count; i++) {
        ActorBase_804A5770* actor = actors[i];
        if (actor == NULL || !actor->mAlive || actor->field_0x94 == 1 || actor->mSpawnParam_0x90 != 0) {
            continue;
        }
        if (hasBombEntry4(this, actor) && hasBombEntry5(this, actor)) {
            actor->mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, 0.0f);
            for (s32 j = 0; j < 0x10; j++) {
                if (field_0x308[j].field_0x0 == actor->mSerial && field_0x308[j].field_0x4 != 0 &&
                    field_0x308[j].field_0x5 != 0) {
                    cls_801D6A58* entry = &field_0x308[j];
                    entry->fn_801D7830();
                    JGeometry::TVec3f p = actor->mPos;
                    p += entry->vel;
                    if (p.z < 0.0f) {
                        entry->vel.x = 0.0f;
                        entry->vel.y = 0.0f;
                        p.z = 0.0f;
                        entry->vel.z = 0.0f;
                    }
                    actor->mPos = p;
                    break;
                }
            }
        } else {
            f32 dy = mPos.y - mPrevPos.y;
            f32 dx = mPos.x - mPrevPos.x;
            JGeometry::TVec3f pos;
            pos.set(actor->mPos);
            if (hasBombEntry4(this, actor) && !hasBombEntry5(this, actor)) {
                pos.x += dx;
                pos.y += dy;
            }
            f32 distX = pos.x - mPos.x;
            f32 distY = pos.y - mPos.y;
            f32 maxDist = field_0x464.field_0xc.field_0xc;
            if (!(square(distX) + square(distY) <= square(maxDist))) {
                continue;
            }
            JGeometry::TVec3f out;
            field_0x464.fn_801EB00C(&pos, &out);
            u32 pulling = mFlags & 0x4000;
            if (!pulling && hasBombEntry4(this, actor) && !hasBombEntry5(this, actor)) {
                fn_801E5BA8(actor, &out, false);
            } else if (pulling && !out.isZero()) {
                if (out.z > 0.0f) {
                    actor->mVel_0x3c = JGeometry::TVec3f(0.0f, 0.0f, 0.0f);
                    if (!hasBombEntry4(this, actor)) {
                        for (s32 j = 0; j < 0x10; j++) {
                            if (field_0x308[j].field_0x4 == 0) {
                                cls_801D6A58* entry = &field_0x308[j];
                                entry->field_0x0 = actor->mSerial;
                                entry->field_0x4 = 1;
                                break;
                            }
                        }
                    }
                }
                JGeometry::TVec3f newPos;
                newPos.set(pos);
                newPos += out;
                if (newPos.z > field_0x464.field_0xc.field_0x8) {
                    fn_801E5BA8(actor, &out, true);
                }
                actor->mPos = newPos;
            }
        }
    }
}

void VaatiBoss_804A4958::fn_801E4D74() {
    field_0x5ce = 0x32;
    field_0x464.field_0x58 = 255.0f;
}

void VaatiBoss_804A4958::fn_801E4D54() {
    mFlags &= ~0x4000;
    field_0x5ce = 0;
    field_0x464.field_0x58 = 0.0f;
}
