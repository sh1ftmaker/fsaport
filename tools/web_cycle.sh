#!/bin/sh
# Rebuild the web build and run it once in the private Chromium (CDP 9334).
#   tools/web_cycle.sh NAME [SECONDS] [EVERY] [KEYS] [EVALS]
set -e
cd "$(dirname "$0")/.."
. /bigdata/agent-tom/projects/fsadecomp/tools/emsdk/emsdk_env.sh >/dev/null 2>&1
ninja -C build/web fsa > build/web-link.log 2>&1 || { grep -E " error:|FAILED" build/web-link.log | head -20; exit 1; }
cp platform/web/index.html platform/web/touch.js platform/web/gputrace.js build/web/
timeout $(( ${2:-30} + 60 )) node tools/web_run.mjs --cdp 9334 --url 'http://127.0.0.1:8811/?iso=local.iso' \
  --seconds "${2:-30}" --every "${3:-10}" --out "build/web-run/$1" --quiet ${4:+--keys "$4"} ${5:+--evals "$5"}
grep -v "^\[log.verbose\]\|^\[log\]   max\|^\[log\]   min" "build/web-run/$1/console.log"
