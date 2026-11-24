// Behaviour harness, game side (docs/HARNESS.md): the scene triple the sync
// points watch and the "state" dump written at each sync point. Not game code.
//
// Every value is named by its symbol and field in the decomp (never by
// address), so the Dolphin side can read the same thing through its symbol
// map. To add a field, add a row to one of the tables below: a name and a
// getter that writes one JSON value.
#include "game/ActorBase.h"
#include "game/ActorManager.h"
#include "game/Camera.h"
#include "game/GameGlobals.h"
#include "game/GameState.h"
#include "game/GameView.h"
#include "game/Player.h"

#include <stdio.h>
#include <string.h>

namespace {

// A JSON writer into a fixed buffer; output is cut (and marked) when full.
struct Json {
    char* buf;
    int size;
    int len;
    bool comma;

    void raw(const char* s) {
        int n = (int)strlen(s);
        if (len + n >= size - 2) {
            len = size; // full: harness.cpp reports the record as truncated
            return;
        }
        memcpy(buf + len, s, n);
        len += n;
        buf[len] = '\0';
    }
    void sep() {
        if (comma)
            raw(",");
        comma = false;
    }
    void key(const char* k) {
        sep();
        raw("\"");
        raw(k);
        raw("\":");
    }
    void done() { comma = true; }
    void i(s32 v) {
        sep();
        char t[16];
        snprintf(t, sizeof(t), "%d", (int)v);
        raw(t);
        done();
    }
    void u(u32 v) {
        sep();
        char t[16];
        snprintf(t, sizeof(t), "%u", (unsigned)v);
        raw(t);
        done();
    }
    void b(bool v) {
        sep();
        raw(v ? "true" : "false");
        done();
    }
    // f32 with 9 significant digits round-trips exactly; non-finite values are
    // strings (JSON has no NaN).
    void f(f32 v) {
        sep();
        char t[32];
        if (v != v)
            snprintf(t, sizeof(t), "\"nan\"");
        else if (v > 3.4e38f || v < -3.4e38f)
            snprintf(t, sizeof(t), v > 0 ? "\"inf\"" : "\"-inf\"");
        else
            snprintf(t, sizeof(t), "%.9g", (double)v);
        raw(t);
        done();
    }
    void vec(const JGeometry::TVec3f& v) {
        open('[');
        f(v.x);
        f(v.y);
        f(v.z);
        close(']');
    }
    void str(const char* s) {
        sep();
        raw("\"");
        raw(s);
        raw("\"");
        done();
    }
    void null() {
        sep();
        raw("null");
        done();
    }
    void open(char c) {
        sep();
        char t[2] = {c, 0};
        raw(t);
        comma = false;
    }
    void close(char c) {
        char t[2] = {c, 0};
        raw(t);
        done();
    }
};

// ActorId as its four characters when they are printable.
void actor_name(Json& j, u32 id) {
    char s[5];
    for (int k = 0; k < 4; k++) {
        char c = (char)(id >> (24 - 8 * k));
        s[k] = (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') ? c : '?';
    }
    s[4] = 0;
    j.str(s);
}

GameRoot_8049EF20* root() { return gGameRoot_8053AAF8; }
GameRootSub04_801F856C* stage_sub() { return root() ? root()->field_0x4 : NULL; }

// --- Globals: one row per value, "symbol->field" -------------------------------
// get() returns false when the value does not exist yet (written as null).
struct GlobalField {
    const char* name;
    bool (*get)(Json& j);
};
const GlobalField kGlobals[] = {
    {"gGameRoot_8053AAF8->field_0x4c", [](Json& j) { return root() && (j.i(root()->field_0x4c), true); }},
    {"gGameRoot_8053AAF8->field_0x50", [](Json& j) { return root() && (j.i(root()->field_0x50), true); }},
    {"gGameRoot_8053AAF8->field_0x58", [](Json& j) { return root() && (j.i(root()->field_0x58), true); }},
    {"gGameRoot_8053AAF8->field_0x60", [](Json& j) { return root() && (j.i(root()->field_0x60), true); }},
    {"gGameRoot_8053AAF8->field_0x7f", [](Json& j) { return root() && (j.i(root()->field_0x7f), true); }},
    {"gGameRoot_8053AAF8->field_0xfd", [](Json& j) { return root() && (j.i(root()->field_0xfd), true); }},
    {"gGameRoot_8053AAF8->mPlayerMask", [](Json& j) { return root() && (j.i(root()->mPlayerMask), true); }},
    // TRandom_fast_::seed is protected; it is the class's only member.
    {"gGameRoot_8053AAF8->mRandom.seed",
     [](Json& j) {
         if (!root())
             return false;
         u32 seed;
         memcpy(&seed, &root()->mRandom, sizeof(seed));
         j.u(seed);
         return true;
     }},
    {"lbl_80529DEC.mStage_0x118", [](Json& j) { return j.i(lbl_80529DEC.mStage_0x118), true; }},
    {"lbl_80529DEC.mLevel_0x11c", [](Json& j) { return j.i(lbl_80529DEC.mLevel_0x11c), true; }},
    // The stage's view (getGameView_802D6A9C), only while a stage is loaded.
    {"getGameView_802D6A9C()->mStage_0x4", [](Json& j) { return stage_sub() && (j.i(getGameView_802D6A9C()->mStage_0x4), true); }},
    {"getGameView_802D6A9C()->mLevel_0x8", [](Json& j) { return stage_sub() && (j.i(getGameView_802D6A9C()->mLevel_0x8), true); }},
    {"getGameView_802D6A9C()->mScreenX_0x14", [](Json& j) { return stage_sub() && (j.i(getGameView_802D6A9C()->mScreenX_0x14), true); }},
    {"getGameView_802D6A9C()->mScreenY_0x18", [](Json& j) { return stage_sub() && (j.i(getGameView_802D6A9C()->mScreenY_0x18), true); }},
    {"fn_80119DD0()->field_0x0", [](Json& j) { return stage_sub() && (j.i(fn_80119DD0()->field_0x0), true); }},
    {"fn_80119DD0()->field_0x4", [](Json& j) { return stage_sub() && (j.i(fn_80119DD0()->field_0x4), true); }},
    {"fn_80119DD0()->field_0x14", [](Json& j) { return stage_sub() && (j.f(fn_80119DD0()->field_0x14), true); }},
    {"fn_8011921C()->field_0x8", [](Json& j) { return stage_sub() && (j.i(fn_8011921C()->field_0x8), true); }},
    {"gGameRoot2_8053AB10->field_0x24->mViewBox_0x164",
     [](Json& j) {
         if (!stage_sub() || gGameRoot2_8053AB10 == NULL || gGameRoot2_8053AB10->field_0x24 == NULL)
             return false;
         const JGeometry::TBox2f& bx = gGameRoot2_8053AB10->field_0x24->mViewBox_0x164;
         j.open('[');
         j.f(bx.i.x);
         j.f(bx.i.y);
         j.f(bx.f.x);
         j.f(bx.f.y);
         j.close(']');
         return true;
     }},
};

// --- Actors: ActorManager_804A5800::mActors[slot], one object per live slot ----
struct ActorField {
    const char* name;
    void (*get)(Json& j, ActorBase_804A5770* a);
};
const ActorField kActorFields[] = {
    {"mSlot", [](Json& j, ActorBase_804A5770* a) { j.i(a->mSlot); }},
    {"mActorId", [](Json& j, ActorBase_804A5770* a) { actor_name(j, a->mActorId); }},
    {"mSerial", [](Json& j, ActorBase_804A5770* a) { j.i(a->mSerial); }},
    {"mAlive", [](Json& j, ActorBase_804A5770* a) { j.b(a->mAlive); }},
    {"mLayer_0x4", [](Json& j, ActorBase_804A5770* a) { j.i(a->mLayer_0x4); }},
    {"mPos", [](Json& j, ActorBase_804A5770* a) { j.vec(a->mPos); }},
    {"mDir_0x98", [](Json& j, ActorBase_804A5770* a) { j.i(a->mDir_0x98); }},
    {"mLife", [](Json& j, ActorBase_804A5770* a) { j.i(a->mLife); }},
};

// --- Players: gPlayerManager_8053AC90->mPlayers[0..3] --------------------------
struct PlayerField {
    const char* name;
    void (*get)(Json& j, Player_804A6F38* p);
};
const PlayerField kPlayerFields[] = {
    {"mPlayerNo_0x4", [](Json& j, Player_804A6F38* p) { j.i(p->mPlayerNo_0x4); }},
    {"mActive", [](Json& j, Player_804A6F38* p) { j.b(p->mActive); }},
    {"mPos", [](Json& j, Player_804A6F38* p) { j.vec(p->mPos); }},
    {"mLayer_0x3b4", [](Json& j, Player_804A6F38* p) { j.i(p->mLayer_0x3b4); }},
    {"mFacing_0x3dc", [](Json& j, Player_804A6F38* p) { j.i(p->mFacing_0x3dc); }},
    {"mState_0x1240", [](Json& j, Player_804A6F38* p) { j.i(p->mState_0x1240); }},
    {"mLife_0xbfc", [](Json& j, Player_804A6F38* p) { j.i(p->mLife_0xbfc); }},
};

} // namespace

// The scene the sync points watch: gGameRoot_8053AAF8->field_0x4c (-1 before
// the root exists), lbl_80529DEC.mStage_0x118 and .mLevel_0x11c.
extern "C" void fsa_harness_scene(s32* out) {
    out[0] = root() ? root()->field_0x4c : -1;
    out[1] = lbl_80529DEC.mStage_0x118;
    out[2] = lbl_80529DEC.mLevel_0x11c;
}

// One "state" record (no newline) into buf; returns its length, or -1 when it
// did not fit.
extern "C" int fsa_harness_state(char* buf, int size, u32 n, const char* sync) {
    Json j = {buf, size, 0, false};
    buf[0] = '\0';
    j.open('{');
    j.key("n");
    j.u(n);
    j.key("sync");
    j.str(sync);

    j.key("globals");
    j.open('{');
    for (u32 k = 0; k < sizeof(kGlobals) / sizeof(kGlobals[0]); k++) {
        j.key(kGlobals[k].name);
        if (!kGlobals[k].get(j))
            j.null();
    }
    j.close('}');

    j.key("actors");
    GameRootSub04_801F856C* sub = stage_sub();
    if (sub == NULL || sub->mActorManager == NULL) {
        j.null();
    } else {
        ActorManager_804A5800* am = sub->mActorManager;
        j.open('[');
        for (s32 i = 0; i < am->getActorEnd(); i++) {
            ActorBase_804A5770* a = am->getActor(i);
            if (a == NULL)
                continue;
            j.open('{');
            for (u32 k = 0; k < sizeof(kActorFields) / sizeof(kActorFields[0]); k++) {
                j.key(kActorFields[k].name);
                kActorFields[k].get(j, a);
            }
            j.close('}');
        }
        j.close(']');
    }

    j.key("players");
    if (gPlayerManager_8053AC90 == NULL) {
        j.null();
    } else {
        j.open('[');
        for (int p = 0; p < 4; p++) {
            Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[p];
            if (pl == NULL) {
                j.null();
                continue;
            }
            j.open('{');
            for (u32 k = 0; k < sizeof(kPlayerFields) / sizeof(kPlayerFields[0]); k++) {
                j.key(kPlayerFields[k].name);
                kPlayerFields[k].get(j, pl);
            }
            j.close('}');
        }
        j.close(']');
    }
    j.close('}');
    return j.len >= size ? -1 : j.len;
}
