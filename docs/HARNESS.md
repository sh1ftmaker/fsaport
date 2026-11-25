# Behaviour harness (port side)

The decomp drives Dolphin (the original game) and this port with the same
input and compares state at sync points. This is the port's half: input
recording and replay, closed-loop scripted input, sync points and state
dumps, and a lockstep mode that makes a run repeatable.

Code: `platform/harness.cpp` (hook, record/replay, the script interpreter,
sync points, virtual clock), `game/src/port/harness_state.cpp` (the state
dump: field tables), `platform/web/fsa_harness.js` (URL options, loads the
inputs file, compiles a script), `tools/harness_scripts/` (scripts),
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
| `?script=URL` | run that input script (below; fetched relative to the page like `inputs`); its input goes to the `inputs` stream as in record mode; implies lockstep; not with `inputs` |
| `?lockstep=1` | lockstep only, live input (`lockstep=0` turns it off under record/inputs/script) |
| `?fast=1` | lockstep without wall-clock pacing: the next retrace as soon as the game is idle |

Use `nosave=1` too (an empty memory card, as a fresh Dolphin card), and
`stage=S[-L]` to skip the menus if wanted (that path is port-only; for
comparisons with Dolphin go through the menus).

## Scripted input (`?script=URL`)

A fixed per-read input file cannot drive the port and Dolphin through the
same screens: the port loads in fewer main-loop frames than Dolphin (which
emulates disc timing), e.g. the title appears at read 122 in the port and
350 in Dolphin, so a press timed for one lands on another screen in the
other. A script is closed-loop instead: each step waits on game state, so
both sides do the same thing in each scene whatever the load took. The page
compiles the JSON (`compileScript` in `platform/web/fsa_harness.js`) into a
table of doubles; `platform/harness.cpp` interprets it at each pad read,
after the scene sync point and before the input is recorded.

    {"version": 1, "steps": [STEP, ...]}

| Step | Input |
|---|---|
| `{"wait": COND}` | neutral until COND holds |
| `{"wait_reads": N}` | neutral for N reads |
| `{"press": BUTTONS, "every": K, "until": COND}` | BUTTONS held 1 read, released K-1 reads, repeated; COND is checked before each press (at the step's reads 0, K, 2K, ...) and ends the step when it holds. `every` defaults to 30, must be >= 2; no `until` = only a timeout ends it |
| `{"hold": BUTTONS, "reads": N}` | BUTTONS held N reads |
| `{"stick": [x, y], "reads": N, "buttons": BUTTONS}` | main stick (s8, as `PADStatus`: up and right positive) for N reads, `buttons` (optional) held too |
| `{"end": true}` | stop: neutral from here on, sync `script_end` |

- **Exactly one step is active.** A step is checked at each read before it
  gives input: when it is finished (its COND holds; its N reads are used
  up), it ends *before* that read, and the next step starts *at* that read
  (and is checked in turn). So `{"wait": C}` takes no read when C already
  holds, `{"wait_reads": 0}` takes none, and the first read the next step
  feeds is the first read at which C was seen to hold. Each step start
  writes `{"sync":"step","n":N,"step":i}` (i from 0); several zero-read steps
  give several lines at the same n.
- **`"timeout": N`** on any step: when the step is still active N reads after
  it started (and not finished at that read), the script stops with
  `{"sync":"script_timeout","n":N,"step":i}` and neutral input from then on.
- Running past the last step is the same as `{"end": true}`.
- **COND** is an object over the harness state globals, by their names in
  `kGlobals` (`globals` of the state stream), plus the short aliases `mode`,
  `stage`, `level`. Every key must hold; a value may be a list (any of).
  Values are numbers, booleans (= 1/0) or `null` (matches a value that does
  not exist yet, i.e. `null` in the state stream). `mode` and
  `gGameRoot_8053AAF8->field_0x4c` are -1 before GameRoot exists (as in the
  `scene` sync line, not `null` as in the state stream); `stage`/`level` are
  `lbl_80529DEC.mStage_0x118` / `.mLevel_0x11c`. Values are compared as
  numbers (floats exactly, as printed with 9 digits).
- **BUTTONS**: a list of `A B X Y Z L R START UP DOWN LEFT RIGHT`
  (case-insensitive), the `PAD_BUTTON_*` / `PAD_TRIGGER_*` bits of the SDK
  pad header (L and R are the digital bits only; analog triggers stay 0).
- Input: pad 0 only, connected; ports 1-3 unplugged
  (`PAD_ERR_NO_CONTROLLER`). Neutral = buttons 0, sticks 0, triggers 0.
- A `"comment"` key is allowed on a step and at the top level. Any other
  unknown key, an unknown button or condition key is an error at load.
- The conditions are evaluated on the game state at read n (after it handled
  reads 0..n-1), the same state the sync points at read n describe.

**Ticks in script mode** are relative to anchors: the `tick` sync points
(and state records) are at the reads 60, 120, ... after the latest anchor
(a `scene` or `step` sync), not at n % 60 == 0, and each step start writes a
state record (`"sync":"step"`, unless the scene changed at the same read: then
`"scene"`). So both sides dump state at the same offsets into each scene and
step. Other modes keep n % 60.

`tools/harness_scripts/opening.json`: title (START every 30 until mode 2),
main menu (A every 30 until mode 3: Hyrulean Adventure, then its START!),
file select: wait for the "create a file for this game on the Memory Card
in Slot A?" prompt (`lbl_8053ADB0->mState` 7), LEFT until its cursor
`field_0x114` is 0 (Yes), A until state 8/9/10, wait for the card write
(state 10, "A file has been created."), wait for the file rows (state 5),
A every 30 until mode 4 (file 1, then its menu), player count: A every 30
until stage 8 level 3 (Single-Player Game), wait for mode 7/8/3 (the opening
narration), 900 reads of it, end. Serve it next to the page:

    mkdir -p build/web/harness_scripts && cp tools/harness_scripts/opening.json build/web/harness_scripts/
    node tools/web_run.mjs --cdp 9345 --url 'http://127.0.0.1:8825/?iso=local.iso&nosave=1&script=harness_scripts/opening.json' \
         --seconds 60 --out build/web-run/open --evals 'window.fsaDiagFlush()@58'

## Sync stream (`sync.jsonl`)

    {"sync":"start","n":0,"lockstep":true,"record":true,"replay":false,"script":false}   once, before main()
    {"sync":"scene","n":149,"mode":7,"stage":1,"level":0}
    {"sync":"tick","n":180}
    {"sync":"inputs_end","n":3406}                                        replay only
    {"sync":"step","n":466,"step":4}                                      script only
    {"sync":"script_end","n":1838}                                        script only
    {"sync":"script_timeout","n":3763,"step":9}                           script only

- `scene`: at read n when `(mode, stage, level)` differs from read n-1 (and
  at read 0). `mode` is `gGameRoot_8053AAF8->field_0x4c` (-1 while the root
  does not exist yet, i.e. in the boot loader), `stage`/`level` are
  `lbl_80529DEC.mStage_0x118` / `.mLevel_0x11c` (GameState).
- `tick`: every read with n % 60 == 0; in script mode every 60th read after
  the latest `scene` or `step` line (see above).
- At one read the order is: `scene`, then `step` / `script_end` /
  `script_timeout`, then `tick`.

## State stream (`state.jsonl`)

One record per read that has a sync point (`"sync"` is `"scene"` when the
scene changed at that read, else `"step"` when a script step started, else
`"tick"`):

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
  These are what `fsa_debug_state` prints. For scripts in the menus, the file
  select screen (mode 3, `lbl_8053ADB0`, null while it does not exist): its
  `mState` (index into `lbl_804AFD70`), `field_0x7c` (message shown),
  `field_0x114` (the message window's cursor: 0 yes, 1 no) and `field_0x116`
  (the message window's phase).

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

Scene-anchored (runs whose loads take different numbers of reads, e.g. the
port against Dolphin):

    python3 tools/harness_compare.py logs/diag/<runA> logs/diag/<runB> --anchor scene

The anchors are the `scene` and `step` sync lines. The two runs' anchor
sequences are aligned by identity (scene `(mode, stage, level)` or step
index; a sequence diff, so an extra scene in one run does not shift the rest);
every sync and state record is re-keyed as (anchor, reads since it, sync
kind), belonging to the latest anchor at or before its read (the last one
when several share a read). For each matched anchor the segment (up to the
next anchor or the run's end) is compared up to the shorter of the two
lengths, and one line per anchor gives its reads in each run, the segment
lengths and how many records match; anchors only one run has are listed.
Segment lengths that differ are shown (`reads 341/150`) but are not counted
as a difference. Ticks match by offset, so this suits script runs (ticks
relative to the anchors); in per-read runs (ticks at n % 60) a tick matches
only where both runs' anchors fall on the same read modulo 60, which is the
case for two runs of the same input in the port.

With `--anchor scene` the RNG seed (`gGameRoot_8053AAF8->mRandom.seed`) is
compared within each segment by default (`--seed within`): each state
record's seed is replaced by how many `TRandom_fast_` steps the generator took
since the segment's first record (`advances N`; `reseeded` when the game set a
new seed). Runs reach a scene after menus of different lengths (the port's card
writes are instant: 14 reads against Dolphin's 69), so raw seeds differ from
the main menu on; `--seed raw` compares them as they are. Port vs Dolphin over
the scripted opening: 40/54 state records identical within segments (6/54
raw); the rest differ by ±1-2 advances that swing both ways without growing,
i.e. a random call landing a frame earlier or later, not a missing call.

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

### Scripted opening (2026-11-24)

`?script=harness_scripts/opening.json&nosave=1`, two runs (paced, 60 s):
title (mode 1) at read 122, main menu (2) at 269, file select (3) at 419,
the prompt (step 4) at 466, the card write done at 520, player count (4) at
760, stage 8/3 chosen at 912, mode 7/8/3 at 938, `script_end` at 1838, in
both runs; `inputs.jsonl` identical up to the shorter run.

| Comparison | result |
|---|---|
| run 1 vs run 2, `--anchor scene` | 20/20 anchors matched; sync 68/68, state 67/67 identical |
| run 1 vs run 2, per read | sync 71/71, state 67/67 identical (reads 0..3338) |
| replay of run 1's `inputs.jsonl` (`?inputs=`, per-read ticks) twice, `--anchor scene` | 7/7 scene anchors; sync 53/53, state 52/52 identical (the same as per read) |
| run 1 vs that replay, `--anchor scene` | the same 7 scenes at the same reads; the 13 step anchors only in the script run; ticks differ in phase (one tick only in the replay) |
