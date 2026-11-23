#!/usr/bin/env python3
"""Extract the binary blobs the sources #include as "assets/..." from your own main.dol.

These have no source form (fonts, a message file, DSP microcode). They are game data,
so they are generated into the build directory and never committed.

    python3 tools/extract_assets.py path/to/main.dol build/assets
"""
import os
import struct
import sys

# (header path, address, size): from the decomp's config/G4SE01/config.yml `extract:`
# and symbols.txt.
ASSETS = [
    ("assets/dolphin/gba/GBAKey_ucode.inc", 0x804985D0, 0x380),
    ("assets/game/tu_80400DC4/msg_data.inc", 0x80479CA0, 0x260),
    ("assets/game/tu_80400DC4/font_data.inc", 0x80479F00, 0x12260),
    ("assets/JSystem/JUtility/Ascfont_fix12.inc", 0x8045EEE0, 0x4160),
]


def read_dol(path):
    data = open(path, "rb").read()
    offs = struct.unpack(">18I", data[0x00:0x48])
    addrs = struct.unpack(">18I", data[0x48:0x90])
    sizes = struct.unpack(">18I", data[0x90:0xD8])
    return data, [(a, o, s) for o, a, s in zip(offs, addrs, sizes) if s]


def fetch(data, sections, addr, size):
    for a, o, s in sections:
        if a <= addr and addr + size <= a + s:
            return data[o + addr - a:o + addr - a + size]
    raise SystemExit(f"0x{addr:08X}+0x{size:X} is not in any DOL section (wrong main.dol?)")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data, sections = read_dol(sys.argv[1])
    for header, addr, size in ASSETS:
        blob = fetch(data, sections, addr, size)
        out = os.path.join(sys.argv[2], header)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w") as f:
            for i in range(0, len(blob), 16):
                f.write(", ".join(f"0x{b:02X}" for b in blob[i:i + 16]) + ",\n")
    print(f"wrote {len(ASSETS)} assets to {sys.argv[2]}")


if __name__ == "__main__":
    main()
