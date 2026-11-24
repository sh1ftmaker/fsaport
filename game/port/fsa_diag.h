// Port diagnostics API (docs/DIAGNOSTICS.md). Plain C declarations only, so
// game units (which cannot include emscripten.h) and the platform layer can
// both include it. Everything except fsa_diag() exists only in an FSA_DIAG
// build (cmake -DFSA_DIAG=ON); guard uses with #ifdef FSA_DIAG.
#ifndef FSA_PORT_FSA_DIAG_H
#define FSA_PORT_FSA_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

// Append one JSON line to stream `name` (DIR/<run>/<name>.jsonl via
// tools/serve.py --diag DIR). Exists in every build (platform/diag.cpp).
void fsa_diag(const char* name, const char* json);

#ifdef FSA_DIAG
// --- Function identity --------------------------------------------------------
// A function id is the function's wasm table index, i.e. (u32)&function; it is
// what -finstrument-functions passes to the entry hook. Ids index the name table
// fsa.diag.tsv that tools/diag_symtab.py writes next to fsa.wasm at link time.
unsigned fsa_diag_fn_id(const void* fn);
const char* fsa_diag_fn_sym(unsigned id);   // decomp (MWCC) symbol, "" if unmapped; JSON-escaped
const char* fsa_diag_fn_name(unsigned id);  // demangled clang name; JSON-escaped
const char* fsa_diag_fn_kind(unsigned id);  // "" (game code), "stub", "missing", "asm-only", "asm-partial", "platform"
unsigned fsa_diag_fn_calls(unsigned id);    // entries so far this run

// --- Call stack ---------------------------------------------------------------
// Each OS thread (and the interrupt context) has a shadow stack of instrumented
// function ids. fsa_diag_caller(0) is the innermost function running now (when
// called from an enter hook: the callee that is being entered), 1 its caller,
// and so on; 0 when deeper than the stack.
unsigned fsa_diag_caller(unsigned depth);
unsigned fsa_diag_context(void);            // 0 = interrupt/host, n = OS thread coroutine n-1

// --- Scene ----------------------------------------------------------------------
// The scene is sampled once per retrace: game mode (GameRoot field_0x4c), the
// GameState stage and level, and whether a stage is loaded.
unsigned fsa_diag_scene(void);              // current scene id (index into the "scenes" stream)
// Writes {"id":..,"mode":..,"stage":..,"level":..,"loaded":..} for scene id; returns length.
int fsa_diag_scene_json(unsigned scene, char* buf, int size);
unsigned fsa_diag_frame(void);              // retraces since boot

// --- Hooks ------------------------------------------------------------------------
// enter: called on every instrumented function entry, after the counters and the
// shadow stack are updated, with callee = the function entered and caller = the
// function that was on top of this context's shadow stack (0 if none). It is
// not re-entered: functions the hook calls are counted but do not call it again.
typedef void (*fsa_diag_enter_hook)(unsigned callee, unsigned caller);
void fsa_diag_set_enter_hook(fsa_diag_enter_hook hook);
// flush: called after each periodic flush of the built-in streams (every
// FSA_DIAG_FLUSH_RETRACES retraces and at page exit), from an interrupt context
// (must not block or switch threads). Up to 8 hooks.
typedef void (*fsa_diag_flush_hook)(void);
void fsa_diag_add_flush_hook(fsa_diag_flush_hook hook);
// Flush everything now (also exported to the page as Module._fsa_diag_flush_all).
void fsa_diag_flush_all(void);

// The scene probe the game side provides (game_root_sub_04_80139BE8.cpp).
void fsa_diag_read_scene(int out[4]);
#endif

#ifdef __cplusplus
}
#endif

#endif
