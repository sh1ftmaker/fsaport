# Stage sweep: stages 3-5 (levels 0-3)

Build: main at 53dcb9c (Release, web). Each run: `?stage=S-L&nosave=1`, 110 s,
Space (A) at 40/44/48/52 s, stick right/up/left/down from 60 s to 100 s,
`_fsa_debug_state()` at 105 s, a screenshot every 10 s
(`node tools/web_run.mjs ... --out build/web-run/sSL`). Baseline `?stage=1-0`
played normally (60 frames per 60 retraces, game 1.0 ms, 112 draws).

Every run loaded ("[stage] entering stage S level L"), had no RuntimeError,
TypeError, `main:` or exception, never went back to the title, and kept 60
frames per 60 retraces. The only console errors are the two 404s and
`aurora::card: Failed to open file: gc4sword` that the 1-0 baseline also has.
ms/frame is the mean of the last 12 `[vi]` reports (the machine was shared
with three other sweep agents, so the timings are noisy).

| stage-level | archive | loads? | crash? | game ms / aurora_end ms (draws) | visual state | problems | triage | fix |
|---|---|---|---|---|---|---|---|---|
| 3-0 | boss040.arc | yes | no | 2.9 / 0.4 (204) | Rainy forest; opening dialogue ("Hyrule Castle fights an even greater darkness"), then play; rain overlay, HUD, 4 Links move | none seen | - | - |
| 3-1 | boss041.arc | yes | no | 4.2 / 0.6 (367) | Swamp with poison pool, Octoroks attack (hearts lost), owl dialogue "A poison swamp lies ahead!" appears at ~70 s and stays (no A presses left) | none seen | (c) dialogue needs more A | - |
| 3-2 | boss202.arc | yes | no | 0.8 / 0.9 (37) | Single-Link forest clearing, walks around, HUD | none seen | - | - |
| 3-3 | boss043.arc | yes | no | 5.6 / 0.6 (114) | Dark (darkness 130, colour 96c8ff) cliff plateau by water; 4 Links move | game logic 5-7 ms for 114 draws (highest of the sweep); 85 `[vi]` reports in 110 s vs ~100 elsewhere (startup took longer, frames still 60/60) | probably load from the other agents; not investigated | - |
| 4-0 | boss050.arc | yes | no | 2.6 / 0.5 (200) | Forest of Light opening dialogue with fairy; still in dialogue at 110 s | none seen | (c) needs more than 4 A presses | - |
| 4-1 | boss051.arc | yes | no | 4.2 / 0.7 (349) | Forest with fires; Kakariko gate cutscene + dialogue at ~100 s | none seen | - | - |
| 4-2 | boss203.arc | yes | no | 0.7 / 1.0 (17) | Single Link by a Great-Fairy-like NPC on a dirt patch; walks the field | none seen | - | - |
| 4-3 | boss053.arc | yes | no | 3.8 / 0.5 (103) | Dim field with Deku-scrub plants, reaches a castle wall | none seen | - | - |
| 5-0 | boss060.arc | yes | no | 2.8 / 0.4 (89) | Desert; opening dialogue ("The road you must take is enveloped in a fierce wind") | **large opaque dark-grey (#202020) soft blobs drift over the desert and hide the Links** | (a) Aurora / port, see below | open |
| 5-1 | boss061.arc | yes | no | 2.0 / 0.3 (86) | Desert with stairs | same blobs | (a) | open |
| 5-2 | boss205.arc | yes | no | 0.7 / 1.2 (18) | Single Link + dark-haired NPC by a fence | none seen | - | - |
| 5-3 | boss063.arc | yes | no | 2.5 / 0.3 (78) | Desert, then a brick temple wall | same blobs | (a) | open |

## Open problem: desert heat-haze particles draw as opaque #202020 blobs (5-0, 5-1, 5-3)

What draws them: actor SEV2 (`act_environment_control_2.cpp`, param
`00200000`, bit 21) sets `GameRootSub24_8016A3AC::field_0x1d2`, and
`fn_8016DE30` (`game_root_sub_24_8016A2B4.cpp:247`) spawns JPA emitters 0x543
and 0x544 (plus wind sound 0x50078). Both are "kagerou" (heat haze) effects:

- 0x543: BSP1 flags 01900002 (billboard, projected texture, tex-coord anim),
  blend 05d9 (BLEND, SRCALPHA / INVSRCALPHA), colour arg set 0 (TEXC),
  alpha set 0 (TEXA * A0), textures AK_kagerouSwap00 (4x4 placeholder),
  AK_kagerouIndMM00 (IA8 indirect map), AK_kagerouMask00 (IA4 mask); ETS
  indirect 0.75 / 0.75, exp -5; ESP alpha base 0.25.
- 0x544: same without the ETS, alpha base 0.3.

`EffectSystem_8049F178` swaps AK_kagerouSwap00 for the screen-capture texture
`gGameRoot2_8053AB10->field_0x34` (`effect_system_8013C5C8.cpp:106`), which
`fn_8013D6E4` fills each frame with `JUTTexture::capture` (GXCopyTex, RGB565,
608x448) after the scene, before emitter group 5 (2D) draws. On the GameCube
this re-draws the screen slightly distorted (nearly invisible shimmer).

What was checked (with temporary instrumentation, since removed):

- The copy runs every frame (Aurora `GXCopyTex`: rect 0,0 640x480, dst 640x480,
  fmt 4, no clear) and the JPA texture binds the copy handle (same `dest`
  pointer 0xf54de0, found in `copyTextures`).
- The projected texture coordinates are right: the 2D draw info maps the
  particle to screen space (0x544 centre at s,t = 0.5,0.5 for pos 304,224;
  0x543 at 0.575,1.05 for pos 350,470 just below the screen).
- Turning off the indirect stage, or the mask stage, leaves the colour
  #202020 (the frame-buffer border / clear colour); without the mask stage the
  whole rotated quad is solid #202020. So stage 0 samples #202020 from the copy
  with alpha 1.
- Aurora resolves an RGB565 copy through `tex_copy_conv` (`FragRGB565`, alpha
  forced to 1). Forcing the copy through the passthrough blit or as RGBA8 makes
  the blobs disappear, but that only keeps the EFB alpha (probably 0, so the
  particles become transparent). It does not prove the colour is right, so it
  was not committed.
- Changing `FragRGB565` to sample by fragment position instead of the
  `uv_xf` uniform did not change the result.

Next steps: read back the conv texture, or draw it full-screen in a debug
mode, to tell bad copy contents (`tex_copy_conv` RGB565 path on the web,
`extern/aurora/lib/gfx/common.cpp` render() resolve, `tex_copy_conv.cpp`)
from bad sampling of a conv texture in the game shader
(`extern/aurora/lib/gx/shader.cpp`, MTX3x4 / POS texgen). Other games or
stages that use RGB565 `JUTTexture::capture` probably have the same problem.

## Notes

- 4-0 and 3-1 end in dialogue because the run only presses A four times.
  This is not a bug.
- No reverb (a known limitation).
- Decomp: nothing new. The JPA, BSP1 and ETS data for 0x543 and 0x544 read
  correctly big-endian (sane flags, colours, matrices and alpha timings).
