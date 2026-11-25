# The GBA client program (`client_thread.bin`): a brief for its decompilation

Four Swords Adventures JoyBoots a small program into every linked Game Boy
Advance. The port does not run it: a native endpoint answers the link
(docs/MULTIPLAYER.md §2) and the GBA screen is rebuilt natively (approach A)
or drawn with the GameCube's own renderer (approach B; MULTIPLAYER.md, "GBA
screen"). This file says where the program is, what it is, and which of its
behaviours the native rebuild has to guess, so a decompilation knows where to
start. Nothing here decompiles it.

Conventions as in MULTIPLAYER.md: **verified** = seen in a run or in the bytes;
**read** = from the decomp source; **assumed** = neither.

## Where it is

| | |
|---|---|
| disc | `/GC4Sword_usa/Agb/client_thread.arc` (FST; 33,728 bytes at disc offset 0x4E009C), a RARC archive, **not** Yaz0, one directory `agb` with one file (verified: parsed from G4SE01.iso) |
| file | `client_thread.bin`, **33,436 bytes (0x829C)**, stored uncompressed in the archive; SHA-1 `05025ac26d3a3342163194be9b5e6f1a6533ff25` (verified) |
| loaded by | `BootLoader` mounts the archive (`boot_loader_803FF2C4.cpp:68`, `requestMountMemArchive_80401E10("/GC4Sword_usa/agb/client_thread.arc")`); `startGbaClients_803007D0` (`tu_803006E8.cpp:555-580`) takes `getResource('????', "client_thread.bin")` and its size, patches the header and hands it to `GBAJoyBoot` (read) |
| port pipeline | none of its own: the port reads it from the disc image like every other file (the DVD layer); it is not in `FSA_ASSETS` and not extracted. The native endpoint (`platform/gba_link.cpp`) receives the pointer in `GBAJoyBoot` and ignores the bytes (verified) |

## What it is

- **A GBA multiboot image** (verified, header bytes):
  - word 0 = `0xEA00002E`, an ARM `b` to 0x020000C0 (multiboot images run
    from EWRAM, 0x02000000);
  - title (0xA0-0xAB) empty, game code `g4sj` at 0xAC, maker `01`, the fixed
    byte 0x96 at 0xB2, header checksum 0x78 at 0xBD (valid for the stored
    header); the string `g4se01` also at 0x324 (the codes it accepts);
  - 0xC0 = `0xEA00001F`, the multiboot entry (ARM `b`); 0xC4/0xC5 (boot mode,
    slave id) 0; 0xC8-0xDF zero, ARM code from 0xE0 (a `ldr`/`strh` sequence:
    the crt0).
  - The game patches it before booting: the disc's game code at 0xAC-0xAF, the
    checksum at 0xBD, and the boot tick (`OSGetTick`) at 0xC8, which the
    handshake later echoes (MULTIPLAYER.md §1). In the port the tick is
    written in host (little-endian) order.
- **Code**: from the decomp session's survey: about 57 Thumb functions with
  `push {..., lr}` (we count 57 `0xB5xx` halfwords too), probably 60-100 with
  leaf functions, no ARM functions apart from the crt0. A good part of the
  33 KB is data (tiles, palettes, tables).
- **Memory**: the program occupies 0x02000000-0x0200829C. Everything else it
  draws with is sent by the game as files after boot (verified, all arrive
  and decompress; MULTIPLAYER.md §1):

| address | file | what it holds (verified from the bytes and the GameCube's use of the same data) |
|---|---|---|
| 0x02010000 | player_sob (35,284 B) | "SOB " sprite frames: u16 count, u16 frame offsets, 6-byte GBA OAM entries (attr0-2) |
| 0x02018E00 | enemy_both2_sob (30,830 B) | the same, sprite kinds 0-1 |
| 0x02021000 | enemy_both_sob (15,586 B) | the same, kind 2 |
| 0x02033000 | sound_data (1,924 B) | not examined |
| 0x02034000 | player_sch (16 KiB) | 4bpp OBJ characters, the player sheet (OAM tiles 512-1023 with priority bits 2) |
| 0x02038000 | moji_english_sch (16 KiB) | 4bpp font characters (layout not known) |
| 0x0203C000 | bg_a_spl (8 KiB) | BG metatiles: 4 GBA BG map entries (char, flips, palette) per 16 x 16 metatile id |
| 0x05000000 | stage BG palette (512 B, sent to 0x05000001) | 16 BG palettes, BGR555 little-endian |
| 0x05000200 | gb_cl_obj_base_scl (512 B, sent to 0x05000201) | 16 OBJ palettes (12-15 the players') |
| 0x06000000 | stage BG characters (24 KiB) | 4bpp |
| 0x0600C000 / 0x0600E000 | a room's two tile grids (2 KiB each), per room entered | u16 metatile ids, 2 x 2 blocks of 16 x 16 (512 x 512 pixels): the GameCube's TileMap grid layout, **not** GBA screen blocks |
| 0x06010000 | gb_ch_obj_base_sch (28 KiB) | 4bpp OBJ characters, 2D mapping (32 per row) |
| 0x06017000 | gb_ch_obj_tr_sch* (4 KiB, some bosses) | more OBJ characters |

## The protocol it implements (what the endpoint already does)

The handshake, the session packet, the per-frame record and key word, the
file and message acknowledgements: MULTIPLAYER.md §1. The endpoint
(`platform/gba_link.cpp`) is a working model of the client's link side.

One port-only detail for anyone who runs the real program against the port
(e.g. in mGBA): the records are built with the game's bitfields, and the port
is compiled by clang, which lays bitfields out LSB-first where MWCC laid them
MSB-first. So the record bytes the port sends differ from a real GameCube's
(e.g. the record kind is the low nibble of byte 3 in the port, the high one on
hardware). A bridge to a real client must re-pack them.

## What the native rebuild (approach A) guesses

`platform/gba_view.cpp` draws each GBA's screen from the files and records
above. Most rules come from the GameCube's own drawing of the same records
(`TileMap_804AA858::drawPlayerPanel_802FB69C`, `fn_802FAC10`, `fn_8030275C`,
`fn_803022C4`, `fn_80302C34`, `fn_80302F9C`, `drawSobSprite_802F86CC`, and
`fn_803068D0`, the GameCube's copy of the client's sprite table), and the
comparison with approach B shows them right structurally (MULTIPLAYER.md, "GBA
screen"). Where the GameCube's drawing and the client's could differ, or the
GameCube draws nothing, the rebuild guesses. In order of how much they show:

1. **The overworld screen.** While the player is on the overworld (record
   kind 5 with `mValid` 0; marks hidden, no sprite words) the GameCube draws
   no panel. What the client shows then (a status screen? the map? "look at
   the TV"?) is not known; the rebuild shows the backdrop. Word 1 of the
   record cycles through life, rupees / force gems and timers by phase
   (`fillRecordHeader_803085A4`): presumably a HUD the client draws.
2. **Messages** (records 10-12, ack 3). The characters arrive (16-bit codes,
   byte-swapped; `cls_804AA8B8::fn_80307B58`); the window, the font layout in
   moji_english_sch (code -> character cell), line breaks, paging and when it
   acknowledges are not known. The rebuild's message box (8 x 16 cells, code
   - 0x20) is a placeholder; no run of ours has produced a message record yet
   (they are sent while `getMessageWindowState_8037F1E4(player) == 3`).
3. **The sprite table's lifetime.** Each regular record replaces one half of a
   38-word table (record phase bit, `fn_803068D0`); the rebuild assumes the
   client keeps the table the same way (no word carries an id).
4. **Which OBJ characters a frame uses.** On the GameCube the OAM priority
   bits of a SOB entry select one of four sheets (0 = gb_ch_obj_base, 2 =
   player_sch from tile 512, 3 = `gals_sch`, which the GBA is never sent). OBJ
   VRAM (32 KiB) is full with the base and boss sheets, so the client must
   copy player frames into VRAM itself, and sheet 3 must come from its own
   data (or be drawn otherwise). The rebuild reads player tiles straight from
   EWRAM and draws sheet-3 entries as grey outlines.
5. **Layer order and priorities.** The rebuild draws grid 0x0600E000 (the
   GameCube's layer `mValid - 1`), then words and marks of key < 2, then grid
   0x0600C000 when the overlay flag (mark 3's scroll bit) is set
   (`mValid + 7`), then keys >= 2; keys from `fn_803074A4` and the players'
   states in word 1. How the client maps this onto BG / OBJ priorities, and
   how it fills GBA screen blocks from a 512 x 512 metatile grid with the
   9-bit scroll, is its own business.
6. **Sprite effects**: palette 7 -> 12 + player for kinds 4-7 and the marks,
   all palettes -> 7 for "set" marks and spinning words, alpha 0xA0 (the
   rebuild blends 5/8), 2x scale, the vertical mirror of kind 2 ids >= 0x200,
   and the spin (`field_0x3_3`; the GameCube turns it by
   `lbl_80506D78 += 0.7854` a frame, the rebuild by its own count). How the
   GBA does these (affine OBJ, blend registers) is not known.
7. **Screen effects the GameCube applies to the panel**: line shifts
   (`cls_804AA8D8::fn_8030B1E8`, a wave per line), the half-grey / black tints
   (`field_0x130`), the pause darkening, the mosaic when a player joins
   (`count`), the animated BG characters (`mAnimTileData_0x238c`, chars 16-17
   every 10 frames), and BG character rewrites (`bg_tenso_sch` by shutters,
   switches, spike floors). The rebuild does none of them.
8. **Palettes.** The GBA is sent its own palettes (gb_cl_obj_base_scl and the
   stage's GBA palette), brighter than the GameCube's (`cu_cl_obj_base_scl`,
   `adjustChannel`): the rebuild uses the GBA's, so its colours differ from
   approach B's by design.
9. **The key word's bit 15** (copied to `field_0xe6`), sound (`sound_data`),
   and anything the client does with the boot tick and session packet beyond
   echoing them.

## A suggested start for the decomp

The decomp session's plan: a functional (not byte-matched) C decompilation
first, focused on the frame / record protocol and the screen drawing,
verified against headless mGBA fed the same records. The port can supply
those: `fsaGba.dump()` and `fsaGba.memory(port, region, len)` give every
port's latest record and its GBA memory image (EWRAM, palette RAM, VRAM) at
any frame, and `tools/harness_scripts/mp_2_stage_1_2_rooms.json` puts one
player in a cave and the other in a house (stage 1-2) deterministically. With
the bit-order note above, the same files and records can drive the real
client in mGBA, and its screens can be compared with
`platform/gba_view.cpp`'s (approach A) and the GameCube's (approach B).
Items 1, 2 and 4 above are the ones the native rebuild cannot settle without
the client.
