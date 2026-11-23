#include "game/GameAudio.h"
#include "game/ActorBase.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/Message.h"
#include "game/save_data_803B8C74.h"
#include "game/act/act_ganon.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "game/ActorManager.h"
#include "game/Effect.h"
#include "game/Player.h"
#include "game/RotMatrix.h"
#include "game/SpriteParams.h"
#include "JSystem/JMath/JMath.h"
#include "std/math.h"

// .bss tables indexed by player (the unwritten update / init use them; the __sinit constructs
// the two vector tables).
s32 lbl_804C1BC0[4];
JGeometry::TVec3f lbl_804C1BD0[4];
JGeometry::TVec3f lbl_804C1C00[4];

// Animation tables: per animation, a first frame, and for each step the frame offset (0xFF
// ends the animation) and its length, for the body (field_0x25c) and the second layer
// (field_0x260).
s32 lbl_80539FE8 = -1;
s16 lbl_804A1FF8[16] = {48, 0, 5, 5, 5, 12, 16, 16, 16, 30, 30, 30, 41, 53, 53, 53};
u8 lbl_804A2018[30] = {0, 1, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 0xFF};
u8 lbl_804A2038[30] = {8, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0xFF};
u8 lbl_804A2058[17] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0xFF};
u8 lbl_804A206C[17] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0xFF};
u8 lbl_804A2080[26] = {0, 1, 2, 3, 4, 5, 9, 10, 5, 9, 10, 5, 9, 10, 5, 9, 10, 5, 9, 10, 5, 4, 11, 12, 13, 0xFF};
u8 lbl_804A209C[26] = {8, 11, 7, 8, 4, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 8, 9, 40, 0xFF};
u8 lbl_804A20B8[9] = {5, 9, 10, 6, 4, 11, 12, 13, 0xFF};
u8 lbl_804A20C4[9] = {6, 6, 6, 6, 8, 8, 9, 9, 0xFF};
u8 lbl_804A20D0[23] = {0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6, 0xFF};
u8 lbl_804A20E8[23] = {8, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0xFF};
u8 lbl_804A2100[11] = {0, 1, 2, 3, 4, 3, 4, 3, 4, 5, 0xFF};
u8 lbl_804A210C[11] = {6, 6, 7, 7, 7, 7, 7, 7, 7, 6, 0xFF};
u8 lbl_80539FEC[8] = {0, 1, 0, 2, 3, 4, 2, 0xFF};
u8 lbl_80539FF4[8] = {9, 9, 9, 9, 9, 9, 9, 0xFF};
u8 lbl_80539FFC[4] = {0, 1, 2, 0xFF};
u8 lbl_8053A000[4] = {3, 4, 5, 0xFF};
u8 lbl_8053A004[6] = {3, 2, 1, 6, 0, 0xFF};
u8 lbl_8053A00C[4] = {6, 4, 4, 0xFF};
u8 lbl_8053A010[4] = {6, 6, 6, 0xFF};
u8 lbl_8053A014[6] = {7, 10, 8, 6, 40, 0xFF};
u8 lbl_8053A01C[8] = {0, 1, 2, 4, 6, 7, 8, 0xFF};
u8 lbl_8053A024[8] = {8, 11, 15, 6, 6, 6, 6, 0xFF};
u8 lbl_8053A02C[4] = {0, 1, 2, 0xFF};
u8 lbl_8053A030[5] = {3, 4, 3, 5, 0xFF};
u8 lbl_8053A038[6] = {6, 7, 8, 9, 10, 0xFF};
u8 lbl_8053A040[4] = {7, 11, 13, 0xFF};
u8 lbl_8053A044[5] = {9, 9, 9, 9, 0xFF};
u8 lbl_8053A04C[6] = {6, 9, 11, 9, 40, 0xFF};
u8 lbl_8053A054[7] = {0, 1, 2, 3, 4, 5, 0xFF};
u8 lbl_8053A05C[7] = {6, 6, 7, 7, 7, 6, 0xFF};
u8* lbl_804A2118[16] = {
    lbl_804A2018, lbl_80539FEC, lbl_80539FFC, lbl_8053A000,
    lbl_8053A004, lbl_804A2058, lbl_8053A01C, lbl_804A2080,
    lbl_804A20B8, lbl_8053A02C, lbl_8053A030, lbl_8053A038,
    lbl_804A20D0, lbl_8053A054, lbl_804A2100, lbl_8053A054,
};
u8* lbl_804A2158[16] = {
    lbl_804A2038, lbl_80539FF4, lbl_8053A00C, lbl_8053A010,
    lbl_8053A014, lbl_804A206C, lbl_8053A024, lbl_804A209C,
    lbl_804A20C4, lbl_8053A040, lbl_8053A044, lbl_8053A04C,
    lbl_804A20E8, lbl_8053A05C, lbl_804A210C, lbl_8053A05C,
};
u8 lbl_804A2198[30] = {0, 1, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 0xFF};
u8 lbl_804A21B8[30] = {8, 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0xFF};
u8 lbl_804A21D8[17] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0xFF};
u8 lbl_804A21EC[17] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0xFF};
u8 lbl_804A2200[26] = {0, 1, 2, 3, 4, 5, 9, 10, 5, 9, 10, 5, 9, 10, 5, 9, 10, 5, 9, 10, 5, 4, 11, 12, 13, 0xFF};
u8 lbl_804A221C[26] = {8, 11, 7, 8, 4, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 8, 8, 9, 40, 0xFF};
u8 lbl_804A2238[9] = {5, 9, 10, 6, 4, 11, 12, 13, 0xFF};
u8 lbl_804A2244[9] = {6, 6, 6, 6, 8, 8, 9, 9, 0xFF};
u8 lbl_804A2250[23] = {0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6, 0xFF};
u8 lbl_804A2268[23] = {8, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0xFF};
u8 lbl_804A2280[11] = {0, 1, 2, 3, 4, 3, 4, 3, 4, 5, 0xFF};
u8 lbl_804A228C[11] = {6, 6, 7, 7, 7, 7, 7, 7, 7, 6, 0xFF};
u8 lbl_8053A064[8] = {0, 1, 0, 2, 3, 4, 2, 0xFF};
u8 lbl_8053A06C[8] = {9, 9, 9, 9, 9, 9, 9, 0xFF};
u8 lbl_8053A074[5] = {0, 1, 2, 3, 0xFF};
u8 lbl_8053A07C[4] = {4, 5, 3, 0xFF};
u8 lbl_8053A080[6] = {3, 2, 1, 6, 0, 0xFF};
u8 lbl_8053A088[5] = {6, 4, 4, 6, 0xFF};
u8 lbl_8053A090[4] = {6, 6, 6, 0xFF};
u8 lbl_8053A094[6] = {7, 10, 8, 6, 40, 0xFF};
u8 lbl_8053A09C[8] = {0, 1, 2, 4, 6, 7, 8, 0xFF};
u8 lbl_8053A0A4[8] = {8, 11, 15, 6, 6, 6, 6, 0xFF};
u8 lbl_8053A0AC[4] = {0, 1, 2, 0xFF};
u8 lbl_8053A0B0[5] = {3, 4, 3, 5, 0xFF};
u8 lbl_8053A0B8[6] = {6, 7, 8, 9, 10, 0xFF};
u8 lbl_8053A0C0[4] = {7, 11, 13, 0xFF};
u8 lbl_8053A0C4[5] = {9, 9, 9, 9, 0xFF};
u8 lbl_8053A0CC[6] = {6, 9, 11, 9, 40, 0xFF};
u8 lbl_8053A0D4[7] = {0, 1, 2, 3, 4, 5, 0xFF};
u8 lbl_8053A0DC[7] = {6, 6, 7, 7, 7, 6, 0xFF};
u8* lbl_804A2298[16] = {
    lbl_804A2198, lbl_8053A064, lbl_8053A074, lbl_8053A07C,
    lbl_8053A080, lbl_804A21D8, lbl_8053A09C, lbl_804A2200,
    lbl_804A2238, lbl_8053A0AC, lbl_8053A0B0, lbl_8053A0B8,
    lbl_804A2250, lbl_8053A0D4, lbl_804A2280, lbl_8053A0D4,
};
u8* lbl_804A22D8[16] = {
    lbl_804A21B8, lbl_8053A06C, lbl_8053A088, lbl_8053A090,
    lbl_8053A094, lbl_804A21EC, lbl_8053A0A4, lbl_804A221C,
    lbl_804A2244, lbl_8053A0C0, lbl_8053A0C4, lbl_8053A0CC,
    lbl_804A2268, lbl_8053A0DC, lbl_804A228C, lbl_8053A0DC,
};
u16 lbl_804A2318[24] = {
    0x374, 0x375, 0x193, 0x194, 0x378, 0x379, 0x197, 0x198, 0x372, 0x373, 0x191, 0x192,
    0x376, 0x377, 0x195, 0x196, 0x368, 0x369, 0x36C, 0x36D, 0x366, 0x367, 0x36A, 0x36B,
};
u32 lbl_804A2348[5] = {'STAL', 'STAL', 'STAL', 'SLR4', 'SLR4'};
s32 lbl_804A235C[5] = {4, 4, 4, 6, 0x10006};
f32 lbl_804A2370[5][2] = {{-24.0f, 0.0f}, {0.0f, 0.0f}, {24.0f, 0.0f}, {0.0f, -16.0f}, {0.0f, -8.0f}};
const char* lbl_804A2398[59] = {
    "boss_butag_01.bti",
    "boss_butag_02.bti",
    "boss_butag_03.bti",
    "boss_butag_04.bti",
    "boss_butag_05.bti",
    "boss_butag_kougeki_age_01.bti",
    "boss_butag_kougeki_age_02.bti",
    "boss_butag_kougeki_age_03.bti",
    "boss_butag_kougeki_age_04.bti",
    "boss_butag_kougeki_age_05.bti",
    "boss_butag_kougeki_age_06.bti",
    "boss_butag_kougeki_age_07.bti",
    "boss_butag_kougeki_mawashi_01.bti",
    "boss_butag_kougeki_mawashi_02.bti",
    "boss_butag_kougeki_mawashi_03.bti",
    "boss_butag_kougeki_mawashi_04.bti",
    "boss_butag_kougeki_nage_01.bti",
    "boss_butag_kougeki_nage_02.bti",
    "boss_butag_kougeki_nage_03.bti",
    "boss_butag_kougeki_nage_03_2.bti",
    "boss_butag_kougeki_nage_04.bti",
    "boss_butag_kougeki_nage_05.bti",
    "boss_butag_kougeki_nage_05a.bti",
    "boss_butag_kougeki_nage_05a_2.bti",
    "boss_butag_kougeki_nage_05a_3.bti",
    "boss_butag_kougeki_nage_05_2.bti",
    "boss_butag_kougeki_nage_05_3.bti",
    "boss_butag_kougeki_nage_06.bti",
    "boss_butag_kougeki_nage_07.bti",
    "boss_butag_kougeki_nage_08.bti",
    "boss_butag_down_01.bti",
    "boss_butag_down_02.bti",
    "boss_butag_down_03.bti",
    "boss_butag_down_04.bti",
    "boss_butag_down_04_1.bti",
    "boss_butag_down_04_2.bti",
    "boss_butag_down_up_01.bti",
    "boss_butag_down_up_02.bti",
    "boss_butag_down_up_03.bti",
    "boss_butag_down_up_04.bti",
    "boss_butag_down_up_05.bti",
    "boss_butag_shometsu_01.bti",
    "boss_butag_shometsu_02.bti",
    "boss_butag_shometsu_03.bti",
    "boss_butag_shometsu_04.bti",
    "boss_butag_shometsu_05.bti",
    "boss_butag_shometsu_06.bti",
    "boss_butag_shometsu_07.bti",
    "boss_butag_demo_01.bti",
    "boss_butag_demo_02.bti",
    "boss_butag_demo_03.bti",
    "boss_butag_demo_04.bti",
    "boss_butag_demo_05.bti",
    "boss_butag_noke_01.bti",
    "boss_butag_noke_02.bti",
    "boss_butag_noke_03.bti",
    "boss_butag_noke_04.bti",
    "boss_butag_noke_05.bti",
    "boss_butag_noke_06.bti",
};
const char* lbl_804A2484[59] = {
    "boss_butag_yari_01.bti",
    "boss_butag_yari_02.bti",
    "boss_butag_yari_03.bti",
    "boss_butag_yari_04.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_kougeki_age_yari_01.bti",
    "boss_butag_kougeki_age_yari_02.bti",
    "boss_butag_kougeki_age_yari_03.bti",
    "boss_butag_kougeki_age_yari_04.bti",
    "boss_butag_kougeki_age_yari_05.bti",
    "boss_butag_kougeki_age_yari_06.bti",
    "boss_butag_kougeki_age_yari_07.bti",
    "boss_butag_yari_mawashi_01.bti",
    "boss_butag_yari_mawashi_02.bti",
    "boss_butag_yari_mawashi_03.bti",
    "boss_butag_yari_mawashi_04.bti",
    "boss_butag_kougeki_nage_yari01.bti",
    "boss_butag_kougeki_nage_yari02.bti",
    "boss_butag_kougeki_nage_yari03.bti",
    "boss_butag_kougeki_nage_yari03_2.bti",
    "boss_butag_kougeki_nage_yari04.bti",
    "boss_butag_kougeki_nage_yari06.bti",
    "boss_butag_kougeki_nage_yari05a.bti",
    "boss_butag_kougeki_nage_yari05a_2.bti",
    "boss_butag_kougeki_nage_yari05a_3.bti",
    "boss_butag_kougeki_nage_yari05a_2.bti",
    "boss_butag_kougeki_nage_yari05a_3.bti",
    "boss_butag_kougeki_nage_yari06.bti",
    "boss_butag_kougeki_nage_yari07.bti",
    "boss_butag_kougeki_nage_yari08.bti",
    "boss_butag_down_yari_01.bti",
    "boss_butag_down_yari_02.bti",
    "boss_butag_down_yari_03.bti",
    "boss_butag_down_yari_04.bti",
    "boss_butag_down_yari_04_1.bti",
    "boss_butag_down_yari_04_2.bti",
    "boss_butag_down_up_yari_01.bti",
    "boss_butag_down_up_yari_02.bti",
    "boss_butag_down_up_yari_03.bti",
    "boss_butag_down_up_yari_04.bti",
    "boss_butag_down_up_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_yari_05.bti",
    "boss_butag_demo_yari_01.bti",
    "boss_butag_demo_yari_02.bti",
    "boss_butag_demo_yari_03.bti",
    "boss_butag_demo_yari_04.bti",
    "boss_butag_demo_yari_05.bti",
    "boss_butag_noke_yari_01.bti",
    "boss_butag_noke_yari_02.bti",
    "boss_butag_noke_yari_03.bti",
    "boss_butag_noke_yari_04.bti",
    "boss_butag_noke_yari_05.bti",
    "boss_butag_noke_yari_06.bti",
};
f32 lbl_804A2570[16][2] = {
    {32.0f, 80.0f},  {0.0f, 88.0f},  {8.0f, -48.0f},  {8.0f, -48.0f},
    {8.0f, -48.0f},  {-24.0f, -40.0f}, {-40.0f, 0.0f},  {-40.0f, 0.0f},
    {-40.0f, 0.0f},  {0.0f, -48.0f}, {0.0f, -48.0f},  {0.0f, -48.0f},
    {-100.0f, 88.0f}, {0.0f, 0.0f},   {0.0f, 0.0f},    {0.0f, 0.0f},
};
u8 lbl_8053AB90;
u8 lbl_8053AB91;

void fn_80154718(const JGeometry::TVec3f* pos);
void copyBox_801546F4(JGeometry::TBox2f* dst, const JGeometry::TBox2f* src);
void fn_801546D0(JGeometry::TVec3f* dst, const JGeometry::TVec3f* src);
void zeroVec_801546BC(JGeometry::TVec3f* v);
s32 atan2Short_8015465C(f32 y, f32 x);
void fn_801545F0(JGeometry::TVec3f* a, const JGeometry::TVec3f* b);
void setBox_80154590(JGeometry::TBox2f* box, f32 x0, f32 y0, f32 x1, f32 y1);
void addVec2_80154558(JGeometry::TVec2f* dst, const JGeometry::TVec2f* src);
f32 sinShort_801544FC(s16 angle);
f32 cosShort_801544E4(s16 angle);
ActorBase_804A5770* findActorBySerial_8015445C(s32 serial);
f32 sqrtf_80154378(f32 x);
f32 fabsf_8015436C(f32 x);
s32 spawnActor_80154310(u32 id, s32 layer, const JGeometry::TVec3f* pos, u32 arg, s32 a, s32 b);
u8 fn_8017ECC0();
u8 fn_8017EC70(const JGeometry::TVec3f* pos, f32 value);
void fn_8017EC38(JGeometry::TBox2f* dst, const JGeometry::TBox2f* src);
u8 fn_8017EB44(const JGeometry::TVec3f* pos);
bool fn_8017EB1C(f32 value);
void invalidateEmitter_8017EAB4(JPABaseEmitter* emitter);
JGeometry::TBox2f* fn_801826A4(s32 layer);
AudioMgr_8045A080* fn_8018A6AC();
s32 fn_8018A660();
void fn_8018A63C();
JPAEmitterManager* getEmitterManager_8018A5F0();
s32 fn_8018E89C();

// The scale part of m (its translation column is the caller's); the callers load the
// scales last argument first (cf. act_terrorpin.cpp).
static inline void setScaleMtx(Mtx m, f32 x, f32 y, f32 z) {
    m[0][0] = x;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = y;
    m[1][2] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = z;
}

static inline void setRotScaleMtx3(Mtx m, const JGeometry::TVec3f& rot, f32 sx, f32 sy, f32 sz) {
    f32 sinx = sin(rot.x);
    f32 siny = sin(rot.y);
    f32 sinz = sin(rot.z);
    f32 cosx = cos(rot.x);
    f32 cosy = cos(rot.y);
    f32 cosz = cos(rot.z);
    m[0][0] = sx * (cosz * cosy);
    m[1][0] = sx * (sinz * cosy);
    m[2][0] = sx * -siny;
    m[0][1] = sy * (siny * sinx * cosz - cosx * sinz);
    m[1][1] = sy * (siny * sinx * sinz + cosx * cosz);
    m[2][1] = sy * (sinx * cosy);
    m[0][2] = sz * (sinx * sinz + cosx * cosz * siny);
    m[1][2] = sz * (cosx * sinz * siny - sinx * cosz);
    m[2][2] = sz * (cosx * cosy);
}

void fn_80196E00() {
    lbl_8053AED0->field_0xdc |= 0x10;
}

u8 fn_80196DF0() {
    return lbl_80529DEC.field_0x24f;
}

void fn_80196D8C(s32 index, bool flag, f32 a, f32 b) {
    getGameView_802D6A9C()->fn_802D79E8(index, a, b, flag);
}

// Out-of-line copy of TVec3<f32>::set(const TVec3&).
void copyVec3_80196D70(JGeometry::TVec3f* dst, const JGeometry::TVec3f* src) {
    dst->set(*src);
}

// Out-of-line copy of TVec3<f32>::set(x, y, z).
JGeometry::TVec3f* fn_80196D60(JGeometry::TVec3f* v, f32 x, f32 y, f32 z) {
    v->set(x, y, z);
    return v;
}

void fn_80196D30(s32 a, s32 b) {
    gGameRoot_8053AAF8->startRumble_80134DE4(a, b);
}

// Out-of-line copy of TVec2<f32>::set(x, y).
void fn_80196D24(JGeometry::TVec2f* v, f32 x, f32 y) {
    v->set(x, y);
}

// Sets the emitter's particle scale (0xB0, 0xB4).
void fn_80196D18(JPABaseEmitter* emitter, f32 x, f32 y) {
    emitter->mGlobalPScl.set(x, y);
}

// Out-of-line copy of JPABaseEmitter::setGlobalScale(const TVec3f&).
void fn_80196CF4(JPABaseEmitter* emitter, const JGeometry::TVec3f* scale) {
    emitter->setGlobalScale(*scale);
}

void Ganon_804A25F0::init() {
    field_0x25c = 0;
    field_0x260 = 0;
    field_0x264 = 0;
    field_0x268 = 0;
    field_0x26c = 0;
    field_0x270 = 0;
    field_0x274 = 0;
    field_0x278 = 0;
    field_0x27c = 0;
    field_0x280 = 0;
    for (u32 i = 0; i < 10; i++) {
        field_0x234[i] = 0;
    }
    for (s32 i = 0; i < 4; i++) {
        field_0x318[i] = -1;
    }
    field_0x94 = 0;
    field_0x196 = 1;
    for (s32 i = 0; i < 4; i++) {
        field_0x308[i] = -1;
    }
    field_0x284 = 0;
    field_0x288 = 0;
    field_0x2b4 = 0;
    field_0x29c = 0;
    field_0x28c = 0;
    field_0x290 = 0;
    field_0x2b8.zero();
    mVel_0x3c.zero();
    field_0x298 = 0x1E0;
    field_0x2a0 = 0;
    field_0x2a8 = 0.0f;
    field_0x294 = -1;
    field_0x2f8 = -1;
    field_0x2ac = 255.0f;
    field_0x2b0 = 255.0f;
    field_0x2a5 = 0;
    field_0x2b6 = (s16)(65536.0f * gGameRoot_8053AAF8->mRandom.get_ufloat_1());
    field_0x2dc.set(mPos);
    field_0x2e8.set(mPos);
    field_0x54.set(1.0f, 1.0f, 1.0f);
    field_0x2d0.set(1.0f, 1.0f, 1.0f);
    field_0x2f4 = 0.0f;
    field_0x300 = 0;
    field_0x2fc = -1;
    field_0x304 = 0;
    for (s32 i = 0; i < 5; i++) {
        mEmitters[i] = NULL;
    }
    field_0x2a4 = 0;
    field_0x2a6 = 1;
    mParamAtcn_0x118 = 2;
    if (mSpawnParam_0x90 == 0) {
        field_0xb0 = 0xA040004;
        lbl_8053AB91 = 0;
        lbl_80539FE8 = mLayer_0x4;
        for (s32 i = 0; i < 4; i++) {
            if (isPlayerActive_8023F264(i)) {
                lbl_804C1C00[i].set(*getPlayerPos_8023E724(i));
            }
        }
        lbl_8053AB90 = 0;
        mBox.i.x = -48.0f;
        mBox.i.y = -48.0f;
        mBox.f.x = 48.0f;
        mBox.f.y = 48.0f;
        mMapBox_0x80.i.x = -48.0f;
        mMapBox_0x80.i.y = -48.0f;
        mMapBox_0x80.f.x = 48.0f;
        mMapBox_0x80.f.y = 50.0f;
        mState = 0;
    }
    if (mSpawnParam_0x90 == 10 || (mSpawnParam_0x90 - 11) <= 1u || mSpawnParam_0x90 == 13) {
        mBox.i.x = -12.0f;
        mBox.i.y = -12.0f;
        mBox.f.x = 12.0f;
        mBox.f.y = 12.0f;
        mMapBox_0x80.i.x = -12.0f;
        mMapBox_0x80.i.y = -12.0f;
        mMapBox_0x80.f.x = 12.0f;
        mMapBox_0x80.f.y = 12.0f;
        mState = 0x1F;
    }
    if (mSpawnParam_0x90 == 20 || (mSpawnParam_0x90 - 21) <= 1u || mSpawnParam_0x90 == 23) {
        mBox.i.x = -8.0f;
        mBox.i.y = -8.0f;
        mBox.f.x = 8.0f;
        mBox.f.y = 8.0f;
        mMapBox_0x80.i.x = -8.0f;
        mMapBox_0x80.i.y = -8.0f;
        mMapBox_0x80.f.x = 8.0f;
        mMapBox_0x80.f.y = 8.0f;
        mParamAtcp_0x114 = 2;
        mState = 0x23;
    }
    if (mSpawnParam_0x90 == 1 || (mSpawnParam_0x90 - 2) <= 1u || mSpawnParam_0x90 == 4) {
        field_0x25c = mSpawnParam_0x90 - 1;
        lbl_804C1BC0[field_0x25c] = mLayer_0x4;
        lbl_804C1BD0[field_0x25c].set(mPos);
        lbl_804C1BD0[field_0x25c].y += 48.0f;
        mState = 0x28;
    }
    if (mSpawnParam_0x90 == 31) {
        field_0xb0 = 0x5000000;
        mState = 0x2C;
    }
    if (mSpawnParam_0x90 == 30) {
        field_0xb0 = 0xA040004;
        mParamAtcp_0x114 = 4;
        mBox.i.x = -64.0f;
        mBox.i.y = -64.0f;
        mBox.f.x = 64.0f;
        mBox.f.y = 64.0f;
        mState = 0x2A;
    }
}

Ganon_804A25F0::~Ganon_804A25F0() {
    for (s32 i = 0; i < 5; i++) {
        releaseEmitter_801EE434(mEmitters[i]);
    }
}

// Steps the body animation (field_0x25c / field_0x234[0]) and the second layer (field_0x260 /
// field_0x234[1]) through the frame tables.
// FIXME(shotgun): 99.73 %, same-text; behaviour clean.
void Ganon_804A25F0::fn_801964AC() {
    if (field_0x28c != field_0x290) {
        field_0x290 = field_0x28c;
        field_0x2a4 = 0;
        field_0x234[0] = 0;
        field_0x234[1] = 0;
        field_0x25c = 0;
        field_0x260 = 0;
    }
    if (field_0x234[0] == 0) {
        field_0x284 = lbl_804A2118[field_0x28c][field_0x25c];
        field_0x284 += lbl_804A1FF8[field_0x28c];
        field_0x234[0] = lbl_804A2158[field_0x28c][field_0x25c];
        field_0x25c++;
        if (field_0x28c == 8) {
            if (field_0x264 == 0) {
                if (field_0x25c > 2) {
                    field_0x25c = 0;
                }
            } else {
                field_0x2a5 = 0;
            }
        }
        if (lbl_804A2118[field_0x28c][field_0x25c] == 0xFF) {
            s32 anim = field_0x28c;
            if ((u32)anim <= 1 || anim == 5 || anim == 3 || anim == 10 || anim == 12) {
                field_0x25c = 0;
                if (field_0x28c == 10) {
                    if (field_0x234[2] != 0 || mState != 0xB) {
                        startSound_801F0E34(0x50006, 0);
                    }
                }
            } else if (anim == 6) {
                field_0x25c = 4;
            } else {
                field_0x25c--;
            }
            field_0x2a4 = 1;
            if (field_0x28c == 2) {
                field_0x28c = 3;
                field_0x25c = 0;
            }
            if (field_0x28c == 13 || field_0x28c == 14) {
                field_0x2a4 = 0;
                field_0x25c = 0;
                field_0x28c = 9;
            }
            if (field_0x28c == 9) {
                field_0x28c = 10;
                startSound_801F0E34(0x50006, 0);
                field_0x25c = 0;
            }
        }
    }
    if (field_0x234[1] == 0) {
        field_0x288 = lbl_804A2298[field_0x28c][field_0x260];
        field_0x288 += lbl_804A1FF8[field_0x28c];
        field_0x234[1] = lbl_804A22D8[field_0x28c][field_0x260];
        field_0x260++;
        if (field_0x28c == 8) {
            if (field_0x264 == 0) {
                if (field_0x260 > 2) {
                    field_0x260 = 0;
                }
            }
        }
        if (lbl_804A2298[field_0x28c][field_0x260] == 0xFF) {
            s32 anim = field_0x28c;
            if ((u32)anim <= 1 || anim == 5 || anim == 3 || anim == 10 || anim == 12) {
                field_0x260 = 0;
            } else if (anim == 6) {
                field_0x260 = 4;
            } else {
                field_0x260--;
            }
            if (field_0x28c == 2) {
                field_0x28c = 3;
                field_0x260 = 0;
            }
            if (field_0x28c == 13 || field_0x28c == 14) {
                field_0x260 = 0;
            }
            if (field_0x28c == 9) {
                field_0x28c = 10;
                field_0x260 = 0;
            }
        }
    }
}

// Picks the player (of those on this layer) with the most hearts (mode 0) or most of the other
// stat (mode 1), the nearest on ties by a coin flip; stores its distance, angle and index.
// FIXME(shotgun): 99.98 %, operands; behaviour clean.
void Ganon_804A25F0::fn_80196208(s32 mode) {
    field_0x2fc = -1;
    s32 best = -1;
    for (s32 i = 0; i < 4; i++) {
        if (!isPlayerActive_8023F264(i)) {
            continue;
        }
        if (getField_0x4() != getPlayerLayer_8023DE58(i)) {
            continue;
        }
        if (fn_80230188(i)) {
            continue;
        }
        if (fn_8022DCD8(i)) {
            continue;
        }
        if (!((mode == 0 && getPlayerLife_80237774(i) >= best) || (mode == 1 && fn_80236228(i) >= best))) {
            continue;
        }
        if ((mode == 0 && best == getPlayerLife_80237774(i)) || (mode == 1 && best == fn_80236228(i))) {
            if (gGameRoot_8053AAF8->mRandom.get_ufloat_1() < 0.5f) {
                continue;
            }
        }
        if (mode == 0) {
            best = getPlayerLife_80237774(i);
        }
        if (mode == 1) {
            best = fn_80236228(i);
        }
        f32 dx = getPlayerPos_8023E724(i)->x - mPos.x;
        f32 dy = getPlayerPos_8023E724(i)->y - mPos.y;
        field_0x2f4 = std::sqrtf(dx * dx + dy * dy);
        field_0x304 = (s16)(JMath::atanTable_.atan2_(dx, dy) * (32768.0f / JMath::TAngleConstant_<f32>::RADIAN_DEG180()));
        field_0x2fc = i;
    }
}

void Ganon_804A25F0::fn_80196020(s32 player) {
    if (player < 0) {
        return;
    }
    if (field_0x268 < 0) {
        for (s32 i = 0; i < 5; i++) {
            if (mEmitters[i] != NULL) {
                mEmitters[i]->becomeInvalidEmitter();
                mEmitters[i] = NULL;
            }
        }
        mEmitters[0] = spawnEmitter_8013CC50(mLayer_0x4, getPlayerPos_8023E724(player), 0x2A6, 0, 0, NULL, NULL, true, 1.0f);
        mEmitters[1] = spawnEmitter_8013CC50(mLayer_0x4, getPlayerPos_8023E724(player), 0x2A7, 0, 0, NULL, NULL, true, 1.0f);
    }
    if (getPlayerLeader_8022F3DC(player) >= 0) {
        player = getPlayerLeader_8022F3DC(player);
    }
    fn_8022CA6C(player, 5, false);
    if (startPlayerTransition_802317F0(player, lbl_804C1BC0[player], getPlayerPos_8023E724(player), &lbl_804C1BD0[player], 6)) {
        field_0x2a8 = 1.0f;
        field_0x234[0] = 100;
        mState = 0x22;
    } else {
        mState = 0x21;
    }
}

// FIXME(shotgun): 97.82 %, missing/extra + reloc-name; behaviour drift.
void Ganon_804A25F0::update() {
    JGeometry::TVec3f posA;
    JGeometry::TVec3f posB;
    JGeometry::TBox2f box58;
    u8 wallHit = 0;
    zeroVec_801546BC(&posB);
    mLife = 10000;
    if (mSpawnParam_0x90 == 31) {
        fn_80192BD8();
        return;
    }
    if (mSpawnParam_0x90 == 0) {
        fn_8017ECC0();
    }
    fn_80154718(&mPos);
    if (mSpawnParam_0x90 == 0) {
        fn_801964AC();
        if (updateBossDeath_0x2C(0.0f, 16.0f, 50.0f)) {
            return;
        }
        updateHitReaction_801F3D94(0);
        if (field_0x2a0 != 3) {
            if (field_0x2a6 == 0 && field_0x2ac > 150.0f && checkPlayerAttacks_801F2FAC()) {
                if (field_0x298 > 0) {
                    field_0x298 -= 10000 - mLife;
                }
                if (field_0x298 <= 0) {
                    if (mState != 9) {
                        if (field_0x2a0 == 0) {
                            field_0x298 = 0x1E0;
                            startSound_801F0E34(0x50000, 0);
                            field_0x2a0 = 1;
                            lbl_8053AB91 = 1;
                        } else {
                            if (field_0x2a0 == 1) {
                                fn_80196D30(2, 0);
                            }
                            mState = 9;
                        }
                    }
                } else {
                    startSound_801F0E34(0x50000, 0);
                }
                zeroVec_801546BC(&mVel_0x3c);
            }
        } else if (field_0x2a6 == 0) {
            for (s32 i = 0; i < 4; i++) {
                s32 hitPlayer = findOverlappingPlayer_801F285C(NULL, NULL, true, i);
                if (hitPlayer >= 0) {
                    JGeometry::TVec2f dir;
                    getBoxCenter_801F3128(&dir);
                    hitPlayerGroup_80238548(hitPlayer, 0, &dir, 3);
                }
            }
            copyVec3_80196D70(&posA, &mPos);
            mPos.y -= mPos.z;
            mPos.z = 0.0f;
            s32 hitSer = findOverlappingActor_801F2718('GNON');
            copyVec3_80196D70(&mPos, &posA);
            if (hitSer >= 0) {
                ActorBase_804A5770* hit = findActorBySerial_8015445C(hitSer);
                if (hit != NULL && hit->fn_8018A590() == 1 &&
                    (hit->getSpawnParam_801544C8() == 20 || hit->getSpawnParam_801544C8() == 21 ||
                     hit->getSpawnParam_801544C8() == 22 || hit->getSpawnParam_801544C8() == 23)) {
                    field_0x298 = (s32)((f32)field_0x298 - 64.0f);
                    takeHit_801F37AC(0, &mPos, 3, 0, -1);
                    fn_801544D0(0);
                    lbl_8053AB90 = 1;
                    startSound_801F0E34(0x50005, 0);
                    spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1CE, 0, 3, NULL, NULL, true, 1.0f);
                    hit->fn_8018A5A8(0);
                    field_0x2a6 = 1;
                    if (field_0x298 <= 0) {
                        mState = 9;
                    } else {
                        field_0x28c = 15;
                        fn_801964AC();
                    }
                    zeroVec_801546BC(&mVel_0x3c);
                }
            }
        }
    }
    for (int i = 0; i < 10; i++) {
        if (field_0x234[i] > 0) {
            field_0x234[i]--;
        }
    }
    if (field_0x28c == 15) {
        if (field_0x2a4 == 0) {
            return;
        }
        mState = 3;
    }
    if (mSpawnParam_0x90 != 30) {
        addCalc2_801F71A4(&mVel_0x3c.x, field_0x2b8.x, 0.3f, 1.0f);
        addCalc2_801F71A4(&mVel_0x3c.y, field_0x2b8.y, 0.3f, 1.0f);
    }
    fn_801545F0(&mPos, &mVel_0x3c);
    if (mSpawnParam_0x90 == 0 || mSpawnParam_0x90 == 20 || mSpawnParam_0x90 == 21 || mSpawnParam_0x90 == 22 || mSpawnParam_0x90 == 23) {
        fn_801F2B20(0);
    }
    if (mSpawnParam_0x90 == 0) {
        if (mState != 9 && mState != 10 && mState != 12 && mState != 25 && mState != 26 &&
            mState != 27 && field_0x2ac == 255.0f) {
            setBox_80154590(&mBox, -32.0f, -32.0f, 32.0f, 32.0f);
            if (field_0x2a0 != 3) {
                damagePlayersInBox_801F2B7C(-100, -1, 1);
            } else {
                damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 3);
            }
            setBox_80154590(&mBox, -48.0f, -48.0f, 48.0f, 48.0f);
        }
        fn_80196208(0);
        if (mState != 12 && mState != 11 && mState != 16 && mState != 17 && mState != 18) {
            wallHit = fn_801545C0(NULL, false);
        }
        if (mState != 14 && mState != 15) {
            if (mState == 4) {
                if (field_0x2a0 != 3) {
                    addCalc2_801F71A4(&field_0x2ac, 1.0f, 0.5f, 10.0f);
                }
                addCalc2_801F71A4(&mPos.z, 35.0f, 0.5f, 10.0f);
                field_0x26c = 0;
            } else if (field_0x94 >= 1) {
                if (field_0x2a0 != 3) {
                    addCalc2_801F71A4(&field_0x2ac, 255.0f, 0.5f, 10.0f);
                }
                addCalc2_801F71A4(&mPos.z, 0.0f, 0.5f, 10.0f);
            }
            if (field_0x2a0 == 3) {
                addCalc2_801F71A4(&field_0x2ac, 255.0f, 0.5f, 10.0f);
            }
            if (field_0x2ac > 254.0f) {
                field_0x2ac = 255.0f;
            }
            if (field_0x94 >= 1 && field_0x26c == 0 && mPos.z < 0.1f) {
                mPos.z = 0.0f;
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1C8, 0, 3, NULL, NULL, true, 1.0f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1C9, 0, 3, NULL, NULL, true, 1.0f);
                startSound_801F0E34(0x50028, 0);
                fn_80196D8C(mLayer_0x4, false, 3.0f, 0.0f);
                field_0x26c = 1;
            }
        }
        field_0x2b0 = field_0x2ac;
    }
    switch (mState) {
    case 0:
        copyVec3_80196D70(&posA, &mPos);
        posA.y += 16.0f;
        field_0x26c = 1;
        fn_8017EB44(&posA);
        fn_8017EB1C(0.05f);
        field_0x28c = 1;
        field_0x264++;
        if (field_0x264 >= 150) {
            fn_8022ADE4();
            field_0x264 = 0;
            mState = 1;
        }
        // fall through
    case 1: {
        copyVec3_80196D70(&posA, &mPos);
        posA.y += 16.0f;
        fn_8017EB44(&posA);
        fn_8017EB1C(0.05f);
        field_0x28c = 6;
        fn_8022FE80(getRepresentativePlayer_8022EA0C(0), getLayer_8015461C(), getSerial_801545E8(), false, false);
        zeroVec_801546BC(&posA);
        posA.y = -10.0f;
        JGeometry::TVec3f msgPos;
        fn_801546D0(&msgPos, &posA);
        if (openMessage_8038AD24(getRepresentativePlayer_8022EA0C(0), 0x34E, getSerial_801545E8(), getLayer_8015461C(), msgPos, 0x4102)) {
            mState = 2;
        }
        // fall through
    }
    case 2:
        copyVec3_80196D70(&posA, &mPos);
        posA.y += 16.0f;
        fn_8017EB44(&posA);
        fn_8017EB1C(0.05f);
        fn_8022FE80(getRepresentativePlayer_8022EA0C(0), getLayer_8015461C(), getSerial_801545E8(), false, false);
        if (fn_80388CAC(getRepresentativePlayer_8022EA0C(0), getSerial_801545E8())) {
            fn_8018A63C();
            field_0x94 = 1;
            fn_8018A6AC()->startBgm_80458FF0(0x100000C, false, 0);
            field_0x2b6 = -0x8000;
            mState = 3;
        }
        break;
    case 3:
        field_0x264 = 0;
        field_0x234[2] = (s32)(100.0f * randFloat_80154744() + 50.0f);
        lbl_8053AB90 = 0;
        if (field_0x234[3] == 0) {
            field_0x234[3] = (s32)(200.0f * randFloat_80154744() + 100.0f);
            if (field_0x2a0 == 3) {
                field_0x234[3] = (s32)(70.0f * randFloat_80154744() + 70.0f);
            }
        }
        field_0x28c = 5;
        field_0x2a6 = 0;
        field_0x2b4 = 0;
        field_0x2ac = 255.0f;
        field_0x2b0 = 255.0f;
        field_0x274 = 0;
        mState = 4;
        // fall through
    case 4:
        field_0x2b8.x = 3.0f * sinShort_801544FC(field_0x2b6);
        field_0x2b8.y = 3.0f * cosShort_801544E4(field_0x2b6);
        if (field_0x2a0 == 3 && mPos.y > 32.0f + field_0x2dc.y && mVel_0x3c.y > 1.0f && field_0x234[6] == 0) {
            field_0x234[2] = 0;
            field_0x274 = 1;
        }
        field_0x2b4 += 0x1000;
        startSound_801F0E34(0x400B2, 0);
        if (field_0x2a5 == 0) {
            copyVec3_80196D70(&posA, &mPos);
            copyVec3_80196D70(&mPos, &field_0x2c4);
            startSound_801F0E34(0x30052, 0);
            copyVec3_80196D70(&mPos, &posA);
        }
        if ((wallHit || field_0x234[2] == 0) && field_0x234[7] == 0) {
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            mState = 5;
            break;
        }
        if (field_0x2a0 != 3 && field_0x2fc == -1) {
            mState = 7;
            break;
        }
        if (field_0x2a5 == 0 && field_0x234[3] == 0) {
            if (field_0x2a0 == 0 && mPos.y < field_0x2dc.y) {
                mState = 19;
            }
            if (field_0x2a0 == 1) {
                mState = 25;
            }
            if (field_0x2a0 == 3 && mPos.y < field_0x2dc.y) {
                mState = 22;
            }
        }
        break;
    case 5:
        field_0x264 = field_0x234[3];
        field_0x28c = 1;
        field_0x234[2] = 50;
        zeroVec_801546BC(&field_0x2b8);
        zeroVec_801546BC(&mVel_0x3c);
        field_0x268 = 0;
        field_0x26c = 0;
        mState = 6;
        // fall through
    case 6:
        if (field_0x234[2] == 0 && field_0x2a5 == 0) {
            field_0x234[3] = field_0x264;
            mState = 3;
            copyVec3_80196D70(&posA, &mPos);
            if (randFloat_80154744() < 0.3f && field_0x2fc >= 0) {
                mPos.x += 16.0f * sinShort_801544FC(field_0x304);
                mPos.y += 16.0f * cosShort_801544E4(field_0x304);
                mPos.z = 35.0f;
                if (!fn_801545C0(NULL, false)) {
                    field_0x2b6 = field_0x304;
                    if (field_0x274 != 0) {
                        field_0x274 = 0;
                        field_0x234[6] = 30;
                        field_0x2b6 = 0x9000 - (s32)(8192.0f * randFloat_80154744());
                    }
                    copyVec3_80196D70(&mPos, &posA);
                    break;
                }
            }
            f32 dx = field_0x2dc.x - mPos.x;
            f32 dy = field_0x2dc.y - mPos.y;
            field_0x2b6 = atan2Short_8015465C(dx, dy);
            field_0x2b6 += (s16)(8192.0f - 16384.0f * randFloat_80154744());
            field_0x274 = 0;
            copyVec3_80196D70(&posA, &mPos);
            mPos.x += 16.0f * sinShort_801544FC(field_0x2b6);
            mPos.y += 16.0f * cosShort_801544E4(field_0x2b6);
            mPos.z = 35.0f;
            if (fn_801545C0(NULL, false)) {
                field_0x2b6 = atan2Short_8015465C(dx, dy);
            }
            copyVec3_80196D70(&mPos, &posA);
        }
        break;
    case 7:
        field_0x28c = 5;
        field_0x2b4 = 0;
        mState = 8;
        // fall through
    case 8: {
        if (field_0x2fc >= 0 && !fn_8022D75C(field_0x2fc)) {
            mState = 3;
            break;
        }
        f32 dy = field_0x2dc.y - mPos.y;
        f32 dx = field_0x2dc.x - mPos.x;
        if (sqrtf_80154378(dx * dx + dy * dy) > 2.0f) {
            field_0x2b4 += 0x1000;
            startSound_801F0E34(0x400B2, 0);
            field_0x2b6 = atan2Short_8015465C(dx, dy);
            field_0x2b8.x = 4.0f * sinShort_801544FC(field_0x2b6);
            field_0x2b8.y = 4.0f * cosShort_801544E4(field_0x2b6);
        } else {
            field_0x28c = 1;
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
        }
        break;
    }
    case 9:
        if (field_0x2a5 == 1) {
            if (field_0x294 > 0) {
                ActorBase_804A5770* spear = findActorBySerial_8015445C(field_0x294);
                if (spear != NULL) {
                    spear->setAlive_8018A598(false);
                }
            }
            JGeometry::TVec3f one;
            copyVec3_80196D70(&field_0x2d0, fn_80196D60(&one, 1.0f, 1.0f, 1.0f));
            field_0x294 = -1;
            field_0x2a5 = 0;
        }
        if (field_0x2a0 == 1) {
            if (fn_8018A660() == 0) {
                break;
            }
            for (s32 i = 0; i < 4; i++) {
                fn_80230C6C(i);
            }
            startSound_801F0E34(0x50005, 0);
            fn_80196D8C(mLayer_0x4, false, 4.0f, 0.0f);
        }
        field_0x298 = 0x140;
        for (s32 i = 0; i < 5; i++) {
            if (mEmitters[i] != NULL) {
                invalidateEmitter_8017EAB4(mEmitters[i]);
                mEmitters[i] = NULL;
            }
        }
        zeroVec_801546BC(&field_0x2b8);
        zeroVec_801546BC(&mVel_0x3c);
        field_0x28c = 14;
        field_0x2a6 = 1;
        if (field_0x2a0 == 3) {
            field_0x28c = 13;
            field_0x29c++;
            if (field_0x29c > 3) {
                field_0x29c = 3;
            }
            lbl_8053AB90 = 1;
            field_0x298 = 0x140;
            field_0x234[4] = field_0x29c * 100 + 500;
            setBox_80154590(&mBox2, -48.0f, -48.0f, 48.0f, 48.0f);
            field_0xb0 = 14;
        }
        mState = 10;
        // fall through
    case 10:
        if (field_0x94 == 5) {
            field_0x28c = 12;
            startSound_801F0E34(0x50007, 0);
            startSound_801F0E34(0x50008, 0);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x536, 0, 3, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x537, 0, 3, NULL, NULL, true, 1.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x538, 0, 3, NULL, NULL, true, 1.0f);
            if (mEmitters[0] == NULL) {
                mEmitters[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x539, 0, 3, NULL, NULL, true, 1.0f);
            }
            fn_80196D30(2, 0);
            mState = 13;
        } else if (field_0x2a0 == 1) {
            copyVec3_80196D70(&posA, &mPos);
            fn_8017EC70(&posA, 1.0f);
            fn_8017EB1C(0.05f);
            if (field_0x2a4 == 1) {
                fn_80119DD0()->startFade_80119998(50, 0, 2, 0, 1, 50, 0);
                field_0x234[2] = 70;
                field_0x2b0 = 0.0f;
                field_0x2a6 = 1;
                mState = 11;
            }
        } else if (field_0x234[4] <= 1) {
            setBox_80154590(&mBox2, -48.0f, -48.0f, 48.0f, 48.0f);
            field_0xb0 = 0xA040004;
            field_0x2a6 = 1;
            field_0x28c = 11;
            mState = 12;
        }
        break;
    case 11:
        if (field_0x234[2] != 0) {
            copyVec3_80196D70(&posA, &mPos);
            fn_8017EC70(&posA, 1.0f);
            fn_8017EB1C(0.05f);
        } else {
            field_0x94 = 2;
            field_0x2b0 = field_0x2ac;
            copyVec3_80196D70(&mPos, &field_0x2dc);
            mPos.x -= 200.0f;
            if (field_0x28c == 11) {
                field_0x94 = 4;
                mState = 12;
            }
        }
        break;
    case 12:
        if (field_0x2a4 == 1) {
            lbl_8053AB90 = 0;
            if (field_0x2a0 == 1) {
                field_0x2a8 = 0.0f;
                field_0x28c = 5;
                field_0x94 = 2;
                mState = 16;
            } else {
                mState = 3;
            }
        }
        break;
    case 13:
        if (fn_8018A660() == 0) {
            break;
        }
        for (s32 i = 0; i < 4; i++) {
            fn_80230C6C(i);
        }
        field_0x2a5 = 1;
        fn_80196E00();
        fn_8018A6AC()->stopBgm_80458F9C(60);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x6D, 0, 0, NULL, NULL, true, 1.0f);
        field_0x234[1] = 0;
        field_0x264 = 0;
        mState = 14;
        // fall through
    case 14:
        if (field_0x234[1] == 0) {
            field_0x234[1] = (s32)(20.0f * randFloat_80154744() + 20.0f);
            spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x488, 1, 5, NULL, NULL, true, 1.0f);
        }
        addCalc2_801F71A4(&field_0x2ac, 1.0f, 0.2f, 0.6f);
        field_0x264++;
        if (field_0x264 == 220) {
            fn_80119DD0()->startFade_80119998(50, 0, 1, 0, 1, 130, 0);
        }
        if (field_0x264 >= 370) {
            field_0x2ac = 0.0f;
            field_0x234[1] = 10;
            copyVec3_80196D70(&mPos, &field_0x2dc);
            field_0x94 = 6;
            if (mEmitters[0] != NULL) {
                getEmitterManager_8018A5F0()->forceDeleteEmitter(mEmitters[0]);
                mEmitters[0] = NULL;
            }
            fn_80196DF0();
            fn_803B8C74(0);
            mState = 15;
        }
        break;
    case 15:
        if (field_0x234[1] == 0) {
            fn_8022ADCC();
            setAlive_8018A598(false);
        }
        break;
    case 16: {
        copyVec3_80196D70(&posA, &field_0x2dc);
        posA.y = field_0x2dc.y - 80.0f;
        fn_8017EC70(&posA, 1.0f);
        fn_8017EB1C(0.05f);
        fn_8017EC38(&box58, fn_801826A4(getLayer_8015461C()));
        addCalc2_801F71A4(&field_0x2a8, 1.0f, 0.1f, 0.1f);
        f32 stepX = field_0x2a8 * fabsf_8015436C(field_0x2dc.x - mPos.x);
        f32 stepY = field_0x2a8 * fabsf_8015436C((field_0x2dc.y - 80.0f) - mPos.y);
        addCalc2_801F71A4(&mPos.x, field_0x2dc.x, 1.0f, stepX);
        addCalc2_801F71A4(&mPos.y, field_0x2dc.y - 80.0f, 1.0f, stepY);
        if (field_0x2a8 >= 0.9f) {
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            field_0x28c = 1;
            mState = 17;
        }
        break;
    }
    case 17: {
        copyVec3_80196D70(&posA, &mPos);
        fn_8017EC70(&posA, 1.0f);
        fn_8017EB1C(0.05f);
        field_0x264++;
        if (field_0x264 > 50) {
            field_0x264 = 50;
            zeroVec_801546BC(&posA);
            posA.y = -10.0f;
            fn_8022FE80(getRepresentativePlayer_8022EA0C(0), getLayer_8015461C(), getSerial_801545E8(), false, false);
            JGeometry::TVec3f msgPos2;
            fn_801546D0(&msgPos2, &posA);
            if (openMessage_8038AD24(getRepresentativePlayer_8022EA0C(0), 0x2DD, getSerial_801545E8(), getLayer_8015461C(), msgPos2, 0x102)) {
                field_0x28c = 6;
                mState = 18;
            }
        }
        break;
    }
    case 18:
        copyVec3_80196D70(&posA, &mPos);
        fn_8017EC70(&posA, 1.0f);
        fn_8017EB1C(0.05f);
        fn_8022FE80(getRepresentativePlayer_8022EA0C(0), getLayer_8015461C(), getSerial_801545E8(), false, false);
        if (fn_80388CAC(getRepresentativePlayer_8022EA0C(0), getSerial_801545E8())) {
            fn_8018A63C();
            field_0x94 = 3;
            field_0x2a0 = 3;
            mState = 3;
        }
        break;
    case 19:
        if (!(mPos.z < 0.0f)) {
            field_0x234[3] = 0;
            field_0x264 = 0;
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            field_0x234[2] = 120;
            for (s32 i = 0; i < 5; i++) {
                if (mEmitters[i] != NULL) {
                    invalidateEmitter_8017EAB4(mEmitters[i]);
                    mEmitters[i] = NULL;
                }
            }
            field_0x28c = 2;
            mState = 20;
        } else {
            break;
        }
        // fall through
    case 20:
        if (field_0x2a4 == 1) {
            copyVec3_80196D70(&posA, &mPos);
            posA.x += -58.0f;
            posA.y += -110.0f;
            posA.z = 0.0f;
            if (mEmitters[0] == NULL) {
                mEmitters[0] = spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1D1, 1, 7, NULL, NULL, true, 1.0f);
            }
            if (mEmitters[1] == NULL) {
                mEmitters[1] = spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1D2, 1, 7, NULL, NULL, true, 1.0f);
            }
            startSound_801F0E34(0x30053, 0);
            if (field_0x234[2] == 0) {
                field_0x234[2] = 100;
                field_0x264++;
                if (field_0x264 >= 4) {
                    field_0x234[2] = 0;
                    field_0x234[3] = 0;
                    field_0x264 = 0;
                    if (mEmitters[0] != NULL) {
                        getEmitterManager_8018A5F0()->forceDeleteEmitter(mEmitters[0]);
                        mEmitters[0] = NULL;
                    }
                    if (mEmitters[1] != NULL) {
                        getEmitterManager_8018A5F0()->forceDeleteEmitter(mEmitters[1]);
                        mEmitters[1] = NULL;
                    }
                    for (s32 i = 0; i < 5; i++) {
                        if (mEmitters[i] != NULL) {
                            invalidateEmitter_8017EAB4(mEmitters[i]);
                            mEmitters[i] = NULL;
                        }
                    }
                    field_0x28c = 4;
                    mState = 21;
                } else {
                    spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1C5, 1, 7, NULL, NULL, true, 1.0f);
                    spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1C6, 1, 7, NULL, NULL, true, 1.0f);
                    for (s32 i = 0; i < 10; i++) {
                        spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1C7, 1, 7, NULL, NULL, true, 1.0f);
                    }
                    s32 ang = (s32)(-2048.0f - 8192.0f * randFloat_80154744());
                    startSound_801F0E34(0x30054, 0);
                    startSound_801F0E34(0x50001, 0);
                    for (s32 i = 0; i < 4; i++) {
                        s32 subSer = spawnActor_80154310('GNON', getLayer_8015461C(), &posA, i + 10, -1, -1);
                        if (subSer >= 0) {
                            Ganon_804A25F0* sub = static_cast<Ganon_804A25F0*>(findActorBySerial_8015445C(subSer));
                            if (sub != NULL) {
                                sub->field_0x2b6 = (s16)ang;
                            }
                        }
                    }
                }
            }
        }
        break;
    case 21:
        if (field_0x2a4 == 1) {
            mState = 3;
        }
        break;
    case 22:
        if (!(mPos.z < 0.0f)) {
            field_0x234[3] = 0;
            field_0x264 = 0;
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            field_0x234[2] = 24;
            field_0x28c = 2;
            mState = 23;
        } else {
            break;
        }
        // fall through
    case 23:
        if (field_0x2a4 != 0 && field_0x234[2] == 0) {
            s32 angAcc = 0;
            copyVec3_80196D70(&posA, &mPos);
            posA.x += -58.0f;
            posA.y += -110.0f;
            field_0x234[2] = 80;
            for (s32 i = 0; i < 5; i++) {
                s32 subSer = spawnActor_80154310('GNON', getLayer_8015461C(), &posA, field_0x264 + 20, -1, -1);
                if (subSer >= 0) {
                    Ganon_804A25F0* sub = static_cast<Ganon_804A25F0*>(findActorBySerial_8015445C(subSer));
                    if (sub != NULL) {
                        sub->field_0x2b6 = (s16)angAcc;
                        angAcc += 0x3333;
                        copyVec3_80196D70(&sub->field_0x2e8, &field_0x2dc);
                        sub->field_0x2f8 = field_0x2f8;
                        sub->field_0x294 = i;
                    }
                }
            }
            field_0x264++;
            if (field_0x264 >= fn_8018E89C()) {
                field_0x264 = 0;
                mState = 24;
            }
        }
        break;
    case 24:
        if (field_0x234[2] == 0 && field_0x2a4 == 1) {
            mState = 3;
        }
        break;
    case 25:
        if (!(mPos.z < 0.0f)) {
            field_0x234[3] = 0;
            field_0x294 = -1;
            for (s32 i = 0; i < 5; i++) {
                if (mEmitters[i] != NULL) {
                    invalidateEmitter_8017EAB4(mEmitters[i]);
                    mEmitters[i] = NULL;
                }
            }
            field_0x2a6 = 1;
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            field_0x28c = 2;
            mState = 26;
        } else {
            break;
        }
        // fall through
    case 26:
        if (field_0x2a4 != 0 && fn_8018A660() != 0) {
            for (s32 i = 0; i < 4; i++) {
                fn_80230C6C(i);
            }
            copyVec3_80196D70(&posA, &mPos);
            posA.x += -58.0f;
            posA.y += -110.0f;
            playSe_801F0D20(0x44);
            startSound_801F0E34(0x400B3, 0);
            spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1C5, 1, 7, NULL, NULL, true, 1.0f);
            for (s32 i = 0; i < 10; i++) {
                spawnEmitter_8013CC50(mLayer_0x4, &posA, 0x1C7, 1, 7, NULL, NULL, true, 1.0f);
            }
            fn_80119DD0()->startFade_80119998(60, 0, 2, 0, 1, 0, 0);
            field_0x234[2] = 30;
            field_0x234[3] = 60;
            field_0x318[0] = -1;
            field_0x318[1] = -1;
            field_0x318[2] = -1;
            field_0x318[3] = -1;
            field_0x300 = 0;
            for (s32 i = 0; i < 4; i++) {
                if (isPlayerActive_8023F264(i) && !hasPlayerLeader_8022F514(i) && getLayer_8015461C() == getPlayerLayer_8023DE58(i)) {
                    field_0x308[field_0x300] = i;
                    field_0x300++;
                }
            }
            mState = 27;
        } else {
            break;
        }
        // fall through
    case 27:
        if (field_0x234[2] <= 3 && field_0x234[2] >= 1) {
            if (field_0x234[2] == 3 && field_0x300 > 1) {
                for (s32 i = 1; i < field_0x300; i++) {
                    fn_80230498(field_0x308[i], getPlayerPos_8023E724(field_0x308[i - 1]), 0xAF, 0xFF, -0x80, -0x80, true);
                }
                fn_80230498(field_0x308[0], getPlayerPos_8023E724(field_0x308[field_0x300 - 1]), 0xAF, 0xFF, -0x80, -0x80, true);
            }
            if (field_0x234[2] == 2 && field_0x300 > 0) {
                copyVec3_80196D70(&posB, &mPos);
                s32 idx = (s32)((f32)field_0x300 * randFloat_80154744());
                if (idx >= field_0x300) {
                    idx = 0;
                }
                copyVec3_80196D70(&mPos, getPlayerPos_8023E724(field_0x308[idx]));
                fn_8017EC38(&box58, fn_801826A4(getLayer_8015461C()));
                if (mPos.y < box58.i.y + 100.0f) {
                    mPos.y = box58.i.y + 100.0f;
                }
                if (mPos.y > box58.f.y - 100.0f) {
                    mPos.y = box58.f.y - 100.0f;
                }
                if (mPos.x < box58.i.x + 100.0f) {
                    mPos.x = box58.i.x + 100.0f;
                }
                if (mPos.x > box58.f.x - 100.0f) {
                    mPos.x = box58.f.x - 100.0f;
                }
                fn_80230498(field_0x308[idx], &posB, 0xAF, 0xFF, -0x80, -0x80, true);
            }
            if (field_0x234[2] == 1) {
                for (s32 i = 0; i < 4; i++) {
                    if (hasPlayerLeader_8022F514(i) && getLayer_8015461C() == getPlayerLayer_8023DE58(i) && getPlayerLeader_8022F3DC(i) >= 0) {
                        fn_80230498(i, getPlayerPos_8023E724(getPlayerLeader_8022F3DC(i)), 0xAF, 0xFF, -0x80, -0x80, true);
                    }
                }
            }
            if (mEmitters[0] != NULL) {
                getEmitterManager_8018A5F0()->forceDeleteEmitter(mEmitters[0]);
                mEmitters[0] = NULL;
            }
            if (mEmitters[1] != NULL) {
                getEmitterManager_8018A5F0()->forceDeleteEmitter(mEmitters[1]);
                mEmitters[1] = NULL;
            }
        }
        if (field_0x234[3] == 0) {
            fn_8018A63C();
            field_0x2a6 = 0;
            field_0x28c = 7;
            mState = 28;
        }
        break;
    case 28:
        if (field_0x25c == 6 && field_0x294 < 0) {
            copyVec3_80196D70(&posA, &field_0x2c4);
            field_0x294 = spawnActor_80154310('GNON', getLayer_8015461C(), &posA, 30, -1, -1);
            if (field_0x294 < 0) {
                break;
            }
            Ganon_804A25F0* spear = static_cast<Ganon_804A25F0*>(findActorBySerial_8015445C(field_0x294));
            if (spear == NULL) {
                field_0x294 = -1;
                break;
            }
            startSound_801F0E34(0x400B4, 0);
            field_0x2a5 = 1;
            zeroVec_801546BC(&field_0x2d0);
            spear->field_0x25c = 0x1000;
            spear->field_0x2b6 = 0x1000;
            spear->field_0x294 = getSerial_801545E8();
        }
        if (field_0x2a4 == 1) {
            field_0x2b6 = atan2Short_8015465C(field_0x2dc.x - mPos.x, field_0x2dc.y - mPos.y);
            field_0x234[7] = 50;
            mState = 3;
        }
        break;
    case 29: {
        field_0x264 = 0;
        zeroVec_801546BC(&field_0x2b8);
        zeroVec_801546BC(&mVel_0x3c);
        JGeometry::TVec3f one;
        copyVec3_80196D70(&field_0x2d0, fn_80196D60(&one, 1.0f, 1.0f, 1.0f));
        field_0x28c = 8;
        mState = 30;
        // fall through
    }
    case 30:
        if (field_0x2a4 == 1) {
            field_0x234[3] = 0;
            field_0x2b6 = atan2Short_8015465C(field_0x2dc.x - mPos.x, field_0x2dc.y - mPos.y);
            mState = 3;
        }
        break;
    case 31:
        if (mEmitters[0] == NULL) {
            mEmitters[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1CF, 1, 7, NULL, NULL, true, 1.0f);
        }
        if (mEmitters[1] == NULL) {
            mEmitters[1] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1D0, 1, 7, NULL, NULL, true, 1.0f);
        }
        if (mEmitters[0] == NULL || mEmitters[1] == NULL) {
            break;
        }
        if (mSpawnParam_0x90 != 14) {
            field_0x2b6 += (u16)((mSpawnParam_0x90 - 10) << 12);
        }
        mVel_0x3c.x = 10.0f * sinShort_801544FC(field_0x2b6);
        mVel_0x3c.y = 10.0f * cosShort_801544E4(field_0x2b6);
        copyVec3_80196D70(&field_0x2b8, &mVel_0x3c);
        field_0x268 = -1;
        field_0x234[1] = 20;
        field_0x234[0] = 100;
        mState = 32;
        // fall through
    case 32:
        setEmitterPos_8013CB44(mEmitters[0], &mPos, 1, 7, true);
        setEmitterPos_8013CB44(mEmitters[1], &mPos, 1, 7, true);
        if (mSpawnParam_0x90 == 14) {
            if (damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 7)) {
                field_0x234[0] = 0;
            } else if (fn_801F2A10(-1, -1, true, true)) {
                field_0x234[0] = 0;
            } else if (isOutsideView_801F6758(&mPos, getLayer_8015461C(), 16.0f)) {
                field_0x234[0] = 0;
            }
        }
        {
            copyBox_801546F4(&box58, &mBox);
            JGeometry::TVec2f pos2d;
            fn_80196D24(&pos2d, mPos.x, mPos.y);
            addVec2_80154558(&box58.i, &pos2d);
            addVec2_80154558(&box58.f, &pos2d);
            s32 hitPlayer = fn_8023B004(&box58, mLayer_0x4);
            if (hitPlayer >= 0) {
                fn_80196020(hitPlayer);
                field_0x268 = hitPlayer;
                break;
            }
        }
        if (field_0x234[1] == 0) {
            wallHit = fn_801545C0(NULL, false);
        }
        if (wallHit || field_0x234[0] == 0) {
            for (s32 i = 0; i < 5; i++) {
                if (mEmitters[i] != NULL) {
                    invalidateEmitter_8017EAB4(mEmitters[i]);
                    mEmitters[i] = NULL;
                }
            }
            mEmitters[0] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x57, 1, 7, NULL, NULL, true, 1.0f);
            mEmitters[1] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1CC, 1, 7, NULL, NULL, true, 1.0f);
            mEmitters[2] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x1CD, 1, 7, NULL, NULL, true, 1.0f);
            startSound_801F0E34(0x50002, 0);
            field_0x2a8 = 1.0f;
            field_0x234[0] = 50;
            mState = 34;
        }
        break;
    case 33:
        fn_80196020(field_0x268);
        break;
    case 34:
        if (field_0x234[0] == 0) {
            addCalc2_801F71A4(&field_0x2a8, 0.0f, 0.1f, 0.1f);
        }
        if (mEmitters[0] != NULL) {
            fn_80196D18(mEmitters[0], field_0x2a8, field_0x2a8);
        }
        if (mEmitters[1] != NULL) {
            fn_80196D18(mEmitters[1], field_0x2a8, field_0x2a8);
        }
        if (mEmitters[2] != NULL) {
            fn_80196D18(mEmitters[2], field_0x2a8, field_0x2a8);
        }
        if (field_0x2a8 < 0.1f) {
            for (s32 i = 0; i < 5; i++) {
                if (mEmitters[i] != NULL) {
                    invalidateEmitter_8017EAB4(mEmitters[i]);
                    mEmitters[i] = NULL;
                }
            }
            setAlive_8018A598(false);
        }
        break;
    case 35:
        field_0x264 = mSpawnParam_0x90 - 20;
        u32 kind = mSpawnParam_0x90 - 20;
        for (s32 i = 0; i < 4; i++) {
            if (mEmitters[i] == NULL) {
                mEmitters[i] = spawnEmitter_8013CC50(mLayer_0x4, &mPos, lbl_804A2318[kind * 4 + i], 1, 7, NULL, NULL, true, 1.0f);
            }
        }
        field_0x234[0] = 40;
        mState = 36;
        // fall through
    case 36:
        copyVec3_80196D70(&mPos, &field_0x2dc);
        mPos.x += field_0x2f4 * sinShort_801544FC(field_0x2b6);
        mPos.y += field_0x2f4 * cosShort_801544E4(field_0x2b6);
        for (s32 i = 0; i < 4; i++) {
            setEmitterPos_8013CB44(mEmitters[i], &mPos, 1, 7, true);
        }
        addCalc2_801F71A4(&field_0x2f4, 16.0f, 0.2f, 2.0f);
        field_0x2b6 += 0x1000;
        if (field_0x234[0] == 0) {
            s32 rnd = field_0x294;
            if (rnd < 0) {
                rnd = 0;
            }
            if (rnd == 0) {
                startSound_801F0E34(0x50001, 0);
            }
            field_0x2b6 = atan2Short_8015465C(field_0x2e8.x - mPos.x, field_0x2e8.y - mPos.y);
            field_0x2b6 += 0x2000 - (rnd << 12);
            field_0x2b8.x = 5.0f * sinShort_801544FC(field_0x2b6);
            field_0x2b8.y = 5.0f * cosShort_801544E4(field_0x2b6);
            field_0x234[0] = 200;
            startSound_801F0E34(0x5001E, 0);
            mState = 37;
        }
        break;
    case 37:
        if (field_0x234[0] == 0 || lbl_8053AB90 == 1) {
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            mState = 39;
            break;
        }
        setBox_80154590(&mBox, -12.0f, -12.0f, 12.0f, 12.0f);
        for (s32 i = 0; i < 4; i++) {
            s32 hitPlayer = findOverlappingPlayer_801F285C(NULL, NULL, true, i);
            setBox_80154590(&mBox, -8.0f, -8.0f, 8.0f, 8.0f);
            if (hitPlayer >= 0) {
                if (field_0x264 == hitPlayer || (hasPlayerLeader_8022F514(hitPlayer) && field_0x264 == getPlayerLeader_8022F3DC(hitPlayer))) {
                    field_0x94 = 1;
                    mVel_0x3c.x = 10.0f * sinShort_801544FC(-0x8000);
                    mVel_0x3c.y = 10.0f * cosShort_801544E4(-0x8000);
                    copyVec3_80196D70(&field_0x2b8, &mVel_0x3c);
                    startSound_801F0E34(0x30037, 0);
                    field_0x234[0] = 100;
                    mState = 38;
                    break;
                } else {
                    zeroVec_801546BC(&field_0x2b8);
                    zeroVec_801546BC(&mVel_0x3c);
                    mState = 39;
                    break;
                }
            }
        }
        fn_801F2B20(0);
        if (field_0x2f8 >= 0) {
            ActorBase_804A5770* owner = findActorBySerial_8015445C(field_0x2f8);
            if (owner != NULL) {
                f32 dx = owner->getPos_801544DC()->x - mPos.x;
                f32 dy = owner->getPos_801544DC()->y - mPos.y;
                if (sqrtf_80154378(dx * dx + dy * dy) < 8.0f) {
                    zeroVec_801546BC(&field_0x2b8);
                    zeroVec_801546BC(&mVel_0x3c);
                    mState = 39;
                    break;
                }
            }
        }
        if (damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 6)) {
            field_0x234[0] = 0;
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            mState = 39;
            break;
        }
        for (s32 i = 0; i < 4; i++) {
            setEmitterPos_8013CB44(mEmitters[i], &mPos, 1, 7, true);
        }
        break;
    case 38:
        if (field_0x54.x > 0.1f) {
            addCalc2_801F71A4(&field_0x54.x, 0.0f, 0.1f, 0.05f);
            field_0x54.z = field_0x54.y = field_0x54.x;
        }
        for (s32 i = 0; i < 4; i++) {
            setEmitterPos_8013CB44(mEmitters[i], &mPos, 1, 7, true);
        }
        if (field_0x94 == 0 || field_0x234[0] == 0 || lbl_8053AB90 == 1) {
            if (field_0x94 == 0) {
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0xFF, 1, 7, NULL, NULL, true, 1.5f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x100, 1, 7, NULL, NULL, true, 1.5f);
                spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x101, 1, 7, NULL, NULL, true, 1.5f);
            }
            zeroVec_801546BC(&field_0x2b8);
            zeroVec_801546BC(&mVel_0x3c);
            mState = 39;
        }
        break;
    case 39:
        if (field_0x54.x > 0.1f) {
            addCalc2_801F71A4(&field_0x54.x, 0.0f, 0.1f, 0.05f);
            field_0x54.z = field_0x54.y = field_0x54.x;
            for (s32 i = 0; i < 4; i++) {
                setEmitterPos_8013CB44(mEmitters[i], &mPos, 1, 7, true);
                fn_80196CF4(mEmitters[i], &field_0x54);
            }
        } else {
            for (s32 i = 0; i < 5; i++) {
                if (mEmitters[i] != NULL) {
                    invalidateEmitter_8017EAB4(mEmitters[i]);
                    mEmitters[i] = NULL;
                }
            }
            setAlive_8018A598(false);
        }
        break;
    case 40:
        if (!fn_8022F478(field_0x25c) && getLayer_8015461C() == getPlayerLayer_8023DE58(field_0x25c) &&
            !fn_8022D75C(field_0x25c)) {
            field_0x260 = 0;
            field_0x264 = 0;
            field_0x234[1] = 0;
            field_0x318[0] = -1;
            field_0x318[1] = -1;
            field_0x318[2] = -1;
            field_0x318[3] = -1;
            mState = 41;
        } else {
            break;
        }
        // fall through
    case 41:
        if (field_0x264 >= 5 || lbl_8053AB91 == 1) {
            fn_8022CA6C(field_0x25c, 5, false);
            if (startPlayerTransition_802317F0(field_0x25c, lbl_80539FE8, getPlayerPos_8023E724(field_0x25c), &lbl_804C1C00[field_0x25c], 6)) {
                for (s32 i = 0; i < 4; i++) {
                    if (field_0x318[i] >= 0) {
                        ActorBase_804A5770* sub = findActorBySerial_8015445C(field_0x318[i]);
                        if (sub != NULL) {
                            sub->setAlive_8018A598(false);
                        }
                    }
                }
                mState = 40;
            }
        } else if (field_0x234[1] == 0) {
            for (s32 i = 0; i < 4; i++) {
                if (field_0x260 < 4) {
                    if (field_0x318[i] == -1) {
                        copyVec3_80196D70(&posA, &mPos);
                        if (field_0x264 >= 4) {
                            field_0x260 = 4;
                        }
                        posA.x += lbl_804A2370[field_0x260][0];
                        posA.y += lbl_804A2370[field_0x260][1];
                        field_0x318[i] = spawnActor_80154310(lbl_804A2348[field_0x260], getLayer_8015461C(), &posA, lbl_804A235C[field_0x260], -1, -1);
                        if (field_0x318[i] >= 0) {
                            spawnActor_80154310('HBMB', getLayer_8015461C(), &posA, 11, -1, -1);
                            field_0x260++;
                            if (field_0x260 > 5) {
                                field_0x260 = 5;
                            }
                        }
                    }
                } else if (field_0x318[i] >= 0) {
                    if (findActorBySerial_8015445C(field_0x318[i]) == NULL) {
                        field_0x318[i] = -1;
                        field_0x264++;
                        if (field_0x264 < 4) {
                            field_0x260 = 3;
                        }
                    }
                }
            }
            field_0x234[1] = 8;
        }
        break;
    case 42:
        field_0x2b4 += 0x900;
        startSound_801F0E34(0x30055, 0);
        damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 3);
        addCalc2_801F71A4(&mVel_0x3c.x, field_0x2b8.x, 0.2f, 2.0f);
        addCalc2_801F71A4(&mVel_0x3c.y, field_0x2b8.y, 0.2f, 2.0f);
        posB.x = 5.0f * sinShort_801544FC(field_0x2b6);
        posB.y = 5.0f * cosShort_801544E4(field_0x2b6);
        copyVec3_80196D70(&field_0x2b8, &posB);
        addCalc2_801F71A4(&field_0x2a8, 512.0f, 0.4f, 4.0f);
        field_0x25c += (s32)field_0x2a8;
        field_0x2b6 += (s16)field_0x2a8;
        copyVec3_80196D70(&posA, &mPos);
        posA.x += 74.0f * sinShort_801544FC((s16)(field_0x2b4 + 0xA000));
        posA.y += 74.0f * cosShort_801544E4((s16)(field_0x2b4 + 0xA000));
        if (field_0x234[1] == 0) {
            if (!isOutsideView_801F6758(&posA, mLayer_0x4, 16.0f)) {
                spawnActor_80154310('GNON', mLayer_0x4, &posA, 31, -1, -1);
            }
            field_0x234[1] = 1;
        }
        if (field_0x25c > 0xC000) {
            field_0x25c = 0;
            mState = 43;
        }
        break;
    case 43:
        field_0x2b4 += 0x900;
        startSound_801F0E34(0x30055, 0);
        copyVec3_80196D70(&posA, &mPos);
        posA.x += 74.0f * sinShort_801544FC((s16)(field_0x2b4 + 0xA000));
        posA.y += 74.0f * cosShort_801544E4((s16)(field_0x2b4 + 0xA000));
        if (field_0x234[1] == 0) {
            if (!isOutsideView_801F6758(&posA, mLayer_0x4, 16.0f)) {
                spawnActor_80154310('GNON', mLayer_0x4, &posA, 31, -1, -1);
            }
            field_0x234[1] = 1;
        }
        damagePlayersInBox_801F2B7C(getParamAtcp_80154510(), -1, 3);
        addCalc2_801F71A4(&mVel_0x3c.x, field_0x2b8.x, 0.2f, 2.0f);
        addCalc2_801F71A4(&mVel_0x3c.y, field_0x2b8.y, 0.2f, 2.0f);
        {
            Ganon_804A25F0* owner = static_cast<Ganon_804A25F0*>(findActorBySerial_8015445C(field_0x294));
            if (owner != NULL) {
                f32 dx = owner->field_0x2c4.x - mPos.x;
                f32 dy = owner->field_0x2c4.y - mPos.y;
                if (owner->mState == 4 || owner->mState == 6 || owner->mState == 9) {
                    if (sqrtf_80154378(dx * dx + dy * dy) < 64.0f) {
                        owner->mState = 29;
                    }
                }
                field_0x2b6 = atan2Short_8015465C(dx, dy);
                posB.x = 5.0f * sinShort_801544FC(field_0x2b6);
                posB.y = 5.0f * cosShort_801544E4(field_0x2b6);
                copyVec3_80196D70(&field_0x2b8, &posB);
                if (!(sqrtf_80154378(dx * dx + dy * dy) > 7.0f) && owner->mState == 30) {
                    owner->field_0x264 = 1;
                    if (mEmitters[0] != NULL) {
                        getEmitterManager_8018A5F0()->forceDeleteEmitter(mEmitters[0]);
                        mEmitters[0] = NULL;
                    }
                    owner->field_0x25c = 3;
                    owner->field_0x234[0] = 0;
                    owner->field_0x260 = 3;
                    owner->field_0x234[1] = 0;
                    setAlive_8018A598(false);
                }
            }
        }
        break;
    }
}

void Ganon_804A25F0::fn_80192BD8() {
    switch (mState) {
    case 0x2C:
        field_0x234[0] = 50;
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x359, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x35A, 0, 0, NULL, NULL, true, 1.0f);
        spawnEmitter_8013CC50(mLayer_0x4, &mPos, 0x35B, 0, 0, NULL, NULL, true, 1.0f);
        mState = 0x2D;
        // fall through
    case 0x2D:
        if (field_0x234[0] == 0) {
            mAlive = false;
        }
        break;
    }
    damagePlayersInBox_801F2B7C(-100, -1, true);
    for (int i = 0; i < 10; i++) {
        if (field_0x234[i] > 0) {
            field_0x234[i]--;
        }
    }
}

// FIXME(shotgun): 98.29 %, scheduling+regalloc + reloc-name; behaviour clean.
void Ganon_804A25F0::draw() {
    SpriteParams_801F5AE0 params;
    Mtx mtx;
    f32 offX;
    f32 offY;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    if (mSpawnParam_0x90 == 31) {
        return;
    }
    f32 savedX = mPos.x;
    f32 savedY = mPos.y;
    f32 savedZ = mPos.z;
    if (mSpawnParam_0x90 != 0) {
        if (mSpawnParam_0x90 == 30) {
            params.field_0x1f = 0;
            params.field_0x1d = 5;
            mtx[0][3] = 0.0f;
            mtx[1][3] = 0.0f;
            mtx[2][3] = 0.0f;
            JGeometry::TVec3f rot(0.0f, (f32)field_0x2b4 / -10430.228f, 0.0f);
            setRotScaleMtx(mtx, rot, 1.4f);
            void* res = getBossArchiveResource_804032E8('GANO', "boss_butag_yari_mawashi_01.bti");
            params.mMtx = mtx;
            params.field_0x10.a = (u8)field_0x2b0;
            params.field_0x30 = -1;
            drawSpriteRes_801EFD34(res, &params, -1, 0.0f, 0.0f, 0.0f);
        }
        return;
    }
    params.field_0x1f = 0;
    if (mState == 4 || mState == 6) {
        mPos.z = 0.0f;
        params.field_0x10.set(0, 0, 0, 100);
        params.field_0x1d = -2;
        ResTIMG* res1 = static_cast<ResTIMG*>(getBossArchiveResource_804032E8('GANO', lbl_804A2398[field_0x284]));
        drawSpriteRes_801EFD34(res1, &params, -1, 0.0f, 0.0f, 0.0f);
        JUTTexture tex1(res1, 0);
        offX = -(f32)tex1.getWidth() * 0.5f;
        offY = -(f32)tex1.getHeight() * 0.5f;
        ResTIMG* res2;
        if (field_0x28c == 5) {
            mtx[0][3] = 0.0f;
            mtx[1][3] = 0.0f;
            mtx[2][3] = 0.0f;
            JGeometry::TVec3f rot(0.0f, (f32)field_0x2b4 / -10430.228f, 0.0f);
            setRotScaleMtx(mtx, rot, 1.4f);
            res2 = static_cast<ResTIMG*>(getBossArchiveResource_804032E8('GANO', "boss_butag_yari_mawashi_01.bti"));
        } else {
            setScaleMtx(mtx, field_0x2d0.x, field_0x2d0.y, field_0x2d0.y);
            res2 = static_cast<ResTIMG*>(getBossArchiveResource_804032E8('GANO', lbl_804A2484[field_0x288]));
        }
        params.mMtx = mtx;
        params.field_0x10.set(0, 0, 0, 100);
        params.field_0x1d = -1;
        JUTTexture tex2(res2, 0);
        offX += (f32)tex2.getWidth() * 0.5f + lbl_804A2570[field_0x28c][0];
        offY += (f32)tex2.getHeight() * 0.5f + lbl_804A2570[field_0x28c][1];
        field_0x2c4.set(mPos);
        field_0x2c4.x += offX;
        field_0x2c4.y += offY;
        if (field_0x2a5 == 0 && (field_0x28c != 7 || (field_0x28c == 7 && field_0x25c < 6))) {
            drawSpriteRes_801EFD34(res2, &params, -1, offX, 0.0f, -offY);
        }
    }
    mPos.x = savedX;
    mPos.y = savedY;
    mPos.z = savedZ;
    params.field_0x1d = 3;
    params.field_0x1f = 0;
    setScaleMtx(mtx, field_0x54.x, field_0x54.y, field_0x54.y);
    params.mMtx = mtx;
    params.field_0x10.set(255, 255, 255, 255);
    params.field_0x10.a = (u8)field_0x2ac;
    if ((field_0x2a0 == 3 && mState != 10) || mState == 16 || mState == 17 || mState == 18) {
        params.field_0x30 = 16;
    } else {
        params.field_0x30 = 17;
    }
    ResTIMG* res3 = static_cast<ResTIMG*>(getBossArchiveResource_804032E8('GANO', lbl_804A2398[field_0x284]));
    drawSpriteRes_801EFD34(res3, &params, -1, 0.0f, 0.0f, 0.0f);
    JUTTexture tex3(res3, 0);
    offX = -(f32)tex3.getWidth() * 0.5f;
    offY = -(f32)tex3.getHeight() * 0.5f;
    ResTIMG* res4;
    if (field_0x28c == 5) {
        mtx[0][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[2][3] = 0.0f;
        JGeometry::TVec3f rot(0.0f, (f32)field_0x2b4 / -10430.228f, 0.0f);
        setRotScaleMtx3(mtx, rot, 1.4f * field_0x2d0.x, 1.4f * field_0x2d0.y, 1.4f * field_0x2d0.y);
        res4 = static_cast<ResTIMG*>(getBossArchiveResource_804032E8('GANO', "boss_butag_yari_mawashi_01.bti"));
    } else {
        setScaleMtx(mtx, field_0x2d0.x, field_0x2d0.y, field_0x2d0.y);
        res4 = static_cast<ResTIMG*>(getBossArchiveResource_804032E8('GANO', lbl_804A2484[field_0x288]));
    }
    params.mMtx = mtx;
    params.field_0x10.a = (u8)field_0x2b0;
    params.field_0x30 = -1;
    params.field_0x1d = 5;
    JUTTexture tex4(res4, 0);
    offX += (f32)tex4.getWidth() * 0.5f + lbl_804A2570[field_0x28c][0];
    offY += (f32)tex4.getHeight() * 0.5f + lbl_804A2570[field_0x28c][1];
    field_0x2c4.set(mPos);
    field_0x2c4.x += offX;
    field_0x2c4.y += offY;
    if (field_0x2a5 == 0 && (field_0x28c != 7 || (field_0x28c == 7 && field_0x25c < 6))) {
        drawSpriteRes_801EFD34(res4, &params, -1, offX, 0.0f, -offY);
    }
}

void Ganon_804A25F0::die_0x24() {
    startSound_801F0E34(0x40006, 0);
    startBossDeath_0x28(2, 0.0f, 0.0f);
}
