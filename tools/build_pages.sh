#!/bin/bash
# Build the port into a static site for GitHub Pages (or any static host).
#
#   tools/build_pages.sh SITE_DIR [BUILD_DIR]       (BUILD_DIR default: build/pages)
#   FSA_CHECK_DOL=path/to/main.dol tools/build_pages.sh ...   (also prove no game data)
#
# Needs emscripten on PATH (source ../fsadecomp/tools/emsdk/emsdk_env.sh), or
# EMSDK set. SITE_DIR gets only what the page loads: index.html (marked
# fsa-hosting=static: no log/diagnostics POSTs, online rooms only with
# ?relay=URL), fsa.js, fsa.wasm, fsa.netregions.json, the page scripts, a
# .nojekyll, a README.md and THIRD_PARTY_LICENSES.txt. No linker map, no
# harness scripts, no disc manifest, nothing derived from game data: the game
# reads main.dol and everything else from the disc image the player chooses.
# Relative paths only, so the site works under https://USER.github.io/REPO/.
# Publish it with tools/publish_pages.sh SITE_DIR.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SITE=${1:?usage: tools/build_pages.sh SITE_DIR [BUILD_DIR]}
BUILD=${2:-$ROOT/build/pages}

if ! command -v emcmake >/dev/null; then
  ENV="${EMSDK:-$ROOT/../fsadecomp/tools/emsdk}/emsdk_env.sh"
  [ -f "$ENV" ] || { echo "emscripten not found: source emsdk_env.sh or set EMSDK" >&2; exit 1; }
  # shellcheck disable=SC1090
  source "$ENV" >/dev/null 2>&1
fi

# A plain release build: every diagnostics option off.
emcmake cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFSA_DIAG=OFF -DFSA_DIAG_UNINIT=OFF -DFSA_COVERAGE=OFF -DFSA_ENDIAN_DIAG=OFF \
  -DFSA_SANITIZE= -DFSA_TRACE=OFF -DFSA_DEBUG_LINES=OFF -DFSA_NO_AUDIO=OFF >/dev/null
ninja -C "$BUILD" fsa

rm -rf "$SITE"
mkdir -p "$SITE"
for f in fsa.js fsa.wasm fsa.netregions.json; do cp "$BUILD/$f" "$SITE/"; done
for f in touch.js view.js gputrace.js disc_store.js; do cp "$ROOT/platform/web/$f" "$SITE/"; done
sed 's|<meta name="fsa-hosting" content="server">|<meta name="fsa-hosting" content="static">|' \
  "$ROOT/platform/web/index.html" > "$SITE/index.html"
grep -q '<meta name="fsa-hosting" content="static">' "$SITE/index.html" || { echo "index.html: no fsa-hosting meta" >&2; exit 1; }
touch "$SITE/.nojekyll"

# Every page asset is referenced relatively (the site lives under /REPO/).
if grep -nE '(src|href)="/[^/]|fetch\(.?/[a-z]' "$SITE/index.html"; then
  echo "index.html: absolute path (the site is served under a sub-path)" >&2; exit 1
fi

cat > "$SITE/README.md" <<'EOF'
# Four Swords Adventures for the web: the site

This branch is the built site of the port (`tools/build_pages.sh`), served by
GitHub Pages. It contains no game data: open the page, choose **your own**
*The Legend of Zelda: Four Swords Adventures* (USA, G4SE01) disc image, and it
is read in the browser only (kept in the browser's private storage so a reload
does not ask again; "Forget disc" removes it). Third-party licences:
THIRD_PARTY_LICENSES.txt.
EOF

# Licences of the third-party code compiled into fsa.wasm / fsa.js.
EM=$(dirname "$(command -v emcc)")
DEPS="$BUILD/_deps"
{
  echo "Third-party software in fsa.wasm and fsa.js"
  echo "==========================================="
  echo
  echo "The game and SDK sources (game/) are the fsa-decomp decompilation (CC0 1.0); the"
  echo "DSP mixer (platform/audio_dsp.cpp) and some JSystem bodies come from Dusklight (CC0 1.0)."
  echo "Aurora's card/SRAM.hpp is modified libogc code (zlib licence, below in it)."
  echo "SQLite is in the public domain. dlmalloc is public domain (CC0)."
  lic() {
    [ -e "$2" ] || { echo "missing licence file for $1: $2" >&2; exit 1; }
    printf '\n\n---------------------------------------------------------------------------\n%s\n---------------------------------------------------------------------------\n\n' "$1"
    cat "$2"
  }
  lic "Aurora (extern/aurora)" "$ROOT/extern/aurora/LICENSE"
  lic "magic_enum (extern/aurora/include/magic_enum.hpp, MIT)" <(sed -n '1,30p' "$ROOT/extern/aurora/include/magic_enum.hpp")
  lic "SDL 3" "$DEPS/sdl-src/LICENSE.txt"
  lic "FreeType (used under the FreeType License). Portions of this software are copyright (c) The FreeType Project (www.freetype.org). All rights reserved." "$DEPS/freetype-src/docs/FTL.TXT"
  lic "Dear ImGui" "$DEPS/imgui-src/LICENSE.txt"
  lic "{fmt}" "$DEPS/fmt-src/LICENSE"
  lic "Abseil" "$DEPS/abseil-cpp-src/LICENSE"
  lic "xxHash" "$DEPS/xxhash-src/LICENSE"
  lic "Dawn / emdawnwebgpu (WebGPU bindings)" "$EM/cache/ports/emdawnwebgpu/emdawnwebgpu_pkg/webgpu/src/LICENSE"
  lic "Emscripten (runtime, JS library)" "$EM/LICENSE"
  lic "musl libc" "$EM/system/lib/libc/musl/COPYRIGHT"
  lic "libc++" "$EM/system/lib/libcxx/LICENSE.TXT"
  lic "libc++abi" "$EM/system/lib/libcxxabi/LICENSE.TXT"
  lic "compiler-rt" "$EM/system/lib/compiler-rt/LICENSE.TXT"
} > "$SITE/THIRD_PARTY_LICENSES.txt"

# Nothing but what the page needs.
extra=$(cd "$SITE" && find . -type f ! -name index.html ! -name fsa.js ! -name fsa.wasm ! -name fsa.netregions.json \
  ! -name touch.js ! -name view.js ! -name gputrace.js ! -name disc_store.js ! -name .nojekyll ! -name README.md \
  ! -name THIRD_PARTY_LICENSES.txt)
[ -z "$extra" ] || { echo "unexpected files in the site: $extra" >&2; exit 1; }

# No paths of the machine that built it (CMakeLists.txt maps __FILE__).
if grep -a -l -F "$ROOT" "$SITE"/* "$SITE"/.nojekyll; then
  echo "the site contains the build machine's path $ROOT (above)" >&2; exit 1
fi

if [ -n "${FSA_CHECK_DOL:-}" ]; then
  # fsa.wasm is checked in the build dir, next to its fsa.map (the hits'
  # attribution), after checking the site's copy is the same file.
  cmp -s "$BUILD/fsa.wasm" "$SITE/fsa.wasm"
  python3 "$ROOT/tools/check_no_game_data.py" "$FSA_CHECK_DOL" "$BUILD/fsa.wasm" "$SITE/fsa.js" \
    "$SITE/index.html" "$SITE/touch.js" "$SITE/view.js" "$SITE/gputrace.js" "$SITE/disc_store.js" "$SITE/fsa.netregions.json"
fi
echo "site: $SITE"
ls -la "$SITE"
