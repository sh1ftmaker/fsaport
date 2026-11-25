#!/bin/bash
# Builds logs/fntrace-answers/batch3 (main checkout) from the batch-3 runs:
#   tools/harness_scripts/batch3/answers.sh OUT_DIR NOTES.json name=RUN_DIR ...
# names: hitflash hitflash-dense layer8-1-2 force172c leaves players4 gba4 msgeval1 msgeval4
# Each run gets a subdirectory with every function's <name>.md/.jsonl; the top level
# gets each requested function's answer from the run(s) that exercise it.
set -e
cd "$(dirname "$0")/../../.."
OUT=$1; NOTES=$2; shift 2
declare -A RUN
for kv in "$@"; do RUN[${kv%%=*}]=${kv#*=}; done
T=tools/fntrace_summary.py
mkdir -p "$OUT"
for n in "${!RUN[@]}"; do
  rm -rf "${OUT:?}/$n"
  python3 $T "${RUN[$n]}" --build build/diag --notes "$NOTES" --split "$OUT/$n" --out "$OUT/$n/fntrace_summary.md" >/dev/null
done
top() {  # pattern runs...
  local pat=$1; shift
  local tmp; tmp=$(mktemp -d)
  python3 $T "$@" --build build/diag --notes "$NOTES" --split "$tmp" --out "$tmp/all.md" >/dev/null
  for f in "$tmp"/$pat.md; do
    [ -e "$f" ] || continue
    b=$(basename "$f" .md)
    cp "$tmp/$b.md" "$tmp/$b.jsonl" "$OUT/"
  done
  rm -rf "$tmp"
}
top 'drawSprite_801F06F0*' "${RUN[hitflash-dense]}" "${RUN[layer8-1-2]}"
top 'drawSobSprite_801EF574' "${RUN[layer8-1-2]}"
top 'fn_801F172C*' "${RUN[force172c]}"
top 'isBlockedOrOutOfView_0x50*' "${RUN[force172c]}"
top 'getTileCollisionCodeAlt_802265FC' "${RUN[force172c]}"
for p in 'update__22LeafEffectNPC*' 'LeafEffectNPC_804AE2E0__spawnLeaves__' 'tileCodeAt_*' 'fn_8023A51C*' 'fn_802373EC*' \
         'getPlayerPos_8023E724*' 'spawnEmitter_8013CC50*'; do top "$p" "${RUN[leaves]}"; done
top 'fn_80306A94*' "${RUN[players4]}" "${RUN[gba4]}"
top 'fn_80304FB0*' "${RUN[players4]}" "${RUN[gba4]}"
for p in 'fn_80383EB0' 'fn_80383E2C*' 'findMessageIndex_803846BC*' 'fn_803846A4*'; do top "$p" "${RUN[msgeval1]}" "${RUN[msgeval4]}"; done
ls "$OUT"
