# Porting notes

## Goal and ground rules

- **A source port, not an emulator.** The game's own C++ runs natively (wasm32
  first); the GameCube SDK is replaced by a platform layer. Correctness means
  "the game behaves right", not "matches the GameCube": no hardware, register,
  timing or memory-layout fidelity is a goal anywhere. Structures may grow
  (Aurora's GXTexObj is 64 bytes), heaps are larger, floats are IEEE without
  FMA, threads are cooperative.
- **GBA support stays, GBA hardware does not.** The four-player modes and
  Shadow Battle keep their per-player screens, but they are rebuilt natively
  instead of being bound to the joybus protocol, GBA timing or the GBA client
  program. How exactly is open (see "Next" below).
- **No recompilation oracle.** The static recompilation in
  `../fsadecomp/recomp/` is too slow and too buggy to be a reference; tests here
  are the port's own (state invariants, scripted playthroughs, replays recorded
  in the port itself).
- **Upstream keeps moving.** The decomp is still matching functions. This fork
  is based on `cde64f65e` (the first commit here is that tree unmodified);
  upstream's behaviour fixes are pulled by hand (below), and `DECOMP_SYNC`
  records the decomp commit pulled up to.

## Syncing with the decomp

`DECOMP_SYNC` holds the decomp commit the port's game code is synced to (its
`matching` branch, the only one to pull from: every commit there passed the
decomp's checks and review; `grind2` and the worktrees under `fsa-wt/` can hold
commits that publishing later refuses). A sync pulls the decomp's *behaviour*
changes since then, not its matching work:

1. `python3 tools/decomp_sync.py` writes `build/decomp-sync/`: the changed
   files (`files.tsv`, with whether the port has changed each one and whether
   a 3-way merge conflicts), the commits (`log.txt`), a first screen of hunks
   whose literals changed (`screen.txt`: tags, constants, offsets) and 3-way
   merge candidates (`merged/`, reference only).
2. Read `git -C ../fsadecomp/fsa-decomp diff <DECOMP_SYNC> matching -- <file>`
   for every file. Skip what only changes the generated code (register
   allocation, independent statement order, renames, temporaries, inlining,
   `bool`/`BOOL` with the same truthiness). Port by hand what changes
   behaviour: resource and pane tags, constants and tables, which field is read
   or written, branches and conditions, argument order, call targets, virtual
   or not, signedness or width that changes arithmetic, newly written functions.
   Commit subjects do not tell: grind commits ("match lane, grind") have carried
   behaviour fixes (the map screen's `'Tmt01'` -> `'Tmmt01'`).
3. Keep the port's adaptations: `FSA_TAG64("...")` for 5-8 character literals,
   `BE<T>` / `be_read_*` for file data, the bit-field accessors,
   `gc_f32_to_u16`, `gc_div_s32`, `#ifdef __MWERKS__` / `FSA_PORT` blocks.
   The decomp's own portable bodies (`#else` of MWERKS-only asm) are compared
   with the port's, not pulled. Layout changes of structs that the platform
   layer overlays by offset (`JASDsp::TChannel` in `platform/audio_dsp.cpp`)
   need the platform side changed with them.
4. Check with the decomp's harness scripts (`castle`, `stage_1_0`; docs/HARNESS.md)
   against a run of the previous build and against the decomp's Dolphin runs
   (`harness_compare.py --anchor scene --ftol 1e-5`); then write the new
   commit into `DECOMP_SYNC`.

## Toolchain and flags

emscripten 5.0.6 (`../fsadecomp/tools/emsdk`). `tools/compile_check.py` adds to
each unit's own flags (from `game/units.txt`):

| Flag | Why |
|---|---|
| `--std=gnu++17` / `gnu99` | GNU modes: no trigraphs (the game spells resource types `'????'`, and `??'` is the trigraph for `^`). C++17 for the `BE<T>` file-struct wrapper (`game/libs/sdk/include/port/be.h`); `register` is allowed with `-Wno-register`. |
| `-fno-strict-return` | MWCC-era functions that fall off the end without a return were compiled as unreachable code and hung. |
| `-Werror=return-type` | ...but the value such a path returns is still undefined, and MWCC callers read what was left in r3/f1. A missing return is a build error; write the return the original has (see "Functions without a return" below). Every other warning stays off (`-Wno-everything`), except: |
| `-Werror=return-stack-address`, `=sometimes-uninitialized`, `=infinite-recursion`, `=array-bounds` | the other undefined behaviour MWCC's codegen made work by accident; build errors (see "Other undefined behaviour" below). |
| `-gline-tables-only` | `tools/wasm_where.sh` maps a Chrome stack offset to a source line. |
| `-nostdinc` | the units use the decomp's MSL headers for now (see "Open questions"). |
| `-fsigned-char` | wasm's default too; stated so the 64-bit/ARM builds agree. |
| `-fno-rtti -fno-exceptions` | the game was built with `-RTTI off`; it has no `try`/`throw`. |
| `-ffp-contract=off` | no fused multiply-add, so every build computes the same floats (netplay). MWCC fused (`-fp_contract on`); exact Gekko results are not a goal. |
| `-include game/port/port_prefix.h` | host intrinsics and attribute shims, below. |

No leniency flags. `-fms-compatibility` would accept MWCC's template lookup but
it undefines `__GNUC__`, which silently changes every `#if defined(__GNUC__)`
path in the headers; `-fdelayed-template-parsing` hides errors until
instantiation. The headers are fixed instead.

## Kinds of change made to the sources

Every change keeps the original under `#ifdef __MWERKS__` where it was code
(asm), so the matching form stays visible.

1. **Paired-single and other inline asm → C** (`JMath.h`, `JMath/Inline.h`,
   `JMath.cpp`, `OSFastCast.h`, `J2DAnimation.h`, `J3DStruct.h`). Each is a
   transcription of the asm's operations in the same order.
   `JMAMTXApplyScale`'s commented-out C was not what the asm does (it scaled
   row 2 by `zScale` throughout); the port follows the asm.
2. **Metrowerks intrinsics** (`__fres`, `__frsqrte`, `__fabs`, `__cntlzw`,
   `__dcbz`, `__memcpy`, `__abs`, ...) are `static inline` functions in
   `game/port/ppc_intrinsics.h`. `__fres` and `__frsqrte` are exact here where
   Gekko's are 12-bit estimates.
3. **`__declspec(weak)` / `__declspec(section ...)`** are dropped by a macro in
   `port_prefix.h`; they only steer the Metrowerks linker.
4. **Two-phase name lookup**: members of dependent base classes get `this->`
   (`JSUList.h`, `Quat.h`), dependent types get `typename` (`enumerator.h`,
   `JASHeapCtrl.h`), types used before their typedef get their template name
   (`Quat.h`, `Vec.h`). `TBox`'s copy constructor read a member that does not
   exist (`other.y`); MWCC never instantiated it.
5. **Jumps past initialisation**: MWCC lets a `case` label skip an initialised
   local. Where only POD locals were skipped, `T x = e;` became `T x; x = e;`
   (`tools/fix_switch_bypass.py`); elsewhere the case body got braces
   (`tools/brace_case.py`).
6. **clangd hacks that would have been live code**: MSL `math.h` defined
   `__fabs(x)` and `__frsqrte(x)` as `(x)` for any non-MWCC compiler, and
   `JASTrack.h` removed `mMove`'s union for clang. Both now apply only outside
   the port (`FSA_PORT`). `MoveParam_` lost its constructor in the port so it
   can sit in that union (C++98); `JASTrack`'s `mMove()` already zeroes it.
7. **Endianness of the fdlibm word macros** (`__HI`/`__LO`): the MSL copy only
   knew x86 as little-endian; it now checks `__LITTLE_ENDIAN__`.
8. **Small C++ conformance fixes**: case-mismatched and backslash `#include`
   paths, lvalue casts (`*((s16 *)p)++`) in `JASCalc.cpp`, `void *` arithmetic
   in `JKRSolidHeap.cpp`, a missing `template <>`, private base classes whose
   `operator new` callers need public access (`JAISe`, `JAISoundChild`), a
   forward declaration for the `JASKernel.h` / `JASHeapCtrl.h` include cycle,
   `std::cosf` → `cosf`, a missing `<algorithm>`.

## Known gaps in the sources

- `JSystem/JStudio_JParticle/object-particle.cpp` has no source upstream
  (a `NonMatching` unit with no file); it is left out of `units.txt`.
- About 250 game functions are below 95 % match upstream and may differ in
  behaviour; 2 game functions have no source (see the decomp's honest report).
- `fn_80093280` (JMath's fast vector magnitude) is not written upstream either.

## Running in the browser

Built with CMake (`CMakeLists.txt`): one static library per flag group of
`game/units.txt`, the platform layer, Aurora and its dependencies, linked with
JSPI. See README for the commands.

### Threads: JSPI coroutines (platform/os_thread.cpp, platform/web/fsa_sched.js)

Every OSThread is a coroutine: a JSPI promising export (`fsa_thread_entry`) gives
it its own wasm stack, and it gets its own region of emscripten's shadow stack
(saved and restored around each switch). A switch suspends the current
coroutine on a promise and resumes the target from a microtask, so only one runs
at a time. Scheduling is by priority like the SDK's, but cooperative: a thread
runs until it blocks or an OS call readies a higher-priority thread.

- **Interrupts** (VI retrace, alarms, DVD completion) run from the page's event
  loop as `fsa_retrace` / `fsa_timer`. They only make threads ready; if every
  thread is blocked the scheduler is idle and the interrupt resumes one.
- **Polling**: calls the game polls in loops (`DVDGetDriveStatus`,
  `VIGetRetraceCount`, ...) are poll points: after ~8 ms without blocking the
  thread sleeps 1 ms so the page's event loop can run.
- **Starvation**: switches are microtasks, which the browser drains before any
  task; every ~8 ms a switch goes through a MessageChannel task instead so
  rendering, input, timers and DevTools still get turns.
- No Asyncify, no SharedArrayBuffer, no pthreads. Needs `WebAssembly.Suspending`
  (Chrome/Edge 137+).

### Memory and allocation

Aurora allocates MEM1 (64 MB; `OS_BASE_CACHED` is its base, boot info included)
and ARAM (16 MB). JSystem's global `operator new/delete` replace C++'s for the
whole module, so they route by caller: host code (libc++, Aurora, the platform
layer) marks itself with `HostAllocScope` and gets `malloc`; game code gets the
current JKR heap; `delete` frees by address (inside the root heap -> JKR).

### Disc (platform/dvd.cpp, platform/web/fsa_disc.js)

The user's image is read from the page (a dropped `File`, or a URL with HTTP
Range requests) in 1 MiB blocks with an LRU cache; nothing is uploaded. Paths
resolve through the disc's FST. Reads suspend the calling coroutine; async
reads go through a platform worker thread.

### Video and graphics (platform/vi.cpp, platform/glue/gx_pipe.cpp)

- Retraces come from `requestAnimationFrame`, paced to 59.94 Hz whatever the
  display's refresh rate. JUTVideo's retrace callbacks and message queue drive
  the game's own frame pacing.
- One game frame is one Aurora frame: `JFWDisplay::beginRender` calls
  `aurora_begin_frame`, `JFWDisplay::endFrame` calls `aurora_end_frame`.
- GX calls go to Aurora (GX -> WebGPU via emdawnwebgpu). The write-gather pipe
  (`GXWGFifo`) is a proxy object whose stores append big-endian to Aurora's
  command FIFO, so inline vertex writes and J3D's raw command writes work.
- `GXSetArray` takes Aurora's PC form (size, endianness) at every call site.

### Touch controller (platform/web/touch.js, platform/input.cpp)

A GameCube pad on screen, in the style of katamport's: analog stick (the knob
follows the thumb, a wedge shows the direction), A/B/X/Y in the GameCube
arrangement, L/R/Z and Start. In portrait the picture sits above a band that
holds the pads; in landscape they sit beside the 4:3 picture and fade back
until touched. On by default where the primary pointer is coarse, toggled with
"Pad" and remembered. Hit-testing is by coordinate from cached geometry with a
slop margin (nearest centre wins), every finger is tracked on its own, and
blur / backgrounding / the iOS `touches` backstop release everything. The page
calls `fsa_touch_set`; `fsa_pad_after_read` (called by `JUTGamePad::read`
right after `PADRead`) merges it into port 0: buttons ORed, stick and analog
triggers from whichever source is pushed further. `tools/web_touch_test.mjs`
drives it as an emulated phone over CDP.

### Audio

JAudio runs as on the GameCube up to the DSP; the DSP's mixing and the AI's
DMA are replaced:

- **AI** (`platform/audio.cpp`): an OS alarm stands in for the AI DMA
  interrupt. Each one hands the block JAudio programmed last (`AIInitDMA`,
  560 frames of 32 kHz stereo s16, right sample first) to the page and calls
  JAudio's DMA callback, so the audio thread runs `JASDriver::updateDac` once
  per block (57.14 per second). The alarm is paced by the page's audio clock
  to keep ~50 ms queued ahead of playback. It does not fire again until the
  audio thread has programmed the next block.
- **DSP** (`platform/audio_dsp.cpp`, ported from Dusklight's CPU DSP, CC0):
  `JASDriver::finishDSPFrame` (FSA_PORT) renders the frame's 7 subframes at
  once. After each `updateDSP`, which is where the DSP would have mailed the
  audio thread, it mixes the 64 `JASDsp::TChannel` voices: ADPCM4/ADPCM2/
  PCM16/PCM8 from ARAM and the oscillators, linear resampling by pitch, the
  voice's one-pole and biquad filters, and mixer-volume ramps to the L
  (0x0D00) and R (0x0D60) buses. It writes back the feedback fields JAudio reads (finished,
  samples left, ARAM position, reset flag). `DsetupTable` and `DsyncFrame`
  hand it the voice table and the output buffers. Not done: the FX
  (reverb/echo) lines and the Dolby surround buses (Dusklight substitutes
  Freeverb for the former).
- **Page** (`platform/web/fsa_audio.js`): each block becomes an
  `AudioBufferSourceNode` scheduled back to back on a 32 kHz AudioContext.
  The context starts on the first key, touch or click. Until it runs (or with
  `?audio=0`), blocks are dropped and the clock is wall time, so JAudio and
  the movie player (which times video by its audio) keep their pace.
  `fsaAudio.stats()` / `?audiolog=1` give block rate, levels (sent and
  playing), underruns and lead. With `?audiorec=S`, `tools/web_run.mjs --wav F`
  saves what the game sent.
- Movies mix their sound into the DMA block through
  `JASDriver::registerMixCallback`, as on the GameCube.

Endianness fixes this needed: unions that read a host integer through
big-endian byte or bit-field views. `JAISoundID` (its section/group bytes and
short ID choose the sequence: the BGM never loaded), `JASChannel::MixConfig`
(the bus and pan selectors), `JAUAudibleParam`, and the note flags in
`JASSeqParser::execNoteOnGate`.

## Classes of bug found while booting

Worth checking for in any code that misbehaves:

- **5-8 character literals** (`'NIN_Logo'`): MWCC makes them 64-bit; clang keeps
  4 bytes. `tools/fix_tag64.py` rewrote 4,475 of them as `FSA_TAG64("...")`.
- **Raw big-endian reads** (`u32 info[2]; read(info, 8)`): any file field read
  into a plain integer is wrong on wasm.
- **Functions without a return**: `-fno-strict-return` (above) keeps the path,
  `-Werror=return-type` makes it a build error. Read the original's asm
  (`../fsadecomp/fsa-decomp/build/G4SE01/asm/...`) for what is in r3 (f1 for
  floats) at its `blr`: often the last call's result (`return J2DPane::animationPane(...)`)
  or the first argument (`fn_802340A0` returned `player`; dropping it lost the
  player's Link in the GBA views). Functions stripped from the DOL get any
  plain value. A switch that misses cases can return a register the function
  never set (`BigMoldorm init`: r8 from the caller); match that in the port.
- **Other undefined behaviour** (each class a build error, flags above):
  - *A pointer to a local returned* (`fn_8022DFD4`, `ZoraEscortGoalList::fn_80127F90`):
    MWCC callers copied the vector out of the dead frame; clang dropped the
    stores, so the callers read stale stack. Return static storage holding the
    same value.
  - *A local read on a path that never set it*: read the asm for the register
    or stack slot on that path. Usually the path cannot happen (say so in a
    comment and initialise to anything); otherwise it is a callee-saved
    register still holding the caller's value (`fn_802710C8`'s `requested` is
    r29 = `hitPlayerGroup_80238548`'s `hit`, handed over in a port global;
    `BuriedForceGem` serial: r31, a negative pointer, so no gem; `HammerBulge`
    TKTA: r28, the actor manager's `paused`).
  - *Indexing past an array onto the next field* (`f32 field_0x70[2]` written
    at [2], [3]): name the field it lands on. The layout stays as is.
  - *A function calling itself* (`TResourceContainer::getResource_groupID`
    const): call the overload it meant.
- **Return-type mismatches between a declaration and the definition**: C++
  mangling hides them, `wasm-ld` turns the call into a trap or emits invalid
  code. Fixed where found (`fn_8017EC5C`, `CARDInit`, `GXSetArray`).
- **Fixed addresses** (`0x80700000` save region, `0x80000028` boot info): out
  of bounds on wasm.
- **Busy-waits on hardware** (DSP mail, `Dsp_Running_Check`): spin forever in a
  cooperative scheduler; replace the hardware wait, don't just stub it.
- **Hardware tricks Aurora does not emulate**: `JFWDisplay::clearEfb` reset depth
  with a Z texture, so every later depth-tested 2D draw failed (black title).
  The port clears without writing depth; Aurora clears depth each frame.
- **Game structs over file data**: the game's own BMG structs (`Message.h`) were
  host-endian; other game formats will be the same.
- **Byte-wise writes read back as words**: `JKRUnitHeap`'s allocation bitmap is
  written a byte at a time (MSB first) and searched as `u32` from bit 31, a
  big-endian load. On wasm every allocation got the same block, so the actor
  manager overwrote each actor with the next (black stage, no actors).
- **Reads past a table that rely on the original layout**: `ConnectScreen`'s
  4 x 6 loop over a 3 x 6 table read the next table on the GameCube and garbage
  here (null pane, trap). Bounded under `FSA_PORT`.
- **Port changes that contradict each other**: `jsystem_statics.cpp` defined the
  BMG signature byte-swapped for a `memcmp`, while the JMessage port compares the
  big-endian word with the plain value, so no BMG ever parsed into JMessage
  (message windows opened with garbage state).
- **Per-draw texture re-inits**: free on the GameCube, but every `GXInitTexObj` is
  a new texture to Aurora. `JUTResFont` re-inited its glyph page per character
  (1,750 draws, 31 ms/frame and a GPU out-of-memory in the prologue); the port
  keeps one texture object per page.

## Changes to Aurora

- Indexed XF loads (`GX_LOAD_INDX_A..D`) read a 1-byte index, consumed 5 bytes
  instead of 4, picked the wrong array and used the stream's endianness for the
  matrix data (`lib/gx/command_processor.cpp`).
- Browser staging: a mapped staging buffer only maps after the previous frame's
  present, so `begin_frame` waited ~27 ms every frame (30 fps). The web build
  stages in host memory and uploads with `Queue::WriteBuffer`/`WriteTexture`
  (`lib/gfx/common.cpp`).
- `SIProbe` reports a keyboard-bound port as a controller, as `PADRead` already
  treats it (`lib/dolphin/si/si.cpp`); FSA's GBA client probes before reading
  and dropped keyboard input otherwise.
- The texture-object cache drops entries unused for 300 frames
  (`sweep_texture_cache`, `lib/gx/gx.cpp`): every `GXInitTexObj` is a new
  identity, so games that re-init per frame grew it without bound.
- Converted palette (CI) textures are also cached by content: data address,
  layout and a hash of the texel and TLUT bytes (`s_paletteContentCache`,
  `lib/gx/gx.cpp`). `TileMap::drawBlockMap_802F6CF4` re-inits its TLUTs and
  texture object before every draw, so each flying floor tile (stage 8-1)
  converted and uploaded the 128 x 512 tile sheet again (35 ms a frame, then
  a hang).
- Host-allocation scopes: every GX/GD/PAD/VI/SI/CARD entry point the game
  calls, plus Aurora's logging, command push, texture creation and command
  processor, holds `AURORA_HOST_ALLOC` (`lib/host_alloc.hpp`, from deluxe
  ff1a10d). Before, the allocations Aurora made inside them (pipelines, command
  vectors, texture caches, card I/O, log strings) went through JSystem's
  `operator new` into the game's current JKR heap. That took MEM1 from the
  game, fragmented it, and could leave Aurora objects inside a heap the game
  later destroys or resets.
  `?allocprobe=1` checks it (docs/DIAGNOSTICS.md).
- `OSGetTime` takes its time from `aurora_os_time_hook` when it is set: the
  behaviour harness's virtual clock (`lib/dolphin/os/OSTime.cpp`,
  docs/HARNESS.md).

## Current state

- Plays in Chrome from boot through the title, mode select, file select, player
  count and the new-quest opening (the prologue narration), all drawn by the
  game's own code. Keyboard input works (Enter = Start, Space = A, Shift = B,
  WASD = stick).
- Speed (release build): 60 frames per 60 retraces throughout; game logic
  <= 1.4 ms and Aurora <= 2.4 ms per frame, including the 1,750-draw prologue.
- Blocker: after the prologue the narration opens message 0x231 (window 0
  reaches state 8) but `stepMessageWindow_803865B8` never finishes it, so the
  demo holds at tick 281 on a dark screen (the shadow-circle mask at 0 is
  expected there; the window itself is not visible).
- Next: that message window; saves (Aurora's card in MEMFS is
  lost on reload; the game offers "continue without saving"); GBA views; THP
  movies (title attract and menu previews are black).
- Debugging: `FSA_TRACE=ON` enables `[trace]`/`[heap]`/`[blo]` reports;
  `tools/web_cycle.sh NAME SECONDS EVERY KEYS EVALS` rebuilds and runs once in
  a private Chromium over CDP, pressing keys and evaluating JS at given times;
  from the page console, `Module._fsa_debug_state()` prints the game mode,
  stage, fade, message windows and live actors, `Module._fsa_dump_threads()`
  the threads, and `Module._fsa_set_dvd_log(1)` every file opened and every
  failed archive lookup; `[pad]` lines show port 0's buttons as they change;
  `tools/wasm_where.sh` symbolises offsets (`-DFSA_DEBUG_LINES=ON`).

## Open questions

- **MSL headers or emscripten's libc headers.** The units compile against the
  decomp's MSL headers (`-nostdinc`). Linking against emscripten's libc wants
  its headers; MSL's `FILE`, `va_list` and `NAN`/`INFINITY` (via `__float_nan`)
  differ. Switching is probably right but touches every unit.
- **Pointer casts to `u32`** are everywhere (`(u32)ptr`). Fine on wasm32; a
  64-bit desktop build needs them fixed (katam-port's `p32.h` approach).
