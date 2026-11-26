# fsaport

A port of **The Legend of Zelda: Four Swords Adventures** to WebAssembly (and
later native desktop), built from the
[sh1ftmaker/fsa](https://github.com/sh1ftmaker/fsa) decompilation. It is a hard
fork in the way katam-port is: the decompiled sources live here and are changed
freely for the port, and the GameCube's hardware is replaced, not emulated.

**This repository and its builds contain no game data.** You supply your own
disc. The game data with no source form (two fonts, a message file, a DSP
microcode fragment, and the tables listed in `tools/globals_layout.json`) is
not compiled in either: the build reserves its storage, and at startup the page
reads `main.dol` from the disc you chose, checks it (SHA-256 of the G4SE01
`main.dol`; any other disc is refused with a message) and copies the bytes in
before any game code runs (`platform/web/fsa_disc.js`, `platform/dol_data.cpp`).

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
| Audio | yes: music, sound effects and movie sound through Web Audio (starts on the first key, touch or click); no reverb yet |
| Saves | not yet |
| GBA link (branch `deluxe`) | native GBAs in ports 1-4 (`?gba=N`): the game accepts 2-4 players and plays them; GBA screens are a debug view so far (docs/MULTIPLAYER.md) |

## Play in the browser (GitHub Pages)

https://sh1ftmaker.github.io/fsaport/ runs the `deluxe` build. **Bring your
own disc:** choose your *The Legend of Zelda: Four Swords Adventures* (USA,
`G4SE01`) disc image (`.iso` / `.gcm`, 1.4 GB) with the button or drop it on
the page. It is read in the browser only and never uploaded; the page checks
its `main.dol` and refuses any other disc. The image is kept in the browser's
private storage (Origin Private File System), so a reload starts without
choosing it again; **Forget disc** (top right) deletes it. Where there is not
enough storage (some phones) you choose it again after a reload. Memory-card
saves stay in the browser (IndexedDB).

Browser: WebGPU and JavaScript Promise Integration (JSPI) are both needed.
Chrome or Edge 137+ on desktop and Android; on iPhone and iPad, iOS 27 (any
iOS browser: they all use Safari's engine; tested on 27.2); Safari on macOS
of the same release. Firefox only where it has both WebGPU and JSPI on.
No special headers are needed (no SharedArrayBuffer).

Controls: a gamepad (standard mapping), or the keyboard: WASD = stick,
Space = A, Left Shift = B, E = X, Q = Y, Enter = Start, Z = L, C = R, F = Z,
arrow keys = C-stick. On touch screens an on-screen controller appears
(**Pad** toggles it). Local co-op: press Start on another gamepad (or `;`
for a second keyboard player on the arrow keys, or **+ Join** in the roster)
to drop in as the next Link, mid-stage; hold Back (Backspace) to drop out
(docs/MULTIPLAYER.md, "Local co-op"). Online rooms need a relay server (`netplay/relay.mjs`,
docs/NETPLAY.md): on the site they are offered only when the page is opened
with `?relay=wss://your-relay/`.

To build the site: `tools/build_pages.sh build/site` (a plain release build;
`FSA_CHECK_DOL=path/to/main.dol` also runs `tools/check_no_game_data.py`), then
`tools/publish_pages.sh build/site` commits it to the `gh-pages` branch (in a
worktree at `build/gh-pages`; it does not push).

## Build and run (browser)

Needs emscripten 5.0.6 and Chrome/Edge 137+ (WebGPU and JSPI).

```sh
source ../fsadecomp/tools/emsdk/emsdk_env.sh
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

Disc loading: by default the page fetches the disc blocks listed in
`build/web/disc-manifest.json` up front (with a progress bar) and reads
everything else on demand; `?disc=lazy` reads everything on demand and
`?disc=full` downloads the whole image first. To make or extend the manifest,
run `tools/serve.py` with `--record-manifest` and open the page with
`?discrecord=1`: the blocks the game reads are merged into the manifest as you
play. It lists block numbers only (no disc data) and stays in `build/`.
Every block fetched is also kept in the browser's Cache Storage, so later
visits read it locally (`?disccache=0` turns that off; `fsaDiscCacheClear()` in
the console empties it).
docs/PORTING.md has the architecture.

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
| `docs/DIAGNOSTICS.md` | The diagnostics build (`-DFSA_DIAG=ON`): function coverage and stub hits for the decomp, and its hooks. |

## Licenses

The decompiled SDK and JSystem sources are CC0 (`game/LICENSE.sdk`,
`game/LICENSE.jsystem`, from doldecomp/mkdd and the other donors listed in the
decomp). The decomp itself states no license.

The CPU mixer that replaces the DSP (`platform/audio_dsp.cpp`) is adapted from
Dusklight, the Twilight Princess PC port (CC0).
