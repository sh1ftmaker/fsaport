// Input the page supplies on top of Aurora's (keyboard, gamepads): the
// on-screen touch controller (platform/web/touch.js).
//
// JUTGamePad::read calls fsa_pad_after_read right after PADRead and before
// the game clamps the sticks, so the touch state is merged into exactly what
// a real controller in port 0 would have reported. Buttons are ORed in, so a
// keyboard and the touch pads can be used at once; the stick and the analog
// triggers take whichever source is pushed further.
#include "platform.h"

#include <dolphin/pad.h>
#include <emscripten.h>

#include <cstdlib>

namespace {
struct TouchPad {
  u16 buttons = 0;
  s8 stickX = 0;
  s8 stickY = 0;
  u8 triggerL = 0;
  u8 triggerR = 0;
} g_touch;

s8 further(s8 a, s8 b) { return std::abs(b) > std::abs(a) ? b : a; }
s8 clampStick(int v) { return (s8)(v < -127 ? -127 : v > 127 ? 127 : v); }
u8 clampTrigger(int v) { return (u8)(v < 0 ? 0 : v > 255 ? 255 : v); }
}  // namespace

// From the page: the PAD_BUTTON_* / PAD_TRIGGER_* bits held, the main stick
// (-127..127, up positive) and the analog triggers (0..255).
extern "C" EMSCRIPTEN_KEEPALIVE void fsa_touch_set(int buttons, int stickX, int stickY, int triggerL, int triggerR) {
  g_touch.buttons = (u16)buttons;
  g_touch.stickX = clampStick(stickX);
  g_touch.stickY = clampStick(stickY);
  g_touch.triggerL = clampTrigger(triggerL);
  g_touch.triggerR = clampTrigger(triggerR);
}

extern "C" void fsa_pad_after_read(PADStatus* status) {
  PADStatus& p = status[0];
  if (p.err == PAD_ERR_NONE) {
    p.button |= g_touch.buttons;
    p.stickX = further(p.stickX, g_touch.stickX);
    p.stickY = further(p.stickY, g_touch.stickY);
    if (g_touch.triggerL > p.triggerLeft)
      p.triggerLeft = g_touch.triggerL;
    if (g_touch.triggerR > p.triggerRight)
      p.triggerRight = g_touch.triggerR;
  }

  // Port 0's buttons in the log whenever they change, so input problems show.
  // The stick counts as changed when it crosses into another of nine zones
  // (centre and eight directions), not on every unit of travel.
  static u16 last = 0;
  static s8 lastErr = 99;
  static int lastZone = 0;
  static u32 reads = 0;
  reads++;
  int zone = (p.stickX > 40) - (p.stickX < -40) + 3 * ((p.stickY > 40) - (p.stickY < -40));
  if (p.button != last || p.err != lastErr || zone != lastZone) {
    platform_log("[pad] read %u: port0 err %d buttons %04x stick %d,%d\n", reads, p.err, p.button, p.stickX, p.stickY);
    last = p.button;
    lastErr = p.err;
    lastZone = zone;
  }
}
