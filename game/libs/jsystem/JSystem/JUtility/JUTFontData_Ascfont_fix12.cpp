#include "types.h"

// JUtility's built-in ASCII font (FONTbfn1, 12-pixel fixed width; mkdd's
// JSystem/JUtility/FontData/Ascfont_fix12.bfn), used by JFWSystem as the
// system font. A byte-exact asset file (config.yml extract:, blobs.txt).
// JUTFont.h declares it as a ResFONT; tp and tww define it as a
// 32-byte-aligned u8 array of 0x4160 bytes. In the port it is game data the
// build does not carry: tools/gen_globals.py (ASSETS) defines the storage and
// platform/dol_data.cpp copies the bytes from the player's main.dol at startup.
extern const u8 JUTResFONT_Ascfont_fix12[];
