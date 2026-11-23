# Porting notes

## Goal and ground rules

- **A source port, not an emulator.** The game's own C++ runs natively (wasm32
  first); the GameCube SDK is replaced by a platform layer. Correctness means
  "the game behaves right", not "matches 2003 hardware": no register-level or
  cycle-level fidelity is a goal anywhere.
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
| `--std=gnu++98` / `gnu99` instead of `c++98` / `c99` | strict modes enable trigraphs, and the game spells resource types `'????'` (`??'` is the trigraph for `^`). MWCC has no trigraphs. |
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

## Next: the link (489 undefined symbols)

`python3 tools/undef_report.py` lists them. In groups:

| Group | Count | What to do |
|---|--:|---|
| Dolphin SDK | 290 | The platform layer: GX (106) through Aurora's GX API; OS (61: threads, heaps, time, interrupts); CARD, DVD, VI, PAD, DSP/AI/AR (audio), GD, GBA (6), and the PSMTX/PSVEC math (C versions). |
| Game data with no definition | done | 139 globals the matching build takes from dtk's split of the DOL. `tools/gen_globals.py emit main.dol build/generated` writes `build/generated/globals.c` (never committed: it is game data) from the committed `tools/globals_layout.json` (names, addresses, sizes, element widths, pointer targets; no bytes). Each object is a byte image of the DOL's bytes made little-endian per element; pointer tables are typed so they relocate. `units.txt` names it `@GENERATED@/globals.c`. Regenerate the layout with `gen_globals.py layout` when units stop defining or start defining such symbols. |
| JSystem / game functions | done | `game/src/port/jsystem_statics.cpp` (the `JASGlobalInstance<T>::sInstance` definitions MWCC inferred from declarations, JMessage signatures, `J3DPSMtxArrayConcat`, `JKRFileCache::getDvdPathName`, `JKRThreadSwitch`, `JSUOutputStream::_0C`) and `game/src/port/missing_functions.cpp` (`fn_80093280`, EscortBase's vtable slots 0xA0/0xA4); `JASWaveBank.cpp` added to `units.txt`. |
| C library / C++ runtime | 26 + 3 | Link against emscripten's libc (see "Open questions") and libc++abi. |
| Cache maintenance | ~30 | `DCStoreRange`, `DCInvalidateRange`, ...: no-ops on the host. |

Also before anything runs:

- **MMIO addresses**: `dolphin/hw_regs.h` and the inline vertex writes in
  `dolphin/gx/GXVert.h` (`GXWGFifo`, `0xCC008000`) point at GameCube hardware.
  The GX inlines must call Aurora instead; nothing may touch `0xCC00xxxx`.
- **Big-endian data**: every disc format (RARC, Yaz0, BMD/BDL, BTI, BLO, JPA,
  BMG, BFN, audio banks, FSA's stage/map/actor formats) is big-endian and wasm
  is little-endian. The JSystem loaders can follow dusk (TP's port); the game's
  own formats need their own swapping.
- **Threads**: the game runs several OS threads. The plan is cooperative
  threads on one wasm thread (JSPI), as the recompilation does in the browser.
- **GBA**: the GBA screens were drawn by a separate GBA program from a command
  stream. The port replaces that with native rendering of each player's view;
  the design is open (draw the sub-area with the TV renderer as a per-player
  view, or reimplement the client's renderer from the stream).

## Open questions

- **MSL headers or emscripten's libc headers.** The units compile against the
  decomp's MSL headers (`-nostdinc`). Linking against emscripten's libc wants
  its headers; MSL's `FILE`, `va_list` and `NAN`/`INFINITY` (via `__float_nan`)
  differ. Switching is probably right but touches every unit.
- **Pointer casts to `u32`** are everywhere (`(u32)ptr`). Fine on wasm32; a
  64-bit desktop build needs them fixed (katam-port's `p32.h` approach).
