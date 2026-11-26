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
#include "game/file_select_803BC6F0.h"
#include "game/cls_804AF878.h"
#include "game/adv_entry_screen_803E1668.h"

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
AdvEntryScreen_804B0B00* entry() { return lbl_8053AD98 ? lbl_8053AD98->field_0x4 : NULL; }

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
    // The file select screen (mode 3), only while it exists: its state (index
    // into lbl_804AFD70), the message shown, the message window's phase and
    // the window's cursor (0 yes, 1 no). Scripts wait on these in the menus.
    {"lbl_8053ADB0->mState", [](Json& j) { return lbl_8053ADB0 && (j.i(lbl_8053ADB0->mState), true); }},
    {"lbl_8053ADB0->field_0x7c", [](Json& j) { return lbl_8053ADB0 && (j.i(lbl_8053ADB0->field_0x7c), true); }},
    {"lbl_8053ADB0->field_0x114", [](Json& j) { return lbl_8053ADB0 && (j.i(lbl_8053ADB0->field_0x114), true); }},
    {"lbl_8053ADB0->field_0x116", [](Json& j) { return lbl_8053ADB0 && (j.i(lbl_8053ADB0->field_0x116), true); }},
    // The adventure's player-count screen (mode 4, lbl_8053AD98's field_0x4, only
    // while it exists; deluxe): its state, the cursor (players - 1) and the
    // number of controllers / GBAs it counts as connected.
    {"lbl_8053AD98->field_0x4->field_0x1d4", [](Json& j) { return entry() && (j.i(entry()->field_0x1d4), true); }},
    {"lbl_8053AD98->field_0x4->field_0x1bc", [](Json& j) { return entry() && (j.i(entry()->field_0x1bc), true); }},
    {"lbl_8053AD98->field_0x4->field_0x1d0", [](Json& j) { return entry() && (j.i(entry()->field_0x1d0), true); }},
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

// ?allocprobe=1 (platform/alloc_probe.cpp, docs/DIAGNOSTICS.md): at every sync
// point, one "heap" diagnostics record with the JKR heap tree (each heap's
// address, depth, size, total free and largest free block, preorder from the
// root heap) and the probe's counts of host allocations that went to a JKR heap.
extern "C" int fsa_alloc_probe_on;
extern "C" u32 fsa_alloc_probe_hits, fsa_alloc_probe_bytes, fsa_alloc_probe_stacks;
extern "C" void fsa_diag(const char* name, const char* json);
#include "JSystem/JKernel/JKRHeap.h"

namespace {
void heap_tree(Json& j, JKRHeap* heap, int depth, u32* totalFree) {
    char t[32];
    j.open('{');
    j.key("heap");
    snprintf(t, sizeof(t), "0x%08x", (unsigned)(u32)(size_t)heap);
    j.str(t);
    j.key("depth");
    j.i(depth);
    j.key("size");
    j.u(heap->getHeapSize());
    u32 freeTotal = (u32)heap->getTotalFreeSize();
    *totalFree += freeTotal;
    j.key("free");
    j.u(freeTotal);
    j.key("maxfree");
    j.u((u32)heap->getFreeSize());
    if (heap == JKRHeap::getCurrentHeap()) {
        j.key("current");
        j.b(true);
    }
    j.close('}');
    for (JSUTree<JKRHeap>* c = heap->getHeapTree().getFirstChild(); c != NULL; c = c->getNextChild())
        heap_tree(j, c->getObject(), depth + 1, totalFree);
}

char sHeapBuf[16384];

void heap_record(u32 n, const char* sync) {
    if (!fsa_alloc_probe_on || JKRHeap::getRootHeap() == NULL)
        return;
    Json j = {sHeapBuf, (int)sizeof(sHeapBuf), 0, false};
    sHeapBuf[0] = '\0';
    u32 totalFree = 0;
    j.open('{');
    j.key("n");
    j.u(n);
    j.key("sync");
    j.str(sync);
    j.key("heaps");
    j.open('[');
    heap_tree(j, JKRHeap::getRootHeap(), 0, &totalFree);
    j.close(']');
    j.key("free_sum");
    j.u(totalFree);
    j.key("probe");
    j.open('{');
    j.key("hits");
    j.u(fsa_alloc_probe_hits);
    j.key("bytes");
    j.u(fsa_alloc_probe_bytes);
    j.key("stacks");
    j.u(fsa_alloc_probe_stacks);
    j.close('}');
    j.close('}');
    if (j.len < (int)sizeof(sHeapBuf))
        fsa_diag("heap", sHeapBuf);
}
} // namespace

// The scene the sync points watch:gGameRoot_8053AAF8->field_0x4c (-1 before
// the root exists), lbl_80529DEC.mStage_0x118 and .mLevel_0x11c.
extern "C" void fsa_harness_scene(s32* out) {
    out[0] = root() ? root()->field_0x4c : -1;
    out[1] = lbl_80529DEC.mStage_0x118;
    out[2] = lbl_80529DEC.mLevel_0x11c;
}

// One "state" record (no newline) into buf; returns its length, or -1 when it
// did not fit.
extern "C" int fsa_harness_state(char* buf, int size, u32 n, const char* sync) {
    heap_record(n, sync);
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

// --- Script conditions (platform/harness.cpp) ------------------------------------
// The page looks names up by index (fsa_harness_global_name), so a script's
// condition keys are exactly the kGlobals names.
// The `set` script step (docs/HARNESS.md): the GameState fields the
// stage jump writes (?stage=, game_root_80134C04.cpp), by id: 1 mStage_0x118,
// 2 mLevel_0x11c, 3 field_0x128, 4 field_0x12c, 5 "seed" (GameRoot mRandom,
// at once), 6 "seed_on_stage" (when the next stage is built). Returns 0 for an
// unknown id.
extern "C" void fsa_harness_seed_on_stage(u32 seed);  // game_root_80134C04.cpp
#if FSA_DIAG
extern "C" int fsa_harness_diag_set(int id, double value);  // below
#endif
extern "C" int fsa_coop_game_request(int p, int on);  // game/src/port/coop_game.cpp
extern "C" int fsa_harness_set(int id, double value) {
    s32 v = (s32)value;
#if FSA_DIAG
    if (id >= 100) {
        return fsa_harness_diag_set(id, value);
    }
#endif
    if (id == 5 && gGameRoot_8053AAF8 == NULL) {
        return 0;
    }
    switch (id) {
    case 1: lbl_80529DEC.mStage_0x118 = v; return 1;
    case 2: lbl_80529DEC.mLevel_0x11c = v; return 1;
    case 3: lbl_80529DEC.field_0x128 = v; return 1;
    case 4: lbl_80529DEC.field_0x12c = v; return 1;
    case 5: gGameRoot_8053AAF8->mRandom.setSeed((u32)(s64)value); return 1;  // "seed", immediate
    case 6: fsa_harness_seed_on_stage((u32)(s64)value); return 1;            // "seed_on_stage"
    case 7: return fsa_coop_game_request(v, 1);                              // "coop_join" (deluxe)
    case 8: return fsa_coop_game_request(v, 0);                              // "coop_leave"
    }
    return 0;
}

extern "C" int fsa_harness_global_count() { return (int)(sizeof(kGlobals) / sizeof(kGlobals[0])); }
extern "C" const char* fsa_harness_global_name(int k) {
    return k >= 0 && k < fsa_harness_global_count() ? kGlobals[k].name : NULL;
}

// The game's libc headers (decomp) have no strtod; the host libc does.
extern "C" double strtod(const char* s, char** end);

// kGlobals[k] as a number: returns 1 and sets *out for a number or boolean
// (true = 1), 0 for null (the value does not exist yet) or a non-scalar.
extern "C" int fsa_harness_global_value(int k, double* out) {
    if (k < 0 || k >= fsa_harness_global_count())
        return 0;
    char t[128];
    Json j = {t, sizeof(t), 0, false};
    t[0] = '\0';
    if (!kGlobals[k].get(j) || j.len >= (int)sizeof(t))
        return 0;
    if (strcmp(t, "true") == 0 || strcmp(t, "false") == 0) {
        *out = t[0] == 't' ? 1 : 0;
        return 1;
    }
    char* end;
    double v = strtod(t, &end);
    if (end == t || *end != '\0')
        return 0;
    *out = v;
    return 1;
}

#if FSA_DIAG
// --- Diagnostic pokes (docs/HARNESS.md "Diagnostic pokes"), FSA_DIAG builds only -------
// `set` keys with ids >= 100 (platform/web/fsa_harness.js DIAG_SETTABLE). They force game
// states that a scripted run cannot reach (more players without GBAs, a faked GBA link, a
// hit on an actor, a code path no caller takes), so that fntrace can see the paths. Normal
// builds return 0 for them (the script's sync stream gets "set_ignored").
#include "game/GbaClient.h"
#include "game/Message.h"

extern "C" void fsa_diag(const char* name, const char* json);
extern "C" void fsa_harness_set_pads(u32 mask);  // platform/harness.cpp
extern "C" u32 fsa_harness_reads();              // platform/harness.cpp

// Bit 0: ActorBase::sweepMapBox_801F2618 passes flag = true (actor_base_801EE24C.cpp).
extern "C" {
u32 fsa_diag_force = 0;
}

namespace {
u32 sPlayersOnStage = 0;  // the players mask (bits 0-3), applied at the next stage build
s32 sKindsOnStage = -1;   // GameRoot field_0x83 then (-1: kind 2, a controller, per player)
u32 sPads = 1;            // the ports the script connects (`pads`)
u32 sGbaLink = 0;         // GBA clients forced to look linked at every pad read
u32 sB9cMask = 0;         // players whose field_0xb9c is held set at every pad read
s32 sTarget = -1;         // actor target: a slot (0..255) or an ActorId four-character code
s32 sHitType = 0;
s32 sHitStrength = 1;
s32 sHitDy = 16;          // takeHit's `from` = the actor's position + (0, dy, 0)
s32 sMsgWindow = 0;

void poke_log(const char* what, s32 a, s32 b, s32 c) {
    char line[160];
    snprintf(line, sizeof(line), "{\"n\":%u,\"poke\":\"%s\",\"a\":%d,\"b\":%d,\"c\":%d}", fsa_harness_reads(), what,
             (int)a, (int)b, (int)c);
    fsa_diag("harness_poke", line);
}

// Calls f on every live actor sTarget names; returns how many.
template <class F> int each_target(F f) {
    GameRootSub04_801F856C* sub = stage_sub();
    if (sub == NULL || sub->mActorManager == NULL || sTarget == -1) {
        return 0;
    }
    ActorManager_804A5800* am = sub->mActorManager;
    int n = 0;
    for (s32 i = 0; i < am->getActorEnd(); i++) {
        ActorBase_804A5770* a = am->getActor(i);
        if (a == NULL || !a->mAlive) {
            continue;
        }
        if (sTarget >= 0 && sTarget < 256 ? i != sTarget : a->mActorId != (u32)sTarget) {
            continue;
        }
        f(i, a);
        n++;
    }
    return n;
}

// Every message of the loaded BMG that starts with a conditional command
// (fn_80383E2C's list): index, id, command, the command's bytes.
int msg_scan() {
    if (lbl_8053AD78 == NULL || lbl_8053AD78->mBmg == NULL) {
        return 0;
    }
    MessageInf1_804AF070* inf1 = &lbl_8053AD78->mBmg->inf1;
    u32 count = inf1->count;
    int found = 0;
    for (u32 i = 0; i < count; i++) {
        u8 command;
        const char* p = fn_80383E2C(i, &command);
        if (p == NULL) {
            continue;
        }
        char hex[2 * 40 + 1];
        int len = (u8)p[1];
        if (len > 40) {
            len = 40;
        }
        for (int k = 0; k < len; k++) {
            snprintf(hex + 2 * k, 3, "%02x", (u8)p[k]);
        }
        hex[2 * len] = '\0';
        char line[200];
        snprintf(line, sizeof(line), "{\"n\":%u,\"index\":%u,\"id\":%u,\"command\":%u,\"bytes\":\"%s\"}",
                 fsa_harness_reads(), i, (unsigned)(u16)inf1->entries[i].id, command, hex);
        fsa_diag("harness_msg", line);
        found++;
    }
    poke_log("msg_scan", (s32)count, found, 0);
    return 1;
}
}  // namespace

extern "C" u32 fsa_harness_players_on_stage(u8* kinds) {
    u32 m = sPlayersOnStage;
    if (m != 0) {
        u8 k = 0;
        for (int i = 0; i < 4; i++) {
            if ((m >> i) & 1) {
                k |= (u8)(2 << (2 * i));
            }
        }
        *kinds = sKindsOnStage >= 0 ? (u8)sKindsOnStage : k;
    }
    sPlayersOnStage = 0;
    return m;
}

// The GBA-client pokes (`pads`, `gba_link`): at every pad read and again at the top of
// cls_804AA8C8::pollInputs_8030A43C (tu_803099FC.cpp, FSA_DIAG), because the client thread,
// woken by the previous poll's message, may run in between and undo them.
extern "C" void fsa_harness_diag_clients() {
    for (int i = 1; i < 4; i++) {
        // A port the script connects (`pads`) and no GBA is linked on: the client's SI type
        // a standard controller, as SIProbe gives on the console (the port's SIProbe answers
        // only for a keyboard- or gamepad-bound port), so pollInputs_8030A43C reads its pad.
        GbaClient_803007D0& c = lbl_804DB798[i];
        if (((sPads >> i) & 1) && !((sGbaLink >> i) & 1) && c.mState == 0) {
            c.mState = 0x09000000;  // SI_GC_CONTROLLER
        }
    }
    for (int i = 0; i < 4; i++) {
        if (!((sGbaLink >> i) & 1)) {
            continue;
        }
        // What cls_804AA8C8::pollInputs_8030A43C and fn_8030A308 take for a linked GBA:
        // state 0x40000, no error, linked, a key word received, the client thread idle.
        GbaClient_803007D0& c = lbl_804DB798[i];
        c.mState = 0x40000;
        c.field_0xd3 = 0;
        c.field_0xd8 = 1;
        c.field_0xd6 = 1;
        c.field_0xd1 = 0;
        c.field_0xdc = 0;
        // No key pressed; the top byte 2 acknowledges every file transfer (fn_80304FB0 waits
        // for client->mKeys >> 24 == 2 after a transfer's last words); the low byte is the
        // CRC of bytes 1 and 2.
        u32 keys = 0x02000000;
        c.mKeys = keys | fn_803006E8(((keys & 0xFF0000) >> 16) | (keys & 0xFF00));
    }
}

// Every pad read in script mode (platform/harness.cpp), after the script's step.
extern "C" void fsa_harness_diag_tick(u32 n) {
    (void)n;
    fsa_harness_diag_clients();
    if (sB9cMask != 0 && gPlayerManager_8053AC90 != NULL) {
        for (int i = 0; i < 4; i++) {
            Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[i];
            if (pl != NULL && ((sB9cMask >> i) & 1)) {
                pl->field_0xb9c = true;
            }
        }
    }
}

extern "C" int fsa_harness_diag_set(int id, double value) {
    s32 v = (s32)(s64)value;
    switch (id) {
    case 100: sPlayersOnStage = (u32)v & 0xF; break;        // players_on_stage
    case 101: sKindsOnStage = v; break;                     // kinds_on_stage (field_0x83)
    case 102: fsa_harness_set_pads((u32)v); sPads = (u32)v & 0xF; break;  // pads
    case 103: sGbaLink = (u32)v & 0xF; break;               // gba_link
    case 104: fsa_diag_force = (u32)v; break;               // force
    case 110: sTarget = v; break;                           // target
    case 111: sHitType = v; break;                          // hit_type
    case 112: sHitStrength = v; break;                      // hit_strength
    case 113: sHitDy = v; break;                            // hit_dy
    case 114: {                                             // hit (attack)
        int k = each_target([&](s32 slot, ActorBase_804A5770* a) {
            JGeometry::TVec3f from(a->mPos.x, a->mPos.y + (f32)sHitDy, a->mPos.z);
            bool r = a->takeHit_801F37AC(v, &from, sHitStrength, sHitType, 0);
            poke_log("hit", slot, (s32)a->mActorId, r);
        });
        return k > 0;
    }
    case 115: {                                             // shake: fn_801F3780(v)
        int k = each_target([&](s32 slot, ActorBase_804A5770* a) {
            a->fn_801F3780(v);
            poke_log("shake", slot, (s32)a->mActorId, v);
        });
        return k > 0;
    }
    case 116: {                                             // kill: fn_801A3C50(player v)
        int k = each_target([&](s32 slot, ActorBase_804A5770* a) {
            a->fn_801A3C50(v);
            poke_log("kill", slot, (s32)a->mActorId, v);
        });
        return k > 0;
    }
    case 117: {                                             // layer: mLayer_0x4 = v
        int k = each_target([&](s32 slot, ActorBase_804A5770* a) {
            poke_log("layer", slot, (s32)a->mActorId, a->mLayer_0x4);
            a->mLayer_0x4 = v;
        });
        return k > 0;
    }
    case 120:                                               // player_b9c (mask)
        if (gPlayerManager_8053AC90 != NULL) {
            for (int i = 0; i < 4; i++) {
                Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[i];
                if (pl != NULL && ((sB9cMask >> i) & 1) && !((v >> i) & 1)) {
                    pl->field_0xb9c = false;
                }
            }
        }
        sB9cMask = (u32)v & 0xF;
        break;
    case 130: return msg_scan();                            // msg_scan
    case 131: sMsgWindow = v; break;                        // msg_window
    case 132: {                                             // msg_eval: fn_80383EB0(window, index v)
        if (lbl_8053AD78 == NULL || lbl_8053AD78->mBmg == NULL) {
            return 0;
        }
        u32 r = fn_80383EB0(sMsgWindow, (u32)v);
        poke_log("msg_eval", sMsgWindow, v, (s32)r);
        return 1;
    }
    default: return 0;
    }
    poke_log("set", id, v, 0);
    return 1;
}
#endif  // FSA_DIAG

// --- Netplay (deluxe, docs/NETPLAY.md): which parts of MEM1 a snapshot needs ---
// The game's memory is the JKR heap tree under the root heap (JFWSystem). Of an
// expanding heap (JKRExpHeap) only its used blocks matter, plus the free-block
// header that follows each and the one at its start: free bytes are never read
// (blocks are zeroed when handed out, JKRHeap::alloc). A used block that holds a
// child heap is replaced by that heap's own parts; other heap kinds are kept
// whole. The parts are merged when less than kGap apart. Writes (start, end)
// pairs and returns their count; 0 when the tree does not look as expected (the
// caller then saves all of MEM1); *rootStart / *rootEnd = the root heap.
#include "JSystem/JKernel/JKRExpHeap.h"
namespace {
// Coverage at 16 KiB granules of the root heap's range.
const u32 kGranule = 16 * 1024;
const int kMaxGranules = 8192;  // 128 MiB
struct NetParts {
    u32 base, end;
    u8 bits[kMaxGranules / 8];
    bool bad;
    void add(u32 a, u32 b) {
        if (bad || b <= a)
            return;
        if (a < base || b > end) { bad = true; return; }
        for (u32 g = (a - base) / kGranule; g <= (b - 1 - base) / kGranule; g++)
            bits[g >> 3] |= (u8)(1 << (g & 7));
    }
};
void net_heap_parts(JKRHeap* heap, NetParts& parts, int depth) {
    u32 lo = (u32)(size_t)heap->getStartAddr(), hi = (u32)(size_t)heap->getEndAddr();
    if (depth > 16 || heap->getHeapType() != 'EXPH') {
        parts.add(lo, hi);
        return;
    }
    parts.add(lo, lo + 64 < hi ? lo + 64 : hi);
    JKRExpHeap* exp = (JKRExpHeap*)heap;
    int guard = 0;
    for (JKRExpHeap::CMemBlock* b = exp->getHeadUsedList(); b; b = b->getNextBlock()) {
        u32 bs = (u32)(size_t)b, ds = bs + (u32)sizeof(JKRExpHeap::CMemBlock), de = ds + b->getSize();
        if (bs < lo || de > hi || ++guard > 1000000) { parts.bad = true; return; }
        JKRHeap* child = NULL;
        for (JSUTree<JKRHeap>* t = heap->getHeapTree().getFirstChild(); t; t = t->getNextChild()) {
            JKRHeap* c = t->getObject();
            u32 cs = (u32)(size_t)c->getStartAddr(), ce = (u32)(size_t)c->getEndAddr();
            if (cs >= ds && ce <= de) { child = c; break; }
        }
        if (child) {
            // The heap object and its header sit at the block's start.
            u32 cs = (u32)(size_t)child->getStartAddr();
            parts.add(bs, cs);
            net_heap_parts(child, parts, depth + 1);
        } else {
            parts.add(bs, de + 64 < hi ? de + 64 : hi);
        }
    }
}
}  // namespace
static NetParts sNetParts;
extern "C" int fsa_net_heap_ranges(u32* out, int max, u32* rootStart, u32* rootEnd) {
    JKRHeap* root = JKRHeap::getRootHeap();
    if (!root)
        return 0;
    NetParts& parts = sNetParts;
    parts.base = (u32)(size_t)root->getStartAddr();
    parts.end = (u32)(size_t)root->getEndAddr();
    parts.bad = parts.end - parts.base > (u32)kMaxGranules * kGranule;
    memset(parts.bits, 0, sizeof(parts.bits));
    net_heap_parts(root, parts, 0);
    if (parts.bad)
        return 0;
    // Runs of covered granules; gaps of up to 4 granules are bridged.
    int k = 0;
    u32 n = (parts.end - parts.base + kGranule - 1) / kGranule;
    u32 g = 0;
    while (g < n) {
        if (!(parts.bits[g >> 3] & (1 << (g & 7)))) { g++; continue; }
        u32 h = g;
        for (u32 x = g + 1; x < n && x - h <= 5; x++)
            if (parts.bits[x >> 3] & (1 << (x & 7)))
                h = x;
        if (k >= max)
            return 0;
        out[2 * k] = parts.base + g * kGranule;
        u32 e = parts.base + (h + 1) * kGranule;
        out[2 * k + 1] = e < parts.end ? e : parts.end;
        k++;
        g = h + 1;
    }
    *rootStart = parts.base;
    *rootEnd = parts.end;
    return k;
}

// Debugging (netplay): the JKR heap tree, one OSReport line per heap and per
// used block of 64 KiB or more.
namespace {
void net_heap_dump(JKRHeap* heap, int depth) {
    u32 t = heap->getHeapType();
    OSReport("[netheap] %*s%c%c%c%c %p-%p\n", depth * 2, "", (char)(t >> 24), (char)(t >> 16), (char)(t >> 8), (char)t,
             heap->getStartAddr(), heap->getEndAddr());
    if (t == 'EXPH') {
        int n = 0;
        for (JKRExpHeap::CMemBlock* b = ((JKRExpHeap*)heap)->getHeadUsedList(); b && n < 100000; b = b->getNextBlock(), n++)
            if (b->getSize() >= 0x10000)
                OSReport("[netheap] %*s  used %p +0x%x\n", depth * 2, "", b, b->getSize());
        OSReport("[netheap] %*s  %d used blocks\n", depth * 2, "", n);
    }
    for (JSUTree<JKRHeap>* c = heap->getHeapTree().getFirstChild(); c; c = c->getNextChild())
        net_heap_dump(c->getObject(), depth + 1);
}
}  // namespace
extern "C" void fsa_net_heap_dump() {
    if (JKRHeap::getRootHeap())
        net_heap_dump(JKRHeap::getRootHeap(), 0);
}
