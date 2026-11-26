# Netplay (branch `deluxe`)

Online multiplayer where **every browser runs the whole game and only inputs
travel**, with rollback: each browser plays its own player's input at once,
predicts the others', and when a remote input arrives that differs from the
prediction it restores a snapshot of the frame and re-runs the frames since.
This is the model of katam-port's working Path B rooms
(`katamport/port/docs/NETPLAY.md` §3b-3e, `docs/MULTIPLAYER.md` §8-11), built
for this port's very different machine: threads on JSPI coroutines, a 64 MB
MEM1 of JKR heaps, ARAM, and Aurora on WebGPU.

`docs/MULTIPLAYER.md` §4 (the previous agent) recommended host-authoritative
streaming instead; the owner chose this. Lockstep is kept as a mode (`?net=lockstep`):
it validates determinism and the relay without rollback, and is the fallback.

Conventions: **verified** = run in this branch, with the run named;
**read** = taken from the code; **assumed** = neither.

---

## 1. Design

### The seam: the pad read

The harness already indexes everything by n, the game's n-th
`JUTGamePad::read` (one per main-loop frame, docs/HARNESS.md), and lockstep
mode makes a frame a pure function of the state at its read and the four
`PADStatus` given there (virtual time = retrace count; retraces only while every
thread is blocked). Netplay uses exactly that seam:

```
JUTGamePad::read -> PADRead -> fsa_pad_after_read -> platform_harness_pad(status)
   1. platform_net_read_begin(n)      fsa_js_net_begin: roll back (restore an older
                                      snapshot: g_reads goes back too) and/or snapshot
   2. n = g_reads++; scene/tick sync points, state dump, the script
   3. platform_net_inputs(status, n)  the local pad (or the script's pad of this seat)
                                      is stamped into the timeline at n + D and sent;
                                      the timeline's four pads of frame n (known or
                                      predicted) replace status. May wait (JSPI).
   4. record (inputs stream); the GBA local peer takes each GBA's keys from status[port]
```

Each player is a seat = a controller port = a GBA (`?gba=N` is the room's seat
count; docs/MULTIPLAYER.md). Every browser runs the same N-GBA setup; each GBA's
keys come from its port's `PADStatus`, i.e. from the timeline.

### What a snapshot is (platform/netplay.cpp)

Taken at step 1 of read n, before anything of frame n has run:

| part | how | size (stage 1-0, 2-4 players; verified) |
|---|---|---|
| the game's static memory | the `.data`/`.bss` input sections of every game/JSystem unit, plus the platform statics that are game-visible state (virtual clock, harness, GBA endpoints, DSP mixer, audio DMA, DVD, Aurora's OS layer and texture-id counters), from the linker map: `tools/net_regions.py` → `fsa.netregions.json` (25 ranges). Host state that looks like game state is excluded by symbol: heap-owning containers, the host-allocation depth, Aurora's in-frame flag, wall-clock timings | 1.78 MB (1.05 MB of it the GBA endpoints' file buffers) |
| MEM1 | not all 64 MB: the JKR heap tree's used parts (`fsa_net_heap_ranges`, game/src/port/harness_state.cpp): of an expanding heap its used blocks and the free-block headers after them, a block that holds a child heap replaced by the child's parts, other heap kinds whole, at 16 KiB granules. Free heap bytes are never read: the port zeroes every block it hands out (`JKRHeap::alloc`) | 17.4 MB in 2 ranges (all of MEM1 if the tree looks odd) |
| ARAM (16 MB) | copy-on-write: Aurora's `ARQPostRequest` calls `aurora_aram_write_hook` before writing; each live snapshot saves the 64 KiB blocks it is about to lose | 0 in play; the blocks a stage load rewrites |
| coroutine shadow stacks | each running coroutine's `[sp, top)` | ~10 KB |
| the thread layer | `platform_threads_save` (os_thread.cpp): per coroutine wait/suspend/wake-up state, the alarm list, the wait queues, the lockstep poll counters | < 1 KB |

Total 19.2 MB; snapshot 1.4-1.6 ms, restore 1.1-1.5 ms (memcpy; this machine,
under load from other agents' browsers).

**What cannot be in it: the JSPI continuations.** A suspended coroutine's wasm
stack lives in the engine, not in linear memory. So a restore is only valid when
every coroutine that was running at the snapshot is still switched out at the
same place (same shadow-stack pointer = same call path), and the game thread is
in the same hook. `platform_threads_restore` checks exactly that and refuses
otherwise (nothing changed, the refusal is reported as a desync). What *is*
reversible, and undone: coroutines created after the snapshot (dropped; their
continuations abandoned, the re-run creates them again) and ones that had not
started at the snapshot (they start afresh). Measured (`?netlayout=1`): the 14
long-lived coroutines (game, audio, DVD, the four GBA clients, ...) are at the
same sp at every pad read of a whole run; only the game's short-lived loading
threads (created and finished within 2-4 reads, e.g. at reads 124-126 and
362-366 of `mp_2_stage_1_0`) come and go.

Hence the thread rule (fsa_net.js `track`): **a frame runs on predicted inputs
only while every running coroutine has been where it is for `?stable=` reads
(120)**; otherwise it waits for its inputs, as in lockstep. A frame that a
loading thread lives through is therefore never re-run, and a rollback to an
earlier frame only has to drop the newer thread. Cost: lockstep for ~2 s
around thread changes (boot: 128 reads; loads), measured `gatedFrames`.

### Host memory must not be in the game's heaps

`JKRHeap.cpp`'s operator new puts every allocation made while
`fsa_host_alloc_depth == 0` into the current JKR heap. Aurora is called from
game code, so its pipelines, command vectors, texture caches, card I/O strings
and log messages landed in MEM1 (`?netprobe=1` logs every such allocation with
its call stack: 289 distinct stacks in one run). Restoring MEM1 then rolled back
Aurora's objects under it (the first crash: `beginRenderPass` of undefined).
Fixed in Aurora: `AURORA_HOST_ALLOC` (extern/aurora/lib/host_alloc.hpp) on every
GX/GD/PAD/VI/SI/CARD entry point, the command processor, `push_command`, texture
creation and logging; the probe is clean afterwards (verified, run `probe3`).
This is a port behaviour bug outside netplay too (Aurora objects in the game's
heaps); it changes nothing the game sees except where its heap blocks land.

Aurora's texture-object ids (`sNextTexObjId`, `sNextTlutObjId`) are game-visible
bytes (they live in the game's `GXTexObj`s): they are in the snapshot, so a
re-run hands out the same ids, and with `aurora_gx_reusable_ids` each new id
first evicts any cache entry left from the abandoned future (in FIFO order, like
`GXDestroyTexObj`).

### Determinism additions

- Audio DMA is paced by virtual time alone in a room (`audio.cpp`: one block per
  block length from the previous due time), not by the page's audio clock, so
  the audio thread wakes at the same reads in every browser and in a re-run.
  Re-run blocks are not played.
- Rooms force `nosave=1` (an empty card everywhere).

### The timeline (platform/web/fsa_net.js)

Per port and frame: the packed pad (`a` = buttons + main stick, `b` = C stick +
triggers, `err`). Frames `< 8` are neutral for every seat. Each browser stamps
its local input at n + D (D = `?delay=`, 2 by default in rollback); when D
changes it fills or skips frames of its own column, so each frame gets exactly
one value per seat, from its owner. A seat that leaves is neutral from the frame
after its last input (`leave {seat, at}`, ordered by the relay after that seat's
inputs, so every browser agrees on `at`); seats nobody took are neutral from 0.

- **Prediction**: a missing input repeats that seat's latest known one.
- **Rollback**: an arriving input that differs from what frame f used sets
  `pendingRollback = f`; at the next read the newest snapshot ≤ f is restored and
  the frames since are re-run with the picture and sound off (`fsaClock.fast`,
  `fsa_net_set_resim`), then play continues.
- **Bounded lead (the Katam v2 failure, avoided)**: a browser never runs more
  than `?window=` (20) frames past its confirmed frontier; at the edge it waits.
  The ring holds `window + 4` frames, so every misprediction is within reach:
  no input is ever dropped (`refused` counts the impossible case and reports it).
- **Time sync** (GGPO-style frame advantage): from each remote input's read
  count at sending plus half of both links' round trips, a browser more than a
  frame ahead waits one frame (`timeSyncWaits`).
- **Final-only diagnostics**: lines the game writes while frame r runs are held
  until every input of frames < r is known, re-runs of written lines are dropped,
  so each browser's `sync`/`state`/`inputs` streams are what the confirmed
  timeline produced; `tools/net_compare.py` compares them across browsers.
- **Desync detection, loud**: each final `state` record's hash (FNV of the
  harness state: scene, mode, RNG seed, every actor's and player's position,
  life, ...; one per sync point, every 60 reads) goes to the relay, which
  compares the seats and broadcasts `desync {n, hashes}`; the page shows a red
  banner and writes a `net` diag line. A refused rollback is reported the same
  way.

### Lockstep mode

`?net=lockstep`: no snapshots; read n waits until every seat's input of frame n
is known. D adapts every 30 reads to cover half of this link's round trip plus
half of the worst other one plus twice the jitter (+8 ms), within 1-30
(`?delay=D` fixes it).

## 2. Protocol (netplay/relay.mjs, version 1)

No dependencies (its own small WebSocket server on node's http). Text frames
are JSON control; binary frames are inputs.

| direction | message |
|---|---|
| client → relay | `hello {v, room, create, seat, name, build, disc, opts}`; `start {}` (the host, seat 1); `ping {c}`; `stat {head, rtt}`; `hash {n, h}` |
| relay → client | `welcome {room, seat, host, opts}`; `roster {seats, started}`; `start {opts, seats, left}`; `error {code, msg}` (`build`, `disc`, `full`, `started`, `room`, `proto`); `pong {c, s}`; `stat {seat, head, rtt}`; `desync {n, hashes}`; `leave {seat, at}` |
| both | binary `0x01 u8 seat, u32 first, u16 count, u32 head, count × (s8 err, u32 a, u32 b)` |

- Room options are the creator's (`seats`, `mode`, `delay`, `window`,
  `snapevery`, `script`, `autostart`) and win over every joiner's URL.
- A room only takes browsers with the same build (SHA-256 of `fsa.wasm`) and
  the same disc (game id, version, SHA-256 of its first MiB); otherwise `error`.
- The relay checks that a member only sends its own seat, forwards inputs to
  the others in order, and compares hashes once every live seat has sent one
  for a read.
- Artificial latency for tests: `?lat=MS&jitter=MS` on the WebSocket URL (the
  page passes its own `lat`/`jitter`), or `--lat/--jitter` for every
  connection; applied per direction in FIFO order (katam's NETPLAY-HANDOFF §6
  pitfall: independent timers reorder messages).

## 3. How to run

```sh
# build (docs: README), then:
sbg start net-relay -- node netplay/relay.mjs --port 8858
python3 tools/serve.py --port 8848 --dir build/web --iso path/to/G4SE01.iso --diag logs/diag-net
```

In a browser: the start card has **Play online** (create a room for 2-4
players, or join by code), or by URL:

- `?room=new&seats=2` creates a room; the URL becomes `?room=CODE`, which the
  others open (`&seat=N` asks for a seat). Choose the disc; the lobby shows the
  seats; player 1 presses Start.
- Behind the terminal proxy: `https://<your-proxy-host>/lp/8848/?room=new&seats=2`
  (the relay is then `wss://<your-proxy-host>/lp/8858/`, found automatically;
  `?relay=URL` overrides).
- Phones: the on-screen pad drives port 0 of that browser, which is its seat.
  Keyboard and gamepads likewise.
- The chip at the top left shows room, seat, ping, delay, rollbacks and waits.

Tests:

```sh
tools/net_run.sh NAME SECONDS 2 'script=harness_scripts/mp_2_stage_1_0.json&net=rollback' 'lat=100&jitter=20' 9368 9378
tools/net_compare.py logs/diag-net ROOMCODE        # per-seat report + sync/state compare
# single browser, no network: roll back R frames every E reads and check that
# the re-run reaches the same bytes (snapshot completeness):
.../?iso=local.iso&nosave=1&gba=4&script=harness_scripts/mp_4_stage_1_0.json&fast=1&rbtest=97,8
```

Diagnostic switches: `?netprobe=1` (host allocations in JKR heaps),
`?netlayout=1` (thread layout changes), `?mem1trim=0` (all of MEM1),
`?snapevery=K`, `?ring=N`, `?stable=N`; `fsaNetReport()`, `fsaNetMemStats()`;
`tools/net_where.py build/web/fsa.map ADDR...` names the static at an address of
a snapshot diff.

## 4. Measurements

All runs: this branch's Release build, one machine (shared with other agents'
browsers: the game alone runs at ~44 reads/s here, not 60), own Xvfb, two
Chromiums (CDP 9368/9378), `tools/serve.py --diag logs/diag-net` (run
directories in the main checkout's `logs/diag-net/`), relay on 8858.

### Snapshot cost (verified)

| | full MEM1 (first version) | heap-tree parts (now) |
|---|---|---|
| snapshot size | 68.9 MB | 19.2 MB (statics 1.78 MB, MEM1 17.4 MB in 2 ranges, stacks/threads ~10 KB) |
| snapshot (memcpy) | 4.4-6 ms | 1.4-1.7 ms |
| restore | 4-6 ms | 1.0-1.5 ms |
| pages that change per frame | | 70-100 of 4,695 (4 KiB); over 20 frames still ~100 |
| re-simulated frame (with drawing) | 6-13 ms | 10-15 ms (§5, open 1) |

Ring: `window + 4` snapshots at one per read = 24 × 19.2 MB = 460 MB of host
memory (`?snapevery=K` divides it; restores then re-run up to K-1 more frames).

### Rollback self-check (rbtest, single browser, verified)

`?gba=4&script=harness_scripts/mp_4_stage_1_0.json&fast=1&rbtest=97,8`: roll
back 8 frames every 97 reads and compare the state reached again with the
first time, byte for byte, at every re-run frame and at the end.

| run | rollbacks | reads | compared states | differing | note |
|---|---|---|---|---|---|
| rb1 (first try) | 8 | 698 | 8 | 8 | crash "null function": the host-allocation depth was in the snapshot |
| rb3 | 22+ | 6324 | | stage load differs | Aurora's texture-object ids (fixed: in the snapshot, reusable) and a map parser bug (`.L` symbols ended a section: units missing from the table) |
| rb5 (68.9 MB snapshots) | 53 | 5056 | 64 | **0** | sync 115/115, state 108/108 identical to a run without rollbacks over 4,180 reads |
| rb8 (19.2 MB, heap tree, reversible threads) | 82 | 8061 | 100 | **0** | |

The only bytes that differ after a re-run are one or two words in the main
thread's outermost shadow-stack frames (`stackNoise`): host pointers and
doubles in frames above the main loop (main / fsa_game_main), never read back
by the game; reported separately and ignored.

### Two browsers, rollback (verified)

`tools/net_run.sh` with `mp_2_stage_1_0.json` (boot, menus, card file, 2
players, stage 1-0, both players walk and swing), its pad 0 played by browser
1, pad 1 by browser 2; 130 s each (~5,500-6,000 reads). Latency added on
browser 2's relay link, each direction (so the browser-to-browser one-way
latency is about that number), FIFO jitter. `D` = 2 (33 ms local input delay),
window 20, a snapshot every read. Rows: browser 1 / browser 2.

| added one way, jitter | desyncs | streams | rollbacks | max depth | mispredicted frames | waits (count, total, max) | of which thread-gated | lead over frontier (avg) |
|---|---|---|---|---|---|---|---|---|
| 0 ms | 0 / 0 | identical (sync 135, state 128) | 3 / 0 | 1 / 0 | 3 / 0 | 110, 1.8 s, 46 ms / 0 | 128 frames at boot, loads | -0.0 / -2.9 |
| 50 ms, 10 | 0 / 0 | identical (141 / 134) | 13 / 2 | 12 / 2 | 14 / 2 | 120, 2.4 s, 227 ms / 88, 1.2 s, 32 ms | | |
| 100 ms, 20 | 0 / 0 | identical (135 / 128) | 8 / 33 | 6 / 7 | 12 / 40 | 112, 4.4 s, 174 ms / 116, 4.3 s, 139 ms | | 5.1 / 0.3 |
| 150 ms, 30 | 0 / 0 | identical (130 / 123) | 12 / 42 | 11 / 7 | 14 / 50 | 114, 6.6 s, 265 ms / 108, 6.4 s, 270 ms | | 9.1 / 1.0 |

(The 0, 100 and 150 ms rows are the runs after the time-sync fix; the 50 ms row
is from before it. Before the fix, at 100 and 150 ms browser 1 waited
19.2 s and 17.8 s: its waits were `setTimeout`s that the paced clock simply
caught up after; now a wait drops a retrace.)

- **Zero desyncs, zero refused rollbacks, zero dropped inputs**, and the
  browsers' sync/state streams identical, at every latency.
- **Waits** are ~110 per run at every latency: they are the thread rule's
  gated frames (boot, the file screen, the stage load), each lasting a one-way
  trip, plus a few window edges. Outside loads play does not wait.
- **Rollbacks are few** because the script's input is sparse (it holds a
  direction for 60 frames, presses a button for 3-4); human input would
  mispredict more often. Depth stays below the window (max 18 frames at 150 ms
  before the time-sync fix, 11 after).
- **Input delay felt**: own input 2 frames (33 ms) at every latency; the other
  player's input shows after the one-way latency (as a correction).
- Traffic: ~1 KB/s each way per browser (9-byte inputs in 12-byte headers, one
  message a frame, plus a 500 ms stat and a ping a second).

### Two browsers, lockstep (verified)

Same script and setup, `net=lockstep`, adaptive delay. Rows: browser 1 / 2.

| added one way, jitter | desyncs | streams | delay reached (frames) | reads in 126 s | waits (count, total, max) |
|---|---|---|---|---|---|
| 0 ms | 0 | identical (sync 138, state 134) | 3 / 3 | 5,749 / 6,014 | 3,940, 60 s, 59 ms / 1, 8 ms |
| 50 ms, 10 | 0 | identical (150 / 146) | 5 / 6 | 6,540 / 6,818 | 3,398, 51 s, 95 ms / 1,941, 30 s, 48 ms |
| 100 ms, 20 | 0 | identical (151 / 147) | 9 / 10 | 6,577 / 6,861 | 133, 2.6 s, 73 ms / 6,527, 104 s, 71 ms |
| 150 ms, 30 | 0 | identical (155 / 151) | 12 / 13 | 6,769 / 7,070 | 2,045, 24 s, 199 ms / 3,310, 52 s, 197 ms |

- Zero desyncs; identical streams. The delay settles where it covers the link:
  12-13 frames (~210 ms of input delay for everyone) at 150 ms.
- "Waits" here mostly measure that the two browsers do not run equally fast on
  this shared machine: in lockstep the faster one waits every frame for the
  slower one's input (each wait is short, the long ones are the jitter). Both
  progress together (the read counts differ by the 3 s between the tabs' starts).
- Lockstep runs ~15 % more reads than rollback in the same time here
  (6,500-7,000 vs 5,400-6,000): the per-read snapshot (1.5 ms) and the
  re-simulated frames (10-15 ms each, drawing included) cost that on a machine
  that cannot reach 60 Hz anyway.

### Rollback against lockstep

At 100-150 ms lockstep needs 9-13 frames of input delay for every player;
rollback keeps 2 for the local player and corrects the remote one, with no
desync. Lockstep stays as the validation mode (it exercises the relay and the
determinism without snapshots) and as a fallback (`?net=lockstep` on the room
creator's URL).

## 5. Verified, assumed, open

**Verified**

- Snapshot/restore is complete at the pad-read seam: the rbtest self-check
  (§4) re-reaches the same bytes after every rollback through boot, menus,
  memory-card file creation, the player-count screen, a mode switch and stage
  load, and stage play; and a run with 53 forced rollbacks writes sync/state
  streams identical to a run without any.
- Two browsers in one room through the relay, at 0-150 ms added one-way latency
  with jitter, in rollback and lockstep mode: no desync, identical sync and
  state streams (net_compare), no refused rollback, no dropped input (§4).
- Room checks: build hash, disc id, seat taken, room started (read in the
  relay, exercised by every run's hello; mismatch paths not run).

**Assumed / not run**

- **More than two browsers**: this machine creates only two WebGPU devices at
  once (other agents' browsers included); a third game page times out getting
  an adapter. The relay, the timeline and the referee are N-seat code (4 seats,
  any subset), run here only with 2. A 4-player game with 2 browsers works the
  same way (the empty seats are neutral), not run.
- **The terminal proxy**: the page finds `wss://<host>/lp/8858/` when served
  under `/lp/<port>/`; the proxy forwards WebSockets (CLAUDE.md) but no run went
  through it.
- **Phones**: the touch pad drives port 0 = the browser's seat (read). A phone
  must hold the disc and run the game; iOS kills tabs holding ~1.4 GB (README),
  so `?disc=smart` (the default manifest) is the way; not tried on a phone.
- **Different machines / browsers**: wasm floats are deterministic across
  machines and the game reads no wall clock in lockstep; every run here was on
  one machine (two Chromium processes).
- Mid-game join or rejoin: not built. A leaving seat becomes neutral at a
  frame every browser agrees on (built; exercised when a test tab closes:
  the relay logs `seat N left at frame F`).

**Open questions / next steps**

1. **Re-simulation draws**: re-run frames still go through Aurora (GX
   processing and GPU submission), 10-15 ms a frame here (§4); skipping the
   GPU side (keep the FIFO and texture uploads, drop the render passes and the
   present) would bring a re-run frame near the game's own ~1.5 ms. Not done:
   Aurora's frame state (render passes, staging buffers) needs a proper
   "discard" path, outside begin/end it drops commands.
2. **Time sync is biased** by about a frame and a half towards the browser that
   started first (§4, `leadAvg`): the faster browser carries the rollbacks and
   most of the waiting. The estimate uses each input's read count plus half the
   two round trips; messages are only handled between frames, which is
   probably the missing term.
3. **The thread rule costs lockstep around loads** (`gatedFrames`: 128 at boot,
   more at stage loads). Making the loading threads' continuations unnecessary
   (e.g. running the game's loader threads to completion before a pad read) would
   remove it.
4. **A desync is reported, not repaired.** With bit-identical runs there is
   nothing to repair so far; a repair would be a snapshot transfer from a
   majority peer (19 MB, deflates well: 3,373 of 4,695 pages are non-zero).
5. Local co-op (docs/MULTIPLAYER.md "Local co-op") in a room: the page's
   device core is on, every device of a browser drives its seat (port 0, as
   before; `platform/coop.cpp` replaces Aurora's pads before the timeline takes
   them), and joining is off. A join is a pad-read event in the game
   (`coop_game.cpp`, static memory in the snapshot), so making it a timeline
   event (like a `leave`) would let a browser hold several seats; not built.
   Verified unchanged: `mp_2_stage_1_0` two-browser rollback run, 0 desyncs,
   identical streams; a live room with the device core on, 0 desyncs.
6. `client_thread.bin`-free GBA screens are the other deluxe agent's; each
   browser could show its own seat's GBA view (the view is local, not netplay
   state).
