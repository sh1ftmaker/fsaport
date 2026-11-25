#!/bin/bash
# Netplay end-to-end run (docs/NETPLAY.md, "Measuring"): N headless browsers
# (tabs on one or two Chromiums) join one room on the local relay and play a
# harness script with its pads split across them; every browser writes its own
# diagnostics run directory, which tools/net_compare.py then compares.
#
#   tools/net_run.sh NAME SECONDS SEATS 'room options' 'per-browser options' CDP1 [CDP2]
#   e.g. tools/net_run.sh lat50 150 2 'script=harness_scripts/mp_2_stage_1_0.json&net=rollback' \
#          'lat=50&jitter=10' 9368 9378
#
# Needs: tools/serve.py --diag on $NET_WEB (default 8848) serving build/web with
# the disc as local.iso, the relay (netplay/relay.mjs) on 8858, and the
# harness scripts copied to build/web/harness_scripts/. Writes the console logs
# to $NET_OUT/NAME/seatN/ (default the scratch dir given in $NET_OUT).
set -u
NAME=$1; SECS=$2; SEATS=$3; ROOMQ=$4; PEERQ=$5; CDP1=$6; CDP2=${7:-$6}
WEB=${NET_WEB:-8848}
OUT=${NET_OUT:-build/net-run}/$NAME
CODE=$(echo "$NAME" | tr -dc 'A-Za-z0-9' | tr a-z A-Z | cut -c1-8)$RANDOM
mkdir -p "$OUT"
E=""
for t in 30 60 90 120 150 180 240 300 400 500; do
  if [ "$t" -lt "$SECS" ]; then E="${E}console.log('REPORT '+window.fsaNetReport())@$t;"; fi
done
E="${E}console.log('REPORT '+window.fsaNetReport())@$((SECS-4));window.fsaDiagFlush()@$((SECS-2))"
for ((i = 0; i < SEATS; i++)); do
  CDP=$CDP1; if [ $((i % 2)) -eq 1 ]; then CDP=$CDP2; fi
  PQ=$PEERQ; if [ $i -eq 0 ]; then PQ=${PEERQ1:-}; fi  # seat 1 gets $PEERQ1 (default none), the others $PEERQ
  Q="iso=local.iso&nosave=1&room=$CODE&seat=$((i + 1))&name=b$((i + 1))&$PQ"
  if [ $i -eq 0 ]; then Q="$Q&create=1&seats=$SEATS&autostart=$SEATS&$ROOMQ"; fi
  mkdir -p "$OUT/seat$((i + 1))"
  timeout $((SECS + 90)) node tools/web_run.mjs --cdp "$CDP" --url "http://127.0.0.1:$WEB/?$Q" \
    --seconds "$SECS" --every 100000 --quiet --out "$OUT/seat$((i + 1))" --evals "$E" > /dev/null 2>&1 &
  if [ $i -eq 0 ]; then sleep 3; fi
done
wait
echo "room $CODE"
for ((i = 1; i <= SEATS; i++)); do
  echo "--- seat $i"
  grep -E "\[net\]|\[room\]|DESYNC|null function|RuntimeError" "$OUT/seat$i/console.log" | grep -v "Failed to load" | cut -c1-300 | head -8
  grep "^\[log\] REPORT" "$OUT/seat$i/console.log" | tail -1 | cut -c1-1200
done
