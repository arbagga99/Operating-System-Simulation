#!/usr/bin/env bash
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CS="${1:-1}"
ALGOS=("fcfs" "rr" "sjf" "srtf" "prio")

make -C "$ROOT"

for A in "${ALGOS[@]}"; do
  OUT="$ROOT/out/$A"
  mkdir -p "$OUT"
  if [ "$A" = "rr" ]; then
    "$ROOT/bin/os_sim" -i "$ROOT/data/processes.csv" -a rr -q 3 -cs "$CS" -o "$OUT"
  else
    "$ROOT/bin/os_sim" -i "$ROOT/data/processes.csv" -a "$A" -cs "$CS" -o "$OUT"
  fi
  python3 "$ROOT/tests/validator.py" "$ROOT/data/processes.csv" "$OUT/timeline.csv" "$OUT/metrics.csv" "$CS" >/dev/null
  echo "[OK] $A"
done

echo "All tests passed."
