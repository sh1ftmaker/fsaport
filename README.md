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
| Game code (528 units) and JSystem (201 units) compile for `wasm32` | yes |
| Links (game + platform layer + Aurora) | yes |
| Boots in Chrome over WebGPU | yes: threads, disc, archives, audio init, the Nintendo boot logo drawn by the game's own code |
| Title screen | yes, rendered by the game's own code (J2D, J3D, particles) |
| Menus and the new-quest opening | yes: mode/file/player select and the prologue narration |
| Input | keyboard (Enter = Start, Space = A, Shift = B, WASD = stick), gamepads, and an on-screen touch controller on phones and tablets |
| Speed | 60 frames per 60 retraces everywhere tested; <= ~4 ms of CPU per frame |
| Past the prologue | not yet (a message window does not advance) |
| Audio output, saves, GBA views, movies | not yet |

## Build and run (browser)

Needs emscripten 5.0.6 and Chrome/Edge 137+ (WebGPU and JSPI).

```sh
source ../fsadecomp/tools/emsdk/emsdk_env.sh
python3 tools/extract_assets.py path/to/main.dol build/assets
python3 tools/gen_globals.py emit path/to/main.dol build/generated
emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build/web fsa
python3 tools/serve.py --port 8811 --dir build/web          # then open it and drop your disc image
```

`--iso path/to/your.iso` on `tools/serve.py` also serves your image at
`/local.iso`, for testing with `?iso=local.iso` (local only; never expose it).
`--logs DIR` collects the page's log: each page load posts its lines (and
device / WebGPU telemetry) to `/log`, written to `DIR/<time>-<id>.log`, with
`DIR/latest.log` pointing at the newest.
Memory cards persist in the browser (IndexedDB, per site); `?nosave=1`
starts with an empty card that is not written back.
`main.dol` is the one on your own disc. docs/PORTING.md has the architecture.

## Compile check only

```sh
python3 tools/compile_check.py                         # build/obj/*.o + build/compile_report.txt
```

## Layout

| Path | What |
|---|---|
| `game/` | The decomp's `src/`, `include/` and `libs/` at the paths the decomp uses, so its include flags apply unchanged. The first commit is the unmodified import; every later change is a port change. |
| `game/units.txt` | Every unit the port compiles, with its include paths and defines (from the decomp's `compile_commands.json`). |
| `game/port/` | Headers force-included into every unit: host versions of Metrowerks intrinsics and attributes. |
| `platform/` | The port's replacement for the GameCube SDK: threads, DVD, VI, the GX pipe glue, stubs; `platform/web/` holds the page and its scripts. |
| `extern/aurora/` | Aurora (MIT): GX -> WebGPU, PAD, CARD, OS memory. |
| `tools/` | Build and porting scripts. |
| `docs/PORTING.md` | Every kind of change made to the sources and why, and what comes next. |

## Licenses

The decompiled SDK and JSystem sources are CC0 (`game/LICENSE.sdk`,
`game/LICENSE.jsystem`, from doldecomp/mkdd and the other donors listed in the
decomp). The decomp itself states no license.
