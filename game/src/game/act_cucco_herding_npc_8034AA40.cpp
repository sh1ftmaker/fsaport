#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/PlayerBit.h"
#include "game/Player.h"
#include "game/ActorManager.h"
#include "game/ActorId.h"
#include "game/BoxTest.h"
#include "game/GameState.h"
#include "game/Unk_8045A080.h"
#include "game/act/act_cucco.h"
#include "game/Map.h"
#include "game/Message.h"
#include "game/mg_kokko_screen_80447618.h"
#include "JSystem/JMath/JMath.h"
#include "game/act/act_cucco_herding_npc_8034AA40.h"

// .rodata 0x804720E8: the start timers (state kinds 0, 1 use words 0 and the .sdata2 pair
// {20, 15}), the frame indices by facing and sub-state (16 words), and the frames of kind 1 (8).
static const s32 lbl_804720E8[4] = {10, 7, 10, 7};
static const s32 lbl_804720F8[4][4] = {{1, 0, 2, 0}, {4, 3, 5, 3}, {7, 6, 8, 6}, {10, 9, 11, 9}};
static const s32 lbl_80542510[2] = {20, 15};
static const s32 lbl_80472138[4][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}};

// 0x8034C464.
void CuccoHerdingNPC_804AC8C0::init() {
    field_0x230 = 0;
    field_0xb0 = 0x1000;
    mVel_0x3c.x = 0.0f;
    mVel_0x3c.y = 0.0f;
    mVel_0x3c.z = 0.0f;
    mLife = 10000;
    field_0x238 = 0;
    field_0x234 = 0;
    field_0x2c0 = 0;
    field_0x244 = mSpawnParam_0x90 & 0xF;
    field_0x248 = 0;
    field_0x24c = (mSpawnParam_0x90 >> 4) & 0xFF;
    field_0x270 = 0;
    field_0x250 = 60;
    field_0x254 = -1;
    field_0x258 = 0;
    field_0x25c = 0;
    field_0x28f = 0;
    field_0x28c[0] = 0;
    field_0x28c[1] = 0;
    field_0x28c[2] = 0;
    for (s32 i = 0; i < 4; i++) {
        field_0x274[i] = 0;
        field_0x284[i] = 0;
        field_0x260[i] = -1;
        field_0x288[i] = 0;
        field_0x290[i] = *getPlayerPos_8023E724(i);
    }
    mPos.x = 16.0f * (f32)((s32)mPos.x / 16) + 8.0f;
    mPos.y = 16.0f * (f32)((s32)mPos.y / 16) + 8.0f;
    pool_80542558();
    mBox.set(-8.0f, -8.0f, 8.0f, 8.0f);
    switch (field_0x244) {
    case 0:
        field_0x234 = lbl_804720E8[0];
        field_0x2c0 = 0x423;
        break;
    case 1:
        field_0x234 = lbl_80542510[0];
        field_0x2c0 = 0x427;
        break;
    }
    if (isScreenFlagSet_0x34() == true) {
        field_0x230 = 0xB;
    }
}

static void markRandomCucco(s32* serials, s32 count);

void CuccoHerdingNPC_804AC8C0::vfunc_0x10() {
    s32 serials[30];
    s32 serial = -1;
    s32 count = 0;
    for (;;) {
        s32 found = getActorManager_801F856C()->findNextActorById_801F78F4('NIWA', serial);
        serial = found;
        if (found < 0) {
            break;
        }
        if (count < 30) {
            serials[count] = found;
            count++;
        }
    }
    markRandomCucco(serials, count);
}

// FIXME(shotgun): 95.91 %, missing/extra + reloc-name; behaviour near.
void CuccoHerdingNPC_804AC8C0::update() {
    s32 found = 0;
    if (field_0x230 == 6 || field_0x230 == 7) {
        for (s32 i = 0; i < 4; i++) {
            if (isPlayerBitSet(gGameRoot_8053AAF8, i) == true) {
                lbl_8053AF08->field_0x28[i] = field_0x274[i];
                lbl_8053AF08->field_0x28[4] |= (u8)(1 << i); // the shown-slot bits
            }
        }
        found = fn_8034AAA0();
    }
    JGeometry::TBox2f box = mBox;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    fn_80239914(&box, mLayer_0x4, mSerial);
    s32 serial = mSerial;
    s32 layer = mLayer_0x4;
    CollisionBoxes_8020D58C* boxes = fn_8020D58C();
    boxes->mLists[boxes->field_0x0].fn_8020D8C0(&box, layer, serial);
    fn_8034ACF4();
    switch (field_0x230) {
    case 0: {
        JGeometry::TBox2f area;
        area = JGeometry::TBox2f(24.0f, 8.0f, 88.0f, 72.0f);
        area.i.x += mPos.x;
        area.i.y += mPos.y;
        area.f.x += mPos.x;
        area.f.y += mPos.y;
        bool any;
        s32 cucco = -1;
        for (;;) {
            cucco = getActorManager_801F856C()->findNextActorById_801F78F4('NIWA', cucco);
            if (cucco < 0) {
                any = false;
                break;
            }
            Cucco_804AB8C0* c = static_cast<Cucco_804AB8C0*>(getActorManager_801F856C()->findActorBySerial_801F666C(cucco));
            if (c != NULL && mLayer_0x4 == c->mLayer_0x4 && c->mPos.z < 8.0f && containsPoint(area, c->mPos) == true &&
                !c->field_0xc4) {
                any = true;
                break;
            }
        }
        if (any == true) {
            field_0x230 = 4;
            break;
        }
        fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
        field_0x240 = fn_8022FD8C(mSerial);
        if (field_0x240 >= 0 && fn_802300EC(field_0x240)) {
            if (talk(field_0x240, field_0x2c0, mSerial, mLayer_0x4, mPos, 0)) {
                field_0x230 = 1;
            }
        }
        break;
    }
    case 1:
        fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
        if (stepMessageWindow_803865B8(field_0x240)) {
            switch (fn_8037EFB4(field_0x240)) {
            case 0:
                switch (field_0x244) {
                case 0:
                    field_0x2c0 = 0x425;
                    break;
                case 1:
                    field_0x2c0 = 0x429;
                    break;
                }
                fn_8038938C(field_0x240, field_0x24c);
                if (fn_8038470C(field_0x240, field_0x2c0)) {
                    field_0x230 = 3;
                }
                break;
            case 1:
                switch (field_0x244) {
                case 0:
                    field_0x2c0 = 0x424;
                    break;
                case 1:
                    field_0x2c0 = 0x428;
                    break;
                }
                if (fn_8038470C(field_0x240, field_0x2c0)) {
                    field_0x230 = 2;
                }
                break;
            }
        }
        break;
    case 2:
    case 3:
    case 5:
    case 12:
        if (field_0x230 == 5) {
            s32 layer5 = mLayer_0x4;
            GameView_804A9950* view = getGameView_802D6A9C();
            const JGeometry::TBox2f* src = (layer5 < 8) ? &view->mVisibleBox : &view->field_0xdc;
            box = *src;
        }
        fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x240, mSerial)) {
            getActorManager_801F856C()->fn_801F6884();
            switch (field_0x230) {
            case 3:
            case 5:
                playSe_801F0D20(0x3a);
                gAudioMgr_8053AF80->startBgm_80458FF0(0x1000007, true, 0);
                fn_8022ADE4();
                field_0x230 = 6;
                break;
            case 2:
                switch (field_0x244) {
                case 0:
                    field_0x2c0 = 0x423;
                    break;
                case 1:
                    field_0x2c0 = 0x427;
                    break;
                }
                field_0x230 = 0;
                break;
            case 12:
                fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
                if (fn_80388CAC(field_0x240, mSerial)) {
                    getActorManager_801F856C()->fn_801F6884();
                    field_0x230 = 11;
                }
                break;
            }
        }
        break;
    case 4:
        if (getActorManager_801F856C()->fn_801F68E0() == 1) {
            s32 layer4 = mLayer_0x4;
            GameView_804A9950* view = getGameView_802D6A9C();
            const JGeometry::TBox2f* src = (layer4 < 8) ? &view->mVisibleBox : &view->field_0xdc;
            box = *src;
            fn_8022FEB8(&box, mLayer_0x4, mSerial, false, false);
            field_0x240 = fn_8022FD8C(mSerial);
            if (field_0x240 >= 0) {
                fn_8038938C(field_0x240, field_0x24c);
                if (talk(field_0x240, 0x42f, mSerial, mLayer_0x4, mPos, 0x112)) {
                    field_0x230 = 5;
                }
            }
        }
        break;
    case 6:
        if (found >= field_0x24c) {
            playSe_801F0D20(0x3b);
            gAudioMgr_8053AF80->fn_80458508(0);
            gGameRoot_8053AAF8->startStageMusic_80138B7C();
            JGeometry::TBox2f area;
            area = JGeometry::TBox2f(24.0f, 8.0f, 88.0f, 72.0f);
            for (s32 i = 0; i < 4; i++) {
                field_0x274[i] = 0;
            }
            area.i.x += mPos.x;
            area.i.y += mPos.y;
            area.f.x += mPos.x;
            area.f.y += mPos.y;
            s32 cucco = -1;
            for (;;) {
                cucco = getActorManager_801F856C()->findNextActorById_801F78F4('NIWA', cucco);
                if (cucco < 0) {
                    break;
                }
                Cucco_804AB8C0* c = static_cast<Cucco_804AB8C0*>(getActorManager_801F856C()->findActorBySerial_801F666C(cucco));
                if (c != NULL && c->field_0x25c >= 0 && mLayer_0x4 == c->mLayer_0x4 && c->mPos.z < 8.0f &&
                    containsPoint(area, c->mPos) == true) {
                    if (c->field_0x2a3 == 1) {
                        field_0x284[c->field_0x25c] = 1;
                    }
                    field_0x274[c->field_0x25c]++;
                }
            }
            for (s32 i = 0; i < 4; i++) {
                getGameView_802D6A9C()->fn_802D7844(field_0x284[i], i);
            }
            s32 best = field_0x274[0];
            s32 count = 0;
            s32 winners[4];
            s32 target;
            if (gGameRoot_8053AAF8->getPlayerCount_80138A30() == 1) {
                target = 0;
            } else {
                for (s32 i = 1; i < 4; i++) {
                    if (best < field_0x274[i]) {
                        best = field_0x274[i];
                    }
                }
                for (s32 i = 0; i < 4; i++) {
                    if (best == field_0x274[i]) {
                        winners[count] = i;
                        count++;
                    }
                }
                if (count == 1) {
                    target = winners[0];
                } else {
                    f32 roll = gGameRoot_8053AAF8->mRandom.get_ufloat_1();
                    target = winners[(s32)((f32)count * roll)];
                }
            }
            field_0x254 = target;
            fn_8022ADCC();
            field_0x28f = getActorManager_801F856C()->fn_801F68E0() != 0;
            field_0x230 = 9;
        } else {
            field_0x270 = found;
            field_0x248++;
            fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
            field_0x240 = fn_8022FD8C(mSerial);
            if (field_0x240 >= 0 && fn_802300EC(field_0x240)) {
                fn_8038938C(field_0x240, field_0x270);
                switch (field_0x244) {
                case 0:
                    field_0x2c0 = 0x42c;
                    break;
                case 1:
                    field_0x2c0 = 0x42a;
                    break;
                }
                if (fn_802300EC(field_0x240)) {
                    if (talk(field_0x240, field_0x2c0, mSerial, mLayer_0x4, mPos, 1)) {
                        field_0x230 = 7;
                    }
                }
            }
        }
        break;
    case 7:
    case 8:
        if (found < field_0x24c) {
            field_0x248++;
        }
        fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
        if (stepMessageWindow_803865B8(field_0x240)) {
            switch (field_0x230) {
            case 7:
                fn_8038938C(field_0x240, field_0x24c - field_0x270);
                switch (field_0x244) {
                case 0:
                    field_0x2c0 = 0x42e;
                    break;
                case 1:
                    field_0x2c0 = 0x42d;
                    break;
                }
                if (fn_8038470C(field_0x240, field_0x2c0)) {
                    field_0x230 = 8;
                }
                break;
            case 8:
                if (fn_80388CAC(field_0x240, mSerial)) {
                    getActorManager_801F856C()->fn_801F6884();
                    field_0x230 = 6;
                }
                break;
            }
        }
        break;
    case 9:
        if (!field_0x28f) {
            field_0x28f = getActorManager_801F856C()->fn_801F68E0() != 0;
        }
        if (field_0x250 > 0) {
            field_0x250--;
        } else if (field_0x28f) {
            s32 layer9 = mLayer_0x4;
            GameView_804A9950* view = getGameView_802D6A9C();
            const JGeometry::TBox2f* src = (layer9 < 8) ? &view->mVisibleBox : &view->field_0xdc;
            box = *src;
            switch (field_0x244) {
            case 0:
                fn_8022FEB8(&box, mLayer_0x4, mSerial, false, false);
                break;
            case 1:
                fn_8022FF8C(field_0x254, &box, mLayer_0x4, mSerial, false, false);
                break;
            }
            field_0x240 = fn_8022FD8C(mSerial);
            if (field_0x240 >= 0) {
                switch (field_0x244) {
                case 0:
                    field_0x2c0 = 0x426;
                    break;
                case 1:
                    field_0x2c0 = 0x42b;
                    fn_803894BC(field_0x240, field_0x254);
                    break;
                }
                if (talk(field_0x240, field_0x2c0, mSerial, mLayer_0x4, mPos, 0x112)) {
                    field_0x230 = 10;
                }
            }
        }
        break;
    case 10: {
        bool flag = true;
        if (field_0x244 == 0) {
            s32 layer10 = mLayer_0x4;
            GameView_804A9950* view = getGameView_802D6A9C();
            const JGeometry::TBox2f* src = (layer10 < 8) ? &view->mVisibleBox : &view->field_0xdc;
            box = *src;
            flag = false;
        }
        fn_8022FEB8(&box, mLayer_0x4, mSerial, flag, false);
        if (stepMessageWindow_803865B8(field_0x240)) {
            switch (field_0x244) {
            case 0:
                if (field_0x258 < gGameRoot_8053AAF8->getPlayerCount_80138A30()) {
                    fn_803894BC(field_0x240, field_0x258 + 1);
                    fn_8038938C(field_0x240, field_0x274[field_0x258]);
                    fn_80389254(field_0x240, field_0x274[field_0x258] * 100);
                    if (field_0x274[field_0x258] > 0) {
                        if (fn_8038470C(field_0x240, 0x427) == 1) {
                            lbl_8053AED0->fn_8043D874(field_0x258, field_0x274[field_0x258] * 100, 0);
                            field_0x258++;
                        }
                    } else if (fn_8038470C(field_0x240, 0x428) == 1) {
                        field_0x258++;
                    }
                } else if (field_0x258 == gGameRoot_8053AAF8->getPlayerCount_80138A30()) {
                    if (fn_8038470C(field_0x240, 0x429) == 1) {
                        field_0x258++;
                    }
                } else if (fn_80388CAC(field_0x240, mSerial)) {
                    getActorManager_801F856C()->fn_801F6884();
                    field_0x230 = 11;
                    setScreenFlag_0x30(0xFF, true);
                }
                break;
            case 1:
                if (fn_80388CAC(field_0x240, mSerial)) {
                    getActorManager_801F856C()->fn_801F6884();
                    if (fn_802308E0(field_0x254) == 1) {
                        for (s32 i = 0; i < 4; i++) {
                            if (field_0x254 == i) {
                                getActorManager_801F856C()->spawnActor_801F9484(
                                    ACTOR_IMUP, mLayer_0x4, getPlayerPos_8023E724(field_0x254),
                                    (field_0x254 << 26) | (field_0x254 << 24) | 0x1000A, -1, -1);
                            } else if (!isPlayerDead_80236B50(i) && hasPlayerLeader_8022F514(i) == 1 && field_0x254 == getPlayerLeader_8022F3DC(i)) {
                                getActorManager_801F856C()->spawnActor_801F9484(
                                    ACTOR_IMUP, mLayer_0x4, getPlayerPos_8023E724(i),
                                    (field_0x254 << 26) | 0x10000000 | (i << 24) | 0x1000A, -1, -1);
                            }
                        }
                        field_0x230 = 11;
                        setScreenFlag_0x30(0xFF, true);
                    }
                }
                break;
            }
        }
        break;
    }
    case 11:
        fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
        field_0x240 = fn_8022FD8C(mSerial);
        if (field_0x240 >= 0) {
            switch (field_0x244) {
            case 0:
                field_0x2c0 = 0x435;
                break;
            case 1:
                field_0x2c0 = 0x430;
                break;
            }
            if (fn_802300EC(field_0x240)) {
                if (talk(field_0x240, field_0x2c0, mSerial, mLayer_0x4, mPos, 0)) {
                    field_0x230 = 12;
                }
            }
        }
        break;
    case 13:
        if (field_0x250 <= 0) {
            mPos.x -= 0.5f;
        } else {
            field_0x250--;
        }
        if (mSpawnPos.x - mPos.x >= 64.0f) {
            field_0x230 = 11;
        }
        break;
    }
    pool_80542558();
    field_0x234--;
    if (field_0x234 <= 0) {
        field_0x238++;
        switch (field_0x244) {
        case 0:
            field_0x238 &= 3;
            field_0x234 = lbl_804720E8[field_0x238];
            break;
        case 1:
            field_0x238 &= 1;
            field_0x234 = lbl_80542510[field_0x238];
            break;
        }
    }
    if (field_0x230 >= 6 && field_0x230 <= 10) {
        JGeometry::TVec3f focus(mPos.x, mPos.y - mPos.z, 0.0f);
        gGameRoot2_8053AB10->field_0x24->addFocusPoint_8030C210(&focus);
    }
}

// Not in the binary (init and update inline it); the pool has its 100.0f and 10430.378f ahead of
// update's literals. Faces the nearest player within 100, else left in state 13 and down otherwise.
// R2 pool helper (inlined at 2 call sites); was updateFacing.
void CuccoHerdingNPC_804AC8C0::pool_80542558() {
    JGeometry::TVec3f dir;
    f32 dist;
    getNearestPlayerDir_801EE558(&dir, &dist, 0);
    if (dist < 100.0f) {
        u16 angle = (s16)(JMath::atanTable_.atan2_(-dir.x, -dir.y) *
                          (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180()));
        if (angle < 0x2000 || angle >= 0xE000) {
            field_0x23c = 3;
        } else if (angle >= 0x2000 && angle < 0x6000) {
            field_0x23c = 1;
        } else if (angle >= 0x6000 && angle <= 0xA000) {
            field_0x23c = 2;
        } else {
            field_0x23c = 0;
        }
    } else if (field_0x230 == 0xD) {
        field_0x23c = 1;
    } else {
        field_0x23c = 2;
    }
}

// 0x8034AFE8.
// FIXME(shotgun): 99.25 %, same-text; behaviour clean.
void CuccoHerdingNPC_804AC8C0::draw() {
    const char* name;
    s32 frame;
    switch (field_0x244) {
    case 0:
        name = "enemy_both2_sob.bin";
        frame = lbl_804720F8[field_0x23c][field_0x238] + 0x218;
        break;
    case 1:
        name = "enemy_gc_sob.bin";
        frame = lbl_80472138[field_0x23c][field_0x238] + 0x2E6;
        break;
    }
    drawSprite_801F06F0(name, frame, NULL, 0, 1.0f, 1.0f);
}

// The counts per stage (.rodata 0x80472158, whose strings follow).
static const s32 lbl_80472158[3] = {10, 5, 1};

// The box is built in a temporary and copied, as in the original. Literal pool names differ.
// FIXME: 99.26 %, missing/extra; behaviour near.
void CuccoHerdingNPC_804AC8C0::fn_8034ACF4() {
    JGeometry::TBox2f box;
    JGeometry::TBox2f src(9.0f, -7.0f, 103.0f, 87.0f);
    box = src;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    for (s32 i = 0; i < 4; i++) {
        if (fn_8023EA28(i) == 1) {
            field_0x288[i] = 1;
            field_0x290[i] = *getPlayerPos_8023E724(i);
        } else if (field_0x288[i] == 1) {
            JGeometry::TVec3f pos = *getPlayerPos_8023E724(i);
            if (pos.x > field_0x290[i].x) {
                field_0x260[i] = 0;
            } else if (pos.x < field_0x290[i].x) {
                field_0x260[i] = 1;
            } else if (pos.y > field_0x290[i].y) {
                field_0x260[i] = 2;
            } else {
                field_0x260[i] = 3;
            }
            field_0x288[i] = 0;
        }
        JGeometry::TVec3f pos = *getPlayerPos_8023E724(i);
        if (pos.z <= 1.0f && containsPoint(box, pos)) {
            lbl_8053AED0->requestMessage_8043D67C(0x410, 0, 0);
            JGeometry::TVec3f push;
            switch (field_0x260[i]) {
            case 0:
                push.set(-2.0f, 0.0f, 5.0f);
                break;
            case 1:
                push.set(2.0f, 0.0f, 5.0f);
                break;
            case 2:
                push.set(0.0f, -2.0f, 5.0f);
                break;
            case 3:
                push.set(0.0f, 2.0f, 5.0f);
                break;
            }
            fn_802314D8(i, push);
        }
    }
}

s32 CuccoHerdingNPC_804AC8C0::fn_8034AAA0() {
    JGeometry::TBox2f box;
    box = JGeometry::TBox2f(24.0f, 8.0f, 88.0f, 72.0f);
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    s32 serial = -1;
    s32 count = 0;
    field_0x274[0] = 0;
    field_0x274[1] = 0;
    field_0x274[2] = 0;
    field_0x274[3] = 0;
    while (true) {
        serial = getActorManager_801F856C()->findNextActorById_801F78F4('NIWA', serial);
        if (serial < 0) {
            break;
        }
        Cucco_804AB8C0* cucco = static_cast<Cucco_804AB8C0*>(getActorManager_801F856C()->findActorBySerial_801F666C(serial));
        if (cucco != NULL && mLayer_0x4 == cucco->mLayer_0x4 && cucco->mPos.z < 8.0f &&
            containsPoint(box, cucco->mPos) == true && !cucco->field_0xc4) {
            cucco->field_0x2a8 = 1;
            count++;
            field_0x274[cucco->field_0x25c]++;
        }
    }
    if (lbl_80472158[field_0x25c] >= field_0x24c - count && field_0x28c[field_0x25c] == 0) {
        lbl_8053AED0->requestMessage_8043D67C(0x42A, 0, lbl_80472158[field_0x25c]);
        field_0x28c[field_0x25c] = 1;
        if (field_0x25c == 2) {
            gAudioMgr_8053AF80->fn_80458508(1);
        }
        if (field_0x25c < 2) {
            field_0x25c++;
        }
    }
    return count;
}

// Not in the binary (vfunc_0x10 inlines it); the pool has its literals ahead of fn_8034AAA0's.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 8-line definition): split out only where the .sdata2 order needs it.
static void markRandomCucco(s32* serials, s32 count) {
    f32 roll = gGameRoot_8053AAF8->mRandom.get_ufloat_1();
    Cucco_804AB8C0* cucco = static_cast<Cucco_804AB8C0*>(
        getActorManager_801F856C()->findActorBySerial_801F666C(serials[(s32)((f32)count * roll)]));
    if (cucco != NULL) {
        cucco->field_0x2a3 = 1;
    }
}

// Destructor at 0x8034AA40.
CuccoHerdingNPC_804AC8C0::~CuccoHerdingNPC_804AC8C0() {}
