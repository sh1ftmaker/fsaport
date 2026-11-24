# Port diagnostics

Runtime diagnostics for the decomp. Game code writes JSON lines with
`fsa_diag(name, json)` (platform/diag.cpp). The page batches them and
`tools/serve.py --diag DIR` stores them in `DIR/<run>/<name>.jsonl`, one run
per page load.

Two diagnostics builds exist: coverage, stub hits and the virtual dispatch
audit (`-DFSA_DIAG=ON`) and uninitialised reads (`-DFSA_DIAG_UNINIT=ON`). Use a separate build directory for
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

Keep an enter hook cheap: it runs on every call of every instrumented
function (hundreds of thousands per frame on stage 1). The virtual dispatch
audit below is built on these hooks.


## Virtual dispatch audit (`vdispatch`)

The decomp found about 40 functions that sat in the original's vtable slots but
were declared as plain members, so the base class's implementation ran instead
(fixed in the port in ee31197; the decomp's `tools/vtable_check.py` compares
its objects' vtables with the original's statically). This audit is the
runtime side: which function each virtual call in the port actually reached,
on which object's class, compared with what the original's vtable for that
class holds in that slot. It is part of the `FSA_DIAG=ON` build (no extra
flag; `-DFSA_DIAG_VDISPATCH=OFF` leaves it out).

### How it works

The entry hook sees every call but neither whether it was indirect nor the
object. Clang's `-fsanitize-coverage=indirect-calls` would mark indirect calls
(`__sanitizer_cov_trace_pc_indir(callee)`) but gives no access to `this`, and
`-finstrument-functions` passes nothing but the function (wasm-opt even drops
its second argument). So the call sites and prologues are rewritten after the
link instead; no compile flag changes.

1. **Rewrite** (`tools/vdispatch_instrument.py`, a POST_BUILD step after
   `diag_symtab.py`; keeps the input as `fsa.wasm.novd`). In every
   instrumented function (one whose body calls `__cyg_profile_func_enter` with
   its own table index; the `NOINST` diagnostics code is not):
   - each `call_indirect` gets `call $fsa_vd_mark` in front (it takes the
     table index from the top of the stack and returns it) and a store of 0 to
     the pending word after it;
   - when the function's first parameter is an i32, its prologue stores that
     parameter to the `this` word just before its own enter call.
   Current build: 15590 functions rewritten (14901 pass their first
   parameter), 12103 `call_indirect`s marked, no `return_call_indirect`.
2. **Pairing** (`platform/diag_vdispatch.cpp`, an enter hook installed at the
   first retrace). `fsa_vd_mark` records the target, the calling function
   (`fsa_diag_caller(0)`) and the context, and clears the `this` word. The
   next entry of an instrumented function from that caller in that context is
   the call's callee. If it is not the target, the target was an
   uninstrumented thunk or function and the record carries both (`entered`).
3. **Receiver.** The `this` word is the callee's first argument. If it points
   into memory and the word there is an address inside one of the port's
   vtable groups, that is the receiver's vtable pointer: the group names the
   class, and the target's position after the vtable pointer is the slot. The
   group table is `fsa.diag.vt.tsv`, written by the instrumenter from the
   `_ZTV*` symbols in `fsa.map` and the data section (one line per group:
   address, size, demangled class, CodeWarrior `__vt__` name, `_ZTV` name, its
   words); the page fetches it at the first retrace.
4. Edges (caller, target, entered function, vtable pointer) are counted in a
   hash table (64K entries, 3/4 usable) and written by a flush hook.

### Records

`vdispatch`, one record per edge, cumulative like `coverage` (the last record
per edge is its total):

```json
{"caller":9163,"caller_sym":"fn_80118E20__12cls_801192ACFv","caller_name":"cls_801192AC::fn_80118E20()",
 "target":12136,"target_sym":"vfunc_0x5C__26PrincessZeldaDemo_8049D208Fv",
 "target_name":"PrincessZeldaDemo_8049D208::vfunc_0x5C()","kind":"virtual",
 "class":"PrincessZeldaDemo_8049D208","vt":"__vt__26PrincessZeldaDemo_8049D208","vt_addr":426920,
 "vptr_off":8,"slot":88,"calls":958,"first_scene":{"id":8,"mode":7,"stage":8,"level":3,"loaded":1},
 "first_frame":11107,"frame":13200}
```

- `caller` / `target` (+ `_sym`, `_name`): the function making the indirect
  call and the table entry it called (ids as in `coverage`). `entered` /
  `entered_name` only when the instrumented function entered differs from
  the target (the target is an uninstrumented thunk or wrapper).
- `kind`: `virtual` (the receiver has a vtable and the target is in it),
  `not-in-vtable` (the receiver has a vtable but the target is not in it: a
  pointer to a non-virtual member function, e.g. `JASSeqParser`'s command
  table), `fnptr` (no object with a vtable as the first argument: plain
  function pointers, callbacks).
- `class`, `vt`, `vt_addr`: the receiver's dynamic class (the vtable group
  its vtable pointer is in), its CodeWarrior `__vt__` name, the group's
  address. `vptr_off`: where the vtable pointer points in the group (8 for
  the primary vtable; more for a secondary base's). `slot`: byte offset of the
  target from the vtable pointer, in the port's (Itanium) layout; `slots` when
  the target occurs more than once.

`vdispatch_meta`, per flush: `marks` (indirect calls), `matched` (paired with
an entry), `other_entry` (an entry from another function came first),
`edges`, `edge_overflow`, `vtables`.

`tools/diag_summary.py` also writes `vdispatch_final.jsonl` (last record per
edge, summed over the runs given).

### The audit

```sh
python3 tools/vdispatch_audit.py logs/diag/<run> [more runs] --build build/diag \
    --decomp ../fsadecomp/fsa-decomp [--static] [--all] [--json out.json]
```

It reads the original's vtables from the decomp: the `__vt__*` objects in
`config/G4SE01/symbols.txt` (address, size) and their words in
`orig/sys/main.dol` (each slot the address of a function, named through
symbols.txt), the same data `vtable_check.py` gets from dtk's split objects.
The port's vtables come from `fsa.diag.vt.tsv`, their words named with
`fsa.diag.tsv` and the wasm name section.

Slots are matched by position after normalising the two layouts:

- clang's destructor pair (complete D1 + deleting D0) is CodeWarrior's single
  `__dt__` slot. Either half may stand for the class: when the derived
  destructor adds nothing, clang emits D1 as an alias of the base's
  destructor, and for an abstract class D0 is a trap that is not
  instrumented (the audit then mangles its clang name itself);
- CodeWarrior's pure virtual slots are null, clang's are `__cxa_pure_virtual`;
- secondary vtables: the port's start with (negative offset, 0), the
  original's with two null words, indistinguishable from pure slots, so the
  original is split by the port's slot counts and that split is only used when
  the original's header words are where it puts them;
- the original's `@N@name` this-adjusting thunks match the port's
  `non-virtual thunk to name` or `name` itself;
- dtk's sizes include alignment padding (trailing null words).

When the slot counts still differ, the port's function is looked up by name
in the original's vtable (`OK-BYNAME`), or a function with the same name and
parameters (a `?` status). Per (class, slot, target):

| status | meaning |
|---|---|
| `OK` | same function as the original (`OK-LAYOUT`, `OK-BYNAME`: matched with less certainty) |
| `MISSING-OVERRIDE` | the original's slot holds the class's own function, the port called another class's (a base implementation): the decomp declares the override without `virtual` or under another name |
| `PORT-OVERRIDE` | the reverse: the port called the class's own function where the original has a base's |
| `DIFFERENT` | some other function |
| `UNMAPPED` | the port's function has no decomp symbol (port-written) |
| `NO-ORIGINAL` | the original has no `__vt__` for the class (port-only classes: Aurora, libc++) |

A trailing `?` means the layouts did not line up, so the slot match is a
guess. `--static` compares every port vtable that has an original, reached or
not, and marks the reached slots. The exit status is 1 when a reached slot is
`MISSING-OVERRIDE`, `PORT-OVERRIDE` or `DIFFERENT`.

### Results

Sessions as in "What two sessions show": the adventure opening (225 s, to
mode 7 stage 8 level 3) and `?stage=1` with the stick held (150 s):

- 268 M and 43 M indirect calls (about 20 k and 5 k per frame), all but a few
  (calls still pending at the last flush) paired with their callee; 2001 edges: 1748 `virtual`, 79 `not-in-vtable`,
  174 `fnptr`.
- 704 reached (class, slot, target): 687 `OK`, 15 `OK-BYNAME` (the sound
  classes with two bases, `JAISe`, `GameAudio::CustomSoundTable`,
  `Actor/PlayerSoundObj`, `JAUSectionHeap`, whose secondary tables are laid
  out differently), 2 `NO-ORIGINAL` (libc++ inside Aurora). **No reached slot
  dispatches differently from the original.**
- `--static`: 702 port vtables, 11155 slots; all match except the 4 slots
  where ee31197 put port forwarders (`UNMAPPED`: VaatiBoss 0x4C / 0x60,
  ShadowLink2 0x60, RotatingSkull 0x4C, which forward to the decomp's plain
  `fn_*` members).

Verification: with ee31197 undone for PrincessZeldaDemo 0x5C and Hinox 0x44
in a scratch build (the overrides renamed back to plain `fn_8010FA54` /
`fn_8014F76C`), the opening reports

```
MISSING-OVERRIDE  __vt__26PrincessZeldaDemo_8049D208 +0x5C (port +0x60)  calls 1131  first: mode 7 stage 8 level 3
    original vfunc_0x5C__26PrincessZeldaDemo_8049D208Fv
    port     vfunc_0x5C__22DemoActorBase_8049D140Fv
    via      cls_801192AC::fn_80118E20() x1131
```

(Princess Zelda appears about 185 s into the opening), and `--static` lists
both slots. Hinox is not reached by either session.

Caveat when switching one port between build directories: the browser
revalidates `fsa.wasm` and the name tables by modification time, so a page can
keep the newer files of the previous build (ids then name the wrong
functions). Use a fresh browser profile, or one build directory per port.

### Cost

Same build with and without the rewrite (`fsa.wasm.novd` served instead),
game CPU per frame after the first 60 s: opening 2.01 -> 2.16 ms, stage 1
5.10 -> 4.47 ms: within the run-to-run noise of the coverage build on a shared
machine. fsa.wasm grows from 9.09 to 9.40 MB; the edge table is 2.6 MB of
static memory; a flush posts at most a few hundred KB (3 MB per 225 s run).


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

## Endian trap (`endian`)

Finds raw multi-byte reads of big-endian file data: loads that bypass the
byte-order helpers of `port/be.h` (`BE<T>`, `be16`/`be32`/`be32s`/`bef32`).
The triaged list (static audit plus what this trap found) is
[docs/diag/endian_audit.md](diag/endian_audit.md).

### Build and run

```sh
emcmake cmake -S . -B build/web-endian -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFSA_ENDIAN_DIAG=ON -DFSA_DEBUG_LINES=ON -DFSA_ENDIAN_DIAG_UNITS="/game/(src|libs)/"
ninja -C build/web-endian fsa
python3 tools/serve.py --dir build/web-endian --diag logs/diag ...
# play; the stream is flushed every 300 retraces, or call Module._fsa_endian_report()
python3 tools/endian_report.py logs/diag/<run>     # -> <run>/endian_resolved.jsonl + ranking
```

`FSA_ENDIAN_DIAG_UNITS` is a regex over source paths; the default
`/game/src/game/` instruments the game's own code only, the value above adds
JSystem and the SDK. `FSA_DEBUG_LINES=ON` keeps the DWARF line tables that
`tools/endian_report.py` needs for file:line. `Module._fsa_endian_ranges()`
writes the registered file ranges to the `endian_ranges` stream.

### How it works

- The instrumented units are compiled with
  `-fsanitize-coverage=trace-pc-guard,trace-loads`: every basic block calls
  `__sanitizer_cov_trace_pc_guard` (its guard id is kept as the current site) and
  every load calls `__sanitizer_cov_load1/2/4/8` with its address first.
  `platform/endian_diag.cpp` implements them; only 2- and 4-byte loads are
  checked.
- The loaders register the address ranges that hold file bytes
  (`port/endian_diag.h`, macros that vanish in a normal build): the game's DVD
  reads (`platform/dvd.cpp`, not the platform's own FST/header reads, nothing
  under 64 bytes), Yaz0/Yay0 output (`JKRDecomp::decode`, the streaming
  decompressors in `JKRDvdRipper` and `JKRAram`), ARAM-to-RAM copies
  (`JKRAramPiece::startDMA`), and every resource `JKRArchive::getResource`
  returns, labelled with its type and name. `JKRHeap::free`, `freeAll` and
  `destroy` forget the ranges inside the freed memory. A page table makes the
  common "not file data" case one lookup.
- A load from a range is a raw read of file data. Its value is classified:
  *swapped-looking* when the byte-swapped value is much more plausible (a
  small integer, or a sane float where the raw value is denormal or huge),
  *plain-looking* for the reverse (little-endian GBA data, data swapped once at
  load, heap pointers), neutral otherwise (zero, palindromes, four ASCII bytes).
- The first hit of a site captures the JS stack once: the function name and the
  wasm code offset. `BE<T>`, `BEPtr` and `BEOffset` read byte by byte in this
  build, so the helpers' own reads are not reported.
- `tools/endian_report.py` keeps the last record per site, maps the offsets to
  file:line with llvm-symbolizer (undoing JSystem's `#line` directives), fills
  `unit` from `game/units.txt` and `symbol` from the decomp's `symbols.txt`
  (CodeWarrior mangling, when unambiguous), merges sites on the same line, and
  ranks by swapped-looking share. Lines that already use a helper (`bswap`,
  `be32`, `portReadBE`, ...) are marked `helper` and ranked last; word copies of
  byte data (`memcpy`, colours) are marked `copy` and ranked down.

### Records

`endian`, one line per changed site per flush (cumulative; the last one wins):

```json
{"function": "JPALoadCalcTexCrdMtxAnm(JPAEmitterWorkData*, JPABaseParticle*)",
 "file": "", "line": 0, "unit": "", "format": "????",
 "field": "+0x7CBB0 in particle.jpc", "evidence": "runtime",
 "detail": "u32 raw load: 16611 swapped-looking, 0 plain-looking of 55370; e.g. 0x0AD7A33D (swapped 0x3DA3D70A)",
 "count": 55370, "size": 4, "swapped": 16611, "plain": 0, "wasm": "0x3c62ca", "site": 1535}
```

`format` is the resource type (`'TIMG'`, `'J3D '`, `????` for a lookup by
name) or the file extension of a disc read; `field` is the offset into the
innermost registered range and its name. `endian_resolved.jsonl` has the same
records with `file`, `line`, `unit`, `symbol`, `source` (the line's text),
`helper`, `copy` and `score` filled in. The static audit's entries in
`docs/diag/endian_audit.md` use the same fields with `"evidence": "static"`.

### What it found

Title, menus, opening narration, the Hyrule Castle opening up to Zelda's first
lines, and `?stage=1` (fairy dialogue, walking):
**no raw load from file data in `game/src/game`** (the game's GBA-derived
formats are little-endian and read byte by byte). In JSystem it found the JPA
texture-coordinate animation table read as host floats (fixed, 38ec291). The
rest of what it reports is the noise listed in the audit (in-place swapped
tables, word copies of byte data, `BE<>` structs copied by value).

### Limits

- It sees only code that runs: the message-branch operands (audit #1-#3) were
  not reached by the scenes above.
- Memory reused after a free that bypasses `JKRHeap::free` (e.g. `freeTail`,
  solid heaps reset wholesale) keeps its stale range; such hits look plain or
  neutral and point at non-file objects.
- The value classification is a heuristic; a raw read of big-endian zero or of a
  value that is plausible both ways is neutral. Rank by `swapped` and `plain`
  together.

### Cost

Stage 1 runs at about 5.3 ms of game time per frame against 3.5 in the normal
build (all decomp units instrumented); loading is slower, so key scripts need
their times shifted by ~20 s. Normal builds are unchanged.

## Sanitizers (`sanitizer`)

UBSan and ASan over the game code (`game/src`, `game/libs`, `game/port`):
out-of-bounds indexing, misaligned and null accesses, shifts out of range,
signed overflow, `bool`/enum loads of values the type does not have,
out-of-range float-to-int casts, and heap/global out-of-bounds and
use-after-free, including inside the game's own JKR heaps. Two builds behind
one option, each in its own build directory; the normal build is unchanged
when it is empty. Aurora, the platform layer and libc are not instrumented.

### Build and run

```
cmake -S . -B build/ubsan ... -DFSA_SANITIZE=undefined \
      -DFSA_DECOMP_SYMBOLS=../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt
cmake -S . -B build/asan  ... -DFSA_SANITIZE=address  -DFSA_DECOMP_SYMBOLS=...
ninja -C build/ubsan fsa 2>&1 | tee build/ubsan-build.log    # the log feeds san_return.py
python3 tools/serve.py --dir build/ubsan --diag logs/diag ...
node tools/web_run.mjs ... --evals 'Module._fsa_san_flush();fsaDiagFlush()@<end>'
python3 tools/san_summary.py logs/diag/<run>/sanitizer.jsonl [<run2>/sanitizer.jsonl] --skip alignment-wide
python3 tools/san_return.py build/ubsan-build.log > missing_return.jsonl
```

Each build also writes `fsa_symtab.json` next to `fsa.wasm` (post-link,
`tools/san_symtab.py` from `fsa.map`); the page fetches it on the first
finding. A run prints every new site to the console as
`[sanitizer] <kind> <file:line:col> in <function>: <detail>`.

### What each build does

`undefined`: the game units get `-fsanitize=undefined
-fno-sanitize=vptr,function,return -fsanitize-recover=all -Wreturn-type`.
`platform/sanitize/sanitize.cpp` is the whole UBSan runtime: it defines the
`__ubsan_handle_*` entry points clang calls, counts every hit per check site
and returns, so the game keeps running. No compiler-rt UBSan runtime is
linked.

- `vptr` and `function` need RTTI (the port has none).
- `return` cannot run: clang puts `unreachable` (a trap) after the handler,
  and the game does fall off the end of value-returning functions (the
  reason the build has `-fno-strict-return`). Missing returns are found at
  compile time instead: `tools/san_return.py` turns the `-Wreturn-type`
  warnings of a full build into records of kind `return-static`.
- `integer-divide-by-zero` and `unreachable` are reported, but the trap that
  follows them in wasm still happens.

`address`: the game units get `-fsanitize=address -fsanitize-recover=address`,
the link `-fsanitize=address -gsource-map`, 1 GB initial memory, and
`platform/sanitize/san_lib.js` (a fixed `emscripten_pc_get_file`: emscripten's
threw on source-map segments without a file, which killed the report).
Runtime options are compiled in (`__asan_default_options`):
`halt_on_error=0` (keep running), `detect_leaks=0`,
`detect_stack_use_after_return=0` (the fake stack does not survive the JSPI
coroutine switches), `max_malloc_fill_size=0` (fresh `malloc` memory stays
zero as in the normal build; Aurora relied on that) and
`malloc_context_size=0` (no allocation stacks: each costs a JS
`Error().stack` on wasm, and a stage load then took minutes).

The game carves all its heaps out of one big MEM1 allocation, so ASan alone
would only see that block. With `FSA_PORT_ASAN` (`game/port/fsa_asan.h`) the
JKR heaps take part: `JKRExpHeap`, `JKRSolidHeap` and `JKRUnitHeap` poison
everything they manage when created, unpoison exactly the bytes each
`alloc()` asked for (not the rounded-up block), poison a block again when it
is freed (`CMemBlock::free`, `freeAll`, `freeTail`, `JKRUnitHeap::do_free`),
and `JKRHeap::resize` moves the boundary. Those four units are built without
ASan so they can touch their own block headers. Game code then gets
overflow-past-request and use-after-free (or stale pointer into free heap
memory) checks on every JKR block; ASan reports those as
`use-after-poison`, and the record says which heap and block the address is
in (`fsa_jkr_describe` in `JKRExpHeap.cpp`):
`JKRExpHeap 0x...: block 0x... asked for 0x280 bytes (0x280 reserved); access at +0x284`,
`... free memory (freed block or never allocated); nearest live block below: ...`,
`JKRSolidHeap ...: unallocated (past the last allocation, or freed by freeAll/freeTail)`.

Not covered: memory allocated before the root heap exists or by host code
(plain ASan heap checks still apply there), objects inside one block (a
struct's array running into the next field: that is what UBSan's `bounds`
sees), uninitialised reads (the `uninit` stream), and stack use after return.

### Records

One line per distinct site in stream `sanitizer`:

```
{"key":"array-bounds|game/src/game/tile_map_802F5040.cpp:2115:16","kind":"array-bounds",
 "file":"game/src/game/tile_map_802F5040.cpp","line":2115,"col":16,
 "function":"TileMap_804AA858::getTile_802F9908(long, long)",
 "symbol":"_ZN16TileMap_804AA85816getTile_802F9908Ell",
 "sym":"getTile_802F9908__16TileMap_804AA858Fll",
 "unit":"game/src/game/tile_map_802F5040.cpp",
 "detail":"index 8 out of bounds for type 'u16 *[4]' (aka 'unsigned short *[4]')",
 "first_scene":{"mode":7,"stage":8,"level":3,"in_stage":1},"count":106559}
```

- `kind`: the UBSan check (`array-bounds`, `null`, `alignment`,
  `alignment-wide`, `object-size`, `shift-exponent`, `shift-base`,
  `signed-integer-overflow`, `integer-divide-by-zero`, `bool`, `enum`,
  `float-cast-overflow`, `pointer-overflow`, `nonnull-attribute`, `builtin`,
  `vla-bound`, `unreachable`), `asan-<bug>` (`asan-use-after-poison`,
  `asan-heap-buffer-overflow`, `asan-global-buffer-overflow`,
  `asan-null-pointer-dereference`, ...), or `return-static`.
  `alignment-wide` is a pointer that is 4-aligned where the type wants 8 (a
  `u64`/`double` member): the GameCube heaps hand out 4-aligned blocks, and
  on wasm that is harmless, so it is split from real misalignment
  (`alignment`).
- `file:line:col`: where the check is (for code inlined from a header, the
  header). Units with `#line` directives (JSystem assert lines) give those
  numbers.
- `function`: the function the check ran in, from the JS stack (the caller
  of the outermost `__ubsan_handle_*` / `__asan_report_*` frame); for a check
  inlined into another function, that outer function. `symbol` is its clang
  symbol, `sym` the decomp's (symbols.txt spelling: re-mangled the CodeWarrior
  way by `tools/diag_symtab.py`'s code, or the only symbols.txt function with
  that qualified name; `""` if neither), `unit` the source file it was
  compiled in.
- `detail`: the first hit's values. ASan: the access, ASan's own description
  of the address, and the JKR block, plus `stack` (the first frames).
- `first_scene`: the game's `fsa_uninit_scene` (`mode` = `GameRoot`
  `field_0x4c`, save `stage`/`level`, `in_stage`) at the first hit.
- `count`: hits so far (UBSan), or ASan reports at that line (ASan reports a
  given pc once). A record is posted at the first hit and again every 10 s
  while its count grows; keep the last line per `key`
  (`tools/san_summary.py` does, and merges several runs).

### Findings (2026-11-24)

Sessions as in the coverage section: boot -> title (intro movie) -> menus ->
adventure opening (260 s; ends in mode 7, stage 8 level 3) and `?stage=1`
with the stick held (200 s), each in both builds. Merged, without the 1695
`alignment-wide` sites: 39 run-time sites and 37 compile-time missing
returns. 1530 of the 1734 run-time records carry a decomp `sym`.

Port, platform and Aurora (a). The first four were fixed in the port
(separate commits); the sanitizer build found them:

- TEV-register animations (BRK; 3D models and 2D layouts): the port's
  in-place byte swap (`J3DAnmLoader.cpp`) treated the 0x1C-byte
  `J3DAnmCRegKeyTable`/`KRegKeyTable` entries as u16 arrays, which moved the
  u8 register id onto its 0xFF pad. Every register animation then used
  register 255: `J3DMaterialTable::entryTevRegAnimator` wrote past its
  4-entry arrays into the next `JKRSolidHeap` allocation
  (`J3DMaterialAnm.h:138-176`, ASan) and `J2DMaterial::setAnimation` past a
  `u16[4]` (`J2DMaterial.cpp:197`, UBSan index 255).
- The same swap ran past the end of an anim file when a block's `mSize` did
  (ASan: reads past a JKR block in `portSwapAnmBlock`; the swap writes there too). In an archive the
  bytes after the file are the next file's header: `yami_yougan.bmd`'s
  `'J3D2'` became `'2D3J'`, its model failed to load, and
  `loadModelWithAnims_8016A3BC` continued through a null `J3DModelData`
  (12 UBSan `null` sites in `J3DModel.cpp`, `J3DMtxBuffer.cpp`,
  `J3DAnimation.cpp`, `JUTNameTab.cpp`). Silent in the normal build (reads of
  address 0), a crash in the ASan build.
- `JASGlobalInstance<JAUSectionHeap / JAUSoundNameTable / JAUSoundTable>::sInstance`
  were declared weak (`createJASInstance` in `JAUSectionHeap.h`) and never
  defined: all three linked to address 0 and overwrote each other
  (`JASGadget.h:28`, ASan write to 0x0 in `AudioMgr_8045A080::init_80459AE4`).
  Now defined in `game/src/port/jsystem_statics.cpp`.
- Aurora's imgui stub never built a font atlas, so `ImGui::NewFrame` read
  `Fonts[0]` through a null vector every frame (a trap in the ASan build).
- Not fixed, a portability decision: float-to-unsigned casts of negative or
  huge values. PowerPC converts to s32 (`fctiwz`) and keeps the low bits;
  wasm saturates (to 0 for a negative value). `JPAParticle.cpp:98`
  (`mRotateAngle = <float>`, e.g. -2305.4 -> u16: 21k hits per opening, so
  particles start unrotated), `JPABaseShape.cpp:422` (-1.07e8 -> s16),
  `J2DPrint.cpp:536` (-259.5 -> u16). Writing `(u16)(s32)x` gives the
  GameCube result.
- `alignment-wide` (1695 sites): `J2DScreen`, `J2DPane`, `J2DPicture`,
  `J2DTextBox`, `PaneCtrl_804AF86C`, THP threads ... want 8-byte alignment
  (a `u64` pane tag or a `double`) and live in 4-aligned JKR blocks. Harmless
  on wasm; the same placement as on the GameCube.

Decomp (b): wrong array sizes, types and initialisers. Each is a
`file:line` and a `sym` in the merged records.

- `JASDsp::TChannel::_10` is `Mixer[4]` but indexed up to 5
  (`JASDSPInterface.cpp:387, 403, 457`: `setMixerInitVolume`,
  `setMixerVolume`, `setBusConnect`, 1.3 M hits per session). TP's has 6
  mixers. The writes land on the following `TChannel` fields, which the DSP
  side reads.
- `JASTrack::setIIR` copies 8 `s16` (`mIIR[8]`) from
  `JASCalc::CUTOFF_TO_IIR_TABLE[128][4]` rows (`JASTrack.cpp:558`, ASan
  global-buffer-overflow at row 127; `cmdIIRSet` passes an `s16[4]` too).
  Either the loop/`mIIR` or the table width is wrong.
- `JASDsp::setFXLine`: `SEND_TABLE[config->_02]` with `_02 = 0x6000`
  (`JASDSPInterface.cpp:211, 213`). The `fxConfig[4]` initialisers in
  `audio_mgr_80457DFC.cpp` (~line 448) do not fit `FxlineConfig_`'s fields:
  0x6000 looks like a volume in the send-index slot.
- `TileMap_804AA858::mLayerGrid` is `u16*[4]` but indexed 8+
  (`tile_map_802F5040.cpp:2089, 2115`, 100 k hits): layers 8 and up run on
  into `mLayerGrid2` and `field_0x48[16]` (`Map.h:461`). Layout-correct, so
  the behaviour is right; one 24-entry array would say what the code does.
- `J2DMaterialFactory.cpp:441`: `mIndTevStageInfo[p2]` with `p2 = 5` on a
  `J2DIndTevStageInfo[4]` (the material data has more entries than the
  declared array).
- `J2DWindowEx.cpp:44-48`: the 4-iteration loop indexes a `u16[2]`
  (`_158` or `mContentIds`).
- `gba_window_80432638.cpp:65, 72, 92, 99`: `f32[2]` indexed 2 and 3
  (`field_0x70` / `field_0x80` want 4).
- `pane_ctrl_804023D0.cpp:149, 217`: `field_0x249` is `u8[1]`, used as `[2]`.
- `message_window_80390818.cpp:420`, `pageSound`: index -1 into
  `field_0x3b2[3]`.
- Enum ranges: `JUTTransparency` has 2 values but 2 is loaded
  (`JUTPalette.cpp:50`, `JUTPalette.h:18`); `JASSeqParser::BranchCondition`
  is declared empty (`JASSeqParser.cpp:124`).
- Missing returns (`return-static`, 37): among them `J2DPane::getRotate`,
  `JPABaseParticle::getWidth/getHeight` (f32), `J2DTextBoxEx::setBlack/setWhite`
  (bool), 11 in `JMessage/processor.cpp`, the empty `JASDSPChannel` getters,
  `EscortBase_8049E0C8::vfunc_0x84/0x88`, `fn_802340A0` (`tu_8022AD64.cpp:2500`).

Benign or intended (c):

- Left shifts of negative values: `THPAudio.c:93-94, 127-128` (ADPCM decode,
  500 k hits per movie) and `tile_map_802F5040.cpp:62` (`getDrawOffset`).
- Signed overflow of tick differences: `JUTVideo.cpp:67`,
  `JUTProcBar.h:35` (`OSTick` wraparound).
- `tu_80301CD0.cpp:277, 363`: `lbl_8053AD68->fn_80379F84()` on a null
  `lbl_8053AD68`; the callee only tests that global and `root`, as in the
  original.
- `JASHeapCtrl.cpp:183`: `&next->mTree` with `next == NULL` (`mTree` is at
  offset 0).

Nothing in these sessions crashes the normal build. Not reached: the
ConnectScreen 4-row loop over `lbl_80479920[3][4]`
(`connect_screen_803F0A54.cpp:2640`) needs `field_0x1ac == 3`, a screen
these sessions do not show.

### Cost

Stage 1: game time 3.2 ms/frame with UBSan and 4.6 ms/frame with ASan, against
3.0 in the normal build (+7% and +55%); the opening is within noise. Both keep
60 fps. `fsa.wasm` is 15 MB (UBSan) and 18 MB plus a 4 MB source map (ASan)
against 8 MB. The ASan build starts with 1 GB of memory (the shadow is 1/8 of
the 2 GB maximum).
