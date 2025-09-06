#!/usr/bin/env bash
set -e
THIS="$(cd "$(dirname "$0")" && pwd)"
ROOT="$THIS/.."
CS="${1:-1}"
Q="${2:-3}"

make -C "$ROOT"
python3 "$ROOT/scripts/CompareAlgorithms.py" "$CS" "$Q"
echo "Open dashboards/schedule_dashboard.html and upload any out/<algo>/*.csv to view."
