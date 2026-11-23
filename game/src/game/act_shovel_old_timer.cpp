#include "game/GameAudio.h"
#include "game/BgRewrite.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include "game/Message.h"
#include "game/Player.h"
#include "game/Unk_8045A080.h"
#include "game/act/act_shovel_old_timer.h"

void ShovelOldTimer_804AAB40::init() {
    mBox.set(-8.0f, -8.0f, 8.0f, 8.0f);
    field_0x230 = getGameView_802D6A9C()->mVisibleBox;
    field_0x240.x = mPos.x;
    field_0x240.y = mPos.y;
    field_0x240.z = mPos.z;
    mState = 0;
    field_0x250 = -1;
    field_0x254 = -1;
    field_0x258 = false;
    field_0x259 = false;
    field_0x25a = false;
    if (getGameView_802D6A9C()->isScreenFlagSet_802D800C(0x14)) {
        GameView_804A9950* view = getGameView_802D6A9C();
        gBgRewrite_8053AC98->fn_802A3948(&view->field_0x13280, 0, 0, 0x7A);
    } else {
        getGameView_802D6A9C()->setSwitchBit_802D8050(0x15, false, 0xFF);
    }
}

ShovelOldTimer_804AAB40::~ShovelOldTimer_804AAB40() {}

static inline void setViewDigPos(JGeometry::TVec3f pos) {
    getGameView_802D6A9C()->setDigPos(pos);
}

inline void ShovelOldTimer_804AAB40::setDigPos(const JGeometry::TVec3f& pos) {
    field_0x240.x = pos.x;
    field_0x240.y = pos.y;
    field_0x240.z = pos.z;
    setViewDigPos(pos);
}

inline void ShovelOldTimer_804AAB40::showHint(s32 player, s32 msg) {
    if (player == field_0x250 || (arePlayersInSameGroup_8022E438(player, field_0x250) && fn_8022CAF8(player) == 4)) {
        lbl_8053AED0->requestMessage_8043D67C(msg, player, 0);
    }
}

// Stack slot order and volatile r4-r6 in the inlined findDigSpot still differ.
// FIXME: 99.78 %, operands; behaviour clean.
void ShovelOldTimer_804AAB40::update() {
    JGeometry::TBox2f box = mBox;
    box.i.x += mPos.x;
    box.i.y += mPos.y;
    box.f.x += mPos.x;
    box.f.y += mPos.y;
    fn_80239914(&box, mLayer_0x4, mSerial);
    switch (mState) {
    case 0: {
        fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
        s32 player = fn_8022FD8C(mSerial);
        if (player >= 0) {
            if (fn_802300EC(player)) {
                if (getGameView_802D6A9C()->isScreenFlagSet_802D800C(0x14)) {
                    if (talk(player, 0x834, mLayer_0x4, mSerial, mPos, 0x28)) {
                        field_0x254 = player;
                        mState = 3;
                    }
                } else {
                    bool found = findDigSpot();
                    if (!found) {
                        if (talk(player, 0x84F, mLayer_0x4, mSerial, mPos, 0x28)) {
                            field_0x254 = player;
                            mState = 3;
                        }
                    } else {
                        if (talk(player, 0x835, mLayer_0x4, mSerial, mPos, 8)) {
                            field_0x250 = player;
                            mState = 1;
                        }
                    }
                }
            }
        }
        break;
    }
    case 1:
        fn_8022FF8C(field_0x250, &box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x250, mSerial)) {
            field_0x240.x += 8.0f;
            field_0x240.y += 8.0f;
            field_0x258 = false;
            field_0x259 = true;
            mState = 2;
        }
        break;
    case 2:
        if (!getGameView_802D6A9C()->isScreenFlagSet_802D800C(0x14)) {
            fn_8031179C();
        } else if (!field_0x25a) {
            gBgRewrite_8053AC98->fn_802A3948(&field_0x240, 0, 0, 0x7A);
            gAudioMgr_8053AF80->playSystemSound_80458880(3, -1);
            field_0x25a = true;
        }
        if (!field_0x258) {
            fn_8022FEB8(&box, mLayer_0x4, mSerial, true, false);
            s32 p = fn_8022FD8C(mSerial);
            if (p >= 0) {
                if (fn_802300EC(p)) {
                    u32 msg;
                    if (arePlayersInSameGroup_8022E438(p, field_0x250)) {
                        if (getGameView_802D6A9C()->isScreenFlagSet_802D800C(0x14)) {
                            msg = 0x834;
                        } else {
                            msg = 0x832;
                        }
                    } else {
                        msg = 0x833;
                    }
                    if (talk(p, msg, mLayer_0x4, mSerial, mPos, 8)) {
                        field_0x254 = p;
                        field_0x258 = true;
                    }
                }
            }
        } else {
            fn_8022FF8C(field_0x254, &box, mLayer_0x4, mSerial, true, false);
            if (fn_80388CAC(field_0x254, mSerial)) {
                field_0x254 = -1;
                field_0x258 = false;
            }
        }
        break;
    case 3:
        fn_8022FF8C(field_0x254, &box, mLayer_0x4, mSerial, true, false);
        if (fn_80388CAC(field_0x254, mSerial)) {
            field_0x254 = -1;
            mState = 0;
        }
        break;
    }
}

void ShovelOldTimer_804AAB40::draw() {
    drawSprite_801F06F0("enemy_both2_sob.bin", 0x386, NULL, 0, 1.0f, 1.0f);
    showGbaSprite_803075AC(mSerial, mLayer_0x4, &mPos, 0x386, 0, 2, -1, false, 0, 0);
}

// Picks a random start in the visible box, then scans the tiles in 16-unit steps for the
// dig-spot tile code 0x1B0; on a hit it records the spot (field_0x240) and the view's marker.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 31-line definition): split out only where the .sdata2 order needs it.
bool ShovelOldTimer_804AAB40::findDigSpot() {
    JGeometry::TBox2f box = field_0x230;
    box.i.x += 64.0f;
    box.i.y += 32.0f;
    box.f.x -= 32.0f;
    box.f.y -= 32.0f;
    f32 x = box.i.x + gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 416.0f;
    f32 y = box.i.y + gGameRoot_8053AAF8->mRandom.get_ufloat_1() * 320.0f;
    f32 z = 0.0f;
    f32 step = 16.0f;
    for (int j = 0; j < 20; j++) {
        for (int i = 0; i < 26; i++) {
            JGeometry::TVec3f pos(x, y, z);
            if (getRawTile_80226850(&pos, 0) == 0x1B0) {
                pos.x = (f32)((u32)pos.x & ~0xF);
                pos.y = (f32)((u32)pos.y & ~0xF);
                setDigPos(pos);
                return true;
            }
            x += step;
            if (x >= box.f.x) {
                x = box.i.x;
            }
        }
        y += step;
        if (y >= box.f.y) {
            y = box.i.y;
        }
    }
    return false;
}

// update() at 0x80312114 calls fn_8031179C out of line.
#pragma auto_inline off
void ShovelOldTimer_804AAB40::fn_8031179C() {
    for (int i = 0; i < 4; i++) {
        JGeometry::TVec2f pos;
        if (fn_8023A97C(i, &field_0x230, 0, &pos)) {
            // getTileAt_80226998 reads only x and y of its position; the 8-byte stack TVec2f is passed as TVec3f
            // (as act_keese does).
            if (getTileAt_80226998((const JGeometry::TVec3f*)&pos, 0) == 0x1B0) {
                if (!field_0x259) {
                    showHint(field_0x250, 0x826);
                } else {
                    s32 dx = (((u32)pos.x & ~0xF) + 8) - (s32)field_0x240.x;
                    s32 dy = (((u32)pos.y & ~0xF) + 8) - (s32)field_0x240.y;
                    if (dy < -0x30) {
                        showHint(i, 0x825);
                    } else if (dy > 0x30) {
                        showHint(i, 0x826);
                    } else if (dx < -0x30) {
                        showHint(i, 0x827);
                    } else if (dx > 0x30) {
                        showHint(i, 0x828);
                    } else if (dy < -0x10) {
                        showHint(i, 0x829);
                    } else if (dy > 0x10) {
                        showHint(i, 0x82A);
                    } else if (dx < -0x10) {
                        showHint(i, 0x82B);
                    } else if (dx > 0x10) {
                        showHint(i, 0x82C);
                    } else if (dy < -8) {
                        showHint(i, 0x82D);
                    } else if (dy > 8) {
                        showHint(i, 0x82E);
                    } else if (dx < -8) {
                        showHint(i, 0x82F);
                    } else if (dx > 8) {
                        showHint(i, 0x830);
                    } else {
                        getGameView_802D6A9C()->setSwitchBit_802D8050(0x14, true, 0xFF);
                    }
                }
            }
        }
    }
}
#pragma auto_inline reset
