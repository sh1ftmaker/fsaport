# Stage sweep: stages 6, 7 and 8 (levels 0-3)

Runs from the page's `?stage=S-L` debug jump (`?iso=local.iso&nosave=1`), 110 s
each with `tools/web_run.mjs`: Space (A) at 40/44/48/52 s, the stick held
right / up / left / down for 10 s each from 60 s, `_fsa_debug_state()` at
105 s. Baseline `?stage=1-0` before and after. Chromium 155, headless weston,
NVIDIA (Vulkan). "ms" is the game's own time per frame (`[vi] retrace` lines)
after the stage is entered, averaged and peak; every run kept 60 frames per
60 retraces unless noted.

| stage-level | archive | loads? | crash? | ms before fix (avg / peak) | ms after e8e2c50 (avg / peak) | visual state | problems | triage | fix |
|---|---|---|---|---|---|---|---|---|---|
| 1-0 (baseline) | boss010 | yes | no | 2.4 / 5.2 | 1.0 / 1.6 | forest intro dialogue, then play | none | - | - |
| 6-0 | boss060 | yes | no | 6.5 / 8.9 | 1.7 / 2.0 | snowy Frozen Hyrule, fairies' dialogue (needs ~20 A presses), then play with snowfall | none | - | - |
| 6-1 | boss061 | yes | no | 11.4 / 15.5 (50 fps dips) | 1.4 / 1.6 | snow field, 4 Links in formation, ice palace entrance | slow before fix | (a) | e8e2c50 |
| 6-2 | boss205 | yes | no | 0.7 / 0.8 | 0.6 / 0.6 | blue-brick tower interior (Tingle Buster / Vaati Buster actors), one Link | only one Link from the start (see open questions) | ? | - |
| 6-3 | boss063 | yes | no | 8.1 / 11.0 | 2.1 / 2.4 | snow forest, ice temple stairs | slow before fix | (a) | e8e2c50 |
| 7-0 | boss070 | yes | no | 5.0 / 10.0 | 1.4 / 3.4 | sky realm, fire-jet statues; scripted stick walks the Links into the jets: GAME OVER at ~95 s | game over | (c): idle run keeps full hearts | - |
| 7-1 | boss071 | yes | no | 5.9 / 7.2 | 2.0 / 4.5 | lava room with chain bridges, one Link; stick walks it into lava: GAME OVER | game over; one Link | (c) game over; one Link ? | - |
| 7-2 | boss206 | yes | no | 3.1 / 3.7 | 1.8 / 3.0 | night sky, clouds, tower; birds; GAME OVER at ~95 s | game over | (c): idle run keeps full hearts | - |
| 7-3 | boss073 | yes | no | 0.8 / 0.9 | 0.9 / 1.9 | stone sky palace, eye floor tile, 4 Links | none | - | - |
| 8-0 | boss002 | yes | no | 4.9 / 5.9 | 2.9 / 5.7 | test room: six Force Gem pillars, triangle markers, black pits, no HUD; camera stays put while the Links walk off screen | none known (special/debug map) | (c) | - |
| 8-1 | boss207 | yes | **hang** (before fix) | 1.5 / 21.9, then the page stopped responding | 0.9 / 2.1 (tile attack included) | item/chest room, then the flying-floor-tile room (FLYO x 120) | page hung ~88 s in: tile attack; the tiles fell straight down instead of flying at the Links | (a) Aurora; (b) port float->u16 | e8e2c50, 96dead5 |
| 8-2 | boss081 | yes | no | 3.3 / 5.0 | 1.4 / 2.6 | grass test field with Force Gems (picked up: 65) | none | - | - |
| 8-3 | boss990 | yes | no | 1.1 / 3.0 | 1.1 / 2.5 | adventure opening: "Long ago..." scroll, "Link... Link..." light, clouds over Hyrule | the unskippable prologue (Start and A do not skip it) still runs at 150 s, so the castle was not reached; the text, scroll and effects render fine | (c) | - |

No run logged a RuntimeError/TypeError/`main:` crash or went back to the title
(`Title　`). The only console errors in every run, the baseline included, are
the missing memory card file (`nosave`) and two 404s (favicon).

## Fix: Aurora palette textures cached by content (e8e2c50)

8-1's second room is the flying-floor-tile trap: about 120 `FLYO` actors
(`game/src/game/act_flying_tile.cpp`) that lift off one by one and fly at the
player. Each draws itself twice (tile and shadow) with
`TileMap_804AA858::drawBlockMap_802F6CF4`
(`game/src/game/tile_map_802F5040.cpp:2785`), which runs `GXInitTlutObj` for 16
TLUTs and `GXInitTexObjCI` before every draw. In Aurora every init is a new
texture identity, so the texture-object cache never hit: every draw converted
the 128 x 512 C4 tile sheet to RGBA and uploaded it, and each idle cache entry
kept its own 256 KB texture for 300 frames. Game time climbed from 1 ms to
22-36 ms a frame, and in the first run the renderer stopped answering (CDP
`Debugger.enable` timed out; main thread spinning in wasm). A CPU profile
(CDP `Profiler`, 6 s at 92 s) put 50 % in `JKRExpHeap::do_alloc` and 15 % in
`convert_texture_palette`, all under `aurora::gx::fifo::handle_draw` ->
`new_static_texture_2d`.

`resolve_static_palette_texture` (`extern/aurora/lib/gx/gx.cpp`) now also keys
converted textures by content: data address, size, format, mips, TLUT format
and entries, and XXH3 of the texel and TLUT bytes, so a palette or image
rewritten in place still gets a new texture. Idle entries are swept with the
others. 8-1 now peaks at 2.1 ms through the whole tile attack. Every BG layer
re-inits its TLUTs each frame too (`tile_map_802F5040.cpp:1112`, `:1408`), so
the fix lowers the cost everywhere: 1-0 from 2.4 to 1.0 ms, 6-1 from 11.4 to
1.4 ms, aurora_end from ~1.7 to ~0.5 ms.

## Fix: enemy aim via `findTargetPlayer_801EEC98` (96dead5)

With the hang gone, the flying tiles still missed: each fell straight down its
own column and hit the bottom wall while the Links stood at the left wall
(`build/web-run/s81b`, shots 82-94 s). `FlyingTile_804AC100::update` aims with
`ActorBase::findTargetPlayer_801EEC98` (`game/src/game/actor_base_801EE24C.cpp`),
which casts the scaled `atan2_` float straight to `u16`. For a player on the
actor's left the float is negative: the GameCube wraps it (`fctiwz`, low bits),
wasm saturates it to 0, i.e. "straight down". The four casts now go through
`s32` under `FSA_PORT`, as ac04498 did for JPA particles. After the fix the
tiles home in on the Links and the trap empties the 3 hearts within seconds
(`s81aim`, GAME OVER at ~90 s); 1-0 unchanged. Every enemy that targets with
this function was affected, so aim-based attacks across all stages change.

## Open questions

- **One Link in 6-2 and 7-1** (and in 7-2 after the Links were hurt): 6-2
  (`boss205`) and 7-1 (`boss071`) start with only Green Link on screen; the
  other stage/levels show four. The level-2 column of `lbl_8049ED88` is the
  `boss20x` archive in every world, so the same should show in the other
  sweeps' X-2 runs. Not checked against the GameCube; may be by design for
  these maps (6-2 has Tingle Buster / Vaati Buster actors).
- 7-0, 7-1 and 7-2 end in GAME OVER within ~40 s of the scripted stick input
  (fire jets, lava, birds) with the 3 hearts the debug jump starts with. Idle
  runs of 7-0 and 7-2 keep full hearts for 110 s, so this is the input, not
  damage from nowhere.
- 8-0 (`boss002`) looks like a developer test map (no HUD, black pits, fixed
  camera); nothing to compare it with.

## Notes for the decomp

- `TileMap::drawBlockMap_802F6CF4` and the BG layer draws re-init their
  `GXTlutObj`/`GXTexObj` on the stack every call; on the GameCube this is free.
  Ports with identity-keyed texture caches pay a texture conversion per call.
- Float-to-unsigned casts of negative angles (see 96dead5 and ac04498): the
  same `(u16)(atan2_(...) * k)` pattern is still in
  `act_special_conversation_npc.cpp:320`, `act_vaati_time_up.cpp:180`,
  `map_screen_80403D84.cpp:1523`, `act_tingle_tower.cpp:79`,
  `act_cucco.cpp:1506`, `act_digging_game.cpp:34`,
  `act_shooting_gallery.cpp:28`, `act_bow_soldier_801555D8.cpp:301` and
  `act_old_man.cpp:807` (the hammer and whack-a-mole shops already go through
  s32). Left alone here to keep this sweep's edits local; each gives angle 0
  for a target on the left on wasm.
