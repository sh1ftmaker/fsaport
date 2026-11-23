# fsaport

A port of **The Legend of Zelda: Four Swords Adventures** to WebAssembly (and
later native desktop), built from the
[sh1ftmaker/fsa](https://github.com/sh1ftmaker/fsa) decompilation. It is a hard
fork in the way katam-port is: the decompiled sources live here and are changed
freely for the port, and the GameCube's hardware is replaced, not emulated.

**This repository contains no game data.** You supply your own disc. The few
blobs the sources include directly (two fonts, a message file, a DSP microcode
fragment) are extracted from your own `main.dol` into `build/`, which is never
committed.

## Status

| Stage | State |
|---|---|
| Game code (528 units) compiles for `wasm32` | yes, 528/528 |
| JSystem (200 units) compiles for `wasm32` | yes, 200/200 (1 unit has no source upstream, see docs/PORTING.md) |
| Links | no: 489 undefined symbols, listed by `tools/undef_report.py` |
| Platform layer (GX via Aurora, OS, DVD, PAD, audio, card, GBA) | not started |
| Boots | no |

## Build (compile check)

```sh
source ../fsadecomp/tools/emsdk/emsdk_env.sh          # emscripten 5.0.6
python3 tools/extract_assets.py path/to/main.dol build/assets
python3 tools/compile_check.py                         # build/obj/*.o + build/compile_report.txt
python3 tools/undef_report.py > build/undefined.txt    # what the link still needs
```

`make compile DOL=path/to/main.dol` runs the first three steps.

## Layout

| Path | What |
|---|---|
| `game/` | The decomp's `src/`, `include/` and `libs/` at the paths the decomp uses, so its include flags apply unchanged. The first commit is the unmodified import; every later change is a port change. |
| `game/units.txt` | Every unit the port compiles, with its include paths and defines (from the decomp's `compile_commands.json`). |
| `game/port/` | Headers force-included into every unit: host versions of Metrowerks intrinsics and attributes. |
| `platform/` | The port's replacement for the GameCube SDK (empty so far). |
| `tools/` | Build and porting scripts. |
| `docs/PORTING.md` | Every kind of change made to the sources and why, and what comes next. |

## Licenses

The decompiled SDK and JSystem sources are CC0 (`game/LICENSE.sdk`,
`game/LICENSE.jsystem`, from doldecomp/mkdd and the other donors listed in the
decomp). The decomp itself states no license.
