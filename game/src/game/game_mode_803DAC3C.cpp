#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Message.h"
#include "game/THPMovie.h"
#include "game/act/button_icons_80440BF4.h"
#include "game/game_mode_803DAC3C.h"
#include "JSystem/J2D/J2DScreen.h"
#include "JSystem/J2D/J2DGrafContext.h"
#include "JSystem/J2D/J2DPicture.h"
#include "JSystem/JAudio/Interface/JAISound.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "dolphin/dvd.h"
#include "dolphin/os.h"
#include "dolphin/pad.h"
#include <string.h>
#ifdef FSA_PORT
// Port: the save region that survives a reset (0x80700000 on the GameCube) is a
// plain buffer (game/src/port/missing_functions.cpp).
extern u8 fsa_save_region[0x20];
#define FSA_SAVE_REGION fsa_save_region
#else
#define FSA_SAVE_REGION 0x80700000
#endif

// .rodata: the pane tag tables (J2DPane::search), in address order; the
// functions address them off one base (lbl_804774C8). Each has an alpha
// array of the same length in GameMode_804B0688 (field_0xa3 ...).
static const u64 lbl_804774C8[50] = {
    FSA_TAG64("Pmov01"), FSA_TAG64("Pexbk02"), FSA_TAG64("Pexbk01"), FSA_TAG64("Pexb07"), FSA_TAG64("Pexb06"), FSA_TAG64("Pexb05"), FSA_TAG64("Pexb04"),
    FSA_TAG64("Pexb03"), FSA_TAG64("Pexb02"), FSA_TAG64("Pexb01"), FSA_TAG64("Pmf06"), FSA_TAG64("Pmf05"), FSA_TAG64("Pmf04"), FSA_TAG64("Pmf03"),
    FSA_TAG64("Pmf02"), FSA_TAG64("Pmf01"), FSA_TAG64("Wgbco01"), FSA_TAG64("Pogb02"), FSA_TAG64("Pogb01"), FSA_TAG64("Tagb02"), FSA_TAG64("Tagb01"),
    FSA_TAG64("Wnop01"), FSA_TAG64("Tnop02"), FSA_TAG64("Tnop01"), FSA_TAG64("Pexpb09"), FSA_TAG64("Pexpb08"), FSA_TAG64("Pexpb07"), FSA_TAG64("Pexpb06"),
    FSA_TAG64("Pexpb05"), FSA_TAG64("Pexpb04"), FSA_TAG64("Pexpb03"), FSA_TAG64("Pexpb02"), FSA_TAG64("Pexpb01"), FSA_TAG64("Texp01"), FSA_TAG64("Wnop02"),
    FSA_TAG64("Pcon04"), FSA_TAG64("Pcon03"), FSA_TAG64("Pgba04"), FSA_TAG64("Pgba03"), FSA_TAG64("Psls04"), FSA_TAG64("Psls03"), FSA_TAG64("Tnop04"),
    FSA_TAG64("Tnop03"), FSA_TAG64("Wgbco02"), FSA_TAG64("Pogb03"), FSA_TAG64("Pogb04"), FSA_TAG64("Tagb03"), FSA_TAG64("Tagb04"), FSA_TAG64("Tnop05"),
    FSA_TAG64("Tnop06"),
};
static const u64 lbl_80477658[6] = {
    FSA_TAG64("Pmopb04"), FSA_TAG64("Pmopb03"), FSA_TAG64("Pmopb02"), FSA_TAG64("Pmopb01"), FSA_TAG64("Pmop02"), FSA_TAG64("Pmop01"),
};
static const u64 lbl_80477688[17] = {
    FSA_TAG64("Pmadbb07"), FSA_TAG64("Pmadbb06"), FSA_TAG64("Pmadbb05"), FSA_TAG64("Pmadbb04"), FSA_TAG64("Pmadbb03"), FSA_TAG64("Pmadbb02"),
    FSA_TAG64("Pmadbb01"), FSA_TAG64("Pmadb05"), FSA_TAG64("Pmadb04"), FSA_TAG64("Pmadb03"), FSA_TAG64("Pmadb02"), FSA_TAG64("Pmadb01"),
    FSA_TAG64("Pmad02"), FSA_TAG64("Pmad01"), FSA_TAG64("Phy03"), FSA_TAG64("Phy02"), FSA_TAG64("Phy01"),
};
static const u64 lbl_80477710[20] = {
    FSA_TAG64("Pmbabb07"), FSA_TAG64("Pmbabb06"), FSA_TAG64("Pmbabb05"), FSA_TAG64("Pmbabb04"), FSA_TAG64("Pmbabb03"), FSA_TAG64("Pmbabb02"),
    FSA_TAG64("Pmbabb01"), FSA_TAG64("Pmbab05"), FSA_TAG64("Pmbab04"), FSA_TAG64("Pmbab03"), FSA_TAG64("Pmbab02"), FSA_TAG64("Pmbab01"),
    FSA_TAG64("Pmbam02"), FSA_TAG64("Pmbam01"), FSA_TAG64("Pmbalk02"), FSA_TAG64("Pmbal02"), FSA_TAG64("Pmbalk01"), FSA_TAG64("Pmbal01"),
    FSA_TAG64("Pmba02"), FSA_TAG64("Pmba01"),
};
// No function of the unit reads it (mHeightGrid_0x100 would hold its alphas).
static const u64 lbl_804777B0[16] = {
    FSA_TAG64("Pmnvbb07"), FSA_TAG64("Pmnvbb06"), FSA_TAG64("Pmnvbb05"), FSA_TAG64("Pmnvbb04"), FSA_TAG64("Pmnvbb03"), FSA_TAG64("Pmnvbb02"),
    FSA_TAG64("Pmnvbb01"), FSA_TAG64("Pmnvb05"), FSA_TAG64("Pmnvb04"), FSA_TAG64("Pmnvb03"), FSA_TAG64("Pmnvb02"), FSA_TAG64("Pmnvb01"),
    FSA_TAG64("Pmnv02"), FSA_TAG64("Pmnv01"), FSA_TAG64("Pmte02"), FSA_TAG64("Pmte01"),
};
static const u64 lbl_80477830[5] = {
    FSA_TAG64("Pstrb04"), FSA_TAG64("Pstrb03"), FSA_TAG64("Pstrb02"), FSA_TAG64("Pstrb01"), FSA_TAG64("Tstr01"),
};
static const u64 lbl_80477858[5] = {
    FSA_TAG64("Pbacb04"), FSA_TAG64("Pbacb03"), FSA_TAG64("Pbacb02"), FSA_TAG64("Pbacb01"), FSA_TAG64("Tbac01"),
};
// fn_803DEC44.
static const s32 lbl_80477880[3] = {0x34, 0x35, 0x36};
static const s32 lbl_8047788C[3] = {0xF3, 0xF4, 0xF3};

// Run each frame by fn_803DF810 (index: mState).
static GameMode_804B0688::StateFunc lbl_804B0568[8] = {
    &GameMode_804B0688::fn_803DF2B4, &GameMode_804B0688::fn_803DEC44,
    &GameMode_804B0688::fn_803DE58C, &GameMode_804B0688::fn_803DE05C,
    &GameMode_804B0688::fn_803DD970, &GameMode_804B0688::fn_803DD93C,
    &GameMode_804B0688::fn_803DD8E8, &GameMode_804B0688::fn_803DD764,
};
// Run each frame by fn_803DF624 (index: mState).
static GameMode_804B0688::StateFunc lbl_804B0628[8] = {
    &GameMode_804B0688::fn_803DD760, &GameMode_804B0688::fn_803DD3E0,
    &GameMode_804B0688::fn_803DD3DC, &GameMode_804B0688::fn_803DD1A4,
    &GameMode_804B0688::fn_803DD1A0, &GameMode_804B0688::fn_803DD19C,
    &GameMode_804B0688::fn_803DD19C, &GameMode_804B0688::fn_803DD19C,
};

GameMode_804B0688* lbl_8053ADD0;

GameMode_804B0688::GameMode_804B0688() {
    lbl_80529DEC.mHeap->getTotalFreeSize();
    gGameRoot2_8053AB10->field_0x2c->fn_80158B3C();
    if (lbl_80529DEC.field_0x242 == 0) {
        lbl_80529DEC.field_0x242 = 1;
        gGameRoot2_8053AB10->field_0x20->fn_802FF798();
        gGameRoot2_8053AB10->field_0x20->loadSpriteImage_802F9714(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "gb_ch_obj_base_sch.szs"), 0, 0, 0, 0);
        gGameRoot2_8053AB10->field_0x20->loadSpriteImage_802F9714(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "gb_ch_obj_tr_schE.szs"), 0, 0, 0x7000, 0);
        gGameRoot2_8053AB10->field_0x20->loadSpritePalette_802F93F4(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCL ', "cu_cl_obj_base_scl.szs"), 0, 0, 0, 0, false);
        gGameRoot2_8053AB10->field_0x20->loadSpriteImage_802F9714(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "player_sch.szs"), 0, 2, 0x4000, 0);
        gGameRoot2_8053AB10->field_0x20->loadSpritePalette_802F93F4(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCL ', "cu_cl_obj_base_scl.szs"), 0, 2, 0, 0, false);
        gGameRoot2_8053AB10->field_0x20->loadSpriteImage_802F9714(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCH ', "gals_sch.szs"), 0, 3, 0, 0);
        gGameRoot2_8053AB10->field_0x20->loadSpritePalette_802F93F4(
            gGameRoot_8053AAF8->field_0xa4->getResource('SCL ', "cu_cl_obj_base_scl.szs"), 0, 3, 0, 0, false);
        gGameRoot2_8053AB10->field_0x20->mDrawSprites_0x2364 = 1;
    }
    field_0x60 = new cls_804AF860;
    mMovie = new THPMovie_804B15D0;
    mScreen = new J2DScreen;
    mScreen->setPriority("gamemode.blo", 0x1100000, lbl_80529DEC.mGameModeArchive_0x2c);
    mMovieScreen = new J2DScreen;
    mMovieScreen->setPriority("gamemode_movie.blo", 0x1100000, lbl_80529DEC.mGameModeArchive_0x2c);
    field_0x14[0] = new PaneCtrl_804AF86C;
    field_0x14[0]->setPane_80402F04(mMovieScreen->search(FSA_TAG64("Pmov01")));
    field_0x14[1] = new PaneCtrl_804AF86C;
    field_0x14[1]->setPane_80402F04(mScreen->search(FSA_TAG64("Ndata08")));
    field_0x14[2] = new PaneCtrl_804AF86C;
    field_0x14[2]->setPane_80402F04(mScreen->search(FSA_TAG64("Wgbco01")));
    field_0x14[3] = new PaneCtrl_804AF86C;
    field_0x14[3]->setPane_80402F04(mScreen->search(FSA_TAG64("Wnop01")));
    field_0x14[4] = new PaneCtrl_804AF86C;
    field_0x14[4]->setPane_80402F04(mScreen->search(FSA_TAG64("Pexpb09")));
    field_0x14[5] = new PaneCtrl_804AF86C;
    field_0x14[5]->setPane_80402F04(mScreen->search(FSA_TAG64("Wnop02")));
    field_0x14[6] = new PaneCtrl_804AF86C;
    field_0x14[6]->setPane_80402F04(mScreen->search(FSA_TAG64("Wgbco02")));
    field_0x30 = new PaneCtrl_804AF86C;
    field_0x30->setPane_80402F04(mScreen->search(FSA_TAG64("Ndata05")));
    field_0x34[0] = new PaneCtrl_804AF86C;
    field_0x34[0]->setPane_80402F04(mScreen->search(FSA_TAG64("Ndata02")));
    field_0x34[1] = new PaneCtrl_804AF86C;
    field_0x34[1]->setPane_80402F04(mScreen->search(FSA_TAG64("Ndata03")));
    field_0x40 = new PaneCtrl_804AF86C;
    field_0x40->setPane_80402F04(mScreen->search(FSA_TAG64("Pscuro02")));
    field_0x44[0] = new PaneCtrl_804AF86C;
    field_0x44[0]->setPane_80402F04(mScreen->search(FSA_TAG64("Pscurh05")));
    f32 x;
    f32 y = 1.0f + mScreen->search(FSA_TAG64("Pscurs05"))->getBounds()->i.y;
    x = mScreen->search(FSA_TAG64("Pscurs05"))->getBounds()->i.x;
    mScreen->search(FSA_TAG64("Pscurs05"))->move(x, y);
    field_0x44[1] = new PaneCtrl_804AF86C;
    field_0x44[1]->setPane_80402F04(mScreen->search(FSA_TAG64("Pscurs05")));
    field_0x50[0] = new PaneCtrl_804AF86C;
    field_0x50[0]->setPane_80402F04(mScreen->search(FSA_TAG64("Pstrb04")));
    field_0x50[1] = new PaneCtrl_804AF86C;
    field_0x50[1]->setPane_80402F04(mScreen->search(FSA_TAG64("Pbacb04")));
    field_0x58 = new PaneCtrl_804AF86C;
    field_0x58->setPane_80402F04(mScreen->search(FSA_TAG64("Paccur02")));
    field_0x5c = lbl_8053AED8->field_0x8;
    // `tex` and `font` are locals reused for each use: the original loads them
    // before the call (inlined into the call arguments the constructor drops to
    // 93.8 / 99.4 %).
    const ResTIMG* tex = (const ResTIMG*)getMain2DResource_804030C4('TIMG', "kk_kettei_01.bti");
    lbl_8053AED8->fn_80440D78(tex);
    lbl_8053AED8->fn_80440CE4((const ResTIMG*)getMain2DResource_804030C4('TIMG', "kk_modoru_01.bti"));
    tex = (const ResTIMG*)getMain2DResource_804030C4('TIMG', "kk_black.bti");
    field_0x10 = new J2DPicture(tex);
    field_0x10->setBlackWhite(JUtility::TColor(0x000000FF), JUtility::TColor(0x00000000));
    field_0xa2 = 156;
    JUTResFont* font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb01")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb02")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb03")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb04")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop01")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop02")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop03")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop04")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop05")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop06")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Texp01")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tbac01")))->setFont(font);
    font = lbl_8053AD78->mFont;
    static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tstr01")))->setFont(font);
    getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tstr01")))->getStringPtr(), 0x118, 0, NULL, NULL);
    getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tbac01")))->getStringPtr(), 0x11B, 0, NULL, NULL);
    getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop03")))->getStringPtr(), 0x93, 0, NULL, NULL);
    getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop04")))->getStringPtr(), 0x93, 0, NULL, NULL);
    getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop05")))->getStringPtr(), 0xF4, 0, NULL, NULL);
    getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop06")))->getStringPtr(), 0xF4, 0, NULL, NULL);
    strcpy(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb01")))->getStringPtr(), "Only");
    strcpy(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb02")))->getStringPtr(), "Only");
    strcpy(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb03")))->getStringPtr(), "Only");
    strcpy(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tagb04")))->getStringPtr(), "Only");
    field_0x6c[0] = mScreen->search(FSA_TAG64("Ndata02"))->getBounds()->i.y +
                    mScreen->search(FSA_TAG64("Pmadbb07"))->getBounds()->i.y +
                    mScreen->search(FSA_TAG64("Pmadbb01"))->getBounds()->i.y;
    field_0x6c[1] = mScreen->search(FSA_TAG64("Ndata03"))->getBounds()->i.y +
                    mScreen->search(FSA_TAG64("Pmbabb07"))->getBounds()->i.y +
                    mScreen->search(FSA_TAG64("Pmbabb01"))->getBounds()->i.y;
    field_0x11a[0] = mMovieScreen->search(FSA_TAG64("Pim01"))->getAlpha();
    field_0x11a[1] = mMovieScreen->search(FSA_TAG64("Pim02"))->getAlpha();
    field_0x11a[2] = mMovieScreen->search(FSA_TAG64("Pim11"))->getAlpha();
    field_0x11a[3] = mMovieScreen->search(FSA_TAG64("Pim12"))->getAlpha();
    for (s32 i = 0; i < 7; i++) {
        field_0x14[i]->mPane->hide();
    }
    for (s32 i = 0; i < 2; i++) {
        field_0x50[i]->mPane->hide();
    }
    field_0x58->mPane->hide();
    mState = 0;
    field_0x9e = lbl_80529DEC.field_0x23a[0];
    field_0x9f = field_0x9e;
    field_0x80 = 0;
    field_0x88 = 0;
    field_0x7c = 0.0f;
    field_0x78 = 0.0f;
    field_0x9a = 0;
    field_0x98 = 60;
    mMovieState = 0;
    field_0xa1 = 0;
    for (s32 j = 0; j < 50; j++) {
        if (j == 0) {
            field_0xa3[j] = mMovieScreen->search(lbl_804774C8[j])->getAlpha();
        } else {
            field_0xa3[j] = mScreen->search(lbl_804774C8[j])->getAlpha();
        }
    }
    for (s32 j = 0; j < 6; j++) {
        field_0xd5[j] = mScreen->search(lbl_80477658[j])->getAlpha();
    }
    for (s32 j = 0; j < 17; j++) {
        field_0xdb[j] = mScreen->search(lbl_80477688[j])->getAlpha();
    }
    for (s32 j = 0; j < 20; j++) {
        field_0xec[j] = mScreen->search(lbl_80477710[j])->getAlpha();
    }
    for (s32 j = 0; j < 5; j++) {
        field_0x110[j] = mScreen->search(lbl_80477830[j])->getAlpha();
    }
    for (s32 j = 0; j < 5; j++) {
        field_0x115[j] = mScreen->search(lbl_80477858[j])->getAlpha();
    }
    field_0x94 = 1;
    fn_803DAC58(field_0x9e, 0.0f);
    lbl_8053ADD0 = this;
}

GameMode_804B0688::~GameMode_804B0688() {
    lbl_80529DEC.mHeap->getTotalFreeSize();
    if (mMovieState == 2) {
        mMovie->quit_80427D94();
        mMovieState = 0;
    }
    delete field_0x60;
    field_0x60 = NULL;
    delete mMovie;
    mMovie = NULL;
    delete mScreen;
    mScreen = NULL;
    delete mMovieScreen;
    mMovieScreen = NULL;
    for (s32 i = 0; i < 7; i++) {
        delete field_0x14[i];
        field_0x14[i] = NULL;
    }
    delete field_0x30;
    field_0x30 = NULL;
    for (s32 i = 0; i < 2; i++) {
        delete field_0x34[i];
        field_0x34[i] = NULL;
    }
    delete field_0x40;
    field_0x40 = NULL;
    for (s32 i = 0; i < 2; i++) {
        delete field_0x44[i];
        field_0x44[i] = NULL;
    }
    for (s32 i = 0; i < 2; i++) {
        delete field_0x50[i];
        field_0x50[i] = NULL;
    }
    delete field_0x58;
    field_0x58 = NULL;
    delete field_0x10;
    field_0x10 = NULL;
    lbl_80529DEC.mGameModeArchive_0x2c->unmount();
    lbl_8053ADD0 = NULL;
}

void GameMode_804B0688::fn_803DF9E8() {
    field_0x88 = 0;
    field_0x8c = 0;
    field_0x90 = 0;
    field_0x9c = 0;
    field_0x7c = 0.0f;
    field_0x78 = 0.0f;
    field_0x9a = 0;
    field_0x98 = 60;
    field_0xa1 = 0;
    for (s32 i = 0; i < 2; i++) {
        field_0x34[i]->mPane->show();
        field_0x34[i]->moveFromCentre_80402E08(600.0f, 0.0f);
        field_0x34[i]->field_0x20 = 0;
        field_0x44[i]->mPane->show();
        field_0x44[i]->moveFromCentre_80402E08(600.0f, 0.0f);
        field_0x44[i]->field_0x20 = 0;
    }
    field_0x30->mPane->show();
    field_0x30->moveFromCentre_80402E08(0.0f, 150.0f);
    field_0x30->field_0x22 = 0;
    field_0x5c->moveFromCentre_80402E08(0.0f, -200.0f);
    field_0x5c->field_0x22 = 0;
    field_0x40->mPane->hide();
    for (s32 i = 0; i < 7; i++) {
        field_0x14[i]->mPane->hide();
    }
    field_0x50[0]->mPane->hide();
    field_0x50[1]->mPane->hide();
    field_0x58->mPane->hide();
    mMovieScreen->search(FSA_TAG64("Pim01"))->setAlpha(0);
    mMovieScreen->search(FSA_TAG64("Pim02"))->setAlpha(0);
    mMovieScreen->search(FSA_TAG64("Pim11"))->setAlpha(0);
    mMovieScreen->search(FSA_TAG64("Pim12"))->setAlpha(0);
    mState = 0;
    field_0x68 = 2;
}

void GameMode_804B0688::fn_803DF810() {
    if (field_0x94 == 0 && mState != 7) {
        gAudioMgr_8053AF80->startBgm_80458FF0(0x100001A, false, false);
        gAudioMgr_8053AF80->fn_80458408(0x100001A, false);
        lbl_80529DEC.field_0x248 = 0;
    }
    (lbl_8053ADD0->*lbl_804B0568[mState])();
    fn_803DCCA4();
    if (mMovieState == 1) {
        if (mMovie->start_80427DB4(field_0x9e)) {
            field_0xa1 = 0;
            mMovieState = 2;
        } else {
            mMovie->quit_80427D94();
        }
    } else if (mMovieState == 2) {
        BOOL updated = mMovie->update_80427D60();
        if (IsMoviePlaying_804286A0() && field_0xa1 == 0) {
            mMovie->mSetVolume(0.0f);
            field_0xa1 = 1;
        }
        if (field_0x68 != 2 || lbl_8053ACA8->fn_802B1DCC()) {
            mMovie->quit_80427D94();
            mMovieState = 0;
        } else if (mMovie->mGetRestFrame() == 0 || updated == FALSE) {
            mMovie->quit_80427D94();
            if (mMovie->start_80427DB4(field_0x9e)) {
                field_0xa1 = 0;
            } else {
                mMovie->quit_80427D94();
                mMovieState = 1;
            }
        }
    } else if (mMovieState == 3) {
        mMovie->quit_80427D94();
        mMovieState = 0;
    }
}

void GameMode_804B0688::fn_803DF624() {
    (lbl_8053ADD0->*lbl_804B0628[mState])();
    gGameRoot2_8053AB10->setPort_8013B490();
    mMovieScreen->draw(0.0f, 0.0f, gGameRoot2_8053AB10->field_0x10);
    if (mMovieState == 2) {
        mMovie->draw_80427D2C(50, 159, -356, -283);
    }
    gGameRoot2_8053AB10->setPort_8013B490();
    mScreen->draw(0.0f, 0.0f, gGameRoot2_8053AB10->field_0x10);
    lbl_8053AED8->draw_80440EE8();
    if (mState == 5) {
        field_0x10->setAlpha(field_0x7c);
        field_0x10->draw(0.0f, 0.0f, 800.0f, 640.0f, false, false, false);
    } else if (mState == 6) {
        field_0x10->draw(0.0f, 0.0f, 800.0f, 640.0f, false, false, false);
    } else if (mState == 7) {
        field_0x10->setAlpha(field_0x7c);
        field_0x10->draw(0.0f, 0.0f, 800.0f, 640.0f, false, false, false);
    }
    if (field_0x94 != 0) {
        if (lbl_80529DEC.field_0x1a0 > 0) {
            lbl_80529DEC.field_0x1a0--;
        } else {
            lbl_80529DEC.field_0x1a0 = 0;
        }
        field_0x94 = lbl_80529DEC.field_0x1a0;
    }
}

// The pane alphas of entry i. An inline of its own: its loop registers are handed out before
// fn_803DF2B4's (docs/REGALLOC.md, rule 4).
static inline void setEntryAlphas(GameMode_804B0688* self, s32 i, f32 alpha) {
    switch (i) {
    case 0:
        for (s32 j = 0; j < 17; j++) {
            self->mScreen->search(lbl_80477688[j])->setAlpha(alpha * self->field_0xdb[j]);
        }
        break;
    case 1:
        for (s32 j = 0; j < 20; j++) {
            self->mScreen->search(lbl_80477710[j])->setAlpha(alpha * self->field_0xec[j]);
        }
        break;
    }
}

// State 0: slides the entry panes in and fades the screen's panes up, then
// goes to state 1 once the three slides are done.
void GameMode_804B0688::fn_803DF2B4() {
    u8 slid = false;
    u8 titleSlid = false;
    u8 windowSlid = false;
    field_0x9c++;
    for (s32 i = 0; i < 2; i++) {
        if (field_0x9c > i * 3) {
            slid = field_0x34[i]->slideX_80402B10(20, 0, 600.0f, 0.0f);
            field_0x44[i]->slideX_80402B10(20, 0, 600.0f, 0.0f);
            s32 t = field_0x9c - i * 3;
            if (t > 20) {
                t = 20;
            }
            f32 alpha = (f32)(t * t) / 400.0f;
            setEntryAlphas(this, i, alpha);
        }
    }
    if (field_0x9c > 15) {
        titleSlid = field_0x30->animateMoveY_80402940(15, 0, 150.0f, 0.0f);
        s32 t = field_0x9c - 15;
        if (t > 15) {
            t = 15;
        }
        f32 alpha = (f32)(t * t) / 225.0f;
        for (s32 j = 0; j < 6; j++) {
            mScreen->search(lbl_80477658[j])->setAlpha(alpha * field_0xd5[j]);
        }
    }
    if (field_0x9c > 20) {
        windowSlid = field_0x5c->animateMoveY_80402940(25, 0, -200.0f, 0.0f);
        s32 t = field_0x9c - 20;
        if (t > 25) {
            t = 25;
        }
        lbl_8053AED8->fn_80440E0C((f32)(t * t) / 625.0f);
    }
    if (slid && titleSlid && windowSlid) {
        field_0x8c = 0;
        mState = 1;
        field_0x90 = 1;
    }
}

// State 1: the entry menu. A/Start picks the entry (3 leaves the screen),
// B returns, up / down move the cursor (entry 2 is skipped).
void GameMode_804B0688::fn_803DEC44() {
    if (field_0x88 != 0) {
        field_0x88++;
        if (field_0x88 > 10) {
            field_0x88 = 0;
        }
    }
    if (lbl_8053AD30->fn_80309C4C(0x1100, 1)) {
        if (field_0x9e == 3) {
            field_0x9f = field_0x9e;
            field_0x68 = 5;
        } else {
            field_0x9c = 0;
            for (s32 i = 0; i < 2; i++) {
                field_0x34[i]->moveFromCentre_80402E08(0.0f, 0.0f);
                field_0x34[i]->field_0x20 = 0;
                field_0x34[i]->field_0x22 = 0;
                field_0x44[i]->moveFromCentre_80402E08(0.0f, 0.0f);
                field_0x44[i]->field_0x20 = 0;
                field_0x44[i]->field_0x22 = 0;
            }
            field_0x30->moveFromCentre_80402E08(0.0f, 0.0f);
            field_0x30->field_0x22 = 0;
            field_0x40->mPane->hide();
            for (s32 i = 0; i < 7; i++) {
                field_0x14[i]->mPane->show();
                field_0x14[i]->moveFromCentre_80402E08(600.0f, 0.0f);
                field_0x14[i]->field_0x20 = 0;
            }
            for (s32 i = 0; i < 2; i++) {
                field_0x50[i]->mPane->show();
                field_0x50[i]->moveFromCentre_80402E08(0.0f, 150.0f);
                field_0x50[i]->field_0x22 = 0;
            }
            switch (field_0x9e) {
            case 0:
                mScreen->search(FSA_TAG64("Wgbco01"))->hide();
                mScreen->search(FSA_TAG64("Wnop01"))->hide();
                mScreen->search(FSA_TAG64("Wnop02"))->show();
                mScreen->search(FSA_TAG64("Wgbco02"))->show();
                break;
            case 1:
                mScreen->search(FSA_TAG64("Wgbco01"))->show();
                mScreen->search(FSA_TAG64("Wnop01"))->show();
                mScreen->search(FSA_TAG64("Wnop02"))->hide();
                mScreen->search(FSA_TAG64("Wgbco02"))->hide();
                break;
            default:
                mScreen->search(FSA_TAG64("Wgbco01"))->show();
                mScreen->search(FSA_TAG64("Wnop01"))->show();
                mScreen->search(FSA_TAG64("Wnop02"))->hide();
                mScreen->search(FSA_TAG64("Wgbco02"))->hide();
                break;
            }
            getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop01")))->getStringPtr(),
                        lbl_8047788C[field_0x9e], 0, NULL, NULL);
            getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Tnop02")))->getStringPtr(),
                        lbl_8047788C[field_0x9e], 0, NULL, NULL);
            getMessageText_803832BC(static_cast<J2DTextBox*>(mScreen->search(FSA_TAG64("Texp01")))->getStringPtr(),
                        lbl_80477880[field_0x9e], 0, NULL, NULL);
            field_0x44[field_0x9e]->mPane->hide();
            mState = 2;
            field_0x9f = field_0x9e;
            field_0x90 = 1;
        }
        gAudioMgr_8053AF80->playSystemSound_80458880(11, -1);
    } else if (lbl_8053AD30->fn_80309C4C(0x200, 1)) {
        field_0x68 = 1;
        gAudioMgr_8053AF80->playSystemSound_80458880(12, -1);
    } else if (lbl_8053AD30->fn_80309A9C(4, 1)) {
        field_0x9f = field_0x9e;
        if (field_0x9e < 3) {
            field_0x9e++;
            if (field_0x9e == 2) {
                field_0x9e = 3;
            }
        } else {
            field_0x9e = 0;
        }
        field_0x88 = 1;
        if (field_0x9e == 3) {
            field_0x8c = 30;
        } else {
            field_0x8c = 60;
        }
        field_0x90 = 1;
        gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
    } else if (lbl_8053AD30->fn_80309A9C(8, 1)) {
        field_0x9f = field_0x9e;
        if (field_0x9e != 0) {
            field_0x9e--;
            if (field_0x9e == 2) {
                field_0x9e = 1;
            }
        } else {
            field_0x9e = 3;
        }
        field_0x88 = 1;
        if (field_0x9e == 3) {
            field_0x8c = 30;
        } else {
            field_0x8c = 60;
        }
        field_0x90 = 1;
        gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
    }
    field_0x8c++;
    if (field_0x9e == 3) {
        if (field_0x8c >= 60) {
            field_0x8c = 0;
        }
    } else {
        if (field_0x8c >= 120) {
            field_0x8c = 0;
        }
    }
}

// State 2: slides the other entry out and the picked one up, fades the
// entry panes out and the sub-menu panes in, then starts the movie and goes
// to state 3.
// FIXME: 96.35 %, missing/extra + reloc-name; behaviour near.
// The original stores the first other entry through the
// pointer (ours stores it straight to the stack slot) and sets up the tag
// table base (lbl_804774C8, ours via the section) one instruction earlier;
// the alpha loops' counter and tag pointer registers are swapped.
void GameMode_804B0688::fn_803DE58C() {
    s32 others[2];
    field_0x9c++;
    s32* other = others;
    u8 slid = false;
    u8 menuSlid = false;
    for (s32 i = 0; i < 2; i++) {
        if (field_0x9e != i) {
            *other++ = i;
        }
    }
    for (s32 i = 0; i < 1; i++) {
        if (field_0x9c > i) {
            field_0x34[others[i]]->slideX_80402B10(8, 0, 0.0f, 600.0f);
            field_0x44[others[i]]->slideX_80402B10(8, 0, 0.0f, 600.0f);
            s32 t = field_0x9c - i;
            if (t > 8) {
                t = 8;
            }
            f32 alpha = (f32)(t * t) / 64.0f;
            switch (others[i]) {
            case 0:
                for (s32 j = 0; j < 17; j++) {
                    mScreen->search(lbl_80477688[j])->setAlpha((1.0f - alpha) * field_0xdb[j]);
                }
                break;
            case 1:
                for (s32 j = 0; j < 20; j++) {
                    mScreen->search(lbl_80477710[j])->setAlpha((1.0f - alpha) * field_0xec[j]);
                }
                break;
            }
        }
    }
    if (field_0x9c > 0) {
        field_0x34[field_0x9e]->animateMoveY_80402940(20, 0, 0.0f, 27.0f - field_0x6c[field_0x9e]);
        field_0x44[field_0x9e]->animateMoveY_80402940(20, 0, 0.0f, 27.0f - field_0x6c[field_0x9e]);
    }
    if (field_0x9c > 0) {
        field_0x30->animateMoveY_80402940(15, 0, 0.0f, 150.0f);
        s32 t = field_0x9c;
        if (t > 15) {
            t = 15;
        }
        f32 alpha = 1.0f - (f32)(t * t) / 225.0f;
        for (s32 j = 0; j < 6; j++) {
            mScreen->search(lbl_80477658[j])->setAlpha(alpha * field_0xd5[j]);
        }
    }
    if (field_0x9c > 0) {
        slid = field_0x14[0]->slideX_80402B10(20, 0, 600.0f, 0.0f);
        for (s32 i = 1; i < 7; i++) {
            field_0x14[i]->slideX_80402B10(20, 0, 600.0f, 0.0f);
        }
        s32 t = field_0x9c;
        if (t > 20) {
            t = 20;
        }
        f32 alpha = (f32)(t * t) / 400.0f;
        for (s32 j = 0; j < 50; j++) {
            if (j == 0) {
                mMovieScreen->search(lbl_804774C8[j])->setAlpha(alpha * field_0xa3[j]);
            } else {
                mScreen->search(lbl_804774C8[j])->setAlpha(alpha * field_0xa3[j]);
            }
        }
    }
    for (s32 i = 0; i < 2; i++) {
        if (field_0x9c > i * 5) {
            menuSlid = field_0x50[i]->animateMoveY_80402940(20, 0, 150.0f, 0.0f);
            s32 t = field_0x9c - i * 5;
            if (t > 20) {
                t = 20;
            }
            f32 alpha = (f32)(t * t) / 400.0f;
            if (i == 0) {
                for (s32 j = 0; j < 5; j++) {
                    mScreen->search(lbl_80477830[j])->setAlpha(alpha * field_0x110[j]);
                }
            } else {
                for (s32 j = 0; j < 5; j++) {
                    mScreen->search(lbl_80477858[j])->setAlpha(alpha * field_0x115[j]);
                }
            }
        }
    }
    if (slid && menuSlid) {
        field_0x34[others[0]]->mPane->hide();
        field_0x44[others[0]]->mPane->hide();
        field_0x30->mPane->hide();
        field_0x58->mPane->show();
        field_0x80 = 0;
        field_0x88 = 0;
        field_0x8c = 0;
        mMovieState = 1;
        mState = 3;
    }
}

// State 3: the sub-menu of the picked entry (field_0x80: 0 or 1) while its
// movie plays. A confirms, B goes back to the entry menu (state 4).
void GameMode_804B0688::fn_803DE05C() {
    if (field_0x88 != 0) {
        field_0x88++;
        if (field_0x88 > 10) {
            field_0x88 = 0;
        }
    }
    if (field_0x9e == 2 && mMovie->mGetTotalFrame() - mMovie->mGetRestFrame() <= 1) {
        field_0x8c = 0;
        return;
    }
    if (lbl_8053AD30->fn_80309C4C(0x1100, 1)) {
        mMovieState = 3;
        if (field_0x80 == 0) {
            if (field_0x9e == 0) {
                gGameRoot_8053AAF8->field_0x7f = 1;
                field_0x68 = 3;
            } else if (field_0x9e == 1) {
                gGameRoot_8053AAF8->field_0x7f = 0;
                field_0x68 = 3;
            } else if (field_0x9e == 2) {
                field_0x7c = 0.0f;
                mState = 7;
                gAudioMgr_8053AF80->stopBgm_80458F9C(60);
            }
            lbl_80529DEC.field_0x23a[0] = field_0x9e;
            gAudioMgr_8053AF80->playSystemSound_80458880(11, -1);
        } else {
            field_0x9c = 0;
            for (s32 i = 0; i < 2; i++) {
                field_0x34[i]->mPane->show();
                field_0x44[i]->mPane->show();
                if (i == field_0x9e) {
                    field_0x34[i]->moveFromCentre_80402E08(0.0f, 27.0f - field_0x6c[i]);
                    field_0x44[i]->moveFromCentre_80402E08(0.0f, 27.0f - field_0x6c[i]);
                } else {
                    field_0x34[i]->moveFromCentre_80402E08(600.0f, 0.0f);
                    field_0x44[i]->moveFromCentre_80402E08(600.0f, 0.0f);
                }
                field_0x34[i]->field_0x20 = 0;
                field_0x34[i]->field_0x22 = 0;
                field_0x44[i]->field_0x20 = 0;
                field_0x44[i]->field_0x22 = 0;
            }
            field_0x30->mPane->show();
            field_0x30->moveFromCentre_80402E08(0.0f, 150.0f);
            field_0x30->field_0x22 = 0;
            for (s32 i = 0; i < 7; i++) {
                field_0x14[i]->moveFromCentre_80402E08(0.0f, 0.0f);
                field_0x14[i]->field_0x20 = 0;
            }
            for (s32 i = 0; i < 2; i++) {
                field_0x50[i]->moveFromCentre_80402E08(0.0f, 0.0f);
                field_0x50[i]->field_0x22 = 0;
            }
            field_0x58->mPane->hide();
            mState = 4;
            gAudioMgr_8053AF80->playSystemSound_80458880(12, -1);
        }
    } else if (lbl_8053AD30->fn_80309C4C(0x200, 1)) {
        field_0x9c = 0;
        mMovieState = 3;
        for (s32 i = 0; i < 2; i++) {
            field_0x34[i]->mPane->show();
            field_0x44[i]->mPane->show();
            if (i == field_0x9e) {
                field_0x34[i]->moveFromCentre_80402E08(0.0f, 27.0f - field_0x6c[i]);
                field_0x44[i]->moveFromCentre_80402E08(0.0f, 27.0f - field_0x6c[i]);
            } else {
                field_0x34[i]->moveFromCentre_80402E08(600.0f, 0.0f);
                field_0x44[i]->moveFromCentre_80402E08(600.0f, 0.0f);
            }
            field_0x34[i]->field_0x20 = 0;
            field_0x34[i]->field_0x22 = 0;
            field_0x44[i]->field_0x20 = 0;
            field_0x44[i]->field_0x22 = 0;
        }
        field_0x30->mPane->show();
        field_0x30->moveFromCentre_80402E08(0.0f, 150.0f);
        field_0x30->field_0x22 = 0;
        for (s32 i = 0; i < 7; i++) {
            field_0x14[i]->moveFromCentre_80402E08(0.0f, 0.0f);
            field_0x14[i]->field_0x20 = 0;
        }
        for (s32 i = 0; i < 2; i++) {
            field_0x50[i]->moveFromCentre_80402E08(0.0f, 0.0f);
            field_0x50[i]->field_0x22 = 0;
        }
        field_0x58->mPane->hide();
        mState = 4;
        gAudioMgr_8053AF80->playSystemSound_80458880(12, -1);
    } else if (lbl_8053AD30->fn_80309A9C(2, 1)) {
        if (field_0x80 == 0) {
            field_0x80 = 1;
            field_0x88 = 1;
            field_0x8c = 30;
            gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
        }
    } else if (lbl_8053AD30->fn_80309A9C(1, 1)) {
        if (field_0x80 == 1) {
            field_0x80 = 0;
            field_0x88 = 1;
            field_0x8c = 30;
            gAudioMgr_8053AF80->playSystemSound_80458880(0x26, -1);
        }
    }
    field_0x8c++;
    if (field_0x8c >= 60) {
        field_0x8c = 0;
    }
}

// State 4: back from the sub-menu to the entry menu (state 2 in reverse),
// then state 1.
// FIXME: 97.47 %, different + reloc-name; behaviour near.
// Same length; try.py says "different" only for the prologue:
// the original sets up the tag table base (lbl_804774C8; ours goes through
// the section symbol) after `field_0x9c++` and stores the first other entry
// through the pointer (ours straight to the stack slot). The rest is
// registers: the loop counters / tag pointers and f30 / f31 are swapped.
void GameMode_804B0688::fn_803DD970() {
    s32 others[2];
    field_0x9c++;
    s32* other = others;
    u8 slid = false;
    u8 titleSlid = false;
    for (s32 i = 0; i < 2; i++) {
        if (field_0x9e != i) {
            *other++ = i;
        }
    }
    if (field_0x9c > 0) {
        slid = field_0x14[0]->slideX_80402B10(20, 0, 0.0f, 600.0f);
        for (s32 i = 1; i < 7; i++) {
            field_0x14[i]->slideX_80402B10(20, 0, 0.0f, 600.0f);
        }
        s32 t = field_0x9c;
        if (t > 20) {
            t = 20;
        }
        f32 alpha = 1.0f - (f32)(t * t) / 400.0f;
        for (s32 j = 0; j < 50; j++) {
            if (j == 0) {
                mMovieScreen->search(lbl_804774C8[j])->setAlpha(alpha * field_0xa3[j]);
            } else {
                mScreen->search(lbl_804774C8[j])->setAlpha(alpha * field_0xa3[j]);
            }
        }
    }
    for (s32 i = 0; i < 2; i++) {
        if (field_0x9c > i * 5 + 10) {
            field_0x50[i]->animateMoveY_80402940(20, 0, 0.0f, 150.0f);
            s32 t = field_0x9c - (i * 5 + 10);
            if (t > 20) {
                t = 20;
            }
            f32 alpha = (f32)(t * t) / 400.0f;
            if (i == 0) {
                for (s32 j = 0; j < 5; j++) {
                    mScreen->search(lbl_80477830[j])->setAlpha((1.0f - alpha) * field_0x110[j]);
                }
            } else {
                for (s32 j = 0; j < 5; j++) {
                    mScreen->search(lbl_80477858[j])->setAlpha((1.0f - alpha) * field_0x115[j]);
                }
            }
        }
    }
    if (field_0x9c > 0) {
        field_0x34[field_0x9e]->animateMoveY_80402940(20, 0, 27.0f - field_0x6c[field_0x9e], 0.0f);
        field_0x44[field_0x9e]->animateMoveY_80402940(20, 0, 27.0f - field_0x6c[field_0x9e], 0.0f);
    }
    for (s32 i = 0; i < 1; i++) {
        if (field_0x9c > i + 5) {
            slid = field_0x34[others[i]]->slideX_80402B10(20, 0, 600.0f, 0.0f);
            field_0x44[others[i]]->slideX_80402B10(20, 0, 600.0f, 0.0f);
            s32 t = field_0x9c - (i + 5);
            if (t > 20) {
                t = 20;
            }
            f32 alpha = (f32)(t * t) / 400.0f;
            switch (others[i]) {
            case 0:
                for (s32 j = 0; j < 17; j++) {
                    mScreen->search(lbl_80477688[j])->setAlpha(alpha * field_0xdb[j]);
                }
                break;
            case 1:
                for (s32 j = 0; j < 20; j++) {
                    mScreen->search(lbl_80477710[j])->setAlpha(alpha * field_0xec[j]);
                }
                break;
            }
        }
    }
    if (field_0x9c > 15) {
        titleSlid = field_0x30->animateMoveY_80402940(15, 0, 150.0f, 0.0f);
        s32 t = field_0x9c - 15;
        if (t > 15) {
            t = 15;
        }
        f32 alpha = (f32)(t * t) / 225.0f;
        for (s32 j = 0; j < 6; j++) {
            mScreen->search(lbl_80477658[j])->setAlpha(alpha * field_0xd5[j]);
        }
    }
    if (slid && titleSlid) {
        for (s32 i = 0; i < 7; i++) {
            field_0x14[i]->mPane->hide();
        }
        for (s32 i = 0; i < 2; i++) {
            field_0x50[i]->mPane->hide();
        }
        field_0x80 = 0;
        field_0x88 = 0;
        field_0x8c = 0;
        mState = 1;
        field_0x90 = 1;
    }
}

void GameMode_804B0688::fn_803DD93C() {
    field_0x7c += 10.0f;
    if (field_0x7c >= 255.0f) {
        field_0x7c = 255.0f;
        mState = 6;
    }
}

void GameMode_804B0688::fn_803DD8E8() {
    field_0x78 += 10.0f;
    if (field_0x78 >= 255.0f) {
        field_0x78 = 255.0f;
        field_0x7c = 0.0f;
        if (++field_0x9a > 100) {
            mState = 7;
        }
    }
}

// State 7: fades to black, waits for the sound to stop and the countdown
// field_0x98, then copies the save region to 0x80700000 (it survives the
// reset, see main) and reboots into "/DOL/framework.dol".
// FIXME: 93.94 %, missing/extra; behaviour drift.
// The save-region copy loop: the original re-adds the index to
// lbl_80529DEC's address for every eight bytes (add r4, r5, r12), ours steps a
// source pointer (one instruction fewer); the same loop is unsolved in main
// (tu_801344C0) and cls_804A8578::fn_802B1DE8.
void GameMode_804B0688::fn_803DD764() {
    field_0x7c += 10.0f;
    if (field_0x7c >= 255.0f) {
        field_0x7c = 255.0f;
        if (gAudioMgr_8053AF80->fn_80457F80(JAISoundID(11), -2)) {
            if (--field_0x98 <= 0) {
                DVDFileInfo info;
                if (DVDOpen("/DOL/framework.dol", &info) == TRUE) {
                    for (int i = 0; i < 0x20; i++) {
                        ((u8*)FSA_SAVE_REGION)[i] = lbl_80529DEC.mSaveRegion.bytes[i];
                    }
                    OSSetSaveRegion((void*)FSA_SAVE_REGION, (void*)(FSA_SAVE_REGION + 0x1F));
                    __PADDisableRecalibration(TRUE);
                    lbl_8053ACA8->fn_802B1558();
                    OSSetBootDol(info.startAddr);
                    OSResetSystem(0, 0, FALSE);
                }
            }
        }
    }
}

void GameMode_804B0688::fn_803DD760() {}

void GameMode_804B0688::fn_803DD3E0() {
    u8 entry = field_0x9e;
    f32 alpha;
    if (entry == 3 || mState != 1) {
        if (field_0x8c < 30) {
            alpha = (f32)(field_0x8c * field_0x8c) / 900.0f;
        } else {
            s32 t = field_0x8c - 30;
            alpha = 1.0f - (f32)(t * t) / 900.0f;
        }
    } else {
        if (field_0x8c < 60) {
            alpha = (f32)(field_0x8c * field_0x8c) / 3600.0f;
        } else {
            s32 t = field_0x8c - 60;
            alpha = 1.0f - (f32)(t * t) / 3600.0f;
        }
    }
    s32 frame = field_0x88;
    s32 step = frame < 5 ? frame : 10 - frame;
    f32 y = 5.0f * (f32)(step * step) / 25.0f;
    switch (entry) {
    case 0:
        field_0x34[0]->moveFromCentre_80402E08(0.0f, y);
        field_0x34[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x30->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[0]->moveFromCentre_80402E08(0.0f, y);
        field_0x44[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        fn_803DAC58(field_0x9e, alpha);
        field_0x40->mPane->hide();
        break;
    case 1:
        field_0x34[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x34[1]->moveFromCentre_80402E08(0.0f, y);
        field_0x30->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[1]->moveFromCentre_80402E08(0.0f, y);
        fn_803DAC58(field_0x9e, alpha);
        field_0x40->mPane->hide();
        break;
    case 2:
        field_0x34[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x34[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x30->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        fn_803DAC58(field_0x9e, alpha);
        field_0x40->mPane->hide();
        break;
    default:
        field_0x34[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x34[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x44[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x30->moveFromCentre_80402E08(0.0f, y);
        field_0x40->moveFromCentre_80402E08(0.0f, y);
        fn_803DAC58(field_0x9e, alpha);
        field_0x40->mPane->show();
        break;
    }
}

void GameMode_804B0688::fn_803DD3DC() {}

void GameMode_804B0688::fn_803DD1A4() {
    f32 alpha;
    if (field_0x8c < 30) {
        alpha = (f32)(field_0x8c * field_0x8c) / 900.0f;
    } else {
        s32 t = field_0x8c - 30;
        alpha = 1.0f - (f32)(t * t) / 900.0f;
    }
    s32 frame = field_0x88;
    s32 step = frame < 5 ? frame : 10 - frame;
    f32 y = 5.0f * (f32)(step * step) / 25.0f;
    if (field_0x80 == 0) {
        field_0x50[0]->moveFromCentre_80402E08(0.0f, y);
        field_0x50[1]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x58->moveFromCentre_80402E08(0.0f, y);
    } else {
        field_0x50[0]->moveFromCentre_80402E08(0.0f, 0.0f);
        field_0x50[1]->moveFromCentre_80402E08(0.0f, y);
        field_0x58->moveFromCentre_80402E08(field_0x50[1]->field_0x8 - field_0x50[0]->field_0x8, y);
    }
    mScreen->search(FSA_TAG64("Paccur01"))->setAlpha(field_0xa2 * alpha);
    mScreen->search(FSA_TAG64("Paccur02"))->setAlpha(field_0xa2 * alpha);
}

void GameMode_804B0688::fn_803DD1A0() {}

void GameMode_804B0688::fn_803DD19C() {}

// Cross-fades the movie screen's 'Pim' pictures from the previous entry
// (field_0x9f) to the selected one (field_0x9e) over the frames of
// lbl_8050DB0C's halfword 0x12E.
void GameMode_804B0688::fn_803DCCA4() {
    if (field_0x90 == 0) {
        return;
    }
    field_0x90++;
    f32 t = (f32)field_0x90 / (f32)lbl_8050DB0C.field_0x4c[0x71];
    if (field_0x90 >= lbl_8050DB0C.field_0x4c[0x71]) {
        field_0x90 = 0;
    }
    if (field_0x9e != 0 && field_0x9f != 0) {
        mMovieScreen->search(FSA_TAG64("Pim01"))->setAlpha(0);
        mMovieScreen->search(FSA_TAG64("Pim02"))->setAlpha(0);
    }
    if (field_0x9e != 1 && field_0x9f != 1) {
        mMovieScreen->search(FSA_TAG64("Pim11"))->setAlpha(0);
        mMovieScreen->search(FSA_TAG64("Pim12"))->setAlpha(0);
    }
    if (mState != 2) {
        switch (field_0x9e) {
        case 0:
            mMovieScreen->search(FSA_TAG64("Pim01"))->setAlpha(field_0x11a[0] * t);
            mMovieScreen->search(FSA_TAG64("Pim02"))->setAlpha(field_0x11a[1] * t);
            break;
        case 1:
            mMovieScreen->search(FSA_TAG64("Pim11"))->setAlpha(field_0x11a[2] * t);
            mMovieScreen->search(FSA_TAG64("Pim12"))->setAlpha(field_0x11a[3] * t);
            break;
        case 2: // entry 2 has no movie pictures
            break;
        }
    }
    t = 1.0f - t;
    if (mState == 1 && field_0x9e == field_0x9f) {
        return;
    }
    switch (field_0x9f) {
    case 0:
        mMovieScreen->search(FSA_TAG64("Pim01"))->setAlpha(field_0x11a[0] * t);
        mMovieScreen->search(FSA_TAG64("Pim02"))->setAlpha(field_0x11a[1] * t);
        break;
    case 1:
        mMovieScreen->search(FSA_TAG64("Pim11"))->setAlpha(field_0x11a[2] * t);
        mMovieScreen->search(FSA_TAG64("Pim12"))->setAlpha(field_0x11a[3] * t);
        break;
    case 2: // entry 2 has no movie pictures
        break;
    }
}

void GameMode_804B0688::fn_803DAC58(s32 entry, f32 alpha) {
    switch (entry) {
    case 0:
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh01")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh02")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh03")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh04")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh05")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs02")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs03")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs04")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs05")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        mScreen->search(FSA_TAG64("Pscurh05"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurh04"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurh03"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurh02"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurh01"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurs05"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs04"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs03"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs02"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs01"))->setAlpha(100);
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmad01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 210, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmba01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(130, 170, 0, 255));
        break;
    case 1:
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh02")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh03")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh04")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh05")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs01")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs02")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs03")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs04")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs05")))->setBlackWhite(JUtility::TColor(255, 255, 120, 0), JUtility::TColor(255, 255, 120, 255));
        mScreen->search(FSA_TAG64("Pscurh05"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh04"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh03"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh02"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh01"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs05"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurs04"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurs03"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurs02"))->setAlpha(100.0f * alpha);
        mScreen->search(FSA_TAG64("Pscurs01"))->setAlpha(100.0f * alpha);
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmad01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(130, 170, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmba01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 210, 0, 255));
        break;
    case 2:
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh02")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh03")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh04")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh05")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs02")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs03")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs04")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs05")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        mScreen->search(FSA_TAG64("Pscurh05"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh04"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh03"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh02"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh01"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs05"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs04"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs03"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs02"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs01"))->setAlpha(100);
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmad01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(130, 170, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmba01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(130, 170, 0, 255));
        break;
    default:
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh02")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh03")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh04")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurh05")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs02")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs03")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs04")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pscurs05")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
        mScreen->search(FSA_TAG64("Pscurh05"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh04"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh03"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh02"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurh01"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs05"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs04"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs03"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs02"))->setAlpha(100);
        mScreen->search(FSA_TAG64("Pscurs01"))->setAlpha(100);
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmad01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(130, 170, 0, 255));
        static_cast<J2DPicture*>(mScreen->search(FSA_TAG64("Pmba01")))->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(130, 170, 0, 255));
        mScreen->search(FSA_TAG64("Pscuro02"))->setAlpha(field_0xa2 * alpha);
        mScreen->search(FSA_TAG64("Pscuro01"))->setAlpha(field_0xa2 * alpha);
        break;
    }
}

s32 GameMode_804B0688::fn_803DAC3C() {
    if (lbl_8053ADD0 != NULL) {
        return lbl_8053ADD0->field_0x68;
    }
    return 2;
}
