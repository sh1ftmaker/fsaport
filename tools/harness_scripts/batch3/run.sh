#!/bin/bash
# fntrace batch 3: the forced-branch runs (logs/fntrace-answers/batch3/README.md).
# Needs tools/serve.py --dir build/diag (an FSA_DIAG build) on PORT and a Chromium on CDP.
#   tools/harness_scripts/batch3/run.sh [NAME ...]    (default: all; PORT=8842 CDP=9362)
# Prints "name run-dir" lines.
cd "$(dirname "$0")/../../.."
PORT=${PORT:-8842}
CDP=${CDP:-9362}
REQ=${REQ:-tools/harness_scripts/batch3/req_batch3.json}  # req_batch3_dense.json: drawSprite logged densely
# EXTRA= (empty) runs in real time: dense logging in fast mode overflows the 8 MB diag POSTs
python3 tools/harness_scripts/batch3/gen.py
declare -A STAGE=([hitflash_1_0]=1-0 [layer8_1_2]=1-2 [force172c_1_0]=1-0 [leaves_1_0]=1-0 [players4_1_0]=1-0
                  [gba4_1_0]=1-0 [msgeval1_1_0]=1-0 [msgeval4_1_0]=1-0)
declare -A SECS=([hitflash_1_0]=150 [layer8_1_2]=90 [force172c_1_0]=240 [leaves_1_0]=90 [players4_1_0]=150
                 [gba4_1_0]=150 [msgeval1_1_0]=60 [msgeval4_1_0]=60)
NAMES=${*:-"hitflash_1_0 layer8_1_2 force172c_1_0 leaves_1_0 players4_1_0 gba4_1_0 msgeval1_1_0 msgeval4_1_0"}
for n in $NAMES; do
  r=$(timeout $((SECS[$n] + 200)) python3 tools/fntrace_run.py "$REQ" \
      --stage "${STAGE[$n]}" --script "tools/harness_scripts/batch3/$n.json" --keys '' --evals '' --seconds "${SECS[$n]}" \
      --port "$PORT" --cdp "$CDP" --build build/diag --diag logs/diag --extra "${EXTRA-fast=1}" --name "b3_$n" 2>&1 | tail -1)
  echo "$n $r"
done
