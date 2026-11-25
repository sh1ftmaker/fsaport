// Game functions the port links that no compiled unit defines: written here from
// the original's asm, for behaviour (not matching).
#include "game/GameGlobals.h"
#include "game/escort_base_801213EC.h"
#include "JSystem/JGeometry.h"

// JMath's fast vector magnitude (0x80093280, unnamed upstream, see JMath.cpp): the
// asm computes sq = |v|^2 and returns sq == 0 ? sq : sq * frsqrte(sq), i.e. |v|.
f32 fn_80093280(const JGeometry::TVec3f* v) {
    f32 sq = (v->z * v->z + v->x * v->x) + v->y * v->y; // ps_madd then ps_sum0, as the asm adds
    if (sq == 0.0f) {
        return sq;
    }
    return sq * (1.0f / sqrtf(sq));
}

// The save region that survives a reset (0x80700000 on the GameCube). A reload
// of the page is a power cycle, so it starts zeroed like a cold boot.
u8 fsa_save_region[0x20];

#ifdef FSA_PORT_TRACE
// Debug: draw J2DPictureEx panes as plain white quads (set from the page:
// Module._fsa_set_debug_white_panes(1)).
int fsa_debug_white_panes = 0;
#endif
