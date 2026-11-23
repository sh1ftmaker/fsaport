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
  is based on `cde64f65e` (the first commit here is that tree unmodified), so
  upstream's improvements can be merged later by diffing the two.

## Toolchain and flags

emscripten 5.0.6 (`../fsadecomp/tools/emsdk`). `tools/compile_check.py` adds to
each unit's own flags (from `game/units.txt`):

| Flag | Why |
|---|---|
| `--std=gnu++17` / `gnu99` | GNU modes: no trigraphs (the game spells resource types `'????'`, and `??'` is the trigraph for `^`). C++17 for the `BE<T>` file-struct wrapper (`game/libs/sdk/include/port/be.h`); `register` is allowed with `-Wno-register`. |
| `-fno-strict-return` | MWCC-era functions that fall off the end without a return were compiled as unreachable code and hung. |
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

### Audio

Not audible yet. JAudio initialises fully (its file formats read
big-endian), but there is no DSP: the DSP boot handshake is skipped and DSP
commands complete at once. Next: a host mixer in place of the DSP (decode the
ADPCM channels JAudio sets up, mix, output through Web Audio).

## Classes of bug found while booting

Worth checking for in any code that misbehaves:

- **5-8 character literals** (`'NIN_Logo'`): MWCC makes them 64-bit; clang keeps
  4 bytes. `tools/fix_tag64.py` rewrote 4,475 of them as `FSA_TAG64("...")`.
- **Raw big-endian reads** (`u32 info[2]; read(info, 8)`): any file field read
  into a plain integer is wrong on wasm.
- **Functions without a return**: `-fno-strict-return` (above).
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

## Changes to Aurora

- Indexed XF loads (`GX_LOAD_INDX_A..D`) read a 1-byte index, consumed 5 bytes
  instead of 4, picked the wrong array and used the stream's endianness for the
  matrix data (`lib/gx/command_processor.cpp`).
- Browser staging: a mapped staging buffer only maps after the previous frame's
  present, so `begin_frame` waited ~27 ms every frame (30 fps). The web build
  stages in host memory and uploads with `Queue::WriteBuffer`/`WriteTexture`
  (`lib/gfx/common.cpp`).

## Current state

- Boots in Chrome to the title screen: JFW, heaps, threads, the loader thread,
  archive mounts, audio init, the Nintendo logo, GameRoot with its 3D models,
  particles and messages, and the title drawn by the game's own code.
- Speed: 60 frames per 60 retraces at the title; ~0.3 ms of CPU per frame
  (release build, FSA_TRACE and FSA_DEBUG_LINES off).
- Next: keyboard/gamepad input does not advance the title yet; audio output;
  saves (Aurora's card in MEMFS is lost on reload); GBA views; THP movies.
- Debugging: `FSA_TRACE=ON` enables `[trace]`/`[heap]`/`[blo]` reports;
  `tools/web_cycle.sh` rebuilds and runs once in a private Chromium over CDP;
  `tools/wasm_where.sh` symbolises offsets; `fsa_debug_stack()` prints a stack.

## Open questions

- **MSL headers or emscripten's libc headers.** The units compile against the
  decomp's MSL headers (`-nostdinc`). Linking against emscripten's libc wants
  its headers; MSL's `FILE`, `va_list` and `NAN`/`INFINITY` (via `__float_nan`)
  differ. Switching is probably right but touches every unit.
- **Pointer casts to `u32`** are everywhere (`(u32)ptr`). Fine on wasm32; a
  64-bit desktop build needs them fixed (katam-port's `p32.h` approach).
