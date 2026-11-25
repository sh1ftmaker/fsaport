# Stage sweep: stages 0-2

Build: main at 53dcb9c (Release, `build/web`), Chromium 155 headless-weston
(pixman, X11, Vulkan WebGPU). Each run: `?iso=local.iso&nosave=1&stage=S-L`,
110 s, Space (A) at 40/44/48/52 s, stick right/down/left/up for 10 s each
from 60 s, `_fsa_debug_state()` at 105 s (command in the sweep brief).
"game ms" is the game's CPU time per frame after the stage was entered
(average / max of the 60-frame `[vi] retrace` lines); every stage ran at a
steady 60 frames per 60 retraces (one or two 59-frame periods during loads).

Present in every run and harmless: two 404s (the page's `POST /log` without
`serve.py --logs`, and the favicon) and `aurora::card: Failed to open file:
gc4sword` (`nosave=1`, empty card).

| Stage-level | Archive | Loads? | Crash? | Game ms/frame (avg / max) | Visual state | Problems | Triage | Fix |
|---|---|---|---|---|---|---|---|---|
| 0-0 | boss010.arc | yes | no | 2.2 / 2.6 | Opening: black "Link! Link! You must awaken" text box over the four sleeping Links, then the forest with Kaepora Gaebora; text, colour names (green, magenta), sun-shaft overlay and HUD all correct | 4 presses are not enough for the dialogue; see the longer runs below | (c) | - |
| 0-1 | boss011.arc | yes | no | 3.5 / 4.8 | Four Links on Hyrule field (dirt / grass, torches, statues); formation walk, scrolling, HUD fine | none | - | - |
| 0-2 | boss200.arc | yes | no | 3.4 / 4.5 | Single Link (level-2 slot is the boss20x one-Link interlude): grass corridor, rock pillars, a large Force Gem bouncing around; no HUD in this mode | none seen | (c) | - |
| 0-3 | boss013.arc | yes | no | 1.0 / 3.5 | Stone plaza, soldiers with crossbows; formation splits when hit; game over screen from 90 s | Game over: the stick drives the Links into the soldiers and the shared hearts run out (empty hearts, Link dazed at 80 s) | (c) legitimate combat death | - |
| 1-0 | boss020.arc | yes | no | 2.4 / 5.2 | Forest stage (baseline): Links, Octorok, rocks, leaves, HUD fine | none | - | - |
| 1-1 | boss021.arc | yes | no | 2.1 / 5.0 | Shallow-water field with splash rings round the Links' feet, cloud reflections | none | - | - |
| 1-2 | boss201.arc | yes | no | 2.6 / 3.6 | One-Link village interlude: Cuccos, a villager, house, fences; scrolling fine | none | - | - |
| 1-3 | boss023.arc | yes | no | 4.1 / 5.1 | Temple courtyard in rain (streaks, splashes), lightning flash (white-out frame at 100 s), trees overlapping the Links | none | - | - |
| 2-0 | boss030.arc | yes | no | 1.6 / 3.1 | Fairy intro dialogue ("northern Hyrule, base of Death Mountain"), then soldiers / archers; fine | none | - | - |
| 2-1 | boss031.arc | yes | no | 3.7 / 4.6 | Mountain path with Force Gem and cloud shadows; walking up triggers Kaepora's dialogue (stays open at the end: no more A presses) | none | (c) | - |
| 2-2 | boss202.arc | yes | no | 0.7 / 1.0 | One-Link interlude by a tree with an NPC; Link walks under the foreground canopy | none | - | - |
| 2-3 | boss033.arc | yes | no | 0.7 / 1.0 | Mountain plateau, cliff edge with sky and clouds below on the right; formation turns at the wall | none | - | - |

No RuntimeError, TypeError, `main:` crash or unexpected return to the
title ("Title　" heap line) in any of the twelve runs.

## Longer stage 0-0 runs

- 260 s, A every 3 s from 40 s: the whole Kaepora Gaebora dialogue plays
  (through "Let's call him Shadow Link"), then the HUD appears and the green
  Link is under player control in the clearing while the other three Links
  stay asleep. Frames steady, 2.0 ms/frame, no errors.
- 260 s, A every 3 s to 100 s, then the stick up/right/down/left for 15 s
  each: green Link walks the clearing freely (dark grass path, sun-shaft
  overlay, the three sleeping Links, HUD all correct). No return to the title
  within 260 s: getting there needs the opening's story progress (waking the
  others / reaching the exit), which this input does not do. Not chased.

## Summary

All twelve stage/levels load and play with no crash, no script errors and
no rendering defects found in the screenshots. No port-side fixes were needed
in this range. Open items: none; the only non-normal outcomes (0-3 game over,
dialogue still open at the end of 0-0 and 2-1) come from the fixed input
script.
