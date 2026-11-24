# Port diagnostics

Runtime diagnostics for the decomp. Game code writes JSON lines with
`fsa_diag(name, json)` (platform/diag.cpp). The page batches them and
`tools/serve.py --diag DIR` stores them in `DIR/<run>/<name>.jsonl`, one run
per page load.

Two diagnostics builds exist: coverage and stub hits (`-DFSA_DIAG=ON`) and
uninitialised reads (`-DFSA_DIAG_UNINIT=ON`). Use a separate build directory for
each; the normal build is unchanged when they are off.

## Coverage and stub hits (`coverage`, `scenes`, `stubs`)


Runtime diagnostics for the decomp: which functions the port actually runs
(and where in the game they are first reached), and which stubs and
placeholder bodies it runs into. They come from a separate build,
`-DFSA_DIAG=ON`; the normal build is unchanged when it is off.

### Build

Use its own build directory:

```sh
source ../fsadecomp/tools/emsdk/emsdk_env.sh
emcmake cmake -S . -B build/diag -G Ninja -DCMAKE_BUILD_TYPE=Release -DFSA_DIAG=ON \
  -DFSA_DECOMP_SYMBOLS=../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt
ninja -C build/diag fsa
```

What `FSA_DIAG=ON` changes:

- Every game, JSystem and platform-layer unit is compiled with
  `-finstrument-functions` (`FSA_DIAG_INSTRUMENT` sets the flag; with
  `-finstrument-functions-after-inlining` the build is cheaper, but then
  functions that were inlined everywhere never show up). Aurora (the GX, VI,
  PAD, CARD and OS-memory replacement) is not instrumented: SDK coverage is
  limited to what `platform/` implements.
- `FSA_DIAG=1` is defined for those units: `platform/diag_cov.cpp` (the
  counters), the context hooks in `platform/os_thread.cpp`, the per-retrace
  tick in `platform/vi.cpp` and the scene probe `fsa_diag_read_scene` in
  `game/src/game/game_root_sub_04_80139BE8.cpp`.
- The link keeps mangled names in the wasm name section (`-Wl,--no-demangle`)
  and then runs `tools/diag_symtab.py`, which writes `build/diag/fsa.diag.tsv`
  (the name table, below) and `fsa.diag.tsv.report.json` (mapping statistics
  and every function without a decomp symbol).

### Run

```sh
python3 tools/serve.py --port 8823 --dir build/diag --iso path/to/G4SE01.iso --diag logs/diag
node tools/web_run.mjs --cdp 9343 --url 'http://127.0.0.1:8823/?iso=local.iso&nosave=1' --seconds 180 ...
python3 tools/diag_summary.py logs/diag/<run> \
  --structural ../fsadecomp/fsa-decomp/build/G4SE01/structural.json
```

Each page load is one run: `logs/diag/<date>-<time>-<session>/<stream>.jsonl`.
The counters are written every 600 retraces (about 10 s;
`FSA_DIAG_FLUSH_RETRACES`), when the page is hidden or closed, and when
`fsaDiagFlush()` is called in the page (`tools/web_run.mjs` does that before it
closes its tab). The page fetches `fsa.diag.tsv` (synchronously, once, at the
first flush) from the build directory; without it records carry ids only.

In the page console: `Module._fsa_diag_flush_all()` writes the counters now,
`Module._fsa_diag_dump_stacks()` logs each thread's shadow call stack,
`Module._fsa_diag_test_hook(1)` / `(0)` installs / removes a test enter hook
and logs how many entries had a caller.

Cost (game CPU per frame from the `[vi]` log, mean after the first 60 s, same
commit and machine): adventure opening 1.19 ms -> 1.77 ms, stage 1 with the
player walking 3.47 ms -> 4.52 ms. Both keep 60 frames per 60 retraces.
fsa.wasm grows from 7.9 to 9.4 MB; a flush (every 10 s) posts 1-3 MB of
JSON lines. The port's own hottest helpers are not instrumented
(`#pragma clang attribute ... no_instrument_function` under `FSA_DIAG`):
the big-endian accessors in `libs/sdk/include/port/be.h` and the GX
write-gather pipe stores in `dolphin/gx/GXVert.h` were 45% of all entries
(2.8 ms instead of 1.8 ms in the adventure opening) and have no decomp
symbol. Do the same for any other port-only hot spot.

### Streams and records

All counts are cumulative for the run. A record is written again whenever its
count changed since the last flush, so **the last record per `id` is the
total**; `tools/diag_summary.py` collapses them (`coverage_final.jsonl`,
`coverage_by_sym.jsonl`, `stubs_final.jsonl` in the run directory).

#### `coverage`: one record per port function called

```json
{"id":3,"sym":"remove__10JSUPtrListFP10JSUPtrLink","name":"JSUPtrList::remove(JSUPtrLink*)","kind":"",
 "unit":"game/libs/jsystem/JSystem/JSupport/JSUList.cpp","match":"exact","calls":17199,
 "first_scene":{"id":1,"mode":-1,"stage":0,"level":0,"loaded":0},"first_frame":7,"frame":600}
```

- `id`: the function's wasm table index (stable within one build only).
- `sym`: the decomp's symbol, spelled exactly as in `config/G4SE01/symbols.txt`
  (CodeWarrior mangling), or `""` when there is none (see the join key).
- `name`: the clang symbol demangled.
- `kind`: as in the name table: `""` for game code, otherwise one of the stub
  kinds below or `platform`.
- `unit`: the source file with the function's out-of-line definition;
  `inline:<file>` for an inline or template function from a header (the unit
  whose copy the linker kept).
- `match`: how `sym` was found (join key, below).
- `calls`: entries so far this run. Functions inlined by clang are still
  counted (the instrumentation is inserted before inlining).
- `first_scene`: the scene when the function was first entered; `first_frame`:
  retraces since boot at that time; `frame`: when the record was written.

#### `scenes`: one record per new scene

```json
{"scene":{"id":8,"mode":7,"stage":8,"level":3,"loaded":1},"frame":2310}
```

The scene is sampled once per retrace: `mode` is `GameRoot` `field_0x4c` (the
current game mode), `stage` / `level` are the GameState's
`lbl_80529DEC.mStage_0x118` / `mLevel_0x11c`, `loaded` is whether a stage
(`GameRoot` `field_0x4`) exists. Scene 0 (`mode -2`) is "before the first
retrace" (static constructors, `main`); `mode -1` is before `GameRoot` exists.
Ids are in first-seen order and only meaningful within one run, which is why
records embed the whole scene.

#### `stubs`: stubs and placeholder bodies

```json
{"id":2788,"sym":"GXSetMisc","name":"GXSetMisc","kind":"stub","unit":"platform/hw_stubs.cpp","calls":187416,
 "first_scene":{"id":3,"mode":1,"stage":0,"level":0,"loaded":0},"first_frame":140,"frame":10800}
```

The same fields as `coverage`, for the functions whose kind is not `""` or
`platform`. At the first flush the stream also lists every known stub not yet
reached, with `"calls":0`, so it doubles as the inventory. Kinds:

| kind | meaning |
|---|---|
| `stub` | defined in `platform/hw_stubs.cpp`, or a `platform/` function whose body is empty or returns a constant |
| `missing` | written by the port in `game/src/port/missing_functions.cpp` (no decomp unit defines it) |
| `asm-only` | a decomp body that is `#ifdef __MWERKS__` asm with no portable `#else`: compiles to nothing |
| `asm-partial` | part of the body is such asm (`J3DMtxBuffer::calcWeightEnvelopeMtx`) |
| `empty` | a game function whose port body compiled to nothing while the original's is more than a return (ctors/dtors excluded) |

The asm scan is the logic of the old `asmscan.py`, now in
`tools/diag_symtab.py` (`asm_hits`); among the compiled units only
`J3DMtxBuffer.cpp` and `J3DTransform.cpp` have such blocks, and the
`J3DTransform.cpp` one is a whole function the port defines elsewhere.

#### `coverage_meta`: one record per flush

`{"frame","functions_hit","changed","named_ids","scenes","overflow_calls"}`.

### The name table and the join key

`fsa.diag.tsv` has one line per instrumented function:
`id \t sym \t name \t kind \t match \t unit` (strings JSON-escaped). The
runtime reads it; joins outside the game should use `sym`.

`sym` is always a name that occurs in `symbols.txt` (or `""`), so the decomp
can join on it directly, and with `build/G4SE01/structural.json`, whose
`units` / `struct_fns` use the same spelling. `tools/diag_symtab.py` finds it
in this order:

1. `exact`: the clang (Itanium) symbol is parsed and re-mangled the
   CodeWarrior way (`name__<qualifier>[C]F<params>`, `Q<n>` qualified names,
   `<...>` template arguments in mangled form, anonymous namespaces as
   `@unnamed@<file>_cpp@`), and that string is in `symbols.txt`. Works because
   the decomp's headers make `u32`/`s32` `unsigned long`/`long` on wasm32 too.
2. `c`: an unmangled (C or `extern "C"`) name found verbatim.
3. `name`: the qualified name without parameters matches exactly one
   `symbols.txt` function (the port changed a signature); `_GLOBAL__sub_I_x.cpp`
   maps to `__sinit_x_cpp`.
4. `addr`: the name carries the original's address (`fn_8023FBF0`,
   `moveTempo_80458508`), `symbols.txt` has a function at that address in the
   same scope, and no other port function claims it.
5. `ambiguous` (several candidates, `sym` left empty) or `none`.

For the current build: 15230 instrumented functions; `exact` 10278,
`c` 201, `name` 73, `addr` 5, `ambiguous` 14, `none` 4659. 9915 of the 11541
`symbols.txt` functions are matched by some port function; the others are
code the port does not compile (TRK, DSP, the SDK, MSL), thunks (`@16@...`),
static initialisers and template functions the port never instantiates out of
line.

Caveats:

- An unmapped function is almost always a function the original does not
  have: of the out-of-line game definitions about 10% have no symbol, and
  sampled ones are all decomp-written helpers (`allSet`, `probeTile`,
  `drawFade`, `portReadBE<T>`) that MWCC inlined. Inline/template functions
  from headers (`unit` `inline:...`) mostly have no symbol either, for the same
  reason. `fsa.diag.tsv.report.json` lists all of them with the
  CodeWarrior-style guess (`mwcc_guess`).
- Several port functions can map to one symbol: clang emits complete, base
  and deleting variants of a constructor/destructor where CodeWarrior has one
  `__ct`/`__dt`, and the deleting destructor calls the base one, so per symbol
  `coverage_by_sym.jsonl` reports `calls` as the maximum over the variants
  (`calls_sum` is also there).
- `id`s change with every build; never join runs from different builds on
  `id`.
- Counts are entries, including those from inlined copies, so an accessor
  that MWCC inlined (no symbol) still counts; a function the port never calls
  but the original did is simply absent.

### What two sessions show

Boot -> title -> menus -> adventure opening (180 s; keys
`Enter@24,Space@33,Space@38,KeyA@42,Space@45,Space@52,Space@58,Space@66`, then
Space every 3 s; ends in mode 7, stage 8 level 3), and `?stage=1` (120 s;
`Space@40,Space@44`, then the stick held with `Module._fsa_touch_set(0,100,0,0,0)`):

- 5555 / 5046 port functions entered; together 5826, 2912 distinct decomp
  symbols. 254 of 2491 out-of-line game definitions hit in the opening have no
  symbol (10%).
- 51 known stubs, 29 reached in each; the most called are
  `GXInitTexCacheRegion`, `GXSetMisc`, `OSDisableInterrupts`,
  `GXSetZScaleOffset`, `DCStoreRange`.
- 83 / 89 of the hit symbols are not matching in `structural.json`; the most
  called are `JPAGetDirMtx`, `getTileCollisionCode_8022461C`,
  `drawSobSprite_802F86CC__16TileMap_804AA858...` and
  `drawSprite_801F06F0__18ActorBase_804A5770...`.

### Hooks for further diagnostics

Everything is declared in `game/port/fsa_diag.h` (plain C; game units can
include it as `"../port/fsa_diag.h"`, the platform layer as
`"../game/port/fsa_diag.h"`), and exists only when `FSA_DIAG` is defined:

- Function identity: `fsa_diag_fn_id(fnptr)` (the table index: a function
  pointer, a vtable entry and the instrumentation's id are the same number),
  `fsa_diag_fn_sym/name/kind(id)` (from the name table), `fsa_diag_fn_calls(id)`.
- Call stack: one shadow stack per OS thread coroutine and one for interrupts
  (`os_thread.cpp` switches them with `fsa_diag_set_context`).
  `fsa_diag_caller(0)` is the innermost function, `(1)` its caller, ...;
  `fsa_diag_context()` says which stack is live.
- Scene: `fsa_diag_scene()`, `fsa_diag_scene_json(id, buf, size)`,
  `fsa_diag_frame()`.
- `fsa_diag_set_enter_hook(fn)`: `fn(callee, caller)` runs on every
  instrumented entry after the counters and the stack are updated; it is not
  re-entered (functions it calls are counted, without calling it again). One
  hook at a time; chain yourself if you need two.
- `fsa_diag_add_flush_hook(fn)`: runs after each flush of the built-in streams
  (interrupt context: must not block or switch threads); write your own
  stream there with `fsa_diag("<name>", json)`, cumulative like these.

#### A vtable-dispatch audit on top of this

The entry hook sees every call but not whether it was virtual. Clang can mark
the indirect calls: add `-fsanitize-coverage=func,trace-pc,indirect-calls` to
`FSA_DIAG_INSTRUMENT` (or to the units of interest). Before each
`call_indirect` clang then calls `__sanitizer_cov_trace_pc_indir(callee)` with
the callee's table index (checked on emscripten 5.0.6), and
`__sanitizer_cov_trace_pc()` once per function entry (define it empty). So:

1. Define `__sanitizer_cov_trace_pc_indir(void* callee)` (NOINST) to store
   `callee` and the current caller (`fsa_diag_caller(0)`) in a pending slot.
2. In an enter hook: when `callee` equals the pending callee, this entry is an
   indirect call from `caller`; record the edge (caller id, callee id, count,
   first scene) in a hash table and clear the slot. (A callee entered without
   a pending slot came from a direct call or from outside instrumented code,
   e.g. a JS callback or an Aurora callback.)
3. `__sanitizer_cov_trace_pc_indir` only gives the callee, not the object or
   the slot. To tell virtual calls from other function pointers, either look
   the callee up in the vtables (vtables are data whose entries are table
   indices, which `fsa_diag_fn_sym` names; the port's `_ZTV*` symbols are in
   `fsa.map`), or match the edges offline against the decomp's expected
   targets (the `__vt__<class>` objects in symbols.txt and the methods in each
   slot).
4. Write a stream (for example `vdispatch`) from a flush hook with
   `{"caller_sym","callee_sym","calls","first_scene"}` plus ids, cumulative, and
   extend `tools/diag_summary.py` to collapse it like `coverage`.

Keep the hook cheap: the entry hook runs on every call of every instrumented
function (hundreds of thousands per frame on stage 1).


## Uninitialised reads (`uninit`)

A diagnostics build that finds game fields which are read before anything
writes them, and fields that a constructor leaves unset. The motivating case
was `GameRootSub04_801F856C`. Its constructor left `field_0x2c` and
`field_0x30` unset on most stages because the decomp was missing an else
branch. The destructor then deleted a junk pointer, since the port's heap
memory is not zero.

### Building and running

```sh
cmake -S . -B build/web-uninit -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=.../Emscripten.cmake -DFSA_ASSETS=... -DFSA_GENERATED=... \
  -DFSA_DIAG_UNINIT=ON -DFSA_DECOMP_SYMBOLS=/path/to/fsa-decomp/config/G4SE01/symbols.txt
ninja -C build/web-uninit fsa
python3 tools/serve.py --dir build/web-uninit --iso ... --diag logs/diag
tools/uninit_summary.py logs/diag/<run>/uninit.jsonl [more runs] [--all] [--json merged.jsonl]
```

The option is `OFF` by default. In a normal build none of this code is
compiled or linked: there is no poison, no instrumentation, and no
`fsa.uninit.json`.

### How it works

1. **Poison.** `JKRHeap::alloc` fills every new block with `0xA5` bytes. That
   covers every JKR heap type, `operator new`, and `new (heap, align)`. It
   also records the block in an address map (`platform/diag_uninit.cpp`), and
   `JKRHeap::free` releases the record. `malloc` (host code, Aurora, libc++)
   is not poisoned.
2. **Load checks.** After the link, `tools/uninit_instrument.py` rewrites
   `fsa.wasm`. It only changes functions that the linker map places in the
   game's unit libraries (game/src, JSystem, SDK). Each `i32.load`, `f32.load`,
   `i64.load` and `i32.load16_*` gets an inline compare of the loaded value
   with the poison pattern. On a match it calls `fsa_uninit_hit(addr, site)`,
   where `site` is the reading function. The fast path is a tee, a compare and
   a branch, with no call.
3. **Constructor brackets.** Each constructor (Itanium C1/C2, `this` in local
   0) gets a wrapper that calls `fsa_uninit_ctor_enter/exit`, and every direct
   call is sent through the wrapper. When the outermost constructor of a fresh
   block returns, two things happen:
   - The block is tagged with the class, so later reads can name it.
   - The block's bytes that are still `0xA5` are recorded as runs.

   These records are reported when the next block is allocated or
   constructed. If a later constructor runs inside the block (an array
   element, or a placement new), the scan is cut at that point.
4. **Allocation sites.** Each call to `operator new` or `JKRHeap::alloc`
   first stores the calling function's index. A block without a constructor
   (POD, buffers) is then named by the function that allocated it.
5. **Inlining off.** The diagnostics build compiles the game with
   `-fno-inline` and links with `-O1` (no wasm-opt inlining). Otherwise most
   constructors would be inlined into their callers and could not be
   bracketed (64 bracketed instead of 1158).
6. **Names.** The instrumenter writes `fsa.uninit.json` (function index to
   Itanium, demangled and MWCC names plus the original address).
   `platform/web/fsa_uninit.js` uses it to turn the runtime's numbers into the
   records below. MWCC names are generated from the demangled signature and
   kept only when they exist in `symbols.txt`; about 2/3 of the game
   functions match.

### Records

One JSON line per finding:

```json
{"evidence":"read-before-write","class":"GameRootSub04_801F856C",
 "ctor":"GameRootSub04_801F856C::GameRootSub04_801F856C(JKRHeap*)",
 "ctor_mwcc":"__ct__22GameRootSub04_801F856CFP7JKRHeap","ctor_addr":"0x8013A538",
 "field":"0x2c","size":4,"where":"GameRootSub04_801F856C::fn_80139C84()",
 "where_mwcc":"fn_80139C84__22GameRootSub04_801F856CFv","where_addr":"0x80139C84",
 "load":"u32/f32","alloc_size":56,"count":1,
 "scene":{"mode":7,"stage":8,"level":3,"in_stage":1},"key":"1:21926:21917:0:44:4:4"}
```

- `evidence` is one of:
  - `read-before-write`: game code loaded a value that is still the
    untouched poison of a heap block. The field was read before anything
    wrote it. This is proof of a read. `where` is the function that did the
    read.
  - `never-written-after-ctor`: bytes of the block that were still poisoned
    when its outermost constructor returned. `field` is the start of the run
    and `size` its length in bytes. `objects` is the number of objects that
    constructor built, and `count` is how many of them had this run. Many of
    these are fine: padding, fields set by an `init()` later, buffers. They
    are leads, not bugs.
  - `poison-value-outside-heap`: a poison value was loaded from memory that
    is not a known heap block, such as a stack or static copy of an unset
    field. `address` replaces `field`.
- `class` is the most derived constructor that finished on the block, or that
  is still running on it. It is `null` for a block without a constructor,
  which `alloc_site` (the allocating function) identifies instead.
- `field` is the byte offset in the port's layout. That matches the decomp's
  `field_0xNN` wherever the class layout is the same under clang. Watch for
  vtable placement and Aurora's larger GX structures.
- If one function reads more than 8 offsets of one class (a loop over an
  array), the rest are folded into one record with `"field":"many"` and
  `fields_from`/`fields_to`.
- A record is repeated whenever its count changes (every 5 s). Keep the last
  line per `key`, which `tools/uninit_summary.py` does. `key` uses wasm
  function indices, so it is only stable within one build.
- `scene` is taken from the game when the finding is first seen (root mode,
  save stage and level, whether a stage is loaded). During the root object's
  own construction its `mode` can itself be poison (-1515870811).
- Not caught: reads of 1-byte fields (0xA5 is too plausible as a value),
  stack variables, `malloc` memory, and fields of objects whose memory was
  freed and reused without a new allocation (pool allocators such as JPA
  particles are poisoned only when the pool is allocated).

### Verification

With the `GameRootSub04` else branch removed again in a scratch build, the
adventure opening reports:

- `never-written-after-ctor GameRootSub04_801F856C +0x28 size 16`;
- `read-before-write +0x2c in GameRootSub04_801F856C::fn_80139C84()`.

Then the port crashes exactly as the bug did (`StaffRollLoader::fn_8044B8EC`
on the poison pointer). With the fix in place, only `+0x28` and `+0x34`
(4 bytes each) remain as never-written.

### Cost

In stage 1, game time is about 1.45x the normal build (4.3 vs 3.0 ms/frame).
That is the load checks plus `-fno-inline`. Every allocation's poison fill
adds to load times. The address map and the records take about 100 MB of
host memory.
