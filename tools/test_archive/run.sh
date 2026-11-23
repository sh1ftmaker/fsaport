#!/bin/sh
# Build and run the archive test under node (wasm32: the port's target).
#   tools/test_archive/run.sh path/to/G4SE01.iso
# Needs emscripten on PATH.
set -e
here=$(cd "$(dirname "$0")" && pwd)
game=$here/../../game
out=$here/../../build/test_archive
mkdir -p "$out"
cxx() {
  em++ -O1 -std=gnu++17 -nostdinc -fno-rtti -fno-exceptions -fsigned-char -Wno-everything -DFSA_PORT=1 -DNDEBUG=1 \
    -include "$game/port/port_prefix.h" \
    -I"$game/libs/sdk/include" -I"$game/libs/sdk/msl_include" -I"$game/include" -I"$here" \
    -c "$1" -o "$2"
}
# The port's own code under test: JKRDecomp's decoders, the J3D file swap,
# the animation file swap.
cxx "$game/libs/jsystem/JSystem/JKernel/JKRDecomp.cpp" "$out/JKRDecomp.o"
cxx "$game/libs/jsystem/JSystem/J3DGraphLoader/J3DModelLoader.cpp" "$out/J3DModelLoader.o"
cxx "$game/libs/jsystem/JSystem/J3DGraphLoader/J3DAnmLoader.cpp" "$out/J3DAnmLoader.o"
for f in test_formats test_extra test_archive; do
  cxx "$here/$f.cpp" "$out/$f.o"
done
emcc -O1 -c "$here/io.c" -o "$out/io.o"
# Only the functions above are called; their other references stay unresolved.
em++ -O1 "$out/test_archive.o" "$out/test_formats.o" "$out/test_extra.o" "$out/JKRDecomp.o" \
  "$out/J3DModelLoader.o" "$out/J3DAnmLoader.o" "$out/io.o" -o "$out/test_archive.js" \
  -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sERROR_ON_UNDEFINED_SYMBOLS=0 -sENVIRONMENT=node -Wno-undefined \
  -Wl,--allow-multiple-definition # dolphin/os.h defines __OSTVMode/__OSBusClock/__OSCoreClock in every unit
node "$out/test_archive.js" "$1"
