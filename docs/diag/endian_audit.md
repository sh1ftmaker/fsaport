# Endian audit: raw reads of big-endian data (diagnostic #5)

Where the port reads multi-byte values from big-endian file data without a
byte-order helper (`port/be.h`: `BE<T>`, `be16`/`be32`/`be32s`/`bef32`), and the
related host-endian views (bit fields, unions over integers). Each entry is a
field the decomp should type as big-endian-aware or read through a helper.

How the list was made:

- **static**: `python3 tools/endian_audit.py` (raw pointer casts, structs cast
  from resource pointers, bit fields, integer unions, outside `#ifndef FSA_PORT`
  branches) over `game/src/game`, `game/include/game`, `game/libs/jsystem` and
  `game/libs/sdk/include/JSystem`, then each hit read and triaged by hand, plus a
  read of every consumer of the game's own resource types ('SOB ', 'SCH ',
  'SCL ', 'SMP ', 'SPL ', 'ENM ', BMG, BTI, THP).
- **runtime**: the endian trap (`-DFSA_ENDIAN_DIAG=ON`, docs/DIAGNOSTICS.md,
  "Endian trap") over the title, the menus, the opening narration, the Hyrule
  Castle opening up to Zelda's first lines and `?stage=1` (fairy dialogue, ~100 s of walking), all game, JSystem
  and SDK units instrumented.

Main results:

- **The game's own code (`game/src/game`) made no raw multi-byte load from file
  data in any of those scenes.** Its GBA-derived formats ('SOB ', 'SCH ',
  'SCL ', 'SMP ', 'SCN ' tile grids, 'SPL ') are *little-endian* and every
  consumer reads them byte by byte (`data[i] | data[i + 1] << 8`), which is
  portable. The EnemyParam / other `.csv` resources are text.
- The remaining game-code candidates are in the message system (BMG control-code
  operands) and in paths the scenes above did not reach.
- One JSystem field was missed by the JPA port and is hit every frame particles
  draw: fixed (38ec291).

## Ranked list

Likely-real first. "evidence" is static unless the trap saw it.

| # | Where (file:line) | Function (CW symbol) | Format / field | Evidence | Why |
|---|---|---|---|---|---|
| 1 | game/src/game/message_manager_8037DF68.cpp:48 `operand()`; used at :283, :350, :428, :1101-1272 | `fn_80383EB0` (0x80383EB0), `openMessageById_8038C678`, and the two other openers | BMG DAT1 control code 0x1A (group 2): s32 next-message operands at +5/+6/+9/+10/+13/+17 | static (not reached by the scenes) | `*(const s32*)(p + offset)` reads the big-endian, unaligned operands raw. fn_80383EB0 follows every conditional message chain (held key, random pick, player count, slot, stage flag, view bit) and uses the operand as the next INF1 index: on the port it becomes e.g. 0x2A000000, an out-of-range entry. Any NPC whose text starts with a branch picks garbage. Same class as c960d38 (which fixed the tag handlers but not this helper). Fix: `be32s(p + offset)` (patch below). |
| 2 | game/src/game/message_manager_8037DF68.cpp:1283 | `fn_80383EB0` | BMG control 0x23: u16 bit index at +6 | static | `*(const u16*)(p + 6)` raw. Fix: `be16(p + 6)`. |
| 3 | game/src/game/tu_803944C8.cpp:868-871, 876-878, 891, 1015-1016, 1037-1038, 1066-1067, 1450-1453, 1458-1460, 1473, 1597-1598, 1619-1620, 1648-1649 | `MessageSequenceProcessor_804AF774::do_tag` (`do_tag__33MessageSequenceProcessor_804AF774FUlPCvUl`, 0x8039FE78) and the second tag handler below it | BMG tag arguments of 0x20018, 0x2001D, 0x20020 and the 0x2002x branch tags: s32 message indices | static | `((const s32*)data)[i]` reads the tag's arguments raw; the values are message indices passed to `fn_803A30DC` (jump). c960d38 turned the single reads into `TAG_S32` but missed the indexed ones. Fix: `TAG_S32((const s32*)data + i)`. |
| 4 | game/libs/sdk/include/JSystem/JParticle/JPAShape.h:120-129 | `JPABaseShape::getInit*/getInc*` via `JPAGenCalcTexCrdMtxAnm` / `JPALoadCalcTexCrdMtxAnm` (0x800E7290) | jpc BSP1: the 10 f32 of the texture-coordinate animation table | **runtime**: 16611 swapped-looking, 0 plain of 55370 loads in stage 1 (e.g. 0x0AD7A33D, BE 0x3DA3D70A = 0.08) | Read as `((f32 *)mTexCrdMtxAnmTbl)[i]`; an initial UV scale of 1.0 became 4.6e-41. **Fixed in 38ec291** (`BE<f32>`). |
| 5 | game/src/game/act_lift.cpp:98-99 | `Lift_804A6F6C::fn_802565BC` (`fn_802565BC__13Lift_804A6F6CFl`) | BTI (ResTIMG) palette, RGB5A3 u16 entries | static | Per-pixel hit test of the lift's texture reads `palette[i]` raw and tests `color & 0x8000` / `(color & 0xF000) == 0x7000`: the alpha bits come from the wrong byte, so the opaque/transparent decision is wrong. Fix: `be16(palette + i)`. |
| 6 | game/src/game/act_light_environment_control_802EF76C.cpp:38-41 (union in game/include/game/SpriteParams.h:14) | `LightEnvironmentControl_804AA4F0::init` (`init__32LightEnvironmentControl_804AA4F0Fv`) | not file data: `GXColor` viewed through `s32 mWord` | static | `mWord >> 24` is red on the GameCube, alpha on the port: `mColorCur` becomes (a, b, g, r) and `applyColor` hands it to `TColor::set(u32)` as (r, g, b, a), so the shadow colour shows with red = alpha and blue/red swapped until a fade (`vfunc_0x54`) replaces channels 0-2, and the fade's steps start from that colour. act_environment_switch_control does the same split portably with `toUInt32()`. Fix: read `mColor.mColor.r/g/b/a`. |
| 7 | game/include/game/Map.h:96-200 (`cls_804AA858Sub11C`, `GbaSprite_80506F88`, `cls_804AA858Sub11CEntry`), game/src/game/tu_80301CD0.cpp:466 `swapBytes`, :512, :827-838, :1205, :1237, :1335 | `cls_804AA8B8::fn_80307B58` (0x80307B58), `fn_80306A94` (0x80306A94), `fn_80304FB0`; read in `TileMap_804AA858::drawPlayerPanel_802FB69C`, `fn_802FCE74`, `fn_803022C4`, `fn_803068D0` | not file data: the 0x68-byte GBA packet and its sprite words | static | u8/u32 bit fields laid over packet bytes (MWCC allocates from the high bit, clang from the low bit) and `mWord = swapBytes(w)` written as a host word then read back as bytes/fields: both orders flip on the port, so marks, scroll and sprite ids decode scrambled. Only matters if the GBA-screen data path is kept (docs/PORTING.md: GBA features stay, hardware does not). |
| 8 | game/libs/jsystem/JSystem/JMessage/processor.cpp:51 | `JMessage::TProcessor::setBegin_messageCode(u16, u16)` (0x80006DA4) | BMG INF1 entry: s32 text offset | static | `*(int*)entry` raw; `TResource::getMessageText_messageEntry` already uses `be32s`. No caller in FSA today (only `setBegin_messageID`, itself unreferenced). |
| 9 | game/libs/jsystem/JSystem/JGadget/binary.cpp:48-64 | `parseVariableUInt_16_32_following` (0x80027128) | JGadget binary blocks: u16/u32 lengths | static | Raw `*(u16*)` / `((const u32*)p)[1]`. No caller in FSA today. |
| 10 | game/src/game/tu_803006E8.cpp:324, 343 | GBA client state machine (`tu_803006E8`) | not file data: `*(const u32*)"g4se"` | static | The game code as a host u32 for the joybus initial packet; byte order flips on the port. GBA hardware only. |
| 11 | game/src/game/tu_803006E8.cpp:29 `GbaProgram_8053ACBC::field_0xc8` | `startGbaClients_803007D0` | client_thread.bin (GBA program) + OSTick | static | A host-order tick is stored into the GBA program image sent to the GBA. GBA hardware only. |
| 12 | game/libs/jsystem/JSystem/JKernel/JKRHeap.cpp:76 | `JKRHeap::initArena` | OS boot info (+0x28 memory size), not a file | static | Raw u32 of the low-memory boot block; correct only if the platform writes it host-order. |

### Checked and not a problem

- `SobObj_802F86CC` (tile_map_802F5040.cpp:90): already declared in clang's bit
  order under FSA_PORT (32138bc). `SpriteObj_802F86CC` is a host-only struct
  filled field by field.
- tile_map_802F5040.cpp loaders (`loadTileImages_802FA160`,
  `loadSpriteImage_802F9714`, `loadSpritePalette_802F93F4`, `fn_802F9DA8`,
  `fn_802F99DC`, `fn_802F9BE8`, `drawSobSprite_802F86CC`,
  `drawBlockMap_802F6CF4`): little-endian GBA data read byte by byte; the
  palettes are written big-endian byte by byte for GX.
- thp_read_thread_80427D2C.cpp:494, 680: the offsets go through `THP_RD32`.
- tu_803944C8.cpp:3125: takes an address, reads nothing.
- tile_map_802F5040.cpp:1327 (`GXPeekARGB`), JKRUnitHeap bitmap, J3DGD
  register writes, JASSeqParser `cmdParamE` (parsed host arguments),
  J3DModelLoader.cpp:118 / J3DAnmLoader.cpp:115-120 (the port's own swap-at-load
  code), JPAResourceLoader `GetTypeFromByteStream` (= `portReadBE`).
- Message structs `MessageInf1_804AF070` / `MessageFile_804AF070`, `ResTIMG`,
  `ResFONT`: already `BE<>`.

### Runtime noise (what the trap reports that is not a bug)

The trap flags every raw 2/4-byte load from file bytes; these are expected:

- Data swapped once at load (RARC info blocks, J3D/J2D animation and model
  tables): host-order values, mostly plain-looking or neutral; their swap loops
  read raw by design (`portSwap*`).
- Byte data moved as words: `memcpy`/`JASCalc::bcopy`, `GXColor` copies
  (`getPrmClr`, `makeColorTable`, `newTevKColor`).
- `BE<T>` objects copied by value (`JPABEVec3f` into `TVec3f::set`, J3D
  `operator=`): the copy is raw, the later conversion is right.

## Suggested fix for 1-2 (not applied: the scenes reached do not branch)

```cpp
#ifdef FSA_PORT
#include "port/be.h"
static inline s32 operand(const char* p, s32 offset) { return be32s(p + offset); }
#define OPERAND_U16(p) ((u16)be16(p))
#else
static inline s32 operand(const char* p, s32 offset) { return *(const s32*)(p + offset); }
#define OPERAND_U16(p) (*(const u16*)(p))
#endif
...
            s32 bit = OPERAND_U16(p + 6);
```
