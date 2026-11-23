// Game functions the port links that no compiled unit defines: written here from
// the original's asm, for behaviour (not matching).
#include "game/GameGlobals.h"
#include "game/escort_base_801213EC.h"
#include "JSystem/JGeometry.h"

// JMath's fast vector magnitude (0x80093280, unnamed upstream, see JMath.cpp): the
// asm computes sq = |v|^2 and returns sq == 0 ? sq : sq * frsqrte(sq), i.e. |v|.
f32 fn_80093280(const JGeometry::TVec3f* v) {
    f32 sq = v->x * v->x + v->y * v->y + v->z * v->z;
    if (sq == 0.0f) {
        return sq;
    }
    return sq * (1.0f / sqrtf(sq));
}

// EscortBase's vtable slots 0xA0 and 0xA4 hold fn_8012309C and fn_80122F6C
// (__vt__19EscortBase_8049E0C8 in the original); the header declares both the
// virtuals and the plain functions, so the virtuals forward.
void EscortBase_8049E0C8::vfunc_0xA0(u16* outAngle, s32* outDir, f32* outDist, s32* outPlayer, s32 player) {
    fn_8012309C(outAngle, outDir, outDist, outPlayer, player);
}

void EscortBase_8049E0C8::vfunc_0xA4(f32* x, f32* y, s32* player) {
    fn_80122F6C(x, y, player);
}

// The save region that survives a reset (0x80700000 on the GameCube). A reload
// of the page is a power cycle, so it starts zeroed like a cold boot.
u8 fsa_save_region[0x20];
