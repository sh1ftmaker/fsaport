// Deluxe: drop-in / drop-out local co-op (docs/MULTIPLAYER.md "Local co-op"),
// the part that touches the game. Port code only; no game unit is changed.
//
// The game takes its players once, on the player-count screen
// (AdvEntryScreen_804B0B00): mPlayerMask bits 0-3 = player i takes part, bits
// 4-7 = its controller is connected, field_0x83 = each player's controller
// kind (1 GBA, 2 GameCube pad), field_0x7e = 0 for single player. Nothing in
// the game changes them during a stage. In an adventure stage all four Links
// exist and are active whatever the count (Player::resetSlot_80254744): the
// ones of absent players follow a present one (mLeader_0xd60) on its pad
// (mPadPlayer_0x11ec). So a join is not a spawn: the joining player's Link is
// already standing there, following someone, and is handed its own pad; a
// leave hands the Link back to a present player to follow.
//
// What happens, at a pad read (fsa_coop_game_frame, called by platform/coop.cpp
// after the harness, so script steps and live requests land at the same
// point):
//
//   join p   plug a GBA endpoint into port p (platform/gba_link.cpp, the local
//            peer: its keys are PADStatus[p], whatever device the page routes
//            there), and into port 0 if player 1 is on a GameCube pad (a
//            multiplayer player is a GBA, as in the game's own menus; it is
//            also what gives each player a GBA view in houses and caves).
//            The game's own client threads boot and link them. Once linked
//            (and in an adventure stage), p joins: mPlayerMask |= 0x11 << p,
//            kind 1, field_0x7e = 1, Link p leads itself on pad p (a Link the
//            stage switched off gets its slot's stage-start reset), the HUD
//            is rebuilt for the new players, and once the link code has reset
//            the new GBA's transfers the stage's GBA files are queued for it
//            (fn_80303870, as the stage entry does). Outside a stage only the
//            GBAs are plugged: the player-count screen then offers them.
//   leave p  kind 0 first (a kind-0 player is never "disconnected": the
//            disconnect check, GameRoot update, skips it), the mask bits off,
//            the GBA unplugged, and Link p follows a present player again.
//            With player 1 alone the game goes back to single-player rules
//            (field_0x7e = 0: all Links on pad 0, following Link 0).
//
// While a port is between a pad and a GBA (the boot and handshake take a few
// frames), the game's "controller / GBA disconnected" check would see a stale
// player and open its window, so the input object's freshness counters of the
// ports in transition are held at 0 until the GBA is linked.
//
// After a stage rebuild (Player::resetSlot_80254744 assumes players
// 0..count-1) the Links' roles are checked each read while co-op has changed
// anything, and fixed where a Link follows nobody present.
//
// Nothing here runs until a join is requested, so games without it are
// unchanged.
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/GbaClient.h"
#include "game/Player.h"
#include "game/PlayerBit.h"
#include "game/cls_804B13D4.h"
#include "JSystem/J2D/J2DScreen.h"
#include "game/GameState.h"
#include <stdio.h>
#include <string.h>

extern "C" {
int FsaGbaAttach(int chan, void* peer);  // platform/gba_link.cpp
int FsaGbaDetach(int chan);
int FsaGbaPresent(int chan);
void fsa_diag(const char* name, const char* json);
u32 fsa_harness_reads();
}

namespace {
u8 sWant = 1;        // players wanted (bit 0 always)
u8 sLeaving = 0;     // players asked to leave, not yet out of the mask
bool sEngaged = false;
u32 sWaitSince[4];   // read at which a port's GBA was plugged
char sWhy[96] = "";
u8 sLastMask = 0, sLastKinds = 0, sLast7e = 0xFF;

void note(const char* what, s32 p) {
    char line[200];
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    snprintf(line, sizeof(line), "{\"n\":%u,\"coop\":\"%s\",\"player\":%d,\"want\":%u,\"mask\":%u,\"kinds\":%u,\"multi\":%d}",
             fsa_harness_reads(), what, (int)p, (unsigned)sWant, r ? (unsigned)r->mPlayerMask : 0u,
             r ? (unsigned)r->field_0x83 : 0u, r ? (int)r->field_0x7e : -1);
    fsa_diag("coop", line);
    OSReport("[coop] %s player %d (want %x mask %02x kinds %02x multi %d)\n", what, (int)p, sWant,
             r ? r->mPlayerMask : 0, r ? r->field_0x83 : 0, r ? r->field_0x7e : -1);
}

u8 kind_of(s32 p) { return (gGameRoot_8053AAF8->field_0x83 >> (2 * p)) & 3; }
void set_kind(s32 p, u8 k) {
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    r->field_0x83 = (u8)((r->field_0x83 & ~(3 << (2 * p))) | (k << (2 * p)));
}

// The input object's view of port p: a GBA, linked (cls_804AA8C8::fn_8030A308).
bool linked(s32 p) { return lbl_8053AD30 != NULL && lbl_8053AD30->fn_8030A308(p); }

// An adventure stage running: the stage mode, not loading, adventure rules
// (field_0x7f).
bool in_stage() {
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    return r != NULL && gPlayerManager_8053AC90 != NULL && r->field_0x4c == 7 && r->field_0xfd == 0 &&
           r->field_0x7f != 0;
}

bool takes_part(s32 p) { return isPlayerBitSet(gGameRoot_8053AAF8, p) != 0; }

// The Links' roles from the mask: a present player leads itself on its own
// pad; an absent one follows a present player on that player's pad (the one
// with the fewest followers; player k - 2 first, as the 2-player rule does).
// Single player (field_0x7e 0): all on pad 0, Links 1-3 following Link 0.
// Only fields that are wrong are written.
void fix_roles() {
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    s32 followers[4] = {0, 0, 0, 0};
    for (s32 k = 0; k < 4; k++) {
        Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[k];
        if (pl != NULL && pl->mActive && !takes_part(k) && pl->mLeader_0xd60 >= 0 && pl->mLeader_0xd60 < 4 &&
            takes_part(pl->mLeader_0xd60))
            followers[pl->mLeader_0xd60]++;
    }
    for (s32 k = 0; k < 4; k++) {
        Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[k];
        if (pl == NULL || !pl->mActive)
            continue;
        s32 leader, pad;
        if (r->field_0x7e == 0) {
            leader = k == 0 ? -1 : 0;
            pad = 0;
        } else if (takes_part(k)) {
            leader = -1;
            pad = k;
        } else {
            leader = pl->mLeader_0xd60;
            if (leader < 0 || leader >= 4 || !takes_part(leader)) {
                leader = -1;
                if (k >= 2 && takes_part(k - 2))
                    leader = k - 2;
                for (s32 c = 0; c < 4 && leader < 0; c++)
                    if (takes_part(c) && (leader < 0 || followers[c] < followers[leader]))
                        leader = c;
                if (leader < 0)
                    leader = 0;
                followers[leader]++;
            }
            pad = leader;
        }
        if (pl->mLeader_0xd60 != leader || pl->mPadPlayer_0x11ec != pad) {
            char what[48];
            snprintf(what, sizeof(what), "link%d_leader%d_pad%d", (int)k, (int)leader, (int)pad);
            pl->mLeader_0xd60 = leader;
            pl->mPadPlayer_0x11ec = pad;
            note(what, k);
        }
    }
}

// The adventure HUD shows one player's corner per player taking part
// (portrait, force gem counter), or single player's layout; it is set up when
// the stage starts (AdventureHud_804B13D4: the constructor picks the panes'
// pictures by field_0x7e, fn_8041FC2C shows each present player's corner, as
// HudMgr_804B1920::fn_8043EDF8 calls it at a stage start). After a change of
// players do the same: a new HUD object (a reset alone hides the corners of the
// players already there), then its stage-start reset, which also brings up the stage's title banner
// (HudMgr fn_8043A608); its state is put back as it was.
void hud_refresh(bool rebuild) {
    HudMgr_804B1920* hm = lbl_8053AED0;
    if (hm == NULL || hm->field_0x290 == NULL || gGameRoot_8053AAF8->field_0x7f == 0)
        return;
    J2DPane* banner = hm->field_0x8 != NULL ? hm->field_0x8->search(FSA_TAG64("Pw0101")) : NULL;
    J2DPane* plate = hm->field_0x8 != NULL ? hm->field_0x8->search(FSA_TAG64("Pdf01")) : NULL;
    bool bannerShown = banner != NULL && banner->isVisible();
    bool plateShown = plate != NULL && plate->isVisible();
    s16 s106 = hm->field_0x106;
    u8 s10d = hm->field_0x10d;
    if (rebuild) {
        delete hm->field_0x290;
        hm->field_0x290 = new AdventureHud_804B13D4;
    }
    hm->field_0x290->fn_8041FC2C();
    if (banner != NULL) {
        if (bannerShown)
            banner->show();
        else
            banner->hide();
    }
    if (plate != NULL) {
        if (plateShown)
            plate->show();
        else
            plate->hide();
    }
    hm->field_0x106 = s106;
    hm->field_0x10d = s10d;
    note(rebuild ? "hud_rebuilt" : "hud_reset", -1);
}

void plug(s32 p) {
    if (FsaGbaPresent(p))
        return;
    FsaGbaAttach(p, NULL);
    sWaitSince[p] = fsa_harness_reads();
    // A port the game knows as a GameCube pad is probed again only when the
    // pad stops answering; make the client probe now.
    if (lbl_804DB798[p].mState != 0x40000)
        lbl_804DB798[p].mState = 0;
    note("plug", p);
}

void join_stage(s32 p) {
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    if (kind_of(0) != 1) {  // player 1 now a GBA too (the GBA was linked first)
        set_kind(0, 1);
        r->mPlayerMask |= 0x11;
    }
    r->mPlayerMask |= (u8)(0x11 << p);
    set_kind(p, 1);
    r->field_0x7e = 1;
    // In some stages (fn_802DE814: the X-2 levels, 7-1) an absent player's Link
    // is switched off (Player::mActive clear). Give it its slot's reset, as a
    // stage start does for a player who takes part: active, on its own pad,
    // at the stage's start position.
    Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[p];
    if (pl != NULL && !pl->mActive) {
        pl->resetSlot_80254744();
        note("activate", p);
    }
    note("join", p);
    fix_roles();
    hud_refresh(true);
}

void leave_stage(s32 p) {
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    set_kind(p, 0);
    r->mPlayerMask &= (u8)~(0x11 << p);
    sLeaving &= (u8)~(1 << p);
    if ((r->mPlayerMask & 0xE) == 0)
        r->field_0x7e = 0;  // player 1 alone: single-player rules
    // Where absent players' Links are switched off (fn_802DE814), switch it
    // off the way a stage start does.
    Player_804A6F38* pl = gPlayerManager_8053AC90->mPlayers[p];
    if (pl != NULL && fn_802DE814()) {
        pl->resetSlot_80254744();
        note("deactivate", p);
    }
    note("leave", p);
    fix_roles();
    hud_refresh(true);
}
}  // namespace

extern "C" {

// From the page or a script: player p (1-3) wants to play (on) or leaves.
// Returns 0 with a reason (fsa_coop_why) when refused.
int fsa_coop_game_request(int p, int on) {
    if (p < 1 || p > 3) {
        snprintf(sWhy, sizeof(sWhy), "player %d cannot %s", p + 1, on ? "join" : "leave");
        return 0;
    }
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    if (on && r != NULL && r->field_0x7f == 0 && r->field_0x4c >= 6) {
        snprintf(sWhy, sizeof(sWhy), "not in battle / Tetra's Trackers games (players are fixed there)");
        return 0;
    }
    sEngaged = true;
    if (on) {
        sWant |= (u8)(1 << p);
        sLeaving &= (u8)~(1 << p);
    } else {
        sWant &= (u8)~(1 << p);
        sLeaving |= (u8)(1 << p);
    }
    note(on ? "request_join" : "request_leave", p);
    sWhy[0] = 0;
    return 1;
}

const char* fsa_coop_game_why() { return sWhy; }

// Bits 0-3: players taking part in the running game (the mask, or what is
// wanted outside a stage); bits 8-11: players wanted; bits 16-19: ports whose
// GBA is plugged but not linked yet.
u32 fsa_coop_game_state() {
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    u32 playing = r != NULL && r->field_0x4c == 7 ? (u32)(r->mPlayerMask & 0xF) : sWant;
    u32 waiting = 0;
    for (s32 p = 0; p < 4; p++)
        if (FsaGbaPresent(p) && !(lbl_8053AD30 != NULL && lbl_8053AD30->fn_8030A308(p)))
            waiting |= 1u << p;
    return playing | (u32)sWant << 8 | waiting << 16;
}

void fsa_coop_game_frame() {
    if (!sEngaged || gGameRoot_8053AAF8 == NULL || lbl_8053AD30 == NULL)
        return;
    GameRoot_8049EF20* r = gGameRoot_8053AAF8;
    bool stage = in_stage();
    // Players the game took (the menu's count, a stage rebuild) are players.
    if (stage)
        sWant |= (u8)(r->mPlayerMask & 0xF & ~sLeaving);
    bool needP1 = (sWant & 0xE) != 0;
    // Plug what is wanted; hold the freshness of ports in transition.
    for (s32 p = 0; p < 4; p++) {
        bool want = p == 0 ? needP1 : ((sWant >> p) & 1) != 0;
        if (want)
            plug(p);
        // (Player 1 on a pad while its port is already a GBA: the pad path is
        // no longer read, so its pad freshness is held too until it joins as a GBA.)
        if (FsaGbaPresent(p) && !linked(p) && (fsa_harness_reads() - sWaitSince[p]) % 60 == 59) {
            GbaClient_803007D0& c = lbl_804DB798[p];
            OSReport("[coop] port %d linking: state %x d3 %d d8 %d dc %d d1 %d | in c4 %x d4 %d d8 %d\n", (int)p,
                     c.mState, c.field_0xd3, c.field_0xd8, c.field_0xdc, c.field_0xd1, lbl_8053AD30->field_0xc4[p],
                     lbl_8053AD30->field_0xd4[p], lbl_8053AD30->field_0xd8[p]);
        }
        if (FsaGbaPresent(p) && (!linked(p) || (takes_part(p) && kind_of(p) == 2))) {
            lbl_8053AD30->field_0xdc[p] = 0;
            lbl_8053AD30->field_0xe0[p] = 0;
        }
    }
    if (stage) {
        for (s32 p = 1; p < 4; p++) {
            bool want = ((sWant >> p) & 1) != 0;
            if (want && !takes_part(p) && linked(p) && linked(0))
                join_stage(p);
            else if (!want && takes_part(p))
                leave_stage(p);
        }
        fix_roles();
        // A GBA that (re)booted during the stage: the link code reset its
        // transfer state and queued the GBA program's files (fn_80303CE4, once
        // the player takes part); the stage's own files and the "stage files
        // sent" flag (lbl_8053ACE4, which the disconnect window waits for) are
        // only queued at stage entry. Queue them as the stage entry does.
        for (s32 p = 0; p < 4; p++) {
            if (takes_part(p) && kind_of(p) == 1 && linked(p) && lbl_804DB798[p].field_0xdc == 0 &&
                lbl_8053ACE4[p] == 0) {
                fn_80303870(p);
                note("stage_files", p);
            }
        }
    }
    // Unplug the GBAs of players who left (after their kind was cleared).
    for (s32 p = 1; p < 4; p++) {
        if (!((sWant >> p) & 1) && FsaGbaPresent(p) && !(stage ? takes_part(p) : false)) {
            FsaGbaDetach(p);
            note("unplug", p);
        }
    }
    if (r->mPlayerMask != sLastMask || r->field_0x83 != sLastKinds || r->field_0x7e != sLast7e) {
        sLastMask = r->mPlayerMask;
        sLastKinds = r->field_0x83;
        sLast7e = r->field_0x7e;
        note("state", -1);
    }
}

// Testing the rumble path (tools/web_pads_test.mjs): the game's own rumble
// request, as a hit or a landing makes it (GameRoot startRumble_80134DE4;
// kind 1 = 30 frames on port 0, run by the frame update fn_80137300 through
// JUTGamePad::CRumble and PADControlMotor). Only the page calls it, on demand.
// Returns the game's rumble option (lbl_80529DEC.field_0x246; 0 = off, no
// rumble), or -1 before the game root exists.
__attribute__((used, visibility("default"))) int fsa_debug_rumble(int kind) {  // EMSCRIPTEN_KEEPALIVE
    if (gGameRoot_8053AAF8 == NULL) {
        return -1;
    }
    gGameRoot_8053AAF8->startRumble_80134DE4(kind, 0);
    return lbl_80529DEC.field_0x246;
}

}  // extern "C"
