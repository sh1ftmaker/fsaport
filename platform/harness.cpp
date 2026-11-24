// Behaviour harness, port side (docs/HARNESS.md): input recording and replay
// indexed by the game's pad reads, sync points and state dumps, and the
// lockstep mode that makes a run repeatable.
//
// Hook: fsa_pad_after_read (platform/input.cpp), which JUTGamePad::read calls
// right after PADRead and before PADClamp. JUTGamePad::read is the game's only
// PADRead caller outside the crash screen, and the main loop
// (tu_801344C0.cpp) calls it once per iteration, so read n is main-loop frame
// n. At read n, before input is applied:
//   1. sync points: "scene" when (mode, stage, level) changed since read n-1,
//      "tick" when n % 60 == 0; each writes a "sync" line and a "state" dump
//      (the game state after it handled reads 0..n-1);
//   2. replay replaces the four PADStatus with line n of the inputs file;
//      record writes them (as the game sees them, pre-clamp) to "inputs".
#include "platform.h"

#include <dolphin/pad.h>
#include <emscripten.h>

#include <cstdio>
#include <cstring>

extern "C" void fsa_diag(const char* name, const char* json);
extern "C" void fsa_harness_scene(s32* out);                                    // game/src/port/harness_state.cpp
extern "C" int fsa_harness_state(char* buf, int size, u32 n, const char* sync);  // game/src/port/harness_state.cpp
extern "C" OSTime (*aurora_os_time_hook)(void);                                  // extern/aurora/lib/dolphin/os/OSTime.cpp

namespace {
enum : int { kLockstep = 1, kRecord = 2, kReplay = 4 };
int g_flags = 0;
u32 g_reads = 0;

// Replay input: 8 s16 per pad (connected, button, stickX, stickY, substickX,
// substickY, triggerL, triggerR), 4 pads per read.
const s16* g_inputs = nullptr;
u32 g_inputCount = 0;
bool g_endReported = false;

s32 g_scene[3];
bool g_haveScene = false;

// Virtual time: 2026-01-01 00:00:00 (fixed, so dates the game writes do not
// depend on the day) plus one NTSC field (1001/60000 s) per VI retrace.
// (OS_TIMER_CLOCK reads __OSBusClock, set by OSInit: 40.5 MHz.)
OSTime virtual_time() {
  const OSTime clock = (OSTime)OS_TIMER_CLOCK;
  const OSTime epoch = (OSTime)9497 * 86400 * clock;  // days 2000-01-01 .. 2026-01-01
  return epoch + (OSTime)platform_retrace_count() * clock * 1001 / 60000;
}

char g_state[256 * 1024];

void emit_sync(const char* kind, u32 n) {
  char line[160];
  if (strcmp(kind, "scene") == 0)
    snprintf(line, sizeof(line), "{\"sync\":\"scene\",\"n\":%u,\"mode\":%d,\"stage\":%d,\"level\":%d}", n, (int)g_scene[0],
             (int)g_scene[1], (int)g_scene[2]);
  else
    snprintf(line, sizeof(line), "{\"sync\":\"%s\",\"n\":%u}", kind, n);
  fsa_diag("sync", line);
}

void emit_state(const char* kind, u32 n) {
  HostAllocScope host;
  int len = fsa_harness_state(g_state, sizeof(g_state), n, kind);
  if (len < 0) {
    char line[96];
    snprintf(line, sizeof(line), "{\"n\":%u,\"sync\":\"%s\",\"truncated\":true}", n, kind);
    fsa_diag("state", line);
    return;
  }
  fsa_diag("state", g_state);
}

void sync_points(u32 n) {
  s32 scene[3];
  fsa_harness_scene(scene);
  bool changed = !g_haveScene || memcmp(scene, g_scene, sizeof(scene)) != 0;
  if (changed) {
    memcpy(g_scene, scene, sizeof(scene));
    g_haveScene = true;
    emit_sync("scene", n);
  }
  bool tick = n % 60 == 0;
  if (tick)
    emit_sync("tick", n);
  if (changed || tick)
    emit_state(changed ? "scene" : "tick", n);
}

void replay(PADStatus* status, u32 n) {
  for (int i = 0; i < PAD_CHANMAX; i++) {
    PADStatus& p = status[i];
    memset(&p, 0, sizeof(p));
    if (n >= g_inputCount) {
      // After the file: port 0 connected and neutral, the others unplugged.
      p.err = i == 0 ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
      continue;
    }
    const s16* v = g_inputs + ((size_t)n * PAD_CHANMAX + i) * 8;
    if (!v[0]) {
      p.err = PAD_ERR_NO_CONTROLLER;
      continue;
    }
    p.err = PAD_ERR_NONE;
    p.button = (u16)v[1];
    p.stickX = (s8)v[2];
    p.stickY = (s8)v[3];
    p.substickX = (s8)v[4];
    p.substickY = (s8)v[5];
    p.triggerLeft = (u8)v[6];
    p.triggerRight = (u8)v[7];
  }
  if (n >= g_inputCount && !g_endReported) {
    g_endReported = true;
    emit_sync("inputs_end", n);
  }
}

void record(const PADStatus* status, u32 n) {
  char line[256];
  int len = snprintf(line, sizeof(line), "{\"n\":%u,\"pads\":[", n);
  for (int i = 0; i < PAD_CHANMAX; i++) {
    const PADStatus& p = status[i];
    if (p.err == PAD_ERR_NO_CONTROLLER)
      len += snprintf(line + len, sizeof(line) - len, "%snull", i ? "," : "");
    else
      len += snprintf(line + len, sizeof(line) - len, "%s[%u,%d,%d,%d,%d,%u,%u]", i ? "," : "", (unsigned)p.button,
                      (int)p.stickX, (int)p.stickY, (int)p.substickX, (int)p.substickY, (unsigned)p.triggerLeft,
                      (unsigned)p.triggerRight);
  }
  snprintf(line + len, sizeof(line) - len, "]}");
  fsa_diag("inputs", line);
}
}  // namespace

bool platform_lockstep() { return (g_flags & kLockstep) != 0; }

// Called by fsa_pad_after_read after the touch pad is merged in.
void platform_harness_pad(PADStatus* status) {
  u32 n = g_reads++;
  if (!g_flags)
    return;
  sync_points(n);
  if (g_flags & kReplay)
    replay(status, n);
  if (g_flags & kRecord)
    record(status, n);
}

extern "C" {

// From the page before main(): kLockstep | kRecord | kReplay.
EMSCRIPTEN_KEEPALIVE void fsa_harness_config(int flags) {
  g_flags = flags;
  aurora_os_time_hook = (flags & kLockstep) ? virtual_time : nullptr;
  char line[96];
  snprintf(line, sizeof(line), "{\"sync\":\"start\",\"n\":0,\"lockstep\":%s,\"record\":%s,\"replay\":%s}",
           flags & kLockstep ? "true" : "false", flags & kRecord ? "true" : "false", flags & kReplay ? "true" : "false");
  fsa_diag("sync", line);
}

// The replay input (see above), malloc'd by the page and kept.
EMSCRIPTEN_KEEPALIVE void fsa_harness_load(const s16* data, int reads) {
  g_inputs = data;
  g_inputCount = reads > 0 ? (u32)reads : 0;
}

EMSCRIPTEN_KEEPALIVE int fsa_is_idle() { return platform_is_idle() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE u32 fsa_harness_reads() { return g_reads; }

}  // extern "C"
