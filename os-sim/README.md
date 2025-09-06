# Operating System Simulation (CPU Scheduler)

Event-driven CPU scheduling simulator in C with FCFS and Round-Robin (RR), configurable context-switch cost, and CSV outputs (timeline + per-process metrics).

## Features
- Algorithms: `fcfs`, `rr` (more to come: `sjf`, `srtf`, `priority`)
- Context-switch overhead (integer time units)
- Input: CSV of processes (pid,arrival,burst,priority)
- Output: `out/timeline.csv` and `out/metrics.csv`
- Metrics: waiting, turnaround, response, CPU utilization, throughput

## Quickstart
```bash
make
./bin/os_sim -i data/processes.csv -a rr -q 3 -cs 1 -o out
# or FCFS:
./bin/os_sim -i data/processes.csv -a fcfs -cs 1 -o out
