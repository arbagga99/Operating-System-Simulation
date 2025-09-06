#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "process.h"
#include <stdio.h>

typedef struct {
    int context_switch_cost;  // >=0
    int quantum;              // for RR, >0
    const char *algo;         // "fcfs" or "rr"
} SimConfig;

typedef struct {
    double cpu_utilization;   // 0..1
    double throughput;        // processes per unit time
    int    total_time;        // makespan
} SimSummary;

// Writes timeline events directly to timeline_fp as CSV.
// Writes per-process metrics to metrics_fp after simulation.
// Returns summary (utilization/throughput).
SimSummary simulate_fcfs(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);
SimSummary simulate_rr  (Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);

#endif
