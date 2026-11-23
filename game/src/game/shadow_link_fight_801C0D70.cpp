#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/ActorManager.h"
#include "game/GameView.h"
#include "game/act/act_shadow_link_generator.h"
#include "game/act/act_shadow_link_2.h"

inline ShadowLinkFight_801C0D70::ShadowLinkFight_801C0D70() : field_0x54(0.0f, 0.0f, 0.0f) {
    field_0x0 = 0;
    field_0x2 = 0;
    field_0x4 = 0;
    field_0x6 = 0;
    field_0x8 = 0;
    mCount = 0;
    field_0x50 = -1;
}

// 0x801C151C
void ShadowLinkFight_801C0D70::fn_801C151C() {
    field_0x0 = 0;
    field_0x2 = 0;
    field_0x4 = 0;
    field_0x6 = 0;
    field_0x8 = 0;
    mCount = 0;
    field_0x50 = -1;
}

// 0x801C14A8
u16 ShadowLinkFight_801C0D70::fn_801C14A8(s32 serial) {
    mSerials[mCount] = serial;
    mCount++;
    field_0x4++;
    if (field_0x4 == 1) {
        getGameView_802D6A9C()->fn_802D7440(false);
    }
    return field_0x4 - 1;
}

// 0x801C1440
// FIXME: 96.54 %, different; behaviour clean.
// The loop counter in the original starts as a copy of the zero in found's
// register (mr r5, r6); ours loads a separate zero (li r8, 0) and keeps the counter in r8.
void ShadowLinkFight_801C0D70::fn_801C1440(s32 serial) {
    bool found = false;
    s32* p = mSerials;
    if (mCount <= 0) {
        return;
    }
    for (s32 i = 0; i < mCount; i++, p++) {
        if (!found) {
            if (serial == *p) {
                found = true;
            }
        } else {
            p[-1] = *p;
        }
    }
    if (found) {
        mCount--;
    }
}

// 0x801C1374
// FIXME: 94.22 %, missing/extra; behaviour drift.
// The original materialises the (field_0x50 >> 31) == 0 test once into r3 before
// the u16 compare and again (srwi, xori, clrlwi.) before fn_8022ADCC; ours keeps it in r5, tests
// the second read with srwi. alone, and compares field_0x8 with 3 using cmplwi instead of cmpwi.
void ShadowLinkFight_801C0D70::fn_801C1374(bool flag) {
    bool clear = (field_0x50 >> 31) == 0;
    field_0x8++;
    if (field_0x8 == field_0x4 && (!clear || flag)) {
        if (flag) {
            field_0x0 |= 0x200;
        }
        getGameView_802D6A9C()->fn_802D7440(true);
        gGameRoot_8053AAF8->startStageMusic_80138B7C();
        getGameView_802D6A9C()->fn_802D6BC8(0);
        clear = (field_0x50 >> 31) == 0;
        if (!clear) {
            fn_8022ADCC();
        }
    } else if (clear && field_0x8 == 3) {
        field_0x0 |= 0x100;
    }
}

// 0x801C1314
bool ShadowLinkFight_801C0D70::fn_801C1314() {
    bool result = false;
    if ((s32)field_0x50 >= 0 && !(field_0x0 & 0x40) && !(field_0x0 & 0x80) && field_0x4 != 0
        && mCount > 0 && field_0x6 == field_0x4 && field_0x8 != field_0x4) {
        result = true;
    }
    return result;
}

// 0x801C12B8
void ShadowLinkFight_801C0D70::fn_801C12B8() {
    s32 serial = mSerials[randInt_801CD664(mCount)];
    ShadowLink2_804A43E8* actor = static_cast<ShadowLink2_804A43E8*>(getActorManager_801F856C()->findActorBySerial_801F666C(serial));
    if (actor != NULL) {
        actor->mFlags |= 0x10000;
    }
}

// 0x801C1224
void ShadowLinkFight_801C0D70::fn_801C1224(s32 value) const {
    for (s32 i = 0; i < mCount; i++) {
        ShadowLink2_804A43E8* actor =
            static_cast<ShadowLink2_804A43E8*>(getActorManager_801F856C()->findActorBySerial_801F666C(mSerials[i]));
        if (actor != NULL && !(actor->mFlags & 0x10000)) {
            actor->mLastAttacker_0x10c = value;
            actor->die_0x24();
        }
    }
}

// 0x801C11F0
bool ShadowLinkFight_801C0D70::fn_801C11F0() {
    bool result = false;
    if (mCount > 1) {
        if (mCount != field_0x2) {
            if (field_0x0 & 0x6) {
                result = true;
            }
        }
    }
    return result;
}

// 0x801C11A4
void ShadowLinkFight_801C0D70::fn_801C11A4() {
    s32 serial = field_0x50;
    if (serial >= 0) {
        ActorBase_804A5770* generator = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (generator != NULL) {
            static_cast<ShadowLinkGenerator_804A4310*>(generator)->mPieces.fn_8021C468();
        }
    }
}

// 0x801C115C
void ShadowLinkFight_801C0D70::fn_801C115C() {
    s32 serial = field_0x50;
    if (serial >= 0) {
        ActorBase_804A5770* generator = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (generator != NULL) {
            static_cast<ShadowLinkGenerator_804A4310*>(generator)->fn_801C1768();
        }
    }
}

// 0x801C0FEC
bool ShadowLinkFight_801C0D70::fn_801C0FEC(const JGeometry::TVec3f* pos, JGeometry::TVec2f* out) const {
    s32 serial = field_0x50;
    bool found = false;
    out->x = pos->x;
    out->y = pos->y;
    if (serial >= 0) {
        ActorBase_804A5770* actor = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
        if (actor != NULL) {
            JGeometry::TVec2f d(pos->x - actor->mPos.x, pos->y - actor->mPos.y);
            f32 sq = d.x * d.x + d.y * d.y;
            if (sq <= 4096.0f) {
                found = true;
                if (pos->y <= actor->mPos.y) {
                    if (pos->x < actor->mPos.x) {
                        out->x = actor->mPos.x - 90.0f;
                    } else {
                        out->x = 90.0f + actor->mPos.x;
                    }
                } else {
                    d.normalize();
                    d.scale(90.0f);
                    out->x = actor->mPos.x;
                    out->y = actor->mPos.y;
                    out->add(d);
                }
            }
        }
    }
    return found;
}

// 0x801C0DCC
// FIXME: 94.93 %, different; behaviour clean.
// Register allocation (this in r30 vs r31; walk pointer, counters and constant 1 in
// r27/r29/r31 vs r29/r28/r30) and the counting loop's prologue (original copies 0 from count into
// the loop index with mr r31, r29 after li r29, 0 / mr r27, r30; ours emits mr / li / li).
void ShadowLinkFight_801C0D70::fn_801C0DCC() {
    if (field_0x0 & 0x100) {
        if (fn_8011921C()->fn_801186B0(0xF)) {
            s32 serial = field_0x50;
            if (serial >= 0) {
                ActorBase_804A5770* generator = getActorManager_801F856C()->findActorBySerial_801F666C(serial);
                if (generator != NULL) {
                    static_cast<ShadowLinkGenerator_804A4310*>(generator)->fn_801C186C();
                }
            }
            getActorManager_801F856C()->fn_801F7AB8('HBMB', -1);
            for (s32 i = 0; i < mCount; i++) {
                ShadowLink2_804A43E8* actor =
                    static_cast<ShadowLink2_804A43E8*>(getActorManager_801F856C()->findActorBySerial_801F666C(mSerials[i]));
                if (actor != NULL) {
                    actor->field_0x568 = 1;
                }
            }
            field_0x0 &= ~0x40;
            field_0x0 |= 0x400;
            field_0x0 &= ~0x100;
        }
    }
    if (field_0x0 & 0x200) {
        if (fn_8011921C()->fn_801186B0(0xD)) {
            field_0x0 |= 0x80;
            field_0x0 &= ~0x200;
        }
    }
    if (field_0x0 & 0x38) {
        u16 count = 0;
        for (s32 i = 0; i < mCount; i++) {
            ShadowLink2_804A43E8* actor =
                static_cast<ShadowLink2_804A43E8*>(getActorManager_801F856C()->findActorBySerial_801F666C(mSerials[i]));
            if (actor != NULL && (actor->mFlags & 0x8000)) {
                count++;
            }
        }
        bool done = count >= field_0x2;
        for (s32 i = 0; i < mCount; i++) {
            ShadowLink2_804A43E8* actor =
                static_cast<ShadowLink2_804A43E8*>(getActorManager_801F856C()->findActorBySerial_801F666C(mSerials[i]));
            if (actor != NULL) {
                if (field_0x0 & 0x8) {
                    actor->fn_801C27A8();
                }
                if ((field_0x0 & 0x10) && done) {
                    actor->fn_801C272C();
                }
                if (field_0x0 & 0x20) {
                    actor->fn_801C26FC();
                }
            }
        }
        field_0x0 &= ~0x28;
        if (done) {
            field_0x0 &= ~0x10;
        }
    }
}

// 0x801C0D70
ShadowLinkFight_801C0D70* getShadowLinkFight_801C0D70() {
    static ShadowLinkFight_801C0D70 lbl_804C7E70;
    return &lbl_804C7E70;
}
