#!/bin/bash
# Stage sweep for fntrace batch 3: every stage start under a harness script (state
# stream with every actor's layer, the loaded BMG's conditional messages) and,
# with REQ set, an fntrace request. Needs tools/serve.py --dir build/diag on PORT
# and a Chromium on CDP (docs/DIAGNOSTICS.md); prints "stage run-dir" lines.
#   tools/harness_scripts/batch3/sweep.sh [REQ.json]   (PORT=8842 CDP=9362 RUN_SECONDS=40 STAGES="1-0 ...")
cd "$(dirname "$0")/../../.."
PORT=${PORT:-8842}
CDP=${CDP:-9362}
SECONDS_=${RUN_SECONDS:-40}
STAGES=${STAGES:-"1-0 1-1 1-2 1-3 2-0 2-1 2-2 3-0 3-1 3-2 4-0 4-1 4-2 5-0 5-1 5-2 6-0 6-1 6-2 7-0 7-1 7-2 8-0 8-1 8-2 9-0 9-1 9-2 10-0 10-1 10-2 11-0 11-1 12-0"}
mkdir -p build/diag/harness_scripts
cp tools/harness_scripts/batch3/msgscan.json build/diag/harness_scripts/
for s in $STAGES; do
  if [ -n "$1" ]; then
    r=$(timeout 300 python3 tools/fntrace_run.py "$1" --stage "$s" --script tools/harness_scripts/batch3/msgscan.json \
        --keys '' --evals '' --seconds "$SECONDS_" --port "$PORT" --cdp "$CDP" --build build/diag --diag logs/diag \
        --extra fast=1 --name "sweep$(echo "$s" | tr - _)" 2>&1 | tail -1)
  else
    before=$(ls -t logs/diag | head -1)
    timeout 300 node tools/web_run.mjs --cdp "$CDP" --quiet --seconds "$SECONDS_" --every 100 \
      --url "http://127.0.0.1:$PORT/?iso=local.iso&nosave=1&fast=1&stage=$s&script=harness_scripts/msgscan.json" \
      --out "build/web-run/sweep-$s" --evals "window.fsaDiagFlush()@$((SECONDS_ - 2))" >/dev/null 2>&1
    sleep 3
    r=logs/diag/$(ls -t logs/diag | head -1)
    [ "$r" = "logs/diag/$before" ] && r="(no run)"
  fi
  echo "$s $r"
done
