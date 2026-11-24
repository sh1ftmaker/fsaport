# Behaviour harness (port side)

The decomp drives Dolphin (the original game) and this port with the same
input and compares state at sync points. This is the port's half: input
recording and replay, sync points and state dumps, and a lockstep mode that
makes a run repeatable.

Code: `platform/harness.cpp` (hook, record/replay, sync points, virtual
clock), `game/src/port/harness_state.cpp` (the state dump: field tables),
`platform/web/fsa_harness.js` (URL options, loads the inputs file),
`platform/web/fsa_sched.js` (lockstep retrace delivery), `tools/harness_compare.py`.
Streams go through the diagnostics plumbing (`fsa_diag(name, json)` →
page → `tools/serve.py --diag DIR` → `DIR/<run>/<name>.jsonl`, one run
directory per page load).

## The index: pad reads

Everything is indexed by **n, the number of the game's pad read**: the n-th
call (from 0) of `JUTGamePad::read`, which calls `PADRead` once and then the
port's hook `fsa_pad_after_read` (`platform/input.cpp`), before `PADClamp`.

- `JUTGamePad::read` is the game's only `PADRead` caller (the others are in
  `JUTException`'s crash screen). The main loop (`tu_801344C0.cpp`) calls it
  once per iteration, so read n is main-loop frame n, from the boot loader's
  first frame on. It is not a VI retrace count: a frame that takes two
  retraces is still one read.
- Dolphin's DTM counts SI controller polls (per port, at the SI polling
  rate), not `PADRead` calls, so on the Dolphin side the counter and the input
  injection have to sit on `PADRead` (a hook or breakpoint at its return that
  overwrites the four `PADStatus`), not on the movie's input count. With one
  poll per field and no lag frames the two coincide, but nothing guarantees
  that.

At read n the hook does, in order:

1. sync points (below), describing the game state after it has handled reads
   0..n-1;
2. replay: the four `PADStatus` are replaced by line n of the inputs file;
   record: the four `PADStatus` are written to the `inputs` stream;
3. the game goes on with `PADClamp` and the rest of `JUTGamePad::read`.

## Input file (JSONL, one line per read)

    {"n": 1234, "pads": [[button, stickX, stickY, substickX, substickY, trigL, trigR], null, null, null]}

- Values exactly as in `PADStatus`, raw and before clamping: `button` u16 (the
  `PAD_BUTTON_*` bits), sticks s8 (-128..127, up and right positive), triggers
  u8 (0..255). `analogA`/`analogB` are not recorded (always 0 in replay).
- A pad is an array of 7 numbers when connected (`err = PAD_ERR_NONE`), and
  `null` when unplugged (`err = PAD_ERR_NO_CONTROLLER`).
- A pad left out of a short `pads` array: port 0 counts as connected and
  neutral, ports 1-3 as unplugged. A read with no line at all (a gap in n) is
  the same: port 0 connected and neutral, the others unplugged.
- Record mode writes one line per read, with the input as the game saw it:
  Aurora's `PADRead` (keyboard, SDL gamepads) plus the page's touch pad merged
  in by `fsa_pad_after_read`.
- **After the file ends** replay holds neutral input (port 0 connected and
  centred, ports 1-3 unplugged) and the game keeps running; the sync stream
  gets one `{"sync":"inputs_end","n":N}` line at the first read past the file.

## URL options

| Option | Effect |
|---|---|
| `?record=1` | write every read to the `inputs` stream; implies lockstep |
| `?inputs=URL` | replay that file (fetched relative to the page, e.g. `harness/rec1.jsonl` under `build/web/`); implies lockstep |
| `?lockstep=1` | lockstep only, live input (`lockstep=0` turns it off under record/inputs) |
| `?fast=1` | lockstep without wall-clock pacing: the next retrace as soon as the game is idle |

Use `nosave=1` too (an empty memory card, as a fresh Dolphin card), and
`stage=S[-L]` to skip the menus if wanted (that path is port-only; for
comparisons with Dolphin go through the menus).

## Sync stream (`sync.jsonl`)

    {"sync":"start","n":0,"lockstep":true,"record":true,"replay":false}   once, before main()
    {"sync":"scene","n":149,"mode":7,"stage":1,"level":0}
    {"sync":"tick","n":180}
    {"sync":"inputs_end","n":3406}                                        replay only

- `scene`: at read n when `(mode, stage, level)` differs from read n-1 (and
  at read 0). `mode` is `gGameRoot_8053AAF8->field_0x4c` (-1 while the root
  does not exist yet, i.e. in the boot loader), `stage`/`level` are
  `lbl_80529DEC.mStage_0x118` / `.mLevel_0x11c` (GameState).
- `tick`: every read with n % 60 == 0.

## State stream (`state.jsonl`)

One record per read that has a sync point (`"sync"` is `"scene"` when the
scene changed at that read, else `"tick"`):

    {"n":180,"sync":"tick",
     "globals":{"gGameRoot_8053AAF8->field_0x4c":7, ..., "gGameRoot_8053AAF8->mRandom.seed":3788582908, ...},
     "actors":[{"mSlot":0,"mActorId":"PLYR","mSerial":1,"mAlive":true,"mLayer_0x4":0,"mPos":[x,y,z],"mDir_0x98":0,"mLife":0}, ...],
     "players":[{"mPlayerNo_0x4":0,"mActive":true,"mPos":[x,y,z],"mLayer_0x3b4":0,"mFacing_0x3dc":3,"mState_0x1240":1,"mLife_0xbfc":12}, ...]}

- Every value is named by symbol and field as in the decomp, never by
  address: `globals` keys are `symbol->field` paths; actor and player keys are
  the member names of `ActorBase_804A5770` and `Player_804A6F38`. A value that
  does not exist yet (no GameRoot, no stage loaded) is `null`; `actors` is
  `null` without an actor manager, `players` without `gPlayerManager_8053AC90`.
- `actors`: every non-null `ActorManager_804A5800::mActors[slot]` for slot <
  `mActorEnd`, in slot order. `mActorId` is the four-character ActorId.
  ActorBase has no state member of its own (each actor class keeps its own),
  so there is no per-actor `mState` yet.
- `players`: `gPlayerManager_8053AC90->mPlayers[0..3]`.
- Floats are printed with 9 significant digits (exact for f32); NaN/inf as
  strings.
- Current globals: GameRoot `field_0x4c` (mode), `field_0x50` (requested
  mode), `field_0x58`, `field_0x60`, `field_0x7f`, `field_0xfd` (loading),
  `mPlayerMask`, `mRandom.seed` (the game RNG, `JMath::TRandom_fast_`); GameState
  stage/level; the stage view's stage, level and screen; the fade object
  (`fn_80119DD0()`) and demo timer (`fn_8011921C()`); the camera's view box.
  These are what `fsa_debug_state` prints.

**Adding fields:** `game/src/port/harness_state.cpp` has three tables,
`kGlobals`, `kActorFields`, `kPlayerFields`, each a list of `{name, getter}`;
a getter writes one JSON value with the `Json` helper (`i`, `u`, `b`, `f`,
`vec`, `str`). Add a row, rebuild.

## Running

Build and serve as usual, with `--diag`:

    python3 tools/serve.py --port 8825 --dir build/web --iso G4SE01.iso --diag logs/diag

Record (keys: Space = A, Enter = Start, WASD = stick):

    node tools/web_run.mjs --cdp 9345 --url 'http://127.0.0.1:8825/?iso=local.iso&nosave=1&record=1' \
         --seconds 75 --out build/web-run/rec --keys 'Enter@15,Space@25,KeyD@35' --evals 'window.fsaDiagFlush()@73'

The run directory is the newest one in `logs/diag/` (`<date>-<time>-<id>`);
its `inputs.jsonl` is the recording. Serve it next to the page and replay:

    mkdir -p build/web/harness && cp logs/diag/<run>/inputs.jsonl build/web/harness/rec.jsonl
    node tools/web_run.mjs --cdp 9345 --url 'http://127.0.0.1:8825/?iso=local.iso&nosave=1&inputs=harness/rec.jsonl' \
         --seconds 80 --out build/web-run/rep --evals 'window.fsaDiagFlush()@78'

Compare two runs (records keyed by n and sync kind, up to the shorter run):

    python3 tools/harness_compare.py logs/diag/<runA> logs/diag/<runB>

`Module._fsa_harness_reads()` in the page gives the current read count. The
page batches each stream and posts it every 2 s, so call
`window.fsaDiagFlush()` before closing a tab.

## Determinism

Lockstep mode (on under record and replay) pins what made the port's runs
differ:

- **Time.** `OSGetTime`/`OSGetTick` come from a virtual clock: 2026-01-01
  00:00:00 plus 1001/60000 s per VI retrace (through `aurora_os_time_hook`,
  `extern/aurora/lib/dolphin/os/OSTime.cpp`). Time does not move inside a
  frame. This also pins the dates the game writes (save banners, file times).
- **Retraces.** The page delivers a retrace only while every game thread is
  blocked (`fsa_is_idle`); one that falls due while the game is busy (a frame
  still running, a disc read in flight: the reading thread holds the CPU while
  it awaits the bytes) is owed and delivered when it goes idle. So the game
  never sees a retrace in the middle of a frame or a load, and every disc
  read completes within the same frame as on any other run. The cost: the
  game slows down instead of dropping frames.
- **Timers.** No `setTimeout` alarms in lockstep; alarms and timed sleeps fire
  at the first retrace at or after their (virtual) time.
- **Spin loops.** `platform_poll_point` (`DVDGetDriveStatus`,
  `VIGetRetraceCount`, ...) yields after 1000 polls without a thread blocking,
  instead of after 8 ms of wall time.
- **RNG.** The game's RNG (`GameRoot::mRandom`) is seeded with the constant
  0x12D5DA0 (`resetGameState_80138C9C`), and nothing in the game seeds from
  time; JParticle's emitters seed from constants too. `Math.random` in the page
  is only the log session id.
- **Saves.** `nosave=1` starts with an empty card each time; without it the
  card from IndexedDB changes what the menus show.

Still not pinned:

- **Audio.** If audio callbacks are driven by the browser's audio clock (the
  JAudio mixer thread woken from an AudioWorklet), when they wake the audio
  thread is wall-clock dependent. The game's logic does not read audio state
  as far as known, but sound-driven waits would differ.
- **Live input in record mode** is sampled whenever the browser delivered the
  key events; that is fine (the file records what was seen) but two record
  runs with the same `--keys` timings do not produce the same file.
- **Rendering** (Aurora/WebGPU timing) has no effect on game state.
- **The `[pad]`-log-only bits** (`extButton`, `analogA/B`) are not replayed.

Note on live input: in the headless test Chromium, Aurora's `PADRead` reports
ports 1-3 as connected with non-zero values (stickX -1, trigger L 255 on
port 3); the recording keeps them as the game saw them. Edit or regenerate a
file (e.g. set ports 1-3 to `null`) to match what Dolphin is given.

## Results (2026-11-24)

`?stage=1`, `nosave=1`. Recording: 70 s, stick held right / down /
diagonal / left / up through `Module._fsa_touch_set`, A pressed (Space and
the touch pad), 3456 reads; the players walk from (40, 472) to (153, 439).

| Comparison (tools/harness_compare.py) | sync | state |
|---|---|---|
| recording vs replay A (paced) | 62/62 identical | 61/61 identical (reads 0..3420) |
| recording vs replay B (paced) | 62/62 identical | 61/61 identical |
| replay A vs replay B (incl. 800 reads of neutral input past the file) | 76/76 identical | 75/75 identical (reads 0..4260) |
| recording vs `fast=1` replay (86 reads/s instead of ~60) | 62/62 identical | 61/61 identical |
| recording vs replay with `lockstep=0` | stage entry at read 150 instead of 149 | 53 of 58 differ |

An earlier recording (key taps only, 3406 reads) also replayed identically
twice, including one replay run while another tab was busy in the same
browser. Without lockstep the disc-load timing decides at which read the
stage starts, and everything after differs (RNG seed, actor list).
