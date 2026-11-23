#!/bin/sh
# Build and run the archive test under node (wasm32: the port's target).
#   tools/test_archive/run.sh path/to/G4SE01.iso
# Needs emscripten on PATH.
set -e
here=$(cd "$(dirname "$0")" && pwd)
game=$here/../../game
out=$here/../../build/test_archive
mkdir -p "$out"
em++ -O1 -std=gnu++17 -nostdinc -fno-rtti -fno-exceptions -fsigned-char -Wno-everything -DFSA_PORT=1 -DNDEBUG=1 \
  -include "$game/port/port_prefix.h" \
  -I"$game/libs/sdk/include" -I"$game/libs/sdk/msl_include" -I"$game/include" -I"$here" \
  -c "$game/libs/jsystem/JSystem/JKernel/JKRDecomp.cpp" -o "$out/JKRDecomp.o"
for f in test_formats test_extra test_archive; do
  em++ -O1 -std=gnu++17 -nostdinc -fno-rtti -fno-exceptions -fsigned-char -Wno-everything -DFSA_PORT=1 -DNDEBUG=1 \
    -include "$game/port/port_prefix.h" \
    -I"$game/libs/sdk/include" -I"$game/libs/sdk/msl_include" -I"$game/include" -I"$here" \
    -c "$here/$f.cpp" -o "$out/$f.o"
done
emcc -O1 -c "$here/io.c" -o "$out/io.o"
# Only JKRDecomp's decoders are called; its thread/OS references stay unresolved.
em++ -O1 "$out/test_archive.o" "$out/test_formats.o" "$out/test_extra.o" "$out/JKRDecomp.o" "$out/io.o" -o "$out/test_archive.js" \
  -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sERROR_ON_UNDEFINED_SYMBOLS=0 -sENVIRONMENT=node -Wno-undefined \
  -Wl,--allow-multiple-definition # dolphin/os.h defines __OSTVMode/__OSBusClock/__OSCoreClock in every unit
node "$out/test_archive.js" "$1"
