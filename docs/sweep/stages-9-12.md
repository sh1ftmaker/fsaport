# Stage sweep: stages 9-12

Build: worktree branch based on main 53dcb9c (plus the two fixes below for the re-runs), Release wasm, Chromium 155 (WebGPU,
Vulkan, headless weston). Each run: `?iso=local.iso&nosave=1&stage=S-L`, 110 s,
A at 40/44/48/52 s, stick right/up/left/down from 60 s to 100 s,
`Module._fsa_debug_state()` at 105 s (`tools/web_run.mjs`; see the sweep brief).
Baseline `?stage=1-0` before and after the fix: loads, plays, unchanged.

"Archive" is the boss archive from `lbl_8049ED88[stage][level]`
(game/src/game/game_root_80134C04.cpp); the map itself comes from the stage data.
ms/frame is the game's own time per frame (`[vi] retrace ... game N ms/frame`),
median / max over the run. None of the runs logged a RuntimeError, TypeError,
`main:` crash or exception, and none went back to the title.

| stage-level | archive | loads? | crash? | game ms/frame (median / max) | visual state | problems | triage | fix commit |
|---|---|---|---|---|---|---|---|---|
| 9-0 | boss004.arc | yes | no | 1.9 / 3.5 | Ice-floor arena with rails and four warps, Shadow Link (SLK2) chasing the Links; players move. | none seen | - | - |
| 9-1 | boss005.arc | yes | no | 4.8 / 6.3 | Lava room with a stone platform and arrow turrets (60 PNPC); Links take damage and fall into lava and respawn; a boss health bar at the top. | none seen | - | - |
| 9-2 | boss006.arc | yes | no | 0.9 / 1.2 | Dark forest (darkness 128) with cuccos, leaf tiles, signs; players move. | Translucent blue 16x16 squares with an animated water-like pattern on the dirt floor, tile-aligned, scrolling with the map, changing brightness over time. Not identified (see open problems). | unverified: likely animated water tiles (c); could be (a) | - |
| 9-3 | boss991.arc | yes | **yes, fixed** (at ~414 s) | 2.7 / 4.6 (110 s run); up to 40 in the staff roll | The ending cutscene: maidens, the sword in the pedestal, light shafts, Zelda; then (after a fade to black at about 110 s) the staff roll to "The End". | `RuntimeError: divide by zero` in JPAVolumeSphere during the staff roll; two slow stretches (28-48 fps). See the long run below. | crash (a) wasm traps on divw 0/0; slowness open | 389f534 |
| 10-0 | boss007.arc | yes | no | 0.9 / 1.0 | Forest clearing with tree canopy; Links walk under the canopy (hidden, as on GC) and the arrow markers (YAJI) appear. | none | - | - |
| 10-1 | boss008.arc | yes | no | 0.8 / 9.0 | Grass field, then (after walking down) a rainy area with a house, fog and rain; Links under foliage. | Game time rises to 6-9 ms/frame in the rain area (rain + fog emitters); still under a frame. | (c) probably; noted | - |
| 10-2 | boss009.arc | yes | no | 2.7 / 3.2 | Tingle's Tower interior: one Link, Tingle, chests, gem border lights. | none | - | - |
| 10-3 | boss992.arc | **no** | no | 0.1 / 0.2 | Black screen (2 draws per frame). | `boss992.arc` is not on the disc (Boss/ has boss990 and boss991 only), so `requestMountMemArchive_80401E10` never completes; the root stays in mode 1 with request 7, sub 8; `[stage] entering` is never logged. | (c) not a real level | - |
| 11-0 | boss090.arc | yes | no | 0.7 / 7.2 | Stump test room, walk right: Phantom Ganon (FGAN) arena. | **Was** a full-screen opaque cyan, then a bright green tint over the whole arena. Fixed: now Phantom Ganon appears in a cloud and the arena is purple-tinted, he throws energy balls. | (a) byte order | 91e2da3 |
| 11-1 | boss091.arc | yes | no | 0.7 / 0.8 | Stump test room, then a walled room with an item row (boots, bow, lamp...). | none | - | - |
| 11-2 | boss092.arc | yes | no | 0.7 / 6.3 | Stump test room on sand, then a cliff over clouds with one Link (GROK, KETH, HBMB). | none | - | - |
| 11-3 | boss093.arc | yes | no | 0.8 / 5.8 | Stump test room, then a dark room (darkness 10) with Big Poe (BPOH), torches, Poes. | none | - | - |
| 12-0 | boss094.arc | yes | no | 0.7 / 5.9 | Stump test room, then a grey dungeon arena with Phantom Ganon 2 (FGN2) splitting in a smoke cloud. | none (FGN2 does not set the screen colour) | - | - |
| 12-1 | boss095.arc | yes | no | 0.7 / 8.5 | Sand test room, then a purple dungeon with arrows, portals, then a GBA SP window ("Z Close") showing a side-view interior. | none | - | - |
| 12-2 | boss096.arc | yes | no | 0.7 / 5.0 | Ice test room, then a side-view tower interior with one Link. | none | - | - |
| 12-3 | boss097.arc | yes | no | 0.6 / 0.9 | Room with "TEST" written in blocks, then a walled room. | none | - | - |

Stages 11 and 12 start in a test room (stumps or blocks spelling text); walking
right leads into a boss arena or a special room, so they look like the
developers' boss / special-room test maps. They need no story state.

## 9-3 long run (the staff roll question)

The black screen at about 110 s is the fade into the staff roll, not a hang.
Runs of 480-620 s with no input (build/web-run/s93long, s93long2, s93hang):

- 110-420 s: the Hyrule world atlas, then scenes from each world (Helmaroc
  King over the sea with cloud reflections, Kakariko, the Gorons, the
  desert, ...) with the credits text fading in and out, the world titles
  ("Desert of Doubt"), all rendered correctly; then the last credits page
  (Supervisors / Special Thanks / copyright, at about 480 s) and "The End"
  (reached at about 440 s of game time; frames still advancing afterwards).
- **Crash fixed:** before 389f534 the game died at about 414 s with
  `RuntimeError: divide by zero` in `JPAVolumeSphere` (stack:
  JPAVolumeSphere <- JPABaseEmitter::createParticle <- JPAEmitterManager::calc
  <- GameRoot2_8049F108::fn_8013BBE0). With the fix the roll runs to the end.
- **Slow stretches:** at about 126-143 s (the Helmaroc / cloud scene, about
  930 draws per frame) the game takes 14-25 ms per frame and runs at 35-48
  frames per second; at about 410-435 s (the last credits page with a big
  particle burst, up to about 2000 draws) 30-40 ms per frame, 28-30 frames per
  second. One run (s93long2) stopped responding in that second stretch (the
  renderer at 100 % CPU, CDP evaluate and Debugger.pause unanswered); a second
  run without screenshots (s93hang) went through it and reached "The End".
  Not reproduced, so not investigated further; the machine was shared with
  three other sweeps at the time.

## Fix

- **91e2da3** `PhantomGanon: pass the screen colour to fn_8013A884 as 0xRRGGBBAA`.
  `PhantomGanon_804A1318::update` (game/src/game/act_phantom_ganon_8017C01C.cpp)
  sets the full-screen colour (`ShadowCircle_804A0160::field_0x4`) through
  `fn_8013A884(u32*)` by casting a `TColor*` to `u32*`; `fn_8013A884` reads the
  word and calls `TColor::set(u32)`. The word is 0xRRGGBBAA only on a big-endian
  CPU; the port's `TColor::set(u32)` takes the value 0xRRGGBBAA, so on wasm the
  colour came out byte-reversed: the "off" sentinel (255,255,255,0) became
  opaque cyan (0,255,255,255), filling the screen
  (`ShadowCircle_804A0160::fn_80158F70` only skips the pass for 0xFFFFFF00),
  and the arena's purple multiply tint (0x9F,0x7F,0xFF,0) became a green one.
  Every other `fn_8013A884` caller already passes a packed value. Verified on
  11-0 (and 1-0 unchanged).
- **389f534** `JPAVolumeSphere: no wasm trap on the fixed-interval 0 / 0 divisions`.
  A fixed-interval sphere emitter divides by `mVolumeAngleMax - 1` and
  `mDivNumber - 1` (game/libs/jsystem/JSystem/JParticle/JPADynamicsBlock.cpp).
  `JPAResource::calcWorkData_c` starts `mVolumeAngleMax` at 1, so the first
  particle of each emission divides 0 by 0. PowerPC's `divw` yields 0 there
  (non-negative dividend); wasm traps. Under FSA_PORT the quotient is 0 when
  the divisor is 0. Verified: the staff roll runs to "The End"; 1-0
  unchanged. Any other integer division by a data-driven count could trap
  the same way.

## Open problems

1. **Staff-roll slowdown, and one unresponsive run** (medium). See the 9-3
   long run: game time per frame scales with the draw count (about 20 us per
   draw at 900-2000 draws, mostly particles), dropping to 28-30 fps on the
   last credits page; one of three long runs stopped responding there. Where
   the game time goes (JPA particle draw submission is the suspect) was not
   profiled.
2. **9-2: translucent blue squares** (low; unverified). In the 9-2 forest
   (darkness 128) about four 16x16 translucent blue squares with an animated
   water-like pattern sit on the dirt floor, tile-aligned, scrolling with the
   map, each changing brightness over time (build/web-run/s92/shot-020s.png,
   shot-030s.png). They are not actors (LEAF and LFGN have no draw; the WARPs
   are on other layers), so they come from the tile map
   (`TileMap_804AA858::fn_802FC80C` / `fn_802FC268`, or the second
   translucent layer-0 pass when `field_0x2365` is set). They may be animated
   water tiles as designed; there is no reference to compare with. Worth a
   look by someone who knows the level. Related look: the cloud reflections
   on the sea in the staff roll (build/web-run/s93long/shot-140s.png) have a
   few straight, tile-aligned edges.
3. **10-3 does not exist** (expected): `lbl_8049ED88[10][3]` is
   `boss992.arc`, which is not on the disc, and the mount never completes, so
   `?stage=10-3` hangs on a black screen. Not a port bug.

## For the decomp

- `fn_8013A884(u32*)` takes a pointer to a colour word. In
  `act_phantom_ganon_8017C01C.cpp` the original passes a `TColor*` cast to
  `u32*` (TColor's bytes as a big-endian word); the other callers build the
  word with shifts. A port has to pass the packed value there.
- `JPAVolumeSphere` divides 0 by 0 on the first particle of every
  fixed-interval sphere emission (`mVolumeAngleMax` starts at 1); harmless on
  PowerPC (`divw` gives 0), a trap on any host that faults on integer
  division by zero.
