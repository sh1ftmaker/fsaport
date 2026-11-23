#include "game/GameAudio.h"
#include "game/cls_804B146C.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/Player.h"
#include "game/PlayerBit.h"
#include "JSystem/J2D/J2DPicture.h"
#include "JSystem/J2D/J2DScreen.h"
#include "JSystem/JFramework/JFWDisplay.h"
#include "JSystem/JUtility/JUTVideo.h"
#include "JSystem/J2D/J2DGrafContext.h"
#include "game/act/player_window_select_80441318.h"
#include "game/GameGlobals.h"
#include "game/save_data_803B8C74.h"
#include "game/Camera.h"
#include "game/Message.h"

const char* lbl_804B1428[3] = {
    "kk_heart_01.bti",
    "kk_heart_02.bti",
    "kk_heart_03.bti",
};

const char* lbl_804B1434[10] = {
    "kk_rupyNum_00.bti",
    "kk_rupyNum_01.bti",
    "kk_rupyNum_02.bti",
    "kk_rupyNum_03.bti",
    "kk_rupyNum_04.bti",
    "kk_rupyNum_05.bti",
    "kk_rupyNum_06.bti",
    "kk_rupyNum_07.bti",
    "kk_rupyNum_08.bti",
    "kk_rupyNum_09.bti",
};

const char* lbl_804B145C[4] = {
    "kk_mainRank_01.bti",
    "kk_mainRank_02.bti",
    "kk_mainRank_03.bti",
    "kk_mainRank_04.bti",
};

static const u64 lbl_8048E300[4][4] = {
    { 'Pp1m01', 'Pp1m02', 'Pp1m03', 'Pp1m04' },
    { 'Pp2m01', 'Pp2m02', 'Pp2m03', 'Pp2m04' },
    { 'Pp3m01', 'Pp3m02', 'Pp3m03', 'Pp3m04' },
    { 'Pp4m01', 'Pp4m02', 'Pp4m03', 'Pp4m04' },
};

static const u64 lbl_8048E380[4][3] = {
    { 'Pp1mb01', 'Pp1mb02', 'Pp1mb03' },
    { 'Pp2mb01', 'Pp2mb02', 'Pp2mb03' },
    { 'Pp3mb01', 'Pp3mb02', 'Pp3mb03' },
    { 'Pp4mb01', 'Pp4mb02', 'Pp4mb03' },
};

static const u64 lbl_8048E3E0[4][8] = {
    { 'Pp1lf01', 'Pp1lf02', 'Pp1lf03', 'Pp1lf04', 'Pp1lf05', 'Pp1lf06', 'Pp1lf07', 'Pp1lf08' },
    { 'Pp2lf01', 'Pp2lf02', 'Pp2lf03', 'Pp2lf04', 'Pp2lf05', 'Pp2lf06', 'Pp2lf07', 'Pp2lf08' },
    { 'Pp3lf01', 'Pp3lf02', 'Pp3lf03', 'Pp3lf04', 'Pp3lf05', 'Pp3lf06', 'Pp3lf07', 'Pp3lf08' },
    { 'Pp4lf01', 'Pp4lf02', 'Pp4lf03', 'Pp4lf04', 'Pp4lf05', 'Pp4lf06', 'Pp4lf07', 'Pp4lf08' },
};

static const u64 lbl_8048E4E0[4][8] = {
    { 'Pp1lfk01', 'Pp1lfk02', 'Pp1lfk03', 'Pp1lfk04', 'Pp1lfk05', 'Pp1lfk06', 'Pp1lfk07', 'Pp1lfk08' },
    { 'Pp2lfk01', 'Pp2lfk02', 'Pp2lfk03', 'Pp2lfk04', 'Pp2lfk05', 'Pp2lfk06', 'Pp2lfk07', 'Pp2lfk08' },
    { 'Pp3lfk01', 'Pp3lfk02', 'Pp3lfk03', 'Pp3lfk04', 'Pp3lfk05', 'Pp3lfk06', 'Pp3lfk07', 'Pp3lfk08' },
    { 'Pp4lfk01', 'Pp4lfk02', 'Pp4lfk03', 'Pp4lfk04', 'Pp4lfk05', 'Pp4lfk06', 'Pp4lfk07', 'Pp4lfk08' },
};

static const u64 lbl_8048E5E0[4][8] = {
    { 'Pp1lf09', 'Pp1lf10', 'Pp1lf11', 'Pp1lf12', 'Pp1lf13', 'Pp1lf14', 'Pp1lf15', 'Pp1lf16' },
    { 'Pp2lf09', 'Pp2lf10', 'Pp2lf11', 'Pp2lf12', 'Pp2lf13', 'Pp2lf14', 'Pp2lf15', 'Pp2lf16' },
    { 'Pp3lf09', 'Pp3lf10', 'Pp3lf11', 'Pp3lf12', 'Pp3lf13', 'Pp3lf14', 'Pp3lf15', 'Pp3lf16' },
    { 'Pp4lf09', 'Pp4lf10', 'Pp4lf11', 'Pp4lf12', 'Pp4lf13', 'Pp4lf14', 'Pp4lf15', 'Pp4lf16' },
};

static const u64 lbl_8048E6E0[4][8] = {
    { 'Pp1lfk09', 'Pp1lfk10', 'Pp1lfk11', 'Pp1lfk12', 'Pp1lfk13', 'Pp1lfk14', 'Pp1lfk15', 'Pp1lfk16' },
    { 'Pp2lfk09', 'Pp2lfk10', 'Pp2lfk11', 'Pp2lfk12', 'Pp2lfk13', 'Pp2lfk14', 'Pp2lfk15', 'Pp2lfk16' },
    { 'Pp3lfk09', 'Pp3lfk10', 'Pp3lfk11', 'Pp3lfk12', 'Pp3lfk13', 'Pp3lfk14', 'Pp3lfk15', 'Pp3lfk16' },
    { 'Pp4lfk09', 'Pp4lfk10', 'Pp4lfk11', 'Pp4lfk12', 'Pp4lfk13', 'Pp4lfk14', 'Pp4lfk15', 'Pp4lfk16' },
};

static const u64 lbl_8048E7E0[4][3] = {
    { 'Pp1iib01', 'Pp1iib02', 'Pp1iib03' },
    { 'Pp2iib01', 'Pp2iib02', 'Pp2iib03' },
    { 'Pp3iib01', 'Pp3iib02', 'Pp3iib03' },
    { 'Pp4iib01', 'Pp4iib02', 'Pp4iib03' },
};

static const u64 lbl_8048E840[4][3] = {
    { 'P1le201', 'P1le202', 'P1le203' },
    { 'P2le201', 'P2le202', 'P2le203' },
    { 'P3le201', 'P3le202', 'P3le203' },
    { 'P4le201', 'P4le202', 'P4le203' },
};

static const u64 lbl_8048E8A0[4][12] = {
    { 'Pp1ii01', 'Pp1ii03', 'Pp1ii05', 'Pp1ii07', 'Pp1ii09', 'Pp1ii11', 'Pp1ii13', 'Pp1ii15', 'Pp1ii17', 'Pp1ii19', 'Pp1ii21', 'Pp1ii23' },
    { 'Pp2ii01', 'Pp2ii03', 'Pp2ii05', 'Pp2ii07', 'Pp2ii09', 'Pp2ii11', 'Pp2ii13', 'Pp2ii15', 'Pp2ii17', 'Pp2ii19', 'Pp2ii21', 'Pp2ii23' },
    { 'Pp3ii01', 'Pp3ii03', 'Pp3ii05', 'Pp3ii07', 'Pp3ii09', 'Pp3ii11', 'Pp3ii13', 'Pp3ii15', 'Pp3ii17', 'Pp3ii19', 'Pp3ii21', 'Pp3ii23' },
    { 'Pp4ii01', 'Pp4ii03', 'Pp4ii05', 'Pp4ii07', 'Pp4ii09', 'Pp4ii11', 'Pp4ii13', 'Pp4ii15', 'Pp4ii17', 'Pp4ii19', 'Pp4ii21', 'Pp4ii23' },
};

static const u64 lbl_8048EA20[4][12] = {
    { 'Pp1ii02', 'Pp1ii04', 'Pp1ii06', 'Pp1ii08', 'Pp1ii10', 'Pp1ii12', 'Pp1ii14', 'Pp1ii16', 'Pp1ii18', 'Pp1ii20', 'Pp1ii22', 'Pp1ii24' },
    { 'Pp2ii02', 'Pp2ii04', 'Pp2ii06', 'Pp2ii08', 'Pp2ii10', 'Pp2ii12', 'Pp2ii14', 'Pp2ii16', 'Pp2ii18', 'Pp2ii20', 'Pp2ii22', 'Pp2ii24' },
    { 'Pp3ii02', 'Pp3ii04', 'Pp3ii06', 'Pp3ii08', 'Pp3ii10', 'Pp3ii12', 'Pp3ii14', 'Pp3ii16', 'Pp3ii18', 'Pp3ii20', 'Pp3ii22', 'Pp3ii24' },
    { 'Pp4ii02', 'Pp4ii04', 'Pp4ii06', 'Pp4ii08', 'Pp4ii10', 'Pp4ii12', 'Pp4ii14', 'Pp4ii16', 'Pp4ii18', 'Pp4ii20', 'Pp4ii22', 'Pp4ii24' },
};

static const u64 lbl_8048EBA0[4][2] = {
    { 'P1r01', 'P1rk01' },
    { 'P2r01', 'P2rk01' },
    { 'P3r01', 'P3rk01' },
    { 'P4r01', 'P4rk01' },
};

static const u64 lbl_8048EBE0[4][5] = {
    { 'P1s01', 'P1s02', 'P1s03', 'P1s04', 'P1s05' },
    { 'P2s01', 'P2s02', 'P2s03', 'P2s04', 'P2s05' },
    { 'P3s01', 'P3s02', 'P3s03', 'P3s04', 'P3s05' },
    { 'P4r01', 'P4r02', 'P4r03', 'P4r04', 'P4r05' },
};

static const u64 lbl_8048EC80[4][5] = {
    { 'P1sk01', 'P1sk02', 'P1sk03', 'P1sk04', 'P1sk05' },
    { 'P2sk01', 'P2sk02', 'P2sk03', 'P2sk04', 'P2sk05' },
    { 'P3sk01', 'P3sk02', 'P3sk03', 'P3sk04', 'P3sk05' },
    { 'P4rk01', 'P4rk02', 'P4rk03', 'P4rk04', 'P4rk05' },
};

static const u64 lbl_8048ED20[4][6] = {
    { 'Pp1i01', 'Pp1i02', 'Pp1ib01', 'Pp1ib02', 'Pp1ib03', 'Pp1ib04' },
    { 'Pp2i01', 'Pp2i02', 'Pp2ib01', 'Pp2ib02', 'Pp2ib03', 'Pp2ib04' },
    { 'Pp3i01', 'Pp3i02', 'Pp3ib01', 'Pp3ib02', 'Pp3ib03', 'Pp3ib04' },
    { 'Pp4i01', 'Pp4i02', 'Pp4ib01', 'Pp4ib02', 'Pp4ib03', 'Pp4ib04' },
};

static const u64 lbl_8048EDE0[4][2] = {
    { 'P11st01', 'P11st02' },
    { 'P21st01', 'P21st02' },
    { 'P31st01', 'P31st02' },
    { 'P41st01', 'P41st02' },
};

static const u64 lbl_8048EE20[8] = {
    'Pp1lf01', 'Pp1lf02', 'Pp1lf03', 'Pp1lf04', 'Pp1lf05', 'Pp1lf06', 'Pp1lf07', 'Pp1lf08',
};

static const u64 lbl_8048EE60[8] = {
    'Pp1lf09', 'Pp1lf10', 'Pp1lf11', 'Pp1lf12', 'Pp1lf13', 'Pp1lf14', 'Pp1lf15', 'Pp1lf16',
};

static const u64 lbl_8048EEA0[8] = {
    'Pp1lfk01', 'Pp1lfk02', 'Pp1lfk03', 'Pp1lfk04', 'Pp1lfk05', 'Pp1lfk06', 'Pp1lfk07', 'Pp1lfk08',
};

static const u64 lbl_8048EEE0[8] = {
    'Pp1lfk09', 'Pp1lfk10', 'Pp1lfk11', 'Pp1lfk12', 'Pp1lfk13', 'Pp1lfk14', 'Pp1lfk15', 'Pp1lfk16',
};

static const u64 lbl_8048EF20[4] = {
    'P11st02', 'P21st02', 'P31st02', 'P41st02',
};

static const u64 lbl_8048EF40[4] = {
    'P11st01', 'P21st01', 'P31st01', 'P41st01',
};

static const u64 lbl_8048EF60[4] = {
    'Pp1ra02', 'Pp2ra02', 'Pp3ra02', 'Pp4ra02',
};

static const u64 lbl_8048EF80[4] = {
    'Pp1ra01', 'Pp2ra01', 'Pp3ra01', 'Pp4ra01',
};

static const u64 lbl_8048EFA0[4] = {
    'Ndata01', 'Ndata04', 'Ndata07', 'Ndata10',
};

static const u64 lbl_8048EFC0[4] = {
    'Ndata20', 'Ndata21', 'Ndata22', 'Ndata23',
};

static const u64 lbl_8048EFE0[4] = {
    'Pp1noi02', 'Pp1noi03', 'Pp3noi02', 'Pp4noi02',
};

static const u64 lbl_8048F000[4] = {
    'Pp1noi01', 'Pp1noi04', 'Pp3noi01', 'Pp4noi01',
};

static const u64 lbl_8048F020[4] = {
    'Pp1iib03', 'Pp2iib03', 'Pp3iib03', 'Pp4iib03',
};

static const u64 lbl_8048F040[4] = {
    'Ndata03', 'Ndata06', 'Ndata09', 'Ndata12',
};

static const u64 lbl_8048F060[4] = {
    'Ndata02', 'Ndata05', 'Ndata08', 'Ndata11',
};

static const u64 lbl_8048F080[4] = {
    'Pp1nm04', 'Pp2nm04', 'Pp3nm04', 'Pp4nm04',
};

static const u64 lbl_8048F0A0[4] = {
    'Pp1nm03', 'Pp2nm03', 'Pp3nm03', 'Pp4nm03',
};

static const u64 lbl_8048F0C0[4] = {
    'Pp1nm02', 'Pp2nm02', 'Pp3nm02', 'Pp4nm02',
};

static const u64 lbl_8048F0E0[4] = {
    'Pp1nm01', 'Pp2nm01', 'Pp3nm01', 'Pp4nm01',
};

static const u64 lbl_8048F100[3] = {
    'Php1mb03', 'Php1mb01', 'Php1mb01',
};

static const u64 lbl_8048F118[4] = {
    'Php1m04', 'Php1m03', 'Php1m02', 'Php1m01',
};

static const u64 lbl_8048F138[4] = {
    'Php1nm04', 'Php1nm03', 'Php1nm02', 'Php1nm01',
};

static const u64 lbl_8048F158[6] = {
    'Ptk01', 'Pt01', 'Ptk02', 'Pt02', 'Ptim01', 'Ptim02',
};

static const f32 lbl_8048F188[10] = {
    102.5f, 502.5f, 102.5f, 502.5f, 302.5f,
    302.5f, 102.5f, 102.5f, 502.5f, 502.5f,
};

static const f32 lbl_8048F1B0[10] = {
    60.0f, 60.0f, 420.0f, 420.0f, 60.0f,
    420.0f, 180.0f, 300.0f, 180.0f, 300.0f,
};

static const u8 lbl_8048F1D8[4][10] = {
    { 0, 1, 2, 3, 4, 6, 7, 5, 8, 9 },
    { 1, 3, 0, 2, 4, 8, 9, 5, 6, 7 },
    { 2, 0, 3, 1, 5, 7, 6, 4, 9, 8 },
    { 3, 2, 1, 0, 5, 9, 8, 4, 7, 6 },
};

// FIXME(shotgun): 99.97 %, operands; behaviour near.
BattleHud_804B146C::BattleHud_804B146C()
    : field_0x210(0xFFFFFFFF)
    , field_0x214(0xFFFFFFFF)
{
    field_0x25d = 0;
    field_0x25c = 0;
    field_0x25e[0] = 0;
    field_0x25e[1] = 0;
    field_0x25e[2] = 0;
    field_0x25e[3] = 0;
    lbl_8053AED0->field_0x10e |= 1;
    field_0x4c = new J2DOrthoGraph(
        0.0f, 0.0f,
        JFWDisplay::getManager()->getEfbWidth(),
        JFWDisplay::getManager()->getEfbHeight(),
        -10000.0f, 10000.0f
    );
    field_0xf0 = new PlayerWindowSelect_804B19E0();
    field_0x4 = new J2DScreen();
    field_0x4->setPriority("main_bat.blo", 0x1100000, lbl_80529DEC.mGame2DArchive_0x4c);
    for (s32 i = 0; i < 4; i++) {
        field_0x8[i] = new J2DScreen();
        field_0x8[i]->setPriority("main_hubuki.blo", 0x1100000, lbl_80529DEC.mGame2DArchive_0x4c);
        field_0x2c[i] = new J2DScreen();
        field_0x2c[i]->setPriority("main_diving.blo", 0x1100000, lbl_80529DEC.mMain2DArchive_0x20);
    }
    mScreen = new J2DScreen();
    mScreen->setPriority("main_bat_ranking.blo", 0x1100000, lbl_80529DEC.mGame2DArchive_0x4c);
    field_0x4->search('Pp1li01')->hide();
    field_0x4->search('Pp2li01')->hide();
    field_0x4->search('Pp3li01')->hide();
    field_0x4->search('Pp4li01')->hide();
    field_0xf4 = field_0x4->search(lbl_8048E300[0][0])->getWidth();
    field_0x214 = static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[0][0]))->getBlack();
    field_0x210 = static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[0][0]))->getWhite();
    for (s32 i = 0; i < 4; i++) {
        field_0x50[i] = new PaneCtrl_804AF86C();
        field_0x50[i]->setPane_80402F04(mScreen->search(lbl_8048EF20[i]));
        field_0x60[i] = new PaneCtrl_804AF86C();
        field_0x60[i]->setPane_80402F04(mScreen->search(lbl_8048EF60[i]));
        field_0x70[i] = new PaneCtrl_804AF86C();
        field_0x70[i]->setPane_80402F04(field_0x4->search(lbl_8048EF80[i]));
        field_0x80[i] = new PaneCtrl_804AF86C();
        field_0x80[i]->setPane_80402F04(field_0x4->search(lbl_8048EFA0[i]));
        field_0x90[i] = new PaneCtrl_804AF86C();
        field_0x90[i]->setPane_80402F04(field_0x4->search(lbl_8048EFC0[i]));
        field_0xa0[i] = new PaneCtrl_804AF86C();
        field_0xa0[i]->setPane_80402F04(field_0x4->search(lbl_8048EFE0[i]));
        field_0xb0[i] = new PaneCtrl_804AF86C();
        field_0xb0[i]->setPane_80402F04(field_0x4->search(lbl_8048F020[i]));
        field_0xc0[i] = new PaneCtrl_804AF86C();
        field_0xc0[i]->setPane_80402F04(field_0x4->search(lbl_8048F040[i]));
        field_0xd0[i] = new PaneCtrl_804AF86C();
        field_0xd0[i]->setPane_80402F04(field_0x4->search(lbl_8048F060[i]));
        field_0xe0[i] = new PaneCtrl_804AF86C();
        field_0xe0[i]->setPane_80402F04(field_0x4->search(lbl_8048F080[i]));
    }
    field_0x218 = field_0x4->search(lbl_8048E300[0][0])->getAlpha();
    for (s32 i = 0; i < 3; i++) {
        field_0x219[i] = field_0x4->search(lbl_8048E380[0][i])->getAlpha();
    }
    field_0x21c = field_0x4->search(lbl_8048E3E0[0][0])->getAlpha();
    field_0x21d = field_0x4->search(lbl_8048E4E0[0][0])->getAlpha();
    field_0x21e = field_0x4->search(lbl_8048E5E0[0][0])->getAlpha();
    field_0x21f = field_0x4->search(lbl_8048E6E0[0][0])->getAlpha();
    for (s32 i = 0; i < 3; i++) {
        field_0x220[i] = field_0x4->search(lbl_8048E7E0[0][i])->getAlpha();
        field_0x223[i] = field_0x4->search(lbl_8048E840[0][i])->getAlpha();
    }
    field_0x226 = field_0x4->search(lbl_8048E8A0[0][0])->getAlpha();
    field_0x227 = field_0x4->search(lbl_8048EA20[0][0])->getAlpha();
    field_0x228 = field_0x4->search(lbl_8048EBA0[0][0])->getAlpha();
    field_0x229 = field_0x4->search(lbl_8048EBA0[0][1])->getAlpha();
    field_0x22a = field_0x4->search(lbl_8048EBE0[0][0])->getAlpha();
    field_0x22b = field_0x4->search(lbl_8048EC80[0][0])->getAlpha();
    for (s32 i = 0; i < 6; i++) {
        field_0x22c[i] = field_0x4->search(lbl_8048ED20[0][i])->getAlpha();
    }
    field_0x232 = mScreen->search(lbl_8048EF60[0])->getAlpha();
    field_0x233 = field_0x4->search(lbl_8048EF80[0])->getAlpha();
    field_0x234 = mScreen->search(lbl_8048EDE0[0][0])->getAlpha();
    field_0x235 = mScreen->search(lbl_8048EDE0[0][1])->getAlpha();
    field_0x236 = field_0x4->search(lbl_8048EFE0[0])->getAlpha();
    field_0x237 = field_0x4->search(lbl_8048F000[0])->getAlpha();
    field_0x238 = field_0x4->search(lbl_8048F080[0])->getAlpha();
    field_0x239 = field_0x4->search(lbl_8048F0A0[0])->getAlpha();
    field_0x23a = field_0x4->search(lbl_8048F0C0[0])->getAlpha();
    field_0x23b = field_0x4->search(lbl_8048F0E0[0])->getAlpha();
    for (s32 i = 0; i < 6; i++) {
        field_0x247[i] = field_0x2c[0]->search(lbl_8048F158[i])->getAlpha();
    }
    for (s32 p = 0; p < 4; p++) {
        for (s32 i = 0; i < 8; i++) {
            for (s32 k = 0; k < 3; k++) {
                const ResTIMG* res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1428[k]));
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E3E0[p][i]))->append(res, 1.0f);
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E4E0[p][i]))->append(res, 1.0f);
            }
        }
    }
    for (s32 p = 0; p < 4; p++) {
        for (s32 k = 1; k < 4; k++) {
            const ResTIMG* res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B145C[k]));
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF20[p]))->append(res, 1.0f);
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[p]))->append(res, 1.0f);
        }
    }
    for (s32 p = 0; p < 4; p++) {
        for (s32 i = 0; i < 8; i++) {
            for (s32 k = 0; k < 3; k++) {
                const ResTIMG* res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1428[k]));
                static_cast<J2DPicture*>(field_0x8[p]->search(lbl_8048EE20[i]))->append(res, 1.0f);
                static_cast<J2DPicture*>(field_0x8[p]->search(lbl_8048EEA0[i]))->append(res, 1.0f);
            }
        }
    }
}

BattleHud_804B146C::~BattleHud_804B146C() {
    delete field_0x4c;
    field_0x4c = NULL;
    delete field_0xf0;
    field_0xf0 = NULL;
    delete field_0x4;
    field_0x4 = NULL;
    for (s32 i = 0; i < 4; i++) {
        delete field_0x8[i];
        field_0x8[i] = NULL;
        delete field_0x2c[i];
        field_0x2c[i] = NULL;
    }
    delete mScreen;
    mScreen = NULL;
    for (s32 i = 0; i < 4; i++) {
        delete field_0x50[i];
        field_0x50[i] = NULL;
        delete field_0x60[i];
        field_0x60[i] = NULL;
        delete field_0x70[i];
        field_0x70[i] = NULL;
        delete field_0x80[i];
        field_0x80[i] = NULL;
        delete field_0x90[i];
        field_0x90[i] = NULL;
        delete field_0xa0[i];
        field_0xa0[i] = NULL;
        delete field_0xb0[i];
        field_0xb0[i] = NULL;
        delete field_0xc0[i];
        field_0xc0[i] = NULL;
        delete field_0xd0[i];
        field_0xd0[i] = NULL;
        delete field_0xe0[i];
        field_0xe0[i] = NULL;
    }
}

static inline u8 checkBit(u8 mask, s32 bit) {
    u8 b = 1 << bit;
    return (mask & b) != 0;
}

static inline void setBit(u8& mask, s32 bit) {
    u8 b = 1 << bit;
    mask |= b;
}

static inline void clearBit(u8& mask, s32 bit) {
    u8 b = 1 << bit;
    mask &= ~b;
}

static inline void call_80421A34_forActivePlayers(BattleHud_804B146C* self, bool force) {
    for (s32 i = 0; i < 4; i++) {
        if (gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) {
            self->fn_80421A34(i, force);
        }
    }
}

void BattleHud_804B146C::fn_804263A8() {
    for (s32 i = 0; i < 4; i++) {
        bool takesPart = checkBit(gGameRoot_8053AAF8->mPlayerMask, i);
        if (!takesPart) {
            clearBit(field_0x25d, i);
        } else {
            bool flagged = checkBit(lbl_8053AED0->field_0xdd, i);
            if (flagged && !checkBit(field_0x25c, i)) {
                setBit(field_0x25c, i);
                setBit(field_0x25d, i);
            } else if (lbl_8053AED0->field_0xdc & 0x10) {
                clearBit(field_0x25d, i);
            } else if (gGameRoot_8053AAF8->field_0x4c == 11) {
                clearBit(field_0x25d, i);
            }
        }
        lbl_80529DEC.field_0xd8[i] = getPlayerLife_80237774(i);
        lbl_80529DEC.field_0xe8[i] = fn_80236100(i);
        lbl_80529DEC.field_0x108[i] = fn_802375B0(i);
        field_0x158[i] = 0x180;
        field_0x168[i] = 0;
        field_0x178[i] = 0;
        field_0x188[i] = 0;
        field_0xf8[i] = 0.0f;
        field_0x108[i] = 0.0f;
        field_0x118[i] = 0.0f;
        field_0x128[i] = 0.0f;
        field_0x138[i] = 0.0f;
        field_0x251[i] = i;
        field_0x24d[i] = 0xFF;
        field_0x1f0[i] = 0;
        field_0x198[i] = 0;
        field_0x1a8[i] = 0;
        field_0x1b8[i] = 0;
        field_0x1c8[i] = 0;
        field_0x1e8[i] = 0;
        field_0x1f8[i] = 0;
        field_0x200[i] = 0;
        field_0x1d8[i] = 0;
        field_0x208[i] = 0;
    }
    call_80421A34_forActivePlayers(this, true);
    field_0x255 = 0xF;
    field_0x256 = 0xF;
    field_0x257 = 0xF;
    field_0x258 = 0;
    field_0x259 = 0;
    field_0x25a = 0;
    field_0x25b = 0;
    field_0x148[0] = -2;
    field_0x148[1] = -2;
    field_0x148[2] = -2;
    field_0x148[3] = -2;
    for (s32 i = 0; i < 4; i++) {
        if (gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) {
            mScreen->search(lbl_8048EF20[i])->hide();
            mScreen->search(lbl_8048EF60[i])->hide();
            field_0x4->search(lbl_8048EF80[i])->hide();
            field_0x4->search(lbl_8048EFA0[i])->show();
            field_0x4->search(lbl_8048EFE0[i])->hide();
            field_0x4->search(lbl_8048F020[i])->show();
            if (lbl_8050EC80.getCurrentFile().field_0x69 != 0) {
                field_0x4->search(lbl_8048F040[i])->show();
            } else {
                field_0x4->search(lbl_8048F040[i])->hide();
            }
            u32 mode = (gGameRoot_8053AAF8->field_0x83 >> (i * 2)) & 3;
            if (mode == 2) {
                field_0x4->search(lbl_8048EFC0[i])->show();
                field_0x4->search(lbl_8048F060[i])->show();
                field_0x4->search(lbl_8048F080[i])->hide();
            } else {
                field_0x4->search(lbl_8048EFC0[i])->hide();
                field_0x4->search(lbl_8048F060[i])->hide();
                field_0x4->search(lbl_8048F080[i])->hide();
            }
        } else {
            mScreen->search(lbl_8048EF20[i])->hide();
            mScreen->search(lbl_8048EF60[i])->hide();
            field_0x4->search(lbl_8048EF80[i])->hide();
            field_0x4->search(lbl_8048EFA0[i])->hide();
            field_0x4->search(lbl_8048EFC0[i])->hide();
            field_0x4->search(lbl_8048EFE0[i])->hide();
            field_0x4->search(lbl_8048F020[i])->hide();
            field_0x4->search(lbl_8048F040[i])->hide();
            field_0x4->search(lbl_8048F060[i])->hide();
            field_0x4->search(lbl_8048F080[i])->hide();
        }
    }
    for (s32 i = 0; i < 4; i++) {
        const ResTIMG* res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B145C[0]));
        static_cast<J2DPicture*>(mScreen->search(lbl_8048EF20[i]))->changeTexture(res, 0);
        static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))->changeTexture(res, 0);
    }
    for (s32 i = 0; i < 4; i++) {
        if (gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) {
            fn_80424148(i, true);
        }
    }
    field_0xf0->fn_8044172C();
}

static void pool_80543A58(const Vec& screen, f32 sx, f32 sy, f32* x, f32* y);

static inline bool isPaused() {
    if (gGameRoot_8053AAF8->field_0x4c == 11) {
        return true;
    }
    return gGameRoot_8053AAF8->field_0x88 == 2;
}

void BattleHud_804B146C::fn_80425F6C() {
    f32 zoom = gGameRoot2_8053AB10->field_0x24->mZoom_0x858;
    f32 scale = lbl_8050DB0C.field_0x30[4];
    f32 sx = scale * (JFWDisplay::getManager()->getEfbWidth() / zoom);
    f32 sy = scale * (JFWDisplay::getManager()->getEfbHeight() / zoom);
    for (s32 i = 0; i < 4; i++) {
        if (!(gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) || getPlayerLayer_8023DE58(getRepresentativePlayer_8022EA0C(i)) >= 8) {
            continue;
        }
        Vec screen;
        f32 x;
        f32 y;
        if (field_0x1e8[i] > 0 && lbl_8050EC80.getCurrentFile().field_0x69 != 0) {
            gGameRoot2_8053AB10->field_0x24->worldToScreen_8030C5F0(getPlayerPos_8023E724(getRepresentativePlayer_8022EA0C(i)), false, false, &screen);
            pool_80543A58(screen, sx, sy, &x, &y);
            field_0x4c->setOrtho(0.0f, 0.0f, sx, sy, -10000.0f, 10000.0f);
            field_0x4c->setPort();
            field_0x8[i]->draw(x, y, field_0x4c);
            field_0x208[i] = 0;
        } else if (!isPaused() && fn_802366CC(getRepresentativePlayer_8022EA0C(i)) > 0 && fn_802366CC(getRepresentativePlayer_8022EA0C(i)) < 300) {
            gGameRoot2_8053AB10->field_0x24->worldToScreen_8030C5F0(getPlayerPos_8023E724(getRepresentativePlayer_8022EA0C(i)), false, false, &screen);
            pool_80543A58(screen, sx, sy, &x, &y);
            field_0x4c->setOrtho(0.0f, 0.0f, sx, sy, -10000.0f, 10000.0f);
            field_0x4c->setPort();
            field_0x2c[i]->draw(x, y, field_0x4c);
        }
    }
    gGameRoot2_8053AB10->setPort_8013B490();
    field_0x4->draw(0.0f, 0.0f, gGameRoot2_8053AB10->field_0x10);
    field_0xf0->fn_8044148C();
}

void BattleHud_804B146C::fn_80425F24() {
    gGameRoot2_8053AB10->setPort_8013B490();
    mScreen->draw(0.0f, 0.0f, gGameRoot2_8053AB10->field_0x10);
}

// FIXME(shotgun): 97.86 %, regalloc-only; behaviour clean.
void BattleHud_804B146C::fn_804257E4() {
    for (s32 i = 0; i < 4; i++) {
        u8 bit = (u8)(1 << i);
        bool takesPart = checkBit(gGameRoot_8053AAF8->mPlayerMask, i);
        if (!takesPart) {
            field_0x25d &= ~bit;
        } else {
            bool flagged = checkBit(lbl_8053AED0->field_0xdd, i);
            if (flagged && !(field_0x25c & bit)) {
                field_0x25c |= bit;
                field_0x25d |= bit;
            } else if (lbl_8053AED0->field_0xdc & 0x10) {
                field_0x25d &= ~bit;
            } else if (gGameRoot_8053AAF8->field_0x4c == 11) {
                field_0x25d &= ~bit;
            }
        }
        if (gGameRoot_8053AAF8->mPlayerMask & bit) {
            fn_804253E0(i);
            u32 mode = (gGameRoot_8053AAF8->field_0x83 >> (i * 2)) & 3;
            if (mode == 1) {
                if (getPlayerLayer_8023DE58(i) >= 8) {
                    if (!(field_0x258 & bit)) {
                        setBit(field_0x258, i);
                        mScreen->search(lbl_8048EF60[i])->show();
                        field_0x4->search(lbl_8048EF80[i])->show();
                        field_0x4->search(lbl_8048EFA0[i])->hide();
                    }
                } else if (field_0x258 & bit) {
                    field_0x258 &= ~bit;
                    mScreen->search(lbl_8048EF60[i])->hide();
                    field_0x4->search(lbl_8048EF80[i])->hide();
                    field_0x4->search(lbl_8048EFA0[i])->show();
                }
            }
            fn_80423DF0(i);
            u32 mode2 = (gGameRoot_8053AAF8->field_0x83 >> (i * 2)) & 3;
            if (mode2 == 2) {
                if (!(field_0x25a & bit)) {
                    if (fn_8023BD70(i)) {
                        setBit(field_0x25a, i);
                        field_0x4->search(lbl_8048F080[i])->show();
                    }
                } else if (lbl_8053AD30->fn_80309D24(getPlayerInputSlot_8030AAF4(i), 0x100) == 0 || (field_0x255 & bit)) {
                    field_0x25a &= ~bit;
                    field_0x4->search(lbl_8048F080[i])->hide();
                }
            }
            if (field_0x255 & bit) {
                s32 val = lbl_80529DEC.field_0x108[i];
                if (val > 0x180) {
                    val = 0x180;
                }
                if (val < 0) {
                    val = 0;
                }
                for (s32 j = 0; j < 4; j++) {
                    updateGaugeBar(i, j, val);
                }
            }
            fn_80423248(i);
            fn_80423804(i);
        }
    }
    fn_80422824();
    for (s32 i = 0; i < 4; i++) {
        fn_80424148(i, false);
        if (gGameRoot2_8053AB10->field_0x20->field_0x148[i] != 0) {
            s32 x = field_0x4->search(lbl_8048EF80[i])->getBounds()->i.x;
            s32 y = field_0x4->search(lbl_8048EF80[i])->getBounds()->i.y;
            f32 w = field_0x4->search(lbl_8048EF80[i])->getWidth();
            f32 h = field_0x4->search(lbl_8048EF80[i])->getHeight();
            TileMap_804AA858* map = gGameRoot2_8053AB10->field_0x20;
            map->field_0x1d0[i].field_0x0 = x;
            map->field_0x1d0[i].field_0x4 = y;
            map->field_0x1d0[i].field_0x8 = w;
            map->field_0x1d0[i].field_0xc = h;
            if (i == 0 || i == 2) {
                if (field_0xf8[i] < 12.0f) {
                    field_0xf8[i] += 3.0f;
                } else {
                    field_0xf8[i] = 15.0f;
                }
            } else if (field_0xf8[i] > -12.0f) {
                field_0xf8[i] -= 3.0f;
            } else {
                field_0xf8[i] = -15.0f;
            }
        } else if (i == 0 || i == 2) {
            if (field_0xf8[i] < 3.0f) {
                field_0xf8[i] = 0.0f;
            } else {
                field_0xf8[i] -= 3.0f;
            }
        } else if (field_0xf8[i] > -3.0f) {
            field_0xf8[i] = 0.0f;
        } else {
            field_0xf8[i] += 3.0f;
        }
    }
    for (s32 i = 0; i < 4; i++) {
        if (gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) {
            fn_8042215C(i);
            fn_80421D78(i);
        }
    }
    call_80421A34_forActivePlayers(this, false);
    field_0xf0->fn_80441490();
    field_0x255 = 0;
    field_0x256 = 0;
    field_0x257 = 0;
}

void BattleHud_804B146C::fn_804253E0(s32 player) {
    if (lbl_80529DEC.getField0xd8(player) < getPlayerLife_80237774(player)) {
        field_0x168[player]++;
        if (field_0x168[player] >= 10) {
            lbl_80529DEC.field_0xd8[player] += 1;
            for (s32 j = 0; j < 4; j++) {
                if (j != player && hasPlayerLeader_8022F514(j) && player == getPlayerLeader_8022F3DC(j)) {
                    lbl_80529DEC.field_0xd8[j] = lbl_80529DEC.field_0xd8[player];
                }
            }
            field_0x256 |= (u8)(1 << player);
            field_0x168[player] = 0;
            fn_80230EB8(player, 9);
            field_0x1e8[player] = lbl_8050DB0C.field_0x4c[(0x74 - 0x4C) / 2] * lbl_8050DB0C.field_0x4c[(0x76 - 0x4C) / 2] + 30;
            field_0x1b8[player] = field_0x1a8[player];
            field_0x1a8[player] = lbl_80529DEC.field_0xd8[player];
            field_0x1c8[player] = getPlayerMaxLife_80237810(player);
        }
    } else if (lbl_80529DEC.getField0xd8(player) > getPlayerLife_80237774(player)) {
        lbl_80529DEC.field_0x24a |= (u8)(1 << player);
        field_0x168[player]++;
        if (field_0x168[player] >= 10) {
            lbl_80529DEC.field_0xd8[player] -= 1;
            for (s32 j = 0; j < 4; j++) {
                if (j != player && hasPlayerLeader_8022F514(j) && player == getPlayerLeader_8022F3DC(j)) {
                    lbl_80529DEC.field_0xd8[j] = lbl_80529DEC.field_0xd8[player];
                }
            }
            field_0x256 |= (u8)(1 << player);
            field_0x168[player] = 0;
        }
        field_0x1e8[player] = lbl_8050DB0C.field_0x4c[(0x74 - 0x4C) / 2] * lbl_8050DB0C.field_0x4c[(0x76 - 0x4C) / 2] + 30;
        field_0x1b8[player] = field_0x1a8[player];
        field_0x1a8[player] = lbl_80529DEC.field_0xd8[player];
        field_0x1c8[player] = getPlayerMaxLife_80237810(player);
    } else {
        field_0x168[player] = 0;
    }
    if (lbl_80529DEC.getField0xe8(player) < fn_80236100(player)) {
        field_0x178[player]++;
        if (field_0x178[player] >= 2) {
            lbl_80529DEC.field_0xe8[player] += 2;
            if (lbl_80529DEC.getField0xe8(player) >= fn_80236100(player)) {
                lbl_80529DEC.field_0xe8[player] = fn_80236100(player);
                fn_80230EB8(player, 0x51);
            } else {
                fn_80230EB8(player, 4);
            }
            field_0x257 |= (u8)(1 << player);
            field_0x178[player] = 0;
        }
    } else if (lbl_80529DEC.getField0xe8(player) > fn_80236100(player)) {
        field_0x178[player]++;
        if (field_0x178[player] >= 2) {
            lbl_80529DEC.field_0xe8[player] -= 2;
            if (lbl_80529DEC.getField0xe8(player) < fn_80236100(player)) {
                lbl_80529DEC.field_0xe8[player] = fn_80236100(player);
                fn_80230EB8(player, 0x51);
            } else {
                fn_80230EB8(player, 0x4E);
            }
            field_0x257 |= (u8)(1 << player);
            field_0x178[player] = 0;
        }
    } else {
        field_0x178[player] = 0;
    }
    if (field_0x1f8[player] > 0) {
        if (field_0x200[player] == 0) {
            field_0x1f8[player]--;
            fn_80230FD0(player, 0x20034);
            if (field_0x1f8[player] != 0) {
                field_0x200[player] = 12;
            }
        } else {
            field_0x200[player]--;
        }
    }
}

void BattleHud_804B146C::fn_80425234(s32 id, s32 arg1, u32 arg2) {
    s32 target;
    if (hasPlayerLeader_8022F514(id)) {
        target = getPlayerLeader_8022F3DC(id);
    } else {
        target = id;
    }
    if ((u8)arg2) {
        s32 sum = arg1 + fn_80236100(target);
        if (sum > 9999) {
            arg1 = 9999 - fn_80236228(target);
        } else if (sum < -5000) {
            arg1 = -5000 - fn_80236228(target);
        }
    } else {
        s32 sum = arg1 + fn_80236228(target);
        if (sum > 9999) {
            arg1 = 9999 - fn_80236228(target);
        } else if (sum < 0) {
            if (fn_80236228(target) <= 0) {
                arg1 = 0;
            } else {
                arg1 = -fn_80236228(target);
            }
        }
    }
    if (arg1 >= 1000 || arg1 <= -1000) {
        s32 amount = __abs(arg1);
        field_0x1f8[id] = amount / 100;
        field_0x200[id] = 0;
    } else {
        fn_80230FD0(id, 0x20034);
    }
    if ((u8)arg2) {
        fn_8023619C(target, arg1);
    } else {
        fn_802362C4(target, arg1);
    }
    lbl_80529DEC.field_0xe8[target] = fn_80236100(target);
    field_0x178[target] = 0;
    field_0x257 |= (u8)(1 << target);
}

void BattleHud_804B146C::fn_80425128() {
    for (s32 i = 0; i < 4; i++) {
        if (isPlayerBitSet(gGameRoot_8053AAF8, i) != 0) {
            lbl_80529DEC.field_0xd8[i] = getPlayerLife_80237774(i);
            for (s32 j = 0; j < 4; j++) {
                if (j != i && hasPlayerLeader_8022F514(j) && i == getPlayerLeader_8022F3DC(j)) {
                    lbl_80529DEC.field_0xd8[j] = lbl_80529DEC.field_0xd8[i];
                }
            }
            field_0x256 |= (u8)(1 << i);
            field_0x168[i] = 0;
            lbl_80529DEC.field_0xe8[i] = fn_80236100(i);
            field_0x257 |= (u8)(1 << i);
            field_0x178[i] = 0;
        }
    }
}

void BattleHud_804B146C::fn_80424148(s32 player, bool force) {
    f32 prevA = field_0x108[player];
    f32 prevB = field_0x128[player];
    f32 prevC = field_0x138[player];
    if (player >= 2) {
        field_0x50[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D57C());
        field_0x60[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D57C());
        field_0x70[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D57C());
        field_0x80[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D57C());
        field_0x90[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D57C());
        field_0xa0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D57C());
        field_0xb0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D57C());
        field_0xc0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D57C());
        field_0xd0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D57C());
        field_0xe0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D57C());
    } else {
        field_0x50[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D514());
        field_0x60[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D514());
        field_0x70[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D514());
        field_0x80[player]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D514());
        field_0x90[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D514());
        field_0xa0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D514());
        field_0xb0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D514());
        field_0xc0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D514());
        field_0xd0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D514());
        field_0xe0[player]->moveFromCentre_80402E08(field_0xf8[player], lbl_8053AED0->fn_8043D514());
    }
    fn_8042165C();
    fn_8037EB4C();
    if ((lbl_8053AED0->field_0xdc >> 4) & 1) {
        clearBit(lbl_8053AED0->field_0xdc, player);
    }
    bool active = checkBit(field_0x25d, player);
    if (!active || (lbl_8053AED0->field_0xdc & (u8)(1 << player)) ||
        gGameRoot2_8053AB10->field_0x30->field_0x284 != 0 || gGameRoot_8053AAF8->field_0x85 != 0) {
        if (field_0x108[player] > 0.1f) {
            field_0x108[player] -= 0.1f;
        } else {
            field_0x108[player] = 0.0f;
        }
        if (field_0x128[player] > 0.1f) {
            field_0x128[player] -= 0.1f;
        } else {
            field_0x128[player] = 0.0f;
        }
        if (field_0x138[player] > 0.1f) {
            field_0x138[player] -= 0.1f;
        } else {
            field_0x138[player] = 0.0f;
        }
        gGameRoot2_8053AB10->field_0x20->field_0x217[player] = 1;
    } else {
        if (lbl_80529DEC.field_0xd8[player] == 0) {
            if (field_0x108[player] > 0.6f) {
                field_0x108[player] -= 0.1f;
                if (field_0x108[player] <= 0.6f) {
                    field_0x108[player] = 0.6f;
                }
            } else if (field_0x108[player] < 0.6f) {
                field_0x108[player] += 0.1f;
                if (field_0x108[player] >= 0.6f) {
                    field_0x108[player] = 0.6f;
                }
            }
            if (gGameRoot2_8053AB10->field_0x20->field_0x148[player] != 0) {
                if (field_0x128[player] < 0.9f) {
                    field_0x128[player] += 0.1f;
                } else {
                    field_0x128[player] = 1.0f;
                }
                if (field_0x138[player] > 0.6f) {
                    field_0x138[player] -= 0.1f;
                    if (field_0x138[player] <= 0.6f) {
                        field_0x138[player] = 0.6f;
                    }
                } else if (field_0x138[player] < 0.6f) {
                    field_0x138[player] += 0.1f;
                    if (field_0x138[player] >= 0.6f) {
                        field_0x138[player] = 0.6f;
                    }
                }
            } else {
                if (field_0x128[player] > 0.1f) {
                    field_0x128[player] -= 0.1f;
                } else {
                    field_0x128[player] = 0.0f;
                }
                if (field_0x138[player] > 0.1f) {
                    field_0x138[player] -= 0.1f;
                } else {
                    field_0x138[player] = 0.0f;
                }
            }
        } else {
            if (field_0x108[player] < 0.9f) {
                field_0x108[player] += 0.1f;
            } else {
                field_0x108[player] = 1.0f;
            }
            if (field_0x128[player] < 0.9f) {
                field_0x128[player] += 0.1f;
            } else {
                field_0x128[player] = 1.0f;
            }
            if (field_0x138[player] < 0.9f) {
                field_0x138[player] += 0.1f;
            } else {
                field_0x138[player] = 1.0f;
            }
        }
    }
    if (prevA == field_0x108[player] && prevB == field_0x128[player] && prevC == field_0x138[player] && !force) {
        return;
    }
    for (s32 i = 0; i < 4; i++) {
        field_0x4->search(lbl_8048E300[player][i])->setAlpha(field_0x218 * field_0x108[player]);
    }
    for (s32 i = 0; i < 3; i++) {
        field_0x4->search(lbl_8048E380[player][i])->setAlpha(field_0x219[i] * field_0x108[player]);
    }
    for (s32 i = 0; i < 8; i++) {
        field_0x4->search(lbl_8048E3E0[player][i])->setAlpha(field_0x21c * field_0x108[player]);
        field_0x4->search(lbl_8048E4E0[player][i])->setAlpha(field_0x21d * field_0x108[player]);
        field_0x4->search(lbl_8048E5E0[player][i])->setAlpha(field_0x21e * field_0x108[player]);
        field_0x4->search(lbl_8048E6E0[player][i])->setAlpha(field_0x21f * field_0x108[player]);
    }
    for (s32 i = 0; i < 3; i++) {
        field_0x4->search(lbl_8048E7E0[player][i])->setAlpha(field_0x220[i] * field_0x108[player]);
        field_0x4->search(lbl_8048E840[player][i])->setAlpha(field_0x223[i] * field_0x108[player]);
    }
    for (s32 i = 0; i < 12; i++) {
        field_0x4->search(lbl_8048E8A0[player][i])->setAlpha(field_0x226 * field_0x108[player]);
        field_0x4->search(lbl_8048EA20[player][i])->setAlpha(field_0x227 * field_0x108[player]);
    }
    field_0x4->search(lbl_8048EBA0[player][0])->setAlpha(field_0x228 * field_0x108[player]);
    field_0x4->search(lbl_8048EBA0[player][1])->setAlpha(field_0x229 * field_0x108[player]);
    for (s32 i = 0; i < 5; i++) {
        field_0x4->search(lbl_8048EBE0[player][i])->setAlpha(field_0x22a * field_0x108[player]);
        field_0x4->search(lbl_8048EC80[player][i])->setAlpha(field_0x22b * field_0x108[player]);
    }
    for (s32 i = 0; i < 6; i++) {
        field_0x4->search(lbl_8048ED20[player][i])->setAlpha(field_0x22c[i] * field_0x108[player]);
    }
    mScreen->search(lbl_8048EF60[player])->setAlpha(field_0x232 * field_0x138[player]);
    field_0x4->search(lbl_8048EF80[player])->setAlpha(field_0x233 * field_0x128[player]);
    mScreen->search(lbl_8048EDE0[player][0])->setAlpha(field_0x234 * field_0x108[player]);
    mScreen->search(lbl_8048EDE0[player][1])->setAlpha(field_0x235 * field_0x108[player]);
    field_0x4->search(lbl_8048EFE0[player])->setAlpha(field_0x236 * field_0x108[player]);
    field_0x4->search(lbl_8048F000[player])->setAlpha(field_0x237 * field_0x108[player]);
    field_0x4->search(lbl_8048F080[player])->setAlpha(field_0x238 * field_0x108[player]);
    field_0x4->search(lbl_8048F0A0[player])->setAlpha(field_0x239 * field_0x108[player]);
    field_0x4->search(lbl_8048F0C0[player])->setAlpha(field_0x23a * field_0x108[player]);
    field_0x4->search(lbl_8048F0E0[player])->setAlpha(field_0x23b * field_0x108[player]);
}

void BattleHud_804B146C::fn_80423DF0(s32 player) {
    s32 item = -1;
    bool active = checkBit(field_0x259, player);
    if (!active && !fn_80230188(player)) {
        if (fn_8023BE0C(getRepresentativePlayer_8022EA0C(player)) && !checkBit(lbl_8053AED0->field_0x255, player)) {
            setBit(field_0x259, player);
            field_0x4->search(lbl_8048EFE0[player])->show();
        }
    } else {
        if (lbl_8053AD30->fn_80309D24(getPlayerInputSlot_8030AAF4(player), 0x100) == 0 ||
            checkBit(lbl_8053AED0->field_0x255, player) || fn_80230188(player)) {
            clearBit(field_0x259, player);
            field_0x4->search(lbl_8048EFE0[player])->hide();
        }
    }
    switch (fn_802373EC(player)) {
    case 0:
        break;
    case 1:
        item = 5;
        break;
    case 2:
        item = 11;
        break;
    case 3:
        item = 2;
        break;
    case 4:
        item = 3;
        break;
    case 5:
        item = 6;
        break;
    case 6:
        item = 7;
        break;
    case 7:
        item = 4;
        break;
    case 8:
        item = 8;
        break;
    case 9:
        item = 10;
        break;
    case 10:
        item = 1;
        break;
    case 11:
    case 12:
    case 13:
        item = 0;
        break;
    }
    if (item != field_0x148[player] || checkBit(lbl_8053AED0->field_0x255, player)) {
        field_0x148[player] = item;
        for (s32 i = 0; i < 12; i++) {
            if (i == item) {
                field_0x4->search(lbl_8048E8A0[player][i])->show();
                field_0x4->search(lbl_8048EA20[player][i])->show();
            } else {
                field_0x4->search(lbl_8048E8A0[player][i])->hide();
                field_0x4->search(lbl_8048EA20[player][i])->hide();
            }
        }
        if (fn_8023725C(player) == 2) {
            for (s32 i = 0; i < 3; i++) {
                field_0x4->search(lbl_8048E840[player][i])->show();
            }
        } else {
            for (s32 i = 0; i < 3; i++) {
                field_0x4->search(lbl_8048E840[player][i])->hide();
            }
        }
    }
}

void BattleHud_804B146C::fn_80423804(s32 player) {
    if (!(field_0x256 & (u8)(1 << player))) {
        return;
    }
    s32 max = getPlayerMaxLife_80237810(player);
    s32 current = lbl_80529DEC.field_0xd8[player];
    s32 life = current;
    if (max > 32) {
        max = 32;
    }
    if (max < 0) {
        max = 0;
    }
    if (max < current) {
        life = max;
    }
    if (life < 0) {
        life = 0;
    }
    for (s32 i = 0; i < 8; i++) {
        if (life >= 4) {
            static_cast<J2DPicture*>(field_0x4->search(lbl_8048E3E0[player][i]))
                ->setBlendRatio(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            static_cast<J2DPicture*>(field_0x4->search(lbl_8048E4E0[player][i]))
                ->setBlendRatio(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            field_0x4->search(lbl_8048E5E0[player][i])->hide();
            field_0x4->search(lbl_8048E6E0[player][i])->hide();
            life -= 4;
            max -= 4;
        } else {
            if (life == 0) {
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E3E0[player][i]))
                    ->setBlendRatio(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E4E0[player][i]))
                    ->setBlendRatio(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                if (max > 0) {
                    field_0x4->search(lbl_8048E5E0[player][i])->show();
                    field_0x4->search(lbl_8048E6E0[player][i])->show();
                    max -= 4;
                } else {
                    field_0x4->search(lbl_8048E5E0[player][i])->hide();
                    field_0x4->search(lbl_8048E6E0[player][i])->hide();
                }
            } else if (life == 1) {
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E3E0[player][i]))
                    ->setBlendRatio(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E4E0[player][i]))
                    ->setBlendRatio(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                field_0x4->search(lbl_8048E5E0[player][i])->hide();
                field_0x4->search(lbl_8048E6E0[player][i])->hide();
                max -= 4;
            } else if (life == 2) {
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E3E0[player][i]))
                    ->setBlendRatio(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E4E0[player][i]))
                    ->setBlendRatio(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                field_0x4->search(lbl_8048E5E0[player][i])->hide();
                field_0x4->search(lbl_8048E6E0[player][i])->hide();
                max -= 4;
            } else if (life == 3) {
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E3E0[player][i]))
                    ->setBlendRatio(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x4->search(lbl_8048E4E0[player][i]))
                    ->setBlendRatio(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                field_0x4->search(lbl_8048E5E0[player][i])->hide();
                field_0x4->search(lbl_8048E6E0[player][i])->hide();
                max -= 4;
            }
            life = 0;
        }
    }
}

// FIXME(shotgun): 99.96 %, operands; behaviour near.
void BattleHud_804B146C::fn_80423248(s32 player) {
    if (!(field_0x257 & (u8)(1 << player))) {
        return;
    }
    s32 val = lbl_80529DEC.field_0xe8[player];
    s32 start = 0;
    const ResTIMG* tex;
    if (9999 < lbl_80529DEC.field_0xe8[player]) {
        val = 9999;
    }
    if (val < 0) {
        for (s32 i = 0; i < 5; i++) {
            static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][i]))
                ->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0xFF, 0x55, 0x41, 0xFF));
        }
        if (val < -5000) {
            val = -5000;
        }
        val *= -1;
        tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', "kk_rupyNumMinus.bti"));
        static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][0]))->changeTexture(tex, 0);
        static_cast<J2DPicture*>(field_0x4->search(lbl_8048EC80[player][0]))->changeTexture(tex, 0);
        field_0x4->search(lbl_8048EBE0[player][4])->show();
        field_0x4->search(lbl_8048EC80[player][4])->show();
        start = 1;
    } else {
        for (s32 i = 0; i < 5; i++) {
            static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][i]))
                ->setBlackWhite(field_0x214, field_0x210);
        }
        field_0x4->search(lbl_8048EBE0[player][4])->hide();
        field_0x4->search(lbl_8048EC80[player][4])->hide();
    }
    s32 n1000 = val / 1000;
    val %= 1000;
    tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[n1000]));
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][start]))->changeTexture(tex, 0);
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EC80[player][start]))->changeTexture(tex, 0);
    start++;

    s32 n100 = val / 100;
    val %= 100;
    tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[n100]));
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][start]))->changeTexture(tex, 0);
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EC80[player][start]))->changeTexture(tex, 0);
    start++;

    s32 n10 = val / 10;
    val %= 10;
    tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[n10]));
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][start]))->changeTexture(tex, 0);
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EC80[player][start]))->changeTexture(tex, 0);
    start++;

    tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[val]));
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EBE0[player][start]))->changeTexture(tex, 0);
    static_cast<J2DPicture*>(field_0x4->search(lbl_8048EC80[player][start]))->changeTexture(tex, 0);
}

void BattleHud_804B146C::fn_80422824() {
    s32 tied;
    s32 count = 0;
    s32 ranked = 0;
    if (!(field_0x256 & 1) && !(field_0x256 & 2) && !(field_0x256 & 4) && !(field_0x256 & 8)) {
        return;
    }
    if (field_0x25b == 0) {
        field_0x25b = 1;
        return;
    }
    s32 ranks[4];
    s32 done[4];
    for (s32 i = 0; i < 4; i++) {
        ranks[i] = 0;
        done[i] = 0;
        if (gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) {
            if (lbl_8050EC80.getCurrentFile().field_0x69 != 0) {
                mScreen->search(lbl_8048EF20[i])->show();
            } else {
                mScreen->search(lbl_8048EF20[i])->hide();
            }
            count++;
        }
    }
    do {
        s32 best = -5001;
        tied = 0;
        for (s32 i = 0; i < count; i++) {
            if (done[i] == 0 && best < getPlayerLife_80237774(i)) {
                best = getPlayerLife_80237774(i);
            }
        }
        for (s32 i = 0; i < count; i++) {
            if (done[i] == 0 && best == getPlayerLife_80237774(i)) {
                ranks[i] = ranked;
                tied++;
                done[i] = 1;
            }
        }
        ranked += tied;
    } while (ranked < count);

    for (s32 i = 0; i < count; i++) {
        switch (ranks[i]) {
        case 0:
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setWhite(JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF));
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setCornerColor(
                    JUtility::TColor(0xB9, 0x12, 0x12, 0xFF),
                    JUtility::TColor(0xD8, 0x3A, 0x16, 0xFF),
                    JUtility::TColor(0xB9, 0x54, 0x12, 0xFF),
                    JUtility::TColor(0xFF, 0x9D, 0x10, 0xFF)
                );
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF20[i]))
                ->setBlendRatio(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setBlendRatio(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case 1:
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setWhite(JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF));
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setCornerColor(
                    JUtility::TColor(0xF2, 0x48, 0x09, 0xFF),
                    JUtility::TColor(0xF2, 0x48, 0x09, 0xFF),
                    JUtility::TColor(0xF2, 0xAB, 0x24, 0xFF),
                    JUtility::TColor(0xFC, 0xFF, 0x21, 0xFF)
                );
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF20[i]))
                ->setBlendRatio(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setBlendRatio(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case 2:
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setWhite(JUtility::TColor(0xFF, 0xFF, 0x77, 0xFF));
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setCornerColor(
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF),
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF),
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF),
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF)
                );
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF20[i]))
                ->setBlendRatio(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setBlendRatio(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case 3:
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setWhite(JUtility::TColor(0xD8, 0xFF, 0xA1, 0xFF));
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setCornerColor(
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF),
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF),
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF),
                    JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF)
                );
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF20[i]))
                ->setBlendRatio(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            static_cast<J2DPicture*>(mScreen->search(lbl_8048EF40[i]))
                ->setBlendRatio(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            break;
        }
    }
}

void BattleHud_804B146C::fn_8042215C(s32 player) {
    if (field_0x1e8[player] == 0) {
        field_0x1a8[player] = getPlayerLife_80237774(player);
        field_0x1c8[player] = getPlayerMaxLife_80237810(player);
        return;
    }
    s32 max = field_0x1c8[player];
    s32 life = field_0x1a8[player];
    if (max > 32) {
        max = 32;
    }
    if (max < 0) {
        max = 0;
    }
    if (max < life) {
        life = max;
    }
    if (life < 0) {
        life = 0;
    }
    for (s32 i = 0; i < 8; i++) {
        if (life >= 4) {
            field_0x8[player]->search(lbl_8048EE20[i])->show();
            field_0x8[player]->search(lbl_8048EEA0[i])->show();
            static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EE20[i]))
                ->setBlendRatio(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EEA0[i]))
                ->setBlendRatio(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            field_0x8[player]->search(lbl_8048EE60[i])->hide();
            field_0x8[player]->search(lbl_8048EEE0[i])->hide();
            life -= 4;
            max -= 4;
        } else {
            if (life == 0) {
                field_0x8[player]->search(lbl_8048EE20[i])->hide();
                field_0x8[player]->search(lbl_8048EEA0[i])->hide();
                if (max > 0) {
                    field_0x8[player]->search(lbl_8048EE60[i])->show();
                    field_0x8[player]->search(lbl_8048EEE0[i])->show();
                    max -= 4;
                } else {
                    field_0x8[player]->search(lbl_8048EE60[i])->hide();
                    field_0x8[player]->search(lbl_8048EEE0[i])->hide();
                }
            } else if (life == 1) {
                field_0x8[player]->search(lbl_8048EE20[i])->show();
                field_0x8[player]->search(lbl_8048EEA0[i])->show();
                static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EE20[i]))
                    ->setBlendRatio(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EEA0[i]))
                    ->setBlendRatio(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                field_0x8[player]->search(lbl_8048EE60[i])->hide();
                field_0x8[player]->search(lbl_8048EEE0[i])->hide();
                max -= 4;
            } else if (life == 2) {
                field_0x8[player]->search(lbl_8048EE20[i])->show();
                field_0x8[player]->search(lbl_8048EEA0[i])->show();
                static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EE20[i]))
                    ->setBlendRatio(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EEA0[i]))
                    ->setBlendRatio(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                field_0x8[player]->search(lbl_8048EE60[i])->hide();
                field_0x8[player]->search(lbl_8048EEE0[i])->hide();
                max -= 4;
            } else if (life == 3) {
                field_0x8[player]->search(lbl_8048EE20[i])->show();
                field_0x8[player]->search(lbl_8048EEA0[i])->show();
                static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EE20[i]))
                    ->setBlendRatio(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                static_cast<J2DPicture*>(field_0x8[player]->search(lbl_8048EEA0[i]))
                    ->setBlendRatio(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                field_0x8[player]->search(lbl_8048EE60[i])->hide();
                field_0x8[player]->search(lbl_8048EEE0[i])->hide();
                max -= 4;
            }
            life = 0;
        }
    }
    field_0x1e8[player]--;
}

void BattleHud_804B146C::fn_80421D78(s32 player) {
    if (fn_802366CC(getRepresentativePlayer_8022EA0C(player)) > 0 && fn_802366CC(getRepresentativePlayer_8022EA0C(player)) < 300) {
        s32 sec = fn_802366CC(getRepresentativePlayer_8022EA0C(player)) / 60 + 1;
        if (sec != field_0x25e[player]) {
            if (sec > 99) {
                sec = 99;
            }
            if (sec < 0) {
                sec = 0;
            }
            if (sec < 10) {
                const ResTIMG* res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[sec]));
                static_cast<J2DPicture*>(field_0x2c[player]->search(lbl_8048F158[0]))->changeTexture(res, 0);
                static_cast<J2DPicture*>(field_0x2c[player]->search(lbl_8048F158[1]))->changeTexture(res, 0);
                if (field_0x2c[player]->search(lbl_8048F158[2])->isVisible() == true) {
                    field_0x2c[player]->search(lbl_8048F158[2])->hide();
                    field_0x2c[player]->search(lbl_8048F158[3])->hide();
                }
            } else {
                const ResTIMG* res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[sec / 10]));
                static_cast<J2DPicture*>(field_0x2c[player]->search(lbl_8048F158[0]))->changeTexture(res, 0);
                static_cast<J2DPicture*>(field_0x2c[player]->search(lbl_8048F158[1]))->changeTexture(res, 0);
                res = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1434[sec % 10]));
                static_cast<J2DPicture*>(field_0x2c[player]->search(lbl_8048F158[2]))->changeTexture(res, 0);
                static_cast<J2DPicture*>(field_0x2c[player]->search(lbl_8048F158[3]))->changeTexture(res, 0);
                if (field_0x2c[player]->search(lbl_8048F158[2])->isVisible() == false) {
                    field_0x2c[player]->search(lbl_8048F158[2])->show();
                    field_0x2c[player]->search(lbl_8048F158[3])->show();
                }
            }
            field_0x25e[player] = sec;
        }
    } else {
        field_0x25e[player] = 0;
    }
    if (field_0x208[player] > 0) {
        field_0x208[player]--;
    }
}

// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 9-line definition): split out only where the .sdata2 order needs it.
void BattleHud_804B146C::updateGaugeBar(s32 player, s32 bar, s32& val) {
    if (val >= 0x60) {
        field_0x4->search(lbl_8048E300[player][bar])->resize(field_0xf4, field_0x4->search(lbl_8048E300[player][bar])->getHeight());
        val -= 0x60;
    } else {
        field_0x4->search(lbl_8048E300[player][bar])->resize(field_0xf4 * ((f32)val / 96.0f), field_0x4->search(lbl_8048E300[player][bar])->getHeight());
        val = 0;
    }
}

// A projected screen position in the HUD's ortho space, relative to the centre (both callers inline it).
// R2 pool helper (inlined at 2 call sites); was calcHudPos.
static void pool_80543A58(const Vec& screen, f32 sx, f32 sy, f32* x, f32* y) {
    f32 px = sx * (screen.x / JFWDisplay::getManager()->getEfbWidth());
    f32 py = sy * (screen.y / JFWDisplay::getManager()->getEfbHeight());
    *x = px - JFWDisplay::getManager()->getEfbWidth() / 2.0f;
    *y = lbl_8050DB0C.field_0x4c[1] + (py - JFWDisplay::getManager()->getEfbHeight() / 2.0f);
}

void BattleHud_804B146C::fn_80421A34(s32 player, bool force) {
    u8 bit = (u8)(1 << player);
    f32 prev = field_0x118[player];
    if (!(field_0x25d & bit)) {
        if (field_0x118[player] > 0.0f) {
            field_0x118[player] -= 0.1f;
        } else {
            field_0x118[player] = 0.0f;
        }
        gGameRoot2_8053AB10->field_0x20->field_0x217[player] = 1;
    } else if (lbl_80529DEC.field_0xd8[player] == 0) {
        if (field_0x118[player] > 0.6f) {
            field_0x118[player] -= 0.1f;
            if (field_0x118[player] <= 0.6f) {
                field_0x118[player] = 0.6f;
            }
        } else if (field_0x118[player] < 0.6f) {
            field_0x118[player] += 0.1f;
            if (field_0x118[player] >= 0.6f) {
                field_0x118[player] = 0.6f;
            }
        }
    } else {
        if (field_0x118[player] < 1.0f) {
            field_0x118[player] += 0.1f;
        } else {
            field_0x118[player] = 1.0f;
        }
    }
    if (prev == field_0x118[player] && !force) {
        return;
    }
    for (s32 i = 0; i < 8; i++) {
        field_0x8[player]->search(lbl_8048EE20[i])->setAlpha(field_0x21c * field_0x118[player]);
        field_0x8[player]->search(lbl_8048EEA0[i])->setAlpha(field_0x21d * field_0x118[player]);
        field_0x8[player]->search(lbl_8048EE60[i])->setAlpha(field_0x21e * field_0x118[player]);
        field_0x8[player]->search(lbl_8048EEE0[i])->setAlpha(field_0x21f * field_0x118[player]);
    }
    for (s32 i = 0; i < 6; i++) {
        field_0x2c[player]->search(lbl_8048F158[i])->setAlpha(field_0x247[i] * field_0x118[player]);
    }
}

void BattleHud_804B146C::fn_8042165C() {
    field_0x1f0[0] = 0;
    field_0x1f0[1] = 0;
    field_0x1f0[2] = 0;
    field_0x1f0[3] = 0;
    for (s32 i = 0; i < 4; i++) {
        if (isPlayerBitSet(gGameRoot_8053AAF8, i) != 0 && !fn_8022C8EC(i)) {
            for (s32 k = 0; k < 4; k++) {
                if (k != i) {
                    field_0x1f0[k] |= (u16)(1 << i);
                }
            }
            if (getPlayerLayer_8023DE58(i) < 8) {
                Vec screen;
                gGameRoot2_8053AB10->field_0x24->worldToScreen_8030C5F0(getPlayerPos_8023E724(i), false, false, &screen);
                for (s32 j = 0; j < 10; j++) {
                    if (!lbl_8053AED0->fn_8043DF24(lbl_8048F188[j], lbl_8048F1B0[j], screen.x, screen.y, 0.0f)) {
                        for (s32 k = 0; k < 4; k++) {
                            field_0x1f0[k] |= (u16)(1 << j);
                        }
                    }
                }
            }
        }
        if (field_0x24d[i] != 0xFF) {
            for (s32 k = 0; k < 4; k++) {
                if (k != i) {
                    field_0x1f0[k] |= (u16)(1 << field_0x24d[i]);
                }
            }
        }
    }
    for (s32 i = 0; i < 4; i++) {
        if ((gGameRoot_8053AAF8->mPlayerMask & (u8)(1 << i)) && !fn_8022C8EC(i) && getPlayerLayer_8023DE58(i) < 8) {
            for (s32 j = 0; j < 10; j++) {
                if (!(field_0x1f0[i] & (u16)(1 << lbl_8048F1D8[i][j]))) {
                    if (field_0x251[i] != lbl_8048F1D8[i][j] && field_0x24d[i] == 0xFF) {
                        field_0x24d[i] = lbl_8048F1D8[i][j];
                        field_0x198[i] = 0;
                        field_0x1f0[0] |= (u16)(1 << field_0x24d[i]);
                        field_0x1f0[1] |= (u16)(1 << field_0x24d[i]);
                        field_0x1f0[2] |= (u16)(1 << field_0x24d[i]);
                        field_0x1f0[3] |= (u16)(1 << field_0x24d[i]);
                    }
                    break;
                }
            }
            if (field_0x24d[i] != 0xFF) {
                field_0x198[i]++;
                if (field_0x198[i] >= 60) {
                    field_0x251[i] = field_0x24d[i];
                    field_0x24d[i] = 0xFF;
                }
            } else if (field_0x198[i] != 0) {
                field_0x198[i]++;
                if (field_0x198[i] >= 120) {
                    field_0x198[i] = 0;
                }
            }
        }
    }
}
