#!/usr/bin/env bash
set -e
make
mkdir -p out
./bin/os_sim -i data/processes.csv -a rr -q 3 -cs 1 -o out
echo "Timeline -> out/timeline.csv"
echo "Metrics  -> out/metrics.csv"
