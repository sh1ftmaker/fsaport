#include "game/cls_804B16A8.h"
#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/Message.h"
#include "game/save_data_803B8C74.h"
#include "game/act/button_icons_80440BF4.h"
#include "JSystem/J2D/J2DScreen.h"
#include "JSystem/J2D/J2DGrafContext.h"
#include "JSystem/J2D/J2DPicture.h"
#include "JSystem/JUtility/JUTFont.h"

void* getMain2DResource_804030C4(u32 type, const char* name);

typedef void (RuleScreen_804B16A8::*StateFunc)();
StateFunc lbl_804B1690[2] = {
    &RuleScreen_804B16A8::fn_8043651C,
    &RuleScreen_804B16A8::fn_804361A8,
};

static inline void rotateArrowPane(J2DPane* pane, f32 angle) {
    f32 h = pane->getHeight();
    f32 w = pane->getWidth();
    pane->rotate(0.5f * w, 0.5f * h, J2DROTATE_Z, angle);
}

// FIXME(shotgun): 99.90 %, scheduling; behaviour clean.
RuleScreen_804B16A8::RuleScreen_804B16A8() {
    mScreen = new J2DScreen;
    if (gGameRoot_8053AAF8->field_0x7f == 1) {
        mScreen->setPriority("rule.blo", 0x1100000, lbl_80529DEC.mEntryArchive_0x28);
    } else {
        mScreen->setPriority("rule_bat.blo", 0x1100000, lbl_80529DEC.mEntryArchive_0x28);
    }
    field_0x8 = new PaneCtrl_804AF86C;
    field_0x8->setPane_80402F04(mScreen->search('Popbbk03'));
    field_0xc[0] = new PaneCtrl_804AF86C;
    field_0xc[0]->setPane_80402F04(mScreen->search('Par01'));
    field_0xc[1] = new PaneCtrl_804AF86C;
    field_0xc[1]->setPane_80402F04(mScreen->search('Par02'));
    field_0x14 = new PaneCtrl_804AF86C;
    field_0x14->setPane_80402F04(mScreen->search('Ndata03'));
    field_0x18 = new PaneCtrl_804AF86C;
    field_0x18->setPane_80402F04(mScreen->search('Ndata02'));
    field_0x1c[0] = new PaneCtrl_804AF86C;
    field_0x1c[0]->setPane_80402F04(mScreen->search('Prba01'));
    field_0x1c[1] = new PaneCtrl_804AF86C;
    field_0x1c[1]->setPane_80402F04(mScreen->search('Prba02'));
    field_0x1c[2] = new PaneCtrl_804AF86C;
    field_0x1c[2]->setPane_80402F04(mScreen->search('Prba03'));
    field_0x1c[3] = new PaneCtrl_804AF86C;
    field_0x1c[3]->setPane_80402F04(mScreen->search('Prba04'));
    if (gGameRoot_8053AAF8->field_0x7f == 1) {
        field_0x1c[5] = NULL;
        field_0x1c[4] = NULL;
    } else {
        field_0x1c[4] = new PaneCtrl_804AF86C;
        field_0x1c[4]->setPane_80402F04(mScreen->search('Prba05'));
        field_0x1c[5] = new PaneCtrl_804AF86C;
        field_0x1c[5]->setPane_80402F04(mScreen->search('Prba06'));
    }
    field_0x34 = new PaneCtrl_804AF86C;
    field_0x34->setPane_80402F04(mScreen->search('Ptopbb07'));
    field_0x38 = lbl_8053AED8->field_0x8;
    const ResTIMG* tex = (const ResTIMG*)getMain2DResource_804030C4('TIMG', "kk_kettei_01.bti");
    lbl_8053AED8->fn_80440D78(tex);
    lbl_8053AED8->fn_80440CE4((const ResTIMG*)getMain2DResource_804030C4('TIMG', "kk_modoru_01.bti"));
    ((J2DTextBox*)mScreen->search('Ta02'))->setFont(lbl_8053AD78->getFont());
    ((J2DTextBox*)mScreen->search('Tq02'))->setFont(lbl_8053AD78->getFont());
    ((J2DTextBox*)mScreen->search('Ta01'))->setFont(lbl_8053AD78->getFont());
    ((J2DTextBox*)mScreen->search('Tq01'))->setFont(lbl_8053AD78->getFont());
    if (gGameRoot_8053AAF8->field_0x7f == 0) {
        ((J2DTextBox*)mScreen->search('Ta03'))->setFont(lbl_8053AD78->getFont());
        ((J2DTextBox*)mScreen->search('Tq03'))->setFont(lbl_8053AD78->getFont());
    }
    field_0x62 = (s32)mScreen->search('Par01')->getRotate();
    field_0x64 = (s32)mScreen->search('Par02')->getRotate();
    field_0x3c = 0.0f;
    field_0x40 = 0.0f;
    field_0x44 = 0.0f;
    field_0x58 = 0;
    field_0x5a = 0;
    field_0x5c = 0;
    field_0x5e = 0;
    field_0x60 = 0;
    field_0x66 = 0;
    field_0x54 = 0;
    field_0x48 = 0.0f;
    field_0x68 = 0;
    field_0x4c = 0.0f;
    field_0x69 = 0;
    field_0x50 = 0.0f;
    field_0x6a = 0;
    field_0x6b = 0;
    field_0x6c = 0;
}

RuleScreen_804B16A8::~RuleScreen_804B16A8() {
    delete mScreen;
    mScreen = NULL;
    delete field_0x8;
    field_0x8 = NULL;
    delete field_0xc[0];
    field_0xc[0] = NULL;
    delete field_0xc[1];
    field_0xc[1] = NULL;
    delete field_0x14;
    field_0x14 = NULL;
    delete field_0x18;
    field_0x18 = NULL;
    for (s32 i = 0; i < 6; i++) {
        if (field_0x1c[i] != NULL) {
            delete field_0x1c[i];
            field_0x1c[i] = NULL;
        }
    }
    delete field_0x34;
    field_0x34 = NULL;
}

// FIXME(shotgun): 96.84 %, missing/extra + reloc-name; behaviour drift.
void RuleScreen_804B16A8::fn_8043670C(u8 arg) {
    f32 fontSizeW = ((J2DTextBox*)mScreen->search('Tq01'))->mFontSize.x;
    field_0x58 = 0;
    field_0x5a = 0;
    field_0x5c = (s32)(11.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 10;
    field_0x5e = (s32)(11.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 10;
    field_0x60 = 0;
    field_0x66 = -6;
    field_0x6b = 0;
    field_0x6c = arg;
    rotateArrowPane(field_0xc[0]->mPane, (f32)(field_0x62 + field_0x66));
    rotateArrowPane(field_0xc[1]->mPane, (f32)(field_0x64 + field_0x66));
    if (field_0x6c == 0) {
        f32 w1 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Tq01'))->getStringPtr(), 0x129, 0, NULL, NULL);
        field_0x68 = lbl_8050EC80.getCurrentFile().field_0x5c;
        f32 scale = fontSizeW / 24.0f;
        f32 boxW1 = w1 * scale + 24.0f;
        f32 halfW1 = boxW1 * 0.5f;
        field_0x1c[0]->mPane->move(field_0x1c[0]->field_0x10 - halfW1, field_0x1c[0]->field_0xc);
        field_0x1c[0]->mPane->resize(boxW1, field_0x1c[0]->field_0x1c);
        field_0x1c[1]->mPane->move(field_0x1c[1]->field_0x10 - halfW1, field_0x1c[1]->field_0xc);
        field_0x1c[1]->mPane->resize(boxW1, field_0x1c[1]->field_0x1c);

        f32 w2 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Tq02'))->getStringPtr(), 0x24D, 0, NULL, NULL);
        field_0x69 = lbl_8050EC80.getCurrentFile().field_0x68;
        f32 boxW2 = w2 * scale + 24.0f;
        f32 halfW2 = boxW2 * 0.5f;
        field_0x1c[2]->mPane->move(field_0x1c[2]->field_0x10 - halfW2, field_0x1c[2]->field_0xc);
        field_0x1c[2]->mPane->resize(boxW2, field_0x1c[2]->field_0x1c);
        field_0x1c[3]->mPane->move(field_0x1c[3]->field_0x10 - halfW2, field_0x1c[3]->field_0xc);
        field_0x1c[3]->mPane->resize(boxW2, field_0x1c[3]->field_0x1c);
    } else {
        f32 w1 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Tq01'))->getStringPtr(), 0xF8, 0, NULL, NULL);
        field_0x68 = lbl_8050EC80.getCurrentFile().field_0x5d;
        f32 scale = fontSizeW / 24.0f;
        f32 boxW1 = w1 * scale + 24.0f;
        f32 halfW1 = boxW1 * 0.5f;
        field_0x1c[0]->mPane->move(field_0x1c[0]->field_0x10 - halfW1, field_0x1c[0]->field_0xc);
        field_0x1c[0]->mPane->resize(boxW1, field_0x1c[0]->field_0x1c);
        field_0x1c[1]->mPane->move(field_0x1c[1]->field_0x10 - halfW1, field_0x1c[1]->field_0xc);
        field_0x1c[1]->mPane->resize(boxW1, field_0x1c[1]->field_0x1c);

        f32 w2 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Tq02'))->getStringPtr(), 0xF9, 0, NULL, NULL);
        field_0x69 = lbl_8050EC80.getCurrentFile().field_0x5f;
        f32 boxW2 = w2 * scale + 24.0f;
        f32 halfW2 = boxW2 * 0.5f;
        field_0x1c[2]->mPane->move(field_0x1c[2]->field_0x10 - halfW2, field_0x1c[2]->field_0xc);
        field_0x1c[2]->mPane->resize(boxW2, field_0x1c[2]->field_0x1c);
        field_0x1c[3]->mPane->move(field_0x1c[3]->field_0x10 - halfW2, field_0x1c[3]->field_0xc);
        field_0x1c[3]->mPane->resize(boxW2, field_0x1c[3]->field_0x1c);

        f32 w3 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Tq03'))->getStringPtr(), 0x24D, 0, NULL, NULL);
        field_0x6a = lbl_8050EC80.getCurrentFile().field_0x69;
        f32 boxW3 = w3 * scale + 24.0f;
        f32 halfW3 = boxW3 * 0.5f;
        field_0x1c[4]->mPane->move(field_0x1c[4]->field_0x10 - halfW3, field_0x1c[4]->field_0xc);
        field_0x1c[4]->mPane->resize(boxW3, field_0x1c[4]->field_0x1c);
        field_0x1c[5]->mPane->move(field_0x1c[5]->field_0x10 - halfW3, field_0x1c[5]->field_0xc);
        field_0x1c[5]->mPane->resize(boxW3, field_0x1c[5]->field_0x1c);
    }
    if (field_0x68 != 0) {
        field_0x48 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta01'))->getStringPtr(), 0xFC, 0, NULL, NULL);
    } else {
        field_0x48 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta01'))->getStringPtr(), 0xFD, 0, NULL, NULL);
    }
    if (field_0x6c == 0) {
        if (field_0x69 != 0) {
            field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFC, 0, NULL, NULL);
        } else {
            field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFD, 0, NULL, NULL);
        }
    } else {
        if (field_0x69 != 0) {
            field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFE, 0, NULL, NULL);
        } else {
            field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFF, 0, NULL, NULL);
        }
        if (field_0x6a != 0) {
            field_0x50 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta03'))->getStringPtr(), 0xFC, 0, NULL, NULL);
        } else {
            field_0x50 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta03'))->getStringPtr(), 0xFD, 0, NULL, NULL);
        }
    }
    fn_80434724();
    field_0x8->moveFromCentre_80402E08(0.0f, -448.0f);
    field_0x8->field_0x22 = 0;
    field_0xc[0]->moveFromCentre_80402E08(field_0x40, -448.0f + field_0x44);
    field_0xc[1]->moveFromCentre_80402E08(-field_0x40, -448.0f + field_0x44);
    field_0xc[0]->field_0x22 = 0;
    field_0xc[1]->field_0x22 = 0;
    field_0x14->moveFromCentre_80402E08(0.0f, -448.0f);
    field_0x14->field_0x22 = 0;
    field_0x18->moveFromCentre_80402E08(0.0f, -448.0f);
    field_0x18->field_0x22 = 0;
    field_0x34->moveFromCentre_80402E08(0.0f, -200.0f);
    field_0x34->field_0x22 = 0;
    field_0x38->moveFromCentre_80402E08(0.0f, -200.0f);
    field_0x38->field_0x22 = 0;
    if (gGameRoot_8053AAF8->field_0x7f != 0) {
        field_0x54 = lbl_80529DEC.field_0x23c + 1;
    } else {
        field_0x54 = lbl_80529DEC.field_0x23d + 2;
    }
}

s32 RuleScreen_804B16A8::fn_804366B8() {
    (this->*lbl_804B1690[field_0x6b])();
    if (field_0x6b != 2) {
        return 0;
    }
    return 1;
}

void RuleScreen_804B16A8::fn_80436668() {
    gGameRoot2_8053AB10->setPort_8013B490();
    mScreen->draw(0.0f, 0.0f, gGameRoot2_8053AB10->field_0x10);
    lbl_8053AED8->draw_80440EE8();
}

void RuleScreen_804B16A8::fn_8043651C() {
    u8 a = 0;
    u8 b = 0;
    u8 c = 0;
    field_0x58++;
    if (field_0x58 > 0) {
        a = field_0x8->animateMoveY_80402940(0x10, 0, -448.0f, 0.0f);
        for (int i = 0; i < 2; i++) {
            field_0xc[i]->animateMoveY_80402940(0x10, 0, -448.0f + field_0x44, field_0x44);
        }
        field_0x14->animateMoveY_80402940(0x10, 0, -448.0f, 0.0f);
        field_0x18->animateMoveY_80402940(0x10, 0, -448.0f, 0.0f);
    }
    if (field_0x58 > 0) {
        b = field_0x34->animateMoveY_80402940(0x14, 0, -200.0f, 0.0f);
    }
    if (field_0x58 > 10) {
        c = field_0x38->animateMoveY_80402940(0x16, 0, -200.0f, 0.0f);
    }
    if (a && b && c) {
        field_0x6b = 1;
    }
}

void RuleScreen_804B16A8::fn_804361A8() {
    if (lbl_8053AD30->fn_80309C4C(0x1100, field_0x54)) {
        if (field_0x6c == 0) {
            if (field_0x68 != 0) {
                lbl_8050EC80.getCurrentFile().field_0x5c = 1;
            } else {
                lbl_8050EC80.getCurrentFile().field_0x5c = 0;
            }
            if (field_0x69 != 0) {
                lbl_8050EC80.getCurrentFile().field_0x68 = 1;
            } else {
                lbl_8050EC80.getCurrentFile().field_0x68 = 0;
            }
        } else {
            if (field_0x68 != 0) {
                lbl_8050EC80.getCurrentFile().field_0x5d = 1;
            } else {
                lbl_8050EC80.getCurrentFile().field_0x5d = 0;
            }
            if (field_0x69 != 0) {
                lbl_8050EC80.getCurrentFile().field_0x5f = 1;
            } else {
                lbl_8050EC80.getCurrentFile().field_0x5f = 0;
            }
            if (field_0x6a != 0) {
                lbl_8050EC80.getCurrentFile().field_0x69 = 1;
            } else {
                lbl_8050EC80.getCurrentFile().field_0x69 = 0;
            }
        }
        field_0x6b = 2;
    } else if (lbl_8053AD30->fn_80309C4C(0x200, field_0x54)) {
        field_0x6b = 2;
    } else if (lbl_8053AD30->fn_80309A9C(4, field_0x54)) {
        if (gGameRoot_8053AAF8->field_0x7e != 0) {
            gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
            field_0x5a++;
            if (field_0x6c == 0) {
                if (field_0x5a > 1) {
                    field_0x5a = 0;
                }
            } else {
                if (field_0x5a > 2) {
                    field_0x5a = 0;
                }
            }
            fn_80434724();
        }
    } else if (lbl_8053AD30->fn_80309A9C(8, field_0x54)) {
        if (gGameRoot_8053AAF8->field_0x7e != 0) {
            gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
            field_0x5a--;
            if (field_0x6c == 0) {
                if (field_0x5a < 0) {
                    field_0x5a = 1;
                }
            } else {
                if (field_0x5a < 0) {
                    field_0x5a = 2;
                }
            }
            fn_80434724();
        }
    } else if (lbl_8053AD30->fn_80309A9C(1, field_0x54)) {
        gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
        field_0x60 = -1;
        fn_80434370();
    } else if (lbl_8053AD30->fn_80309A9C(2, field_0x54)) {
        gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
        field_0x60 = 1;
        fn_80434370();
    }
    fn_80435E20();
}

void RuleScreen_804B16A8::fn_80435E20() {
    if (field_0x60 == 0) {
        if (--field_0x5e <= 0) {
            field_0x5e = (s32)(11.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1()) + 10;
            if (field_0x66 == -6) {
                field_0x66 = 6;
            } else {
                field_0x66 = -6;
            }
            rotateArrowPane(field_0xc[0]->mPane, (f32)(field_0x62 + field_0x66));
            rotateArrowPane(field_0xc[1]->mPane, (f32)(field_0x64 + field_0x66));
        }
        return;
    }
    if (field_0x60 == 1 || field_0x60 == -1) {
        field_0x66 = 0;
        rotateArrowPane(field_0xc[0]->mPane, (f32)(field_0x62 + field_0x66));
        rotateArrowPane(field_0xc[1]->mPane, (f32)(field_0x64 + field_0x66));
    }
    s32 t;
    if (field_0x60 < 0) {
        if (field_0x60 > -5) {
            t = -field_0x60;
        } else {
            t = field_0x60 + 10;
            if (field_0x60 == -10) {
                field_0x60 = 0;
            }
        }
        field_0xc[0]->moveFromCentre_80402E08(field_0x40 + (-5.0f * (f32)(t * t) / 25.0f), field_0x44);
        field_0xc[1]->moveFromCentre_80402E08(-field_0x40, field_0x44);
        if (field_0x60 != 0) {
            field_0x60--;
        }
    } else if (field_0x60 > 0) {
        if (field_0x60 < 5) {
            t = field_0x60;
        } else {
            t = 10 - field_0x60;
            if (field_0x60 == 10) {
                field_0x60 = 0;
            }
        }
        field_0xc[0]->moveFromCentre_80402E08(field_0x40, field_0x44);
        field_0xc[1]->moveFromCentre_80402E08(-field_0x40 + (5.0f * (f32)(t * t) / 25.0f), field_0x44);
        if (field_0x60 != 0) {
            field_0x60++;
        }
    }
}


// FIXME: 99.77 %, scheduling+regalloc; behaviour clean.
void RuleScreen_804B16A8::fn_80434724() {
    f32 fontSizeW = ((J2DTextBox*)mScreen->search('Ta01'))->mFontSize.x;
    f32 scale = fontSizeW / lbl_8053AD78->mFont->getCellWidth();

    switch (field_0x5a) {
    case 0:
        ((J2DTextBox*)mScreen->search('Ta01'))->setBlackWhite(JUtility::TColor(100, 20, 0, 255), JUtility::TColor(100, 20, 0, 0));
        ((J2DTextBox*)mScreen->search('Ta02'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
        ((J2DTextBox*)mScreen->search('Tq01'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
        ((J2DTextBox*)mScreen->search('Tq02'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
        ((J2DPicture*)mScreen->search('Prba01'))->setBlackWhite(JUtility::TColor(255, 255, 120, 255), JUtility::TColor(255, 255, 120, 0));
        ((J2DPicture*)mScreen->search('Prba02'))->setBlackWhite(JUtility::TColor(255, 255, 120, 255), JUtility::TColor(255, 255, 120, 0));
        ((J2DPicture*)mScreen->search('Prba03'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
        ((J2DPicture*)mScreen->search('Prba04'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
        mScreen->search('Prba01')->setAlpha(0x78);
        mScreen->search('Prba02')->setAlpha(0x78);
        mScreen->search('Prba03')->setAlpha(0x46);
        mScreen->search('Prba04')->setAlpha(0x46);
        field_0x40 = (mScreen->search('Ta01')->getWidth() - field_0x48 * scale) * 0.5f;
        field_0x3c = mScreen->search('Tq01')->getBounds()->i.y - mScreen->search('Tq02')->getBounds()->i.y;
        field_0x44 = mScreen->search('Ta01')->getBounds()->i.y - mScreen->search('Ta02')->getBounds()->i.y;
        if (field_0x6c == 1) {
            ((J2DTextBox*)mScreen->search('Ta03'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DTextBox*)mScreen->search('Tq03'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DPicture*)mScreen->search('Prba05'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            ((J2DPicture*)mScreen->search('Prba06'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            mScreen->search('Prba05')->setAlpha(0x46);
            mScreen->search('Prba06')->setAlpha(0x46);
        }
        break;

    case 1:
        ((J2DTextBox*)mScreen->search('Ta02'))->setBlackWhite(JUtility::TColor(100, 20, 0, 255), JUtility::TColor(100, 20, 0, 0));
        ((J2DTextBox*)mScreen->search('Ta01'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
        ((J2DTextBox*)mScreen->search('Tq02'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
        ((J2DTextBox*)mScreen->search('Tq01'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
        ((J2DPicture*)mScreen->search('Prba03'))->setBlackWhite(JUtility::TColor(255, 255, 120, 255), JUtility::TColor(255, 255, 120, 0));
        ((J2DPicture*)mScreen->search('Prba04'))->setBlackWhite(JUtility::TColor(255, 255, 120, 255), JUtility::TColor(255, 255, 120, 0));
        ((J2DPicture*)mScreen->search('Prba01'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
        ((J2DPicture*)mScreen->search('Prba02'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
        mScreen->search('Prba03')->setAlpha(0x78);
        mScreen->search('Prba04')->setAlpha(0x78);
        mScreen->search('Prba01')->setAlpha(0x46);
        mScreen->search('Prba02')->setAlpha(0x46);
        field_0x40 = (mScreen->search('Ta02')->getWidth() - field_0x4c * scale) * 0.5f;
        field_0x3c = 0.0f;
        field_0x44 = 0.0f;
        if (field_0x6c == 1) {
            ((J2DTextBox*)mScreen->search('Ta03'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DTextBox*)mScreen->search('Tq03'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DPicture*)mScreen->search('Prba05'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            ((J2DPicture*)mScreen->search('Prba06'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            mScreen->search('Prba05')->setAlpha(0x46);
            mScreen->search('Prba06')->setAlpha(0x46);
        }
        break;

    default:
        if (field_0x6c == 1) {
            ((J2DTextBox*)mScreen->search('Ta03'))->setBlackWhite(JUtility::TColor(100, 20, 0, 255), JUtility::TColor(100, 20, 0, 0));
            ((J2DTextBox*)mScreen->search('Ta02'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DTextBox*)mScreen->search('Ta01'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DTextBox*)mScreen->search('Tq03'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            ((J2DTextBox*)mScreen->search('Tq02'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DTextBox*)mScreen->search('Tq01'))->setBlackWhite(JUtility::TColor(60, 60, 60, 255), JUtility::TColor(60, 60, 60, 0));
            ((J2DPicture*)mScreen->search('Prba05'))->setBlackWhite(JUtility::TColor(255, 255, 120, 255), JUtility::TColor(255, 255, 120, 0));
            ((J2DPicture*)mScreen->search('Prba06'))->setBlackWhite(JUtility::TColor(255, 255, 120, 255), JUtility::TColor(255, 255, 120, 0));
            ((J2DPicture*)mScreen->search('Prba03'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            ((J2DPicture*)mScreen->search('Prba04'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            ((J2DPicture*)mScreen->search('Prba01'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            ((J2DPicture*)mScreen->search('Prba02'))->setBlackWhite(JUtility::TColor(0, 0, 0, 255), JUtility::TColor(0, 0, 0, 0));
            mScreen->search('Prba05')->setAlpha(0x78);
            mScreen->search('Prba06')->setAlpha(0x78);
            mScreen->search('Prba03')->setAlpha(0x46);
            mScreen->search('Prba04')->setAlpha(0x46);
            mScreen->search('Prba01')->setAlpha(0x46);
            mScreen->search('Prba02')->setAlpha(0x46);
            field_0x40 = (mScreen->search('Ta03')->getWidth() - field_0x50 * scale) * 0.5f;
            field_0x3c = mScreen->search('Tq02')->getBounds()->i.y - mScreen->search('Tq03')->getBounds()->i.y;
            field_0x44 = mScreen->search('Ta02')->getBounds()->i.y - mScreen->search('Ta03')->getBounds()->i.y;
        }
        break;
    }

    field_0xc[0]->moveFromCentre_80402E08(field_0x40, field_0x44);
    field_0xc[1]->moveFromCentre_80402E08(-field_0x40, field_0x44);
    field_0x60 = 0;
}

void RuleScreen_804B16A8::fn_80434370() {
    f32 fontSizeW = ((J2DTextBox*)mScreen->search('Ta01'))->mFontSize.x;
    f32 cellW = lbl_8053AD78->mFont->getCellWidth();
    f32 scale = fontSizeW / cellW;

    if (field_0x5a == 0) {
        if (field_0x68 == 1) {
            field_0x68 = 0;
            field_0x48 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta01'))->getStringPtr(), 0xFD, 0, NULL, NULL);
        } else {
            field_0x68 = 1;
            field_0x48 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta01'))->getStringPtr(), 0xFC, 0, NULL, NULL);
        }
        J2DPane* pane = mScreen->search('Ta01');
        field_0x40 = (pane->getWidth() - field_0x48 * scale) / 2.0f;
    } else if (field_0x5a == 1) {
        if (field_0x6c == 0) {
            if (field_0x69 == 1) {
                field_0x69 = 0;
                field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFD, 0, NULL, NULL);
            } else {
                field_0x69 = 1;
                field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFC, 0, NULL, NULL);
            }
        } else {
            if (field_0x69 == 1) {
                field_0x69 = 0;
                field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFF, 0, NULL, NULL);
            } else {
                field_0x69 = 1;
                field_0x4c = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta02'))->getStringPtr(), 0xFE, 0, NULL, NULL);
            }
        }
        J2DPane* pane = mScreen->search('Ta02');
        field_0x40 = (pane->getWidth() - field_0x4c * scale) / 2.0f;
    } else if (field_0x5a == 2) {
        if (field_0x6c == 1) {
            if (field_0x6a == 1) {
                field_0x6a = 0;
                field_0x50 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta03'))->getStringPtr(), 0xFD, 0, NULL, NULL);
            } else {
                field_0x6a = 1;
                field_0x50 = getMessageText_803832BC(((J2DTextBox*)mScreen->search('Ta03'))->getStringPtr(), 0xFC, 0, NULL, NULL);
            }
        }
    }
}
