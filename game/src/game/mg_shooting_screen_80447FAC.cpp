#include "game/GameAudio.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/cls_804B1C58.h"
#include "JSystem/J2D/J2DScreen.h"
#include "JSystem/J2D/J2DPicture.h"
#include "JSystem/J2D/J2DGrafContext.h"

// Per player, 0x70 bytes of J2DPane::search tags (as u64): the digit pictures are at 0x48 / 0x50 (tens),
// 0x58 / 0x60 (ones), the frame panes at 0x30 and 0x68.
extern const u64 lbl_804906A8[4][14] = {
    {
        0x0000507031693031ULL, 0x0000507031693032ULL, 0x0050703169623031ULL, 0x0050703169623032ULL,
        0x0050703169623033ULL, 0x0050703169623034ULL, 0x004E646174613031ULL, 0x00000050316F3031ULL,
        0x00000050316F3032ULL, 0x0000005031743031ULL, 0x00005031746B3031ULL, 0x0000005031743032ULL,
        0x00005031746B3032ULL, 0x004E646174613230ULL,
    },
    {
        0x0000507032693031ULL, 0x0000507032693032ULL, 0x0050703269623031ULL, 0x0050703269623032ULL,
        0x0050703269623033ULL, 0x0050703269623034ULL, 0x004E646174613034ULL, 0x00000050326F3031ULL,
        0x00000050326F3032ULL, 0x0000005032743031ULL, 0x00005032746B3031ULL, 0x0000005032743032ULL,
        0x00005032746B3032ULL, 0x004E646174613231ULL,
    },
    {
        0x0000507033693031ULL, 0x0000507033693032ULL, 0x0050703369623031ULL, 0x0050703369623032ULL,
        0x0050703369623033ULL, 0x0050703369623034ULL, 0x004E646174613037ULL, 0x00000050336F3031ULL,
        0x00000050336F3032ULL, 0x0000005033743031ULL, 0x00005033746B3031ULL, 0x0000005033743032ULL,
        0x00005033746B3032ULL, 0x004E646174613232ULL,
    },
    {
        0x0000507034693031ULL, 0x0000507034693032ULL, 0x0050703469623031ULL, 0x0050703469623032ULL,
        0x0050703469623033ULL, 0x0050703469623034ULL, 0x004E646174613130ULL, 0x00000050346F3031ULL,
        0x00000050346F3032ULL, 0x0000005034743031ULL, 0x00005034746B3031ULL, 0x0000005034743032ULL,
        0x00005034746B3032ULL, 0x004E646174613233ULL,
    },
};
// The digit textures (kk_rupyNum_00.bti ...).
const char* lbl_804B1C30[10] = {
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

MgShootingScreen_804B1C58::MgShootingScreen_804B1C58() {
    lbl_8053AF10 = this;
    mScreen = new J2DScreen;
    mScreen->setPriority("mg_shooting.blo", 0x1100000, lbl_80529DEC.mBossArchive_0x10);
    for (s32 i = 0; i < 4; i++) {
        field_0x8[i] = new PaneCtrl_804AF86C;
        field_0x8[i]->setPane_80402F04(mScreen->search(lbl_804906A8[i][6]));
        field_0x18[i] = new PaneCtrl_804AF86C;
        field_0x18[i]->setPane_80402F04(mScreen->search(lbl_804906A8[i][13]));
    }
    field_0x28[0] = 0;
    field_0x28[1] = 0;
    field_0x28[2] = 0;
    field_0x28[3] = 0;
    field_0x2c = 0;
}

MgShootingScreen_804B1C58::~MgShootingScreen_804B1C58() {
    delete mScreen;
    mScreen = NULL;
    for (s32 i = 0; i < 4; i++) {
        delete field_0x8[i];
        field_0x8[i] = NULL;
        delete field_0x18[i];
        field_0x18[i] = NULL;
    }
    lbl_8053AF10 = NULL;
}

void MgShootingScreen_804B1C58::fn_80448334() {
    const ResTIMG* zero;
    for (s32 i = 0; i < 4; i++) {
        field_0x2c &= ~(u8)(1 << i);
        zero = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1C30[0]));
        static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][9]))->changeTexture(zero, 0);
        static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][10]))->changeTexture(zero, 0);
        static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][11]))->changeTexture(zero, 0);
        static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][12]))->changeTexture(zero, 0);
        mScreen->search(lbl_804906A8[i][6])->hide();
        mScreen->search(lbl_804906A8[i][13])->hide();
    }
}

void MgShootingScreen_804B1C58::fn_804482EC() {
    gGameRoot2_8053AB10->setPort_8013B490();
    mScreen->draw(0.0f, 0.0f, gGameRoot2_8053AB10->field_0x10);
}

void MgShootingScreen_804B1C58::fn_80447FAC() {
    for (s32 i = 0; i < 4; i++) {
        if (i < 2) {
            field_0x8[i]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D514());
            field_0x18[i]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D514());
        } else {
            field_0x8[i]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D57C());
            field_0x18[i]->moveFromCentre_80402E08(0.0f, lbl_8053AED0->fn_8043D57C());
        }
        if (field_0x2c & (u8)(1 << i)) {
            if (!mScreen->search(lbl_804906A8[i][6])->isVisible()) {
                mScreen->search(lbl_804906A8[i][6])->show();
                mScreen->search(lbl_804906A8[i][13])->show();
            }
        } else if (mScreen->search(lbl_804906A8[i][6])->isVisible()) {
            mScreen->search(lbl_804906A8[i][6])->hide();
            mScreen->search(lbl_804906A8[i][13])->hide();
        }
        if (mScreen->search(lbl_804906A8[i][6])->isVisible()) {
            if (field_0x28[i] >= 100) {
                field_0x28[i] = 99;
            }
            const ResTIMG* tex;
            s32 tens = field_0x28[i] / 10;
            s32 ones = field_0x28[i] % 10;
            tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1C30[tens]));
            static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][9]))->changeTexture(tex, 0);
            static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][10]))->changeTexture(tex, 0);
            tex = static_cast<const ResTIMG*>(getMain2DResource_804030C4('TIMG', lbl_804B1C30[ones]));
            static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][11]))->changeTexture(tex, 0);
            static_cast<J2DPicture*>(mScreen->search(lbl_804906A8[i][12]))->changeTexture(tex, 0);
        }
    }
    field_0x2c = 0;
}
