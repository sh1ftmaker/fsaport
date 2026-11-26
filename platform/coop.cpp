// Local co-op on one screen (deluxe; docs/MULTIPLAYER.md "Local co-op"), the
// game's half. The page's device core (platform/web/fsa_coop.js) decides which
// device drives which player; this file
//
//   - replaces the four PADStatus of every pad read with the page's four pads
//     while co-op is on (fsa_coop_enable), before the harness and netplay see
//     them, so a device moving between players changes only which input
//     drives which port;
//   - applies join / leave requests at a pad read (fsa_coop_request, or a
//     harness script's "set": {"coop_join": P} / {"coop_leave": P}), see
//     coop_apply below.
//
// With co-op off (the default until the page turns it on; always in harness
// script and replay runs) nothing here touches the game.
#include "platform.h"
#include "gba_link.h"

#include <dolphin/pad.h>
#include <emscripten.h>

#include <cstring>

namespace {
bool g_on = false;
PADStatus g_pads[PAD_CHANMAX];
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE void fsa_coop_enable(int on) {
  g_on = on != 0;
  platform_log("[coop] %s\n", g_on ? "on: the page routes devices to players" : "off");
}

int fsa_coop_on() { return g_on ? 1 : 0; }

// game/src/port/coop_game.cpp
int fsa_coop_game_request(int p, int on);
const char* fsa_coop_game_why();
u32 fsa_coop_game_state();
void fsa_coop_game_frame();

// The page: player p (1-3) joins (on) or leaves; applied at the next pad read.
// 0 = refused, the reason in fsa_coop_why().
EMSCRIPTEN_KEEPALIVE int fsa_coop_request(int p, int on) { return fsa_coop_game_request(p, on); }
EMSCRIPTEN_KEEPALIVE const char* fsa_coop_why() { return fsa_coop_game_why(); }
// Bits 0-3 players in the game, 8-11 wanted, 16-19 GBAs still linking.
EMSCRIPTEN_KEEPALIVE u32 fsa_coop_state() { return fsa_coop_game_state(); }

// One player's pad from the page: PAD_BUTTON_* bits, sticks -127..127 (up
// positive), analog triggers 0..255; connected = 0 for a port with nobody.
EMSCRIPTEN_KEEPALIVE void fsa_coop_pad(int port, int connected, int buttons, int sx, int sy, int cx, int cy, int tl,
                                       int tr) {
  if (port < 0 || port >= PAD_CHANMAX)
    return;
  PADStatus& p = g_pads[port];
  memset(&p, 0, sizeof(p));
  p.err = connected ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
  if (!connected)
    return;
  auto c8 = [](int v) { return (s8)(v < -127 ? -127 : v > 127 ? 127 : v); };
  auto u8c = [](int v) { return (u8)(v < 0 ? 0 : v > 255 ? 255 : v); };
  p.button = (u16)buttons;
  p.stickX = c8(sx);
  p.stickY = c8(sy);
  p.substickX = c8(cx);
  p.substickY = c8(cy);
  p.triggerLeft = u8c(tl);
  p.triggerRight = u8c(tr);
}

// fsa_pad_after_read, before the harness: the page's four pads replace what
// Aurora read (its keyboard and SDL gamepads, which the page now routes).
void platform_coop_pads(PADStatus* status) {
  if (!g_on)
    return;
  memcpy(status, g_pads, sizeof(g_pads));
}

// fsa_pad_after_read, after the harness: joins and leaves (coop_game.cpp).
void platform_coop_frame() { fsa_coop_game_frame(); }

}  // extern "C"
