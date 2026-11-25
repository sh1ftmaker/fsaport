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
//   2. script mode: the script's active step decides the four PADStatus (and
//      may start the next step: a "step" sync and an anchor, see below);
//      replay replaces them with line n of the inputs file;
//      record writes them (as the game sees them, pre-clamp) to "inputs".
// In script mode the "tick" sync points are every 60 reads from the latest
// anchor (a scene change or a step start) instead of every n % 60 == 0, so
// two runs whose loads take different numbers of reads still dump state at
// the same offsets from each scene (tools/harness_compare.py --anchor scene).
#include "platform.h"

#include <dolphin/pad.h>
#include <emscripten.h>

#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" void fsa_diag(const char* name, const char* json);
extern "C" void fsa_harness_scene(s32* out);                                    // game/src/port/harness_state.cpp
extern "C" int fsa_harness_state(char* buf, int size, u32 n, const char* sync);  // game/src/port/harness_state.cpp
extern "C" OSTime (*aurora_os_time_hook)(void);                                  // extern/aurora/lib/dolphin/os/OSTime.cpp
extern "C" int fsa_harness_global_value(int k, double* out);                     // game/src/port/harness_state.cpp
extern "C" int fsa_harness_global_count();                                       // game/src/port/harness_state.cpp
extern "C" const char* fsa_harness_global_name(int k);                           // game/src/port/harness_state.cpp

namespace {
enum : int { kLockstep = 1, kRecord = 2, kReplay = 4, kScript = 8 };
int g_flags = 0;
u32 g_reads = 0;

// Replay input: 8 s16 per pad (connected, button, stickX, stickY, substickX,
// substickY, triggerL, triggerR), 4 pads per read.
const s16* g_inputs = nullptr;
u32 g_inputCount = 0;
bool g_endReported = false;

s32 g_scene[3];
bool g_haveScene = false;
u32 g_anchor = 0;  // script mode: the latest scene change or step start

// The script (docs/HARNESS.md "Scripted input"), compiled by the page
// (platform/web/fsa_harness.js) into doubles:
//   [0]               step count S
//   [1 + 8 * i ...]   step i: kind, buttons, every, reads, stickX, stickY,
//                     timeout (-1: none), condition offset (-1: none)
//   condition at c:   term count T, then T terms: key, value count V, V values
// A key is a kGlobals index, or kMode/kStage/kLevel (the scene triple, mode -1
// before GameRoot exists); a NaN value matches null (the value does not exist).
enum : int { kWait, kWaitReads, kPress, kHold, kStick, kEnd, kSet };
enum : int { kMode = -1, kStage = -2, kLevel = -3 };
const double* g_script = nullptr;
int g_scriptLen = 0;
int g_step = 0;
bool g_stepActive = false;
bool g_scriptDone = false;
u32 g_stepStart = 0;

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

// The "scene" sync point: true (and a sync line) when (mode, stage, level)
// changed since read n-1.
bool scene_point(u32 n) {
  s32 scene[3];
  fsa_harness_scene(scene);
  bool changed = !g_haveScene || memcmp(scene, g_scene, sizeof(scene)) != 0;
  if (changed) {
    memcpy(g_scene, scene, sizeof(scene));
    g_haveScene = true;
    emit_sync("scene", n);
  }
  return changed;
}

extern "C" int fsa_harness_set(int id, double value);  // game/src/port/harness_state.cpp

#if FSA_DIAG
// Diagnostics builds only (docs/HARNESS.md "Diagnostic pokes"): script mode's
// connected pads (bit i = port i; the `pads` set key, default port 0 alone) and
// the per-read pokes of game/src/port/harness_state.cpp.
u32 g_padsConnected = 1;
extern "C" void fsa_harness_diag_tick(u32 n);  // game/src/port/harness_state.cpp
extern "C" void fsa_harness_set_pads(u32 mask) { g_padsConnected = (mask & 0xF) | 1; }
#endif

// --- Script ------------------------------------------------------------------
bool cond_value(int key, double* v) {
  switch (key) {
  case kMode:
    *v = g_scene[0];
    return true;
  case kStage:
    *v = g_scene[1];
    return true;
  case kLevel:
    *v = g_scene[2];
    return true;
  default:
    return fsa_harness_global_value(key, v) != 0;
  }
}

// Every term holds (a term holds when the value equals any of its values).
// No condition (offset -1) never holds.
bool cond_holds(int at) {
  if (at < 0 || at >= g_scriptLen)
    return false;
  int terms = (int)g_script[at++];
  for (int t = 0; t < terms; t++) {
    int key = (int)g_script[at];
    int vals = (int)g_script[at + 1];
    at += 2;
    double v;
    bool have = cond_value(key, &v);
    bool any = false;
    for (int k = 0; k < vals; k++) {
      double want = g_script[at + k];
      if (std::isnan(want) ? !have : (have && v == want))
        any = true;
    }
    at += vals;
    if (!any)
      return false;
  }
  return true;
}

void neutral(PADStatus* status) {
  for (int i = 0; i < PAD_CHANMAX; i++) {
    memset(&status[i], 0, sizeof(PADStatus));
#if FSA_DIAG
    status[i].err = (g_padsConnected >> i) & 1 ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
#else
    status[i].err = i == 0 ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
#endif
  }
}

void script_stop(const char* why, u32 n) {
  g_scriptDone = true;
  char line[96];
  if (strcmp(why, "script_timeout") == 0)
    snprintf(line, sizeof(line), "{\"sync\":\"script_timeout\",\"n\":%u,\"step\":%d}", n, g_step);
  else
    snprintf(line, sizeof(line), "{\"sync\":\"%s\",\"n\":%u}", why, n);
  fsa_diag("sync", line);
}

// The input of read n. A step that is finished at read n (its condition holds,
// its reads are used up) ends before read n, and the next step starts at read
// n, so a step can take zero reads. Returns true when a step started.
bool script(PADStatus* status, u32 n) {
  neutral(status);
  bool started = false;
  while (!g_scriptDone) {
    int count = g_scriptLen > 0 ? (int)g_script[0] : 0;
    if (!g_stepActive) {
      if (g_step >= count) {  // past the last step: as {"end": true}
        script_stop("script_end", n);
        break;
      }
      g_stepActive = true;
      g_stepStart = n;
      started = true;
      char line[96];
      snprintf(line, sizeof(line), "{\"sync\":\"step\",\"n\":%u,\"step\":%d}", n, g_step);
      fsa_diag("sync", line);
    }
    const double* s = g_script + 1 + 8 * g_step;
    int kind = (int)s[0];
    u16 buttons = (u16)((u32)s[1] & 0xFFFF);
    u32 every = s[2] >= 1 ? (u32)s[2] : 1;
    u32 reads = (u32)s[3];
    int cond = (int)s[7];
    u32 t = n - g_stepStart;
    bool finished = false;
    switch (kind) {
    case kWait: finished = cond_holds(cond); break;
    case kWaitReads:
    case kHold:
    case kStick: finished = t >= reads; break;
    case kPress: finished = t % every == 0 && cond_holds(cond); break;
    case kSet:  // zero reads: write the settings, then the next step starts at read n
      if (cond >= 0) {
        int count = (int)g_script[cond];
        for (int k = 0; k < count; k++) {
          int id = (int)g_script[cond + 1 + 2 * k];
          if (!fsa_harness_set(id, g_script[cond + 2 + 2 * k])) {
            // Not applied: a diagnostics-only key in a normal build, or "seed" before GameRoot exists.
            char line[96];
            snprintf(line, sizeof(line), "{\"sync\":\"set_ignored\",\"n\":%u,\"step\":%d,\"id\":%d}", n, g_step, id);
            fsa_diag("sync", line);
          }
        }
      }
      finished = true;
      break;
    default: script_stop("script_end", n); return started;
    }
    if (finished) {
      g_step++;
      g_stepActive = false;
      continue;
    }
    if (s[6] >= 0 && t >= (u32)s[6]) {
      script_stop("script_timeout", n);
      break;
    }
#if FSA_DIAG
    // The step's pads (bits 16-19 of the buttons word; 0 = port 0), diagnostics builds only.
    u32 pads = ((u32)s[1] >> 16) & 0xF;
    if (!pads)
      pads = 1;
#else
    u32 pads = 1;
#endif
    for (int i = 0; i < PAD_CHANMAX; i++) {
      if (!((pads >> i) & 1))
        continue;
      PADStatus& p = status[i];
      p.err = PAD_ERR_NONE;
      if (kind == kHold || kind == kStick || (kind == kPress && t % every == 0))
        p.button = buttons;
      if (kind == kStick) {
        p.stickX = (s8)s[4];
        p.stickY = (s8)s[5];
      }
    }
    break;
  }
  return started;
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
  bool scene = scene_point(n);
  bool step = (g_flags & kScript) && script(status, n);
#if FSA_DIAG
  if (g_flags & kScript)
    fsa_harness_diag_tick(n);
#endif
  bool tick;
  if (g_flags & kScript) {
    if (scene || step)
      g_anchor = n;
    tick = n != g_anchor && (n - g_anchor) % 60 == 0;
  } else {
    tick = n % 60 == 0;
  }
  if (tick)
    emit_sync("tick", n);
  if (scene || step || tick)
    emit_state(scene ? "scene" : step ? "step" : "tick", n);
  if (g_flags & kReplay)
    replay(status, n);
  if (g_flags & kRecord)
    record(status, n);
}

extern "C" {

// From the page before main(): kLockstep | kRecord | kReplay | kScript.
EMSCRIPTEN_KEEPALIVE void fsa_harness_config(int flags) {
  g_flags = flags;
  aurora_os_time_hook = (flags & kLockstep) ? virtual_time : nullptr;
  char line[128];
  snprintf(line, sizeof(line),
           "{\"sync\":\"start\",\"n\":0,\"lockstep\":%s,\"record\":%s,\"replay\":%s,\"script\":%s}",
           flags & kLockstep ? "true" : "false", flags & kRecord ? "true" : "false", flags & kReplay ? "true" : "false",
           flags & kScript ? "true" : "false");
  fsa_diag("sync", line);
}

// The replay input (see above), malloc'd by the page and kept.
EMSCRIPTEN_KEEPALIVE void fsa_harness_load(const s16* data, int reads) {
  g_inputs = data;
  g_inputCount = reads > 0 ? (u32)reads : 0;
}

// The kGlobals names, for the page to compile a script's condition keys.
EMSCRIPTEN_KEEPALIVE int fsa_harness_keys() { return fsa_harness_global_count(); }
EMSCRIPTEN_KEEPALIVE const char* fsa_harness_key(int k) { return fsa_harness_global_name(k); }

// The compiled script (see above), malloc'd by the page and kept.
EMSCRIPTEN_KEEPALIVE void fsa_harness_script(const double* data, int len) {
  g_script = data;
  g_scriptLen = len;
}

EMSCRIPTEN_KEEPALIVE int fsa_is_idle() { return platform_is_idle() ? 1 : 0; }
// Not instrumented: diag_fntrace.cpp reads it from inside the entry hook.
__attribute__((no_instrument_function)) EMSCRIPTEN_KEEPALIVE u32 fsa_harness_reads() { return g_reads; }

}  // extern "C"
