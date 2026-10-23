#!/usr/bin/env bash
# Closed loop: vmmu on a pty, vprinter (Buddy ProtocolLogic) driving it.
# Usage: [VMMU_ARGS="--fault x"] loop_test.sh <vmmu> <vprinter> [vprinter args...]
set -euo pipefail
vmmu="$1"; vprinter="$2"; shift 2
out="$(mktemp)"; "$vmmu" --port pty ${VMMU_ARGS:-} >"$out" 2>&1 & pid=$!
trap 'kill $pid 2>/dev/null || true; rm -f "$out"' EXIT
for _ in $(seq 50); do grep -q " on " "$out" && break; sleep 0.1; done
"$vprinter" --port "$(awk '/ on /{print $NF}' "$out")" "$@"
