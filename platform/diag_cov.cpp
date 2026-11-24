// Diagnostics build only (cmake -DFSA_DIAG=ON; docs/DIAGNOSTICS.md): function
// coverage and stub-hit counters on -finstrument-functions.
//
// Every game, JSystem and platform function is compiled with
// -finstrument-functions, so it calls __cyg_profile_func_enter(fn, site) on
// entry and __cyg_profile_func_exit(fn, site) on return. In wasm `fn` is the
// function's table index; it indexes flat counter arrays here and the name
// table fsa.diag.tsv that tools/diag_symtab.py writes at link time (table index
// -> decomp symbol, demangled name, unit, kind). Every
// FSA_DIAG_FLUSH_RETRACES retraces, and when the page goes away, the functions
// whose counts changed are written to the "coverage" and "stubs" streams with
// their cumulative counts (platform/diag.cpp -> tools/serve.py --diag).
//
// Nothing here may be instrumented itself (NOINST on every function, no
// inline library code: no STL).
#ifdef FSA_DIAG
#include "platform.h"
#include "../game/port/fsa_diag.h"

#include <emscripten.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define NOINST __attribute__((no_instrument_function))

#ifndef FSA_DIAG_FLUSH_RETRACES
#define FSA_DIAG_FLUSH_RETRACES 600  // ~10 s
#endif

namespace {

constexpr unsigned kMaxIds = 1u << 18;   // wasm table entries we track
constexpr unsigned kMaxContexts = 64;    // OS thread coroutines + 1
constexpr unsigned kStackDepth = 1024;
constexpr unsigned kMaxScenes = 256;

uint64_t g_calls[kMaxIds];
uint64_t g_flushed[kMaxIds];
uint32_t g_firstFrame[kMaxIds];
uint8_t g_firstScene[kMaxIds];
uint64_t g_overflowCalls;  // entries with an id >= kMaxIds (should stay 0)

struct ShadowStack {
  unsigned depth;
  unsigned ids[kStackDepth];
};
ShadowStack g_stacks[kMaxContexts];
// Index into g_stacks, set by os_thread.cpp. Starts as 1: the code before the
// scheduler exists runs on what becomes the main thread (coroutine 0).
unsigned g_ctx = 1;

fsa_diag_enter_hook g_enterHook;
int g_inHook;
fsa_diag_flush_hook g_flushHooks[8];
int g_flushHookCount;

// Scenes: (mode, stage, level, loaded) -> small id, in first-seen order.
struct Scene {
  int v[4];
};
// Scene 0 is "before the first retrace" (static constructors, main's setup):
// mode -2.
Scene g_scenes[kMaxScenes] = {{{-2, -1, -1, 0}}};
unsigned g_sceneCount = 1;
bool g_scene0Sent;
unsigned g_scene;  // current
unsigned g_frame;

// Name table (fsa.diag.tsv): one line per table index,
// "id\tsym\tname\tkind\tmatch\tunit\n", strings already JSON-escaped.
struct Names {
  const char* sym;
  const char* name;
  const char* kind;
  const char* match;
  const char* unit;
};
Names* g_names;
unsigned g_nameCount;
int g_namesTried;

}  // namespace

// The return address clang passes as `site` is __builtin_return_address(0),
// which emscripten implements with a JS stack walk (and only with
// -sUSE_OFFSET_CONVERTER). The diagnostics do not use it: make it free. This
// definition wins over the JS library one for the whole link.
extern "C" NOINST void* emscripten_return_address(int) { return nullptr; }

extern "C" NOINST void __cyg_profile_func_enter(void* fn, void*) {
  unsigned id = (unsigned)(uintptr_t)fn;
  if (id < kMaxIds) {
    if (g_calls[id]++ == 0) {
      g_firstScene[id] = (uint8_t)g_scene;
      g_firstFrame[id] = g_frame;
    }
  } else {
    g_overflowCalls++;
  }
  ShadowStack& s = g_stacks[g_ctx];
  unsigned caller = (s.depth && s.depth <= kStackDepth) ? s.ids[s.depth - 1] : 0;
  if (s.depth < kStackDepth)
    s.ids[s.depth] = id;
  s.depth++;
  if (g_enterHook && !g_inHook) {
    g_inHook = 1;
    g_enterHook(id, caller);
    g_inHook = 0;
  }
}

extern "C" NOINST void __cyg_profile_func_exit(void* fn, void*) {
  unsigned id = (unsigned)(uintptr_t)fn;
  ShadowStack& s = g_stacks[g_ctx];
  if (!s.depth)
    return;
  if (s.depth > kStackDepth) {  // too deep to have stored it
    s.depth--;
    return;
  }
  // Normally the top. A function that never returned normally (a thread that
  // exited from inside it) leaves frames behind: pop down to the match.
  for (unsigned k = 0; k < 8 && k < s.depth; k++) {
    if (s.ids[s.depth - 1 - k] == id) {
      s.depth -= k + 1;
      return;
    }
  }
}

// --- Context switches (os_thread.cpp) -------------------------------------------
extern "C" NOINST void fsa_diag_set_context(unsigned ctx) { g_ctx = ctx < kMaxContexts ? ctx : kMaxContexts - 1; }
extern "C" NOINST unsigned fsa_diag_context(void) { return g_ctx; }

extern "C" NOINST unsigned fsa_diag_caller(unsigned depth) {
  const ShadowStack& s = g_stacks[g_ctx];
  if (depth >= s.depth || s.depth > kStackDepth)
    return 0;
  return s.ids[s.depth - 1 - depth];
}

// --- Name table -------------------------------------------------------------------
EM_JS(int, fsa_diag_js_fetch_table, (), {
  try {
    var x = new XMLHttpRequest();
    x.open('GET', 'fsa.diag.tsv', false);
    x.send();
    if (x.status != 200) return 0;
    Module.fsaDiagTable = new TextEncoder().encode(x.responseText);
    return Module.fsaDiagTable.length;
  } catch (e) {
    return 0;
  }
});
EM_JS(void, fsa_diag_js_copy_table, (char* dst), {
  HEAPU8.set(Module.fsaDiagTable, dst);
  Module.fsaDiagTable = null;
});

namespace {

NOINST void load_names() {
  if (g_namesTried)
    return;
  g_namesTried = 1;
  int n = fsa_diag_js_fetch_table();
  if (n <= 0) {
    platform_log("[diag] fsa.diag.tsv not found: records carry ids only\n");
    return;
  }
  HostAllocScope host;
  char* buf = (char*)malloc(n + 1);
  fsa_diag_js_copy_table(buf);
  buf[n] = 0;
  unsigned maxId = 0;
  for (char* p = buf; *p;) {
    unsigned id = (unsigned)strtoul(p, nullptr, 10);
    if (id > maxId && id < kMaxIds)
      maxId = id;
    char* nl = strchr(p, '\n');
    if (!nl)
      break;
    p = nl + 1;
  }
  g_nameCount = maxId + 1;
  g_names = (Names*)calloc(g_nameCount, sizeof(Names));
  for (char* p = buf; *p;) {
    char* nl = strchr(p, '\n');
    if (nl)
      *nl = 0;
    char* f[6] = {};
    int nf = 0;
    for (char* q = p; nf < 6;) {
      f[nf++] = q;
      char* t = strchr(q, '\t');
      if (!t)
        break;
      *t = 0;
      q = t + 1;
    }
    unsigned id = (unsigned)strtoul(f[0], nullptr, 10);
    if (nf >= 6 && id < g_nameCount && f[0][0] >= '0' && f[0][0] <= '9')
      g_names[id] = Names{f[1], f[2], f[3], f[4], f[5]};
    if (!nl)
      break;
    p = nl + 1;
  }
  platform_log("[diag] name table: %u ids\n", g_nameCount);
}

NOINST const Names* names_of(unsigned id) {
  static const Names kNone = {"", "", "", "", ""};
  if (id < g_nameCount && g_names[id].sym)
    return &g_names[id];
  return &kNone;
}

}  // namespace

extern "C" NOINST unsigned fsa_diag_fn_id(const void* fn) { return (unsigned)(uintptr_t)fn; }
extern "C" NOINST const char* fsa_diag_fn_sym(unsigned id) {
  load_names();
  return names_of(id)->sym;
}
extern "C" NOINST const char* fsa_diag_fn_name(unsigned id) {
  load_names();
  return names_of(id)->name;
}
extern "C" NOINST const char* fsa_diag_fn_kind(unsigned id) {
  load_names();
  return names_of(id)->kind;
}
extern "C" NOINST unsigned fsa_diag_fn_calls(unsigned id) { return id < kMaxIds ? (unsigned)g_calls[id] : 0; }

// --- Scenes -------------------------------------------------------------------------
extern "C" NOINST unsigned fsa_diag_scene(void) { return g_scene; }
extern "C" NOINST unsigned fsa_diag_frame(void) { return g_frame; }

extern "C" NOINST int fsa_diag_scene_json(unsigned scene, char* buf, int size) {
  if (scene >= g_sceneCount)
    return snprintf(buf, size, "{\"id\":%u}", scene);
  const int* v = g_scenes[scene].v;
  return snprintf(buf, size, "{\"id\":%u,\"mode\":%d,\"stage\":%d,\"level\":%d,\"loaded\":%d}", scene, v[0], v[1], v[2],
                  v[3]);
}

namespace {

NOINST void emit_scene(unsigned scene) {
  char line[192], sc[128];
  fsa_diag_scene_json(scene, sc, sizeof(sc));
  snprintf(line, sizeof(line), "{\"scene\":%s,\"frame\":%u}", sc, g_frame);
  fsa_diag("scenes", line);
}

NOINST void sample_scene() {
  if (!g_scene0Sent) {
    g_scene0Sent = true;
    emit_scene(0);
  }
  int v[4];
  fsa_diag_read_scene(v);
  if (memcmp(g_scenes[g_scene].v, v, sizeof(v)) == 0)
    return;
  for (unsigned i = 0; i < g_sceneCount; i++) {
    if (memcmp(g_scenes[i].v, v, sizeof(v)) == 0) {
      g_scene = i;
      return;
    }
  }
  if (g_sceneCount == kMaxScenes) {
    g_scene = kMaxScenes - 1;
    return;
  }
  g_scene = g_sceneCount++;
  memcpy(g_scenes[g_scene].v, v, sizeof(v));
  emit_scene(g_scene);
}

// One record per changed function. `calls` is cumulative for the run: the
// last record for an id is its total (tools/diag_summary.py collapses them).
NOINST void flush_counts() {
  load_names();
  static char line[6144];
  char sc[128];
  unsigned changed = 0, hit = 0;
  for (unsigned id = 0; id < kMaxIds; id++) {
    uint64_t n = g_calls[id];
    if (n)
      hit++;
    if (n == g_flushed[id])
      continue;
    g_flushed[id] = n;
    changed++;
    const Names* nm = names_of(id);
    fsa_diag_scene_json(g_firstScene[id], sc, sizeof(sc));
    // A huge template name: drop the demangled form rather than cut the line.
    const char* name = strlen(nm->name) > sizeof(line) / 2 ? "" : nm->name;
    snprintf(line, sizeof(line),
             "{\"id\":%u,\"sym\":\"%s\",\"name\":\"%s\",\"kind\":\"%s\",\"unit\":\"%s\",\"match\":\"%s\",\"calls\":%llu,"
             "\"first_scene\":%s,\"first_frame\":%u,\"frame\":%u}",
             id, nm->sym, name, nm->kind, nm->unit, nm->match, (unsigned long long)n, sc, g_firstFrame[id], g_frame);
    fsa_diag("coverage", line);
    // "platform" (the port's own SDK replacement) is coverage only; the other
    // kinds are stubs / placeholders.
    if (nm->kind[0] && strcmp(nm->kind, "platform") != 0) {
      snprintf(line, sizeof(line),
               "{\"id\":%u,\"sym\":\"%s\",\"name\":\"%s\",\"kind\":\"%s\",\"unit\":\"%s\",\"calls\":%llu,"
               "\"first_scene\":%s,\"first_frame\":%u,\"frame\":%u}",
               id, nm->sym, name, nm->kind, nm->unit, (unsigned long long)n, sc, g_firstFrame[id], g_frame);
      fsa_diag("stubs", line);
    }
  }
  // Once per run: the stubs not reached yet, with calls 0, so the stream is
  // also the inventory.
  static bool inventory;
  if (!inventory) {
    inventory = true;
    for (unsigned id = 0; id < g_nameCount; id++) {
      const Names* nm = names_of(id);
      if (g_calls[id] || !nm->kind[0] || strcmp(nm->kind, "platform") == 0)
        continue;
      snprintf(line, sizeof(line), "{\"id\":%u,\"sym\":\"%s\",\"name\":\"%s\",\"kind\":\"%s\",\"unit\":\"%s\",\"calls\":0,\"frame\":%u}",
               id, nm->sym, nm->name, nm->kind, nm->unit, g_frame);
      fsa_diag("stubs", line);
    }
  }
  snprintf(line, sizeof(line),
           "{\"frame\":%u,\"functions_hit\":%u,\"changed\":%u,\"named_ids\":%u,\"scenes\":%u,\"overflow_calls\":%llu}",
           g_frame, hit, changed, g_nameCount, g_sceneCount, (unsigned long long)g_overflowCalls);
  fsa_diag("coverage_meta", line);
}

}  // namespace

extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_diag_flush_all(void) {
  flush_counts();
  for (int i = 0; i < g_flushHookCount; i++)
    g_flushHooks[i]();
}

// Debugging: Module._fsa_diag_dump_stacks() logs each context's shadow stack
// (depth and the innermost frames). Depths that keep growing between calls
// mean enters and exits are not pairing up.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_diag_dump_stacks(void) {
  load_names();
  for (unsigned c = 0; c < kMaxContexts; c++) {
    const ShadowStack& s = g_stacks[c];
    if (!s.depth)
      continue;
    platform_log("[diag] context %u depth %u%s\n", c, s.depth, c == g_ctx ? " (current)" : "");
    unsigned n = s.depth < kStackDepth ? s.depth : kStackDepth;
    for (unsigned k = 0; k < n && k < 6; k++)
      platform_log("[diag]   %s\n", names_of(s.ids[n - 1 - k])->name);
  }
}

// Self-test of the hook API: Module._fsa_diag_test_hook(1) installs a hook
// that counts entries with and without a caller; (0) removes it and logs the
// counts.
namespace {
uint64_t g_testWithCaller, g_testNoCaller;
NOINST void test_hook(unsigned callee, unsigned caller) {
  if (caller)
    g_testWithCaller++;
  else
    g_testNoCaller++;
  (void)callee;
}
}  // namespace
extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_diag_test_hook(int on) {
  if (on) {
    g_testWithCaller = g_testNoCaller = 0;
    fsa_diag_set_enter_hook(test_hook);
  } else {
    fsa_diag_set_enter_hook(nullptr);
    platform_log("[diag] test hook: %llu entries with a caller, %llu without\n", (unsigned long long)g_testWithCaller,
                 (unsigned long long)g_testNoCaller);
  }
}

extern "C" NOINST void fsa_diag_set_enter_hook(fsa_diag_enter_hook hook) { g_enterHook = hook; }
extern "C" NOINST void fsa_diag_add_flush_hook(fsa_diag_flush_hook hook) {
  if (g_flushHookCount < 8)
    g_flushHooks[g_flushHookCount++] = hook;
}

// The virtual dispatch audit (diag_vdispatch.cpp) installs its hooks here.
extern "C" void fsa_diag_vdispatch_init(void);

// Once per retrace (vi.cpp, interrupt context).
extern "C" NOINST void fsa_diag_retrace(void) {
  if (!g_frame)
    fsa_diag_vdispatch_init();
  g_frame++;
  sample_scene();
  if (g_frame % FSA_DIAG_FLUSH_RETRACES == 0)
    fsa_diag_flush_all();
}
#endif  // FSA_DIAG
