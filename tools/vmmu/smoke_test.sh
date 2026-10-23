#!/usr/bin/env bash
# CTest entry: start vmmu on a pty, run smoke.py against it ($2 = "load" for the slow load scenario).
set -euo pipefail
# usage: smoke_test.sh <vmmu> [load|load-error|tc-error|cut-rejected|cut-accepted] [vmmu args...]
vmmu="$1"; mode="${2:-}"; shift $(( $# > 1 ? 2 : 1 )); here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp)"; "$vmmu" --port pty "$@" >"$out" 2>&1 & pid=$!
trap 'kill $pid 2>/dev/null || true; rm -f "$out"' EXIT
for _ in $(seq 50); do grep -q " on " "$out" && break; sleep 0.1; done
python3 -I "$here/smoke.py" "$(awk '/ on /{print $NF}' "$out")" $mode
