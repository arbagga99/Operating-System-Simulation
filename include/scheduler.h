#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "process.h"
#include <stdio.h>

typedef struct {
    int context_switch_cost;  // >=0
    int quantum;              // for RR
    const char *algo;         // "fcfs" | "rr" | "sjf" | "srtf" | "prio"
} SimConfig;

typedef struct {
    double cpu_utilization;
    double throughput;
    int    total_time;
} SimSummary;

SimSummary simulate_fcfs(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);
SimSummary simulate_rr  (Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);
SimSummary simulate_sjf (Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);     // non-preemptive
SimSummary simulate_srtf(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);     // preemptive
SimSummary simulate_prio(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp);     // non-preemptive (lower value = higher priority)

#endif
