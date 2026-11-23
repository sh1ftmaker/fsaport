#!/bin/sh
# Map wasm code offsets from a Chrome stack (wasm-function[N]:0xOFFSET) to source lines.
#   tools/wasm_where.sh 0x3cb690 [0x...]
B=/bigdata/agent-tom/projects/fsadecomp/tools/emsdk/upstream/bin
cd "$(dirname "$0")/../build/web"
for a in "$@"; do $B/llvm-symbolizer --obj=fsa.wasm --inlining "$a" | sed 's|/bigdata/agent-tom/projects/fsaport/||'; done
