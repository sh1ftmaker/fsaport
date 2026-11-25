# Multiplayer (branch `deluxe`)

Four Swords Adventures is a four-player game in which every player holds a
Game Boy Advance: it is their controller and, inside houses, caves and the
underground, their own screen. The GameCube runs the one game; each GBA runs a
small client program the GameCube sends it over the controller cable, and from
then on the two exchange a few dozen bytes a frame.

This port keeps the game unmodified above that cable, as katam-port does with
the GBA link cable (`katamport/port/docs/MULTIPLAYER.md` §2). Below it, instead
of a GBA, a native endpoint answers the game: no GBA emulator, no client
program, only what the game checks, word by word.

This document is four things: what the game does on the link (§1, read from
the decompilation: `game/src/game/tu_803006E8.cpp`, `tu_803099FC.cpp`,
`tu_80301CD0.cpp`, `adv_entry_screen_803E1668.cpp`); the seam (§2); what is
implemented and verified (§3); and the options for remote play, with a
recommendation (§4). Open questions for the owner are in §5.

Conventions: **verified** = run in this branch's build, with the run named;
**read** = taken from the code without running it; **assumed** = neither.

---

## 1. What the game does

### Who is a GBA, and why multiplayer needs four of them

`cls_804AA8C8::pollInputs_8030A43C` (`tu_803099FC.cpp`), once a frame, looks
at each controller port through its GBA client (`lbl_804DB798[port]`,
`GbaClient_803007D0`). The client thread probes the port (`SIProbe`); if it
answers `SI_GBA` (0x40000) the port is a GBA and that player's buttons come
from the GBA's key word, otherwise from the GameCube pad (`JUTGamePad`). Both
paths fill the same per-player button word (`field_0x4[port]`), so the rest of
the game does not care. (read; verified for the GBA path)

The adventure's player-count screen (`AdvEntryScreen_804B0B00::fn_803EDBF0`)
counts players like this: a GameCube pad in port 1 means single-player only;
a GBA in port 1 plus GBAs in ports 2, 3, 4 *in a row* allow 2, 3, 4 players
(`field_0x1bc`). So **every player of a multiplayer game, player 1 included,
is a GBA**; a gap (GBA, pad, GBA) stops the count. (read; verified: with
`?gba=4` the menu offers all four counts, with `?gba=1` single-player only)

### The client thread and its messages

One thread per port (`fn_80300E1C`, started by `startGbaClients_803007D0`, priority
8) takes messages from `pollInputs_8030A43C` and drives the SDK's GBA calls:

| msg | what | when |
|---|---|---|
| 6 | `SIProbe`; 0x40000 = GBA (`mState`) | the port's type is unknown |
| 7 | `GBAJoyBoot` the client program, then as 8 | GBA present, not connected (`field_0xd8 == 0`) |
| 8 | `GBAReset`, then the handshake below | after boot |
| 5 / 4 / 3 | resend the session packet / acknowledge / drop and reprobe | posted by the packet callback `fn_80300A8C` when the GBA's packet differs |
| 10 | the frame: write the player's record, read the key word | every frame while connected, once the game has started (`lbl_8053AAF0->mState >= 4` or `field_0x60`) |

Every exchange has a timeout (0.5-1 s of `OSGetTime`) after which the client
is reported broken (`field_0xd3`), the port reprobed and the GBA rebooted.

### The handshake, word by word (what the endpoint answers)

`GBAGetStatus` returns the JOY status byte: `PSF1` 0x20 and `PSF0` 0x10 are
flags the GBA program sets, `SEND` 0x08 means the GBA has a word for the
GameCube, `RECV` 0x02 means the GameCube's last word is still unread.

1. `GBAJoyBoot(port, palette = {2,4,0,6}[port], speed 2, client_thread.bin)`.
   The game has patched the program's header first: the disc's game code at
   0xAC (`G4SE`), the header checksum at 0xBD, and a boot tick
   (`lbl_8053ACC8 = OSGetTick()`) at 0xC8.
2. `GBAReset`; status must be **0x28**; `GBARead` = the GBA's game code, which
   must equal the disc's (or `g4se`/`g4sj`).
3. status **0x20**; `GBAWrite` the disc id; status **0x30** ("session up").
4. `GBAWrite 0x04000000` ("send your packet"); 8 × (status **0x38**,
   `GBARead`) = the GBA's 32-byte session packet (`GbaPacket_80300A8C`).
5. `fn_80300A8C` compares it with the GameCube's copy for that port
   (`lbl_804D6AB8[port]`, which `GBAJoyBoot`'s success filled with
   `{1, port, 0, 1, boot tick, 0...}`). Equal: status **0x30**, `GBAWrite`
   a fresh tick, status **0x38**, `GBARead` must return the same tick. Then the
   client is **connected** (`field_0xd8 = 1`).
   Different: message 5 (`0x02000000` + the GameCube's 8 words, the GBA sends
   them back), 4 (`0x03000000`) or 3 (drop).
6. Each frame (message 10): status 0x30; `GBAWrite` the record's words; then,
   with `RECV` clear, `GBARead` the **key word**:

       bits 16-23  GBA keys 0-7 (A B Select Start Right Left Up Down)
       bits  8-9   GBA keys 8-9 (R L)
       bit  15     unknown (pollInputs copies it to field_0xe6)
       bits  0-7   check byte: fn_803006E8 (a CRC, polynomial 0xCD) over bits 8-23
       bits 24-31  the GBA's acknowledgement: 2 = file received, 3 = message received

   `pollInputs_8030A43C` maps the keys onto the pad's bits: A 0x100, B 0x200,
   Select 0x400 (the pad's X), Start 0x1000, the d-pad 1/2/4/8, R 0x20, L 0x40.

### The per-frame record

`lbl_80506DE8[player]` (`cls_804AA858Sub11C`, `game/Map.h`), built each frame by
`cls_804AA8B8::fn_80304FB0`. Its length depends on how many GBAs are linked
(`lbl_80541618`): **26 words with one, 24 with two, 16 with three, 12 with four**
(the link's bandwidth is shared). Word 0 is a checksum byte, the record kind and
header bits; word 1 depends on the frame's phase (`lbl_80506D48`, 0-3:
life, rupees/force gems, timers, ... in turn). By kind:

| kind | content |
|---|---|
| 5 | the regular view: four player marks (screen x, y, sprite) whose spare bits carry the BG scroll (9-bit x, y); up to 19 sprite words (10-bit id, x, y, kind, flags; sorted by kind); the layer mask word. `mValid` (header) is nonzero when the player is in a sub-area (layer >= 8) |
| 14 | the same for a player who is linked but not taking part |
| 9, 13 | nothing to show (menus, cutscenes, loading) |
| 6, 7, 8 | a file: 6 = header (byte-swapped flag and GBA address), 7 = data, 8 = the last chunk. The GameCube then waits up to 60 frames for ack 2 in the key word, else resends |
| 10, 11, 12 | a message's characters (16-bit codes, byte-swapped), 12 = the end; ack 3 |

The files (read: `fn_80303CE4`, `fn_803049F0`, `fn_80304B00`; verified: all
arrive and decompress):

| when | file | GBA address |
|---|---|---|
| after boot | sound_data, gb_ch_obj_base_sch, gb_cl_obj_base_scl, player_sch, bg_a_spl, player_sob, enemy_both_sob, enemy_both2_sob, moji_english_sch (all Yaz0), framed by end_code_scl.bin at 0xAAAAAAAA and 0xFFFFFFFF | 0x02033000, 0x06010000 (OBJ tiles), 0x05000201 (OBJ palette), 0x02034000, 0x0203C000, 0x02010000, 0x02021000, 0x02018E00, 0x02038000 |
| a stage starts | the stage's BG tiles and palette (24 KiB, 512 B) | 0x06000000, 0x05000001 |
| entering a room | the room's two view files (`fn_802D8128` / `fn_802D8228` names) | 0x0600C000, 0x0600E000 |
| some bosses | extra OBJ tiles (gb_ch_obj_tr_sch*) | 0x06017000 |

The boot set is about 80 KB on the wire. At 22 data words a frame (one or two
GBAs) it takes ~15 s, at 10 (four GBAs) ~33 s; the game plays meanwhile.

So **the GBA screen is drawn by the GBA from data it already holds**: tiles,
palettes and sprite definitions arrive once; each frame says where the camera
is and which sprites stand where. Nothing like a video frame crosses the link.
The GameCube has its own drawing of the same records too
(`TileMap_804AA858::drawPlayerPanel_802FB69C`, `fn_803022C4`, `fn_80302C34`: SOB
sprites at the records' positions), which is a reference for a native renderer.

### A port-only byte-order bug on this path

The client thread picks the frame's word count from `mTransfer[3] & 3`, the
record's `field_0x3_0` under MWCC's MSB-first bitfields. Under clang's
LSB-first layout that byte's low bits are the record *kind*, so the port sent
24 or 16 words where the game meant 12. Fixed under `FSA_PORT` in
`tu_803006E8.cpp` (`(mTransfer[3] >> 6) & 3`). It is a port endianness fix, not
a decomp bug, and only reachable with a GBA attached.

---

## 2. The seam

```
 game (unmodified)                         platform (deluxe)
 ─────────────────                         ─────────────────
 pollInputs_8030A43C ─ msgs ─► client thread fn_80300E1C
                                   │ SIProbe ───────────► aurora_si_probe_hook ─┐
                                   │ GBAJoyBoot/Reset/                           │
                                   │ GetStatus/Read/Write ─► gba_link.cpp: one   │
                                   │                         endpoint per port ◄─┘
                                   │                           │ keys()   ▲
                                   │                           ▼ record() │ file()
                                   │                         FsaGbaPeer (swappable)
                                   │                           local: PADStatus[port]
                                   │                                  + gba_view.cpp
```

`platform/gba_link.h`:

```c
struct FsaGbaPeer {
  const char* name;
  u16  (*keys)(struct FsaGbaPeer* p, int chan);                 /* GBA_* bits, answer at once */
  void (*record)(struct FsaGbaPeer* p, int chan, const u32* words, int count);
  void (*file)(struct FsaGbaPeer* p, int chan, u32 address, const u8* bytes, u32 size, int flag);
  void* user;
};
int FsaGbaAttach(int chan, struct FsaGbaPeer* peer);   /* NULL = the local peer */
int FsaGbaDetach(int chan);
```

- **The game does not change** (one port byte-order fix aside, §1). The
  endpoint (`platform/gba_link.cpp`) implements the SDK's GBA calls and plays
  the client program's side of §1: status flags, the handshake, the packet
  echo, the tick echo, the record, the key word and its acknowledgements.
  `SIProbe` gets one hook in Aurora (`extern/aurora/lib/dolphin/si/si.cpp`,
  `aurora_si_probe_hook`); with nothing attached the GBA calls answer
  `GBA_NOT_READY` as the old stubs did.
- **What is swappable is the peer**, the player behind the GBA: who presses
  its buttons and who looks at its screen. Katam's `PortMpTransport` answers
  "what did the other *game instances* put on the cable"; here the other end
  of every cable is a thin client, so the peer interface is *keys up, records
  and files down*. A network peer buffers keys from a socket (`keys` must
  answer at once: it runs inside the game's client thread) and forwards records
  and files to a remote screen.
- **The local peer** takes a GBA's keys from `PADStatus[port]` as the game read
  it at the latest `JUTGamePad::read` (hook in `platform/input.cpp`, after the
  harness), so keyboard, gamepads and the harness's four pads all drive GBAs,
  and lockstep runs stay deterministic. It feeds the native view.
- **The native view** (`platform/gba_view.cpp`), first phase: a GBA memory
  image per port (EWRAM, palette RAM, VRAM) with the files placed at their
  addresses (Yaz0 decoded), the latest record decoded, and a 240 × 160 debug
  picture: in a sub-area the BG files and the sprites as boxes and the player
  marks; elsewhere a sheet of the tiles the GBA holds (BG and OBJ characters
  with their palettes), which shows the data arrived intact.
- **The page** (`platform/web/fsa_gba.js`): `?gba=N` plugs GBAs into ports
  0..N-1 (`?gba=0,2` for a list), `?gbaview=0` hides the panel, `?gbaview=S`
  scales it. Live keys for ports 2-4: IJKL/U/O/P, TFGH/R/Y/V, arrows/Numpad1-3
  (ORed with that port's gamepad). `window.fsaGba` has `attach(port, on)`
  (hot-plug), `link(port)`, `view(port)`, `memory(port, region, len)`,
  `dump()`.
- **The harness** drives four pads in scripts: a step may say `"pad": 0-3`
  (`docs/HARNESS.md`). The state stream has the player-count screen's state,
  cursor and connected count (`lbl_8053AD98->field_0x4->field_0x1d4/1bc/1d0`).

---

## 3. What is implemented, and verified

| | |
|---|---|
| `platform/gba_link.{h,cpp}`: SDK GBA calls, endpoint, local peer, page exports | done |
| `extern/aurora/lib/dolphin/si/si.cpp`: `SIProbe` hook | done |
| `platform/gba_view.cpp`: files, Yaz0, records, debug picture | done (debug view only) |
| `platform/web/fsa_gba.js`: `?gba=`, keys for ports 2-4, panel | done |
| harness: `"pad"` on script steps, entry-screen globals; `tools/harness_scripts/mp_{2,3,4}_stage_1_0.json` | done |
| a native GBA picture as the client program draws it | **not done** (§5) |
| a network peer, rooms | **not done**: the decision of §4 comes first |

Runs (build of this branch, `tools/serve.py --diag logs/diag-mp`, own Chromium
on CDP 9363; run directories under `logs/diag-mp/` in the main checkout,
screenshots in `logs/multiplayer/`):

- **The link comes up**: every GBA boots, handshakes and connects
  (`field_0xd8 = 1`) within a few frames of the boot loader; 14 files per GBA
  arrive, are acknowledged and decompress to their stated sizes; records of
  kinds 5/6/7/8/9/13 flow every frame, 12/16/24/26 words long per GBA count
  (`fsaGba.dump()`). (verified)
- **The game takes 2, 3 and 4 players**: the player-count menu shows the GBAs
  (`menu-four-gbas.png`); `mp_N_stage_1_0.json` picks N players and enters
  stage 1-0 with `mPlayerMask` 0x33 / 0x77 / 0xFF (runs
  `20261125-213454-1r19idk1`, `-213904-t6fyt177`, `-212642-dzvoe6o6`); the HUD
  shows N players (`stage-1-0-two-players.png`, `stage-1-0-four-players.png`).
- **Each GBA drives its own player**: in the four-player run each pad's step
  moves only that player (state stream, run `20261125-211910-ow6x5zz9`): pad 0
  right x 40→127, pad 1 down y 488→544, pad 2 left (to the wall), pad 3 up
  y 472→423, then each back.
- **Deterministic**: the four-player script twice (`fast=1`): sync 324/324,
  state 319/319 identical over 16,719 reads, inputs identical
  (`-212642-dzvoe6o6` vs `-212754-fmuck9v6`).
- **No regression without GBAs**: `opening.json` on main c425eb8 and on this
  branch without `?gba`: sync 89/89, state 85/85 identical over 4,418 reads
  (the three new state globals stripped; `-213033-9es96hno` vs
  `-213358-n78k6jgk`).
- **The data is right**: the OBJ tiles the game sends render as the GBA's
  sprites (Link, hearts, rupees, the font: `gba-obj-tiles.png`).

Not verified: a sub-area view (no script yet walks a player into a house or
cave, so no kind-5 record with `mValid` set and no room files have been seen);
message records (10-12); unplugging and replugging mid-game (the code path
exists: `fsaGba.attach(p, 0)`, the game's own timeouts and reprobe); Tetra's
Trackers / Navi Trackers / Shadow Battle modes (`connect_screen_803F0A54.cpp`
counts GBAs the same way; untested).

Note: `fn_80300E1C` is a 72 % "behaviour drift" match in the decomp. The
protocol above works through it as built; differences from the original that
only show with a real GBA's timing (stalls, retries) would not show here.

---

## 4. Remote play: the options, mapped from katam-port

What Katam learned (`katamport/port/docs/NETPLAY.md`, `NETPLAY-HANDOFF.md`):
the seam under the game was right; rooms on Cloudflare (partyserver Durable
Objects, room codes) worked; v1 lockstep over the cable was sound but stalls;
v2 rollback with periodic world syncs desynced and rubber-banded beyond ~60 ms
one way and was shelved; its recommended direction was **host-authoritative
state streaming**, because consistency should be structural, not earned
through determinism.

FSA's own design points the same way: the GameCube is a host and every GBA a
thin client by construction.

### (a) Every browser runs the whole game; inputs are exchanged

Each browser loads the disc and runs the simulation; per frame the four GBA
key words (in practice the four `PADStatus`, the harness's input record) are
exchanged, and each browser draws its own player's GBA view (it has all the
state).

- For: the harness has shown the port deterministic (inputs indexed by pad
  read, port vs Dolphin identical over 25k reads; four GBAs here identical
  run to run, §3). Bandwidth is trivial (a few bytes a frame).
- Lockstep (wait for frame n's inputs from everyone): input delay of half an
  RTT plus jitter margin for everyone, and a stall whenever anyone is late.
  Katam's Path A shows it works; it feels bad beyond ~50-80 ms.
- Rollback: needs snapshot/restore of the whole game. Katam's was a memcpy
  of the GBA memory map with a stackless loop at the hook. FSA has the JKR
  heaps, globals, **several OS threads with suspended JSPI stacks** (game,
  audio, DVD, the four GBA clients), the DSP mixer and Aurora's state:
  restoring means restoring threads mid-call. Not available today and not
  small. And Katam's v2 shows the failure mode when the window is shorter
  than the lead.
- Every player needs the disc (1.4 GB; the README notes iOS kills a tab
  holding that much) and a desktop-class device to run it.
- Desync risk across browsers/GPUs/builds is unmeasured (same build, same
  machine only so far); the audio thread's wake-ups are not pinned (HARNESS.md).

### (b) Host-authoritative: the host is the GameCube, phones are the GBAs

One browser (the host) runs the game. Each remote player's phone is that
player's GBA: it sends its buttons up and receives exactly what a GBA would,
the records and files, and draws the GBA screen natively. Nothing else needs to
be built into the game: a network `FsaGbaPeer` on the host, the native GBA
renderer on the phone.

- Traffic per player: up 2 bytes of keys a frame (~120 B/s); down one record
  a frame (≤ 104 B, ~6 KB/s) plus the file bursts (~80 KB at boot, a few KB per
  room). The game designed this for a 1 Mbit/s cable shared by four.
- No determinism needed, no desync possible, no disc on the phones (only the
  host has it; the phone gets the GBA's files, as a real GBA did).
- Latency lands where the game expects it: `keys()` answers at once from the
  latest buffered keys, so the host never stalls on the network; a remote
  player's input arrives RTT/2 late. The link's own timeouts are between the
  game and the local endpoint, never on the network. File acknowledgements can
  take the RTT; the game waits up to 60 frames before resending.
- The TV picture is shared: players in the same room look at the host's
  screen (the original party setup, phones instead of GBAs, no video at all).
  Players elsewhere need the TV streamed: a WebRTC video track of the host
  canvas (`canvas.captureStream()`; 1-3 Mbit/s, ~100-200 ms), or later a
  "TV view" sent as state, which is a much bigger job.
- Transport: Katam's v1 room pieces carry over (room codes, a relay room per
  session on Cloudflare); the host is a room member that owns the game. A
  WebRTC data channel between host and phones is the low-latency variant, with
  the room only for signalling.

### Recommendation: (b), in two steps

1. **Same-room play first**: the host page on the TV or laptop, each player's
   phone joins with a room code and becomes their GBA (screen + touch pad).
   Needs: the native GBA renderer (needed by any option), a network peer, a
   phone page. No video, no determinism, no rollback.
2. **Remote players**: add the TV as a WebRTC video stream to the same session.

Why: it is how the game is built (the GameCube already treats GBAs as thin
clients over a slow link, §1); Katam's measured lesson is that structural
consistency beats earned determinism on real networks; phones cannot hold the
disc; and the work is mostly the GBA renderer, which (a) needs as well. (a)
stays possible later for remote play without video, as plain lockstep (the
harness's pad-read index is the frame number), if (b)'s video latency proves
unacceptable.

Verified vs assumed for this recommendation: the record and file traffic and
its sizes are measured (§3); that a GBA screen can be rebuilt from them
natively is **assumed** (the tiles, palettes and records arrive intact; the
client program's drawing rules are not yet known); the network latency
behaviour is read from the code (the game reads keys, it never waits for
them), not measured over a network.

---

### What was built (owner's choice: (a) with rollback)

Every browser runs the whole game and inputs travel, with rollback, lockstep as
a mode: docs/NETPLAY.md (design, protocol, how to run, measurements).

## 5. Open questions for the owner

1. **(a) or (b)?** §4 recommends (b), same-room first.
2. **How faithful must the GBA screen be?** The client program
   (`client_thread.bin`) is not decompiled. Options: (i) rebuild its drawing
   from the data (BG files + scroll, sprite words + the `*_sob` definitions)
   and check it against Dolphin's GBA screens; (ii) draw it with the game's own
   GameCube-side renderer of the same records (`drawPlayerPanel_802FB69C`,
   `fn_803022C4`), into a 240 × 160 target per player; (iii) decompile the
   client program (ARM; a separate project). (ii) is the quickest picture,
   (i) the most native.
3. **Player 1 on the TV device**: in a same-room session, should player 1 also
   be a phone, or the host's keyboard/pad with a GBA view on the TV corner?
   The game requires player 1 to be a GBA for multiplayer, which the endpoint
   satisfies either way.
4. **Single-player with a GBA** (`?gba=1`): the game then sends the GBA views
   for single-player too. Worth exposing (a phone as a second screen in
   single-player), or keep single-player on the pad only?
5. **What the GBA shows on the overworld** (record `mValid` 0): to be checked
   against Dolphin; the debug view shows a tile sheet there.
6. The four-pad harness extension (`"pad"`) and the entry-screen globals are
   generic: forward them to main?
