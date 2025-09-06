#include "scheduler.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef enum { EV_RUN, EV_CS, EV_IDLE } EventType;

/* ---------- helpers: I/O ---------- */

static void write_timeline(FILE *fp, int start, int end, int pid, EventType ev){
    if (end <= start) return;
    const char *evs = (ev==EV_RUN)?"RUN":(ev==EV_CS)?"CS":"IDLE";
    if (ev==EV_IDLE) fprintf(fp, "%d,%d,-,%s\n", start, end, evs);
    else             fprintf(fp, "%d,%d,%d,%s\n", start, end, pid, evs);
}

static void emit_header(FILE *timeline_fp, FILE *metrics_fp){
    fprintf(timeline_fp, "start,end,pid,event\n");
    fprintf(metrics_fp,  "pid,waiting,turnaround,response,arrival,burst,finish\n");
}

static void write_metrics(FILE *fp, Process *p){
    int turnaround = p->finish_time - p->arrival;
    int waiting    = turnaround - p->burst;
    int response   = (p->first_response_time < 0) ? -1 : (p->first_response_time - p->arrival);
    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d\n",
            p->pid, waiting, turnaround, response, p->arrival, p->burst, p->finish_time);
}

static SimSummary finalize(Process *ps, int n, int total_time, int busy_time){
    SimSummary s;
    s.total_time = total_time;
    s.cpu_utilization = (total_time>0)? ((double)busy_time)/total_time : 0.0;
    s.throughput = (total_time>0)? ((double)n)/total_time : 0.0;
    return s;
}

/* ---------- FCFS ---------- */

SimSummary simulate_fcfs(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps, n);

    Queue rq; q_init(&rq);
    int next_i = 0, t = 0, completed = 0, last_pid = -1, busy = 0;

    // enqueue arrivals at t=0
    while (next_i < n && ps[next_i].arrival <= t) { q_push(&rq, next_i++); }

    while (completed < n) {
        if (q_empty(&rq)) {
            int next_t = (next_i < n) ? ps[next_i].arrival : t;
            if (next_t > t) { write_timeline(timeline_fp, t, next_t, -1, EV_IDLE); t = next_t; }
            while (next_i < n && ps[next_i].arrival <= t) { q_push(&rq, next_i++); }
            continue;
        }

        int idx = q_pop(&rq);
        Process *p = &ps[idx];

        if (last_pid != -1 && cfg.context_switch_cost > 0) {
            write_timeline(timeline_fp, t, t + cfg.context_switch_cost, -1, EV_CS);
            t += cfg.context_switch_cost;
        }

        if (p->first_response_time < 0) p->first_response_time = t;
        if (p->start_time < 0) p->start_time = t;

        int run = p->remaining;                       // run to completion
        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;

        p->remaining = 0;
        p->finish_time = t;
        completed++;

        while (next_i < n && ps[next_i].arrival <= t) { q_push(&rq, next_i++); }
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}

/* ---------- Round Robin ---------- */

SimSummary simulate_rr(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps, n);

    Queue rq; q_init(&rq);
    int next_i = 0, t = 0, completed = 0, last_pid = -1, busy = 0;
    int quantum = (cfg.quantum>0)? cfg.quantum : 4;

    while (next_i < n && ps[next_i].arrival <= t) { q_push(&rq, next_i++); }

    while (completed < n) {
        if (q_empty(&rq)) {
            int next_t = (next_i < n) ? ps[next_i].arrival : t;
            if (next_t > t) { write_timeline(timeline_fp, t, next_t, -1, EV_IDLE); t = next_t; }
            while (next_i < n && ps[next_i].arrival <= t) { q_push(&rq, next_i++); }
            continue;
        }

        int idx = q_pop(&rq);
        Process *p = &ps[idx];

        if (last_pid != -1 && cfg.context_switch_cost > 0 && last_pid != p->pid) {
            write_timeline(timeline_fp, t, t + cfg.context_switch_cost, -1, EV_CS);
            t += cfg.context_switch_cost;
        }

        if (p->first_response_time < 0) p->first_response_time = t;
        if (p->start_time < 0) p->start_time = t;

        int run = (p->remaining < quantum) ? p->remaining : quantum;
        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;
        p->remaining -= run;

        while (next_i < n && ps[next_i].arrival <= t) { q_push(&rq, next_i++); }

        if (p->remaining == 0) {
            p->finish_time = t;
            completed++;
        } else {
            q_push(&rq, idx); // back of queue
        }
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}

/* ---------- SJF / SRTF / Priority pickers (hardened) ---------- */

// SJF: pick shortest original burst among arrived, tie by PID
static int pick_sjf(Process *ps, int n, int t){
    int best = -1, best_burst = INT_MAX;
    for (int i=0;i<n;i++){
        if (ps[i].remaining>0 && ps[i].arrival<=t){
            if (ps[i].burst < best_burst) { best = i; best_burst = ps[i].burst; }
            else if (ps[i].burst == best_burst && best != -1 && ps[i].pid < ps[best].pid) { best = i; }
        }
    }
    return best;
}

// SRTF: pick smallest remaining time among arrived, tie by PID
static int pick_srtf(Process *ps, int n, int t){
    int best = -1, best_rem = INT_MAX;
    for (int i=0;i<n;i++){
        if (ps[i].remaining>0 && ps[i].arrival<=t){
            if (ps[i].remaining < best_rem) { best = i; best_rem = ps[i].remaining; }
            else if (ps[i].remaining == best_rem && best != -1 && ps[i].pid < ps[best].pid) { best = i; }
        }
    }
    return best;
}

// Priority (non-preemptive): lower number = higher priority; ties by arrival then PID
static int pick_prio(Process *ps, int n, int t){
    int best = -1, best_pr = INT_MAX, best_arr = INT_MAX;
    for (int i=0;i<n;i++){
        if (ps[i].remaining>0 && ps[i].arrival<=t){
            if (ps[i].priority < best_pr) { best = i; best_pr = ps[i].priority; best_arr = ps[i].arrival; }
            else if (ps[i].priority == best_pr) {
                if (ps[i].arrival < best_arr) { best = i; best_arr = ps[i].arrival; }
                else if (ps[i].arrival == best_arr && best != -1 && ps[i].pid < ps[best].pid) { best = i; }
            }
        }
    }
    return best;
}

/* ---------- SJF (non-preemptive) ---------- */

SimSummary simulate_sjf(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps, n);

    int t = 0, completed = 0, last_pid = -1, busy = 0;

    while (completed < n) {
        int idx = pick_sjf(ps, n, t);
        if (idx == -1) {
            // idle until next arrival
            int next_t = INT_MAX;
            for (int i=0;i<n;i++)
                if (ps[i].remaining>0 && ps[i].arrival>t && ps[i].arrival<next_t) next_t = ps[i].arrival;
            if (next_t == INT_MAX) break;
            write_timeline(timeline_fp, t, next_t, -1, EV_IDLE);
            t = next_t;
            continue;
        }

        Process *p = &ps[idx];

        if (last_pid != -1 && cfg.context_switch_cost > 0 && last_pid != p->pid) {
            write_timeline(timeline_fp, t, t + cfg.context_switch_cost, -1, EV_CS);
            t += cfg.context_switch_cost;
        }

        if (p->first_response_time < 0) p->first_response_time = t;
        if (p->start_time < 0) p->start_time = t;

        int run = p->remaining;
        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;
        p->remaining = 0;
        p->finish_time = t;
        completed++;
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}

/* ---------- SRTF (preemptive) ---------- */

SimSummary simulate_srtf(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps,n);

    int t = (n>0)? ps[0].arrival : 0;   // start at earliest arrival after sort
    int completed = 0, last_pid = -1, busy = 0;

    while (completed < n) {
        int idx = pick_srtf(ps, n, t);
        if (idx == -1) {
            int next_t = INT_MAX;
            for (int i=0;i<n;i++)
                if (ps[i].remaining>0 && ps[i].arrival>t && ps[i].arrival<next_t) next_t = ps[i].arrival;
            if (next_t == INT_MAX) break;
            write_timeline(timeline_fp, t, next_t, -1, EV_IDLE);
            t = next_t;
            continue;
        }

        Process *p = &ps[idx];

        if (last_pid != -1 && cfg.context_switch_cost > 0 && last_pid != p->pid) {
            write_timeline(timeline_fp, t, t + cfg.context_switch_cost, -1, EV_CS);
            t += cfg.context_switch_cost;
        }

        if (p->first_response_time < 0) p->first_response_time = t;
        if (p->start_time < 0) p->start_time = t;

        // Determine next arrival time (strictly > t)
        int next_arrival = INT_MAX;
        for (int i=0;i<n;i++)
            if (ps[i].remaining>0 && ps[i].arrival>t && ps[i].arrival<next_arrival) next_arrival = ps[i].arrival;

        int run = p->remaining;
        if (next_arrival != INT_MAX) {
            int slice = next_arrival - t;
            if (slice < run) run = slice;
        }
        if (run <= 0) run = 1;  // safety: guarantee progress

        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;
        p->remaining -= run;

        if (p->remaining == 0) {
            p->finish_time = t;
            completed++;
        }
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}

/* ---------- Priority (non-preemptive, lower value = higher priority) ---------- */

SimSummary simulate_prio(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps,n);

    int t = 0, completed = 0, last_pid = -1, busy = 0;

    while (completed < n) {
        int idx = pick_prio(ps, n, t);
        if (idx == -1) {
            int next_t = INT_MAX;
            for (int i=0;i<n;i++)
                if (ps[i].remaining>0 && ps[i].arrival>t && ps[i].arrival<next_t) next_t = ps[i].arrival;
            if (next_t == INT_MAX) break;
            write_timeline(timeline_fp, t, next_t, -1, EV_IDLE);
            t = next_t;
            continue;
        }

        Process *p = &ps[idx];

        if (last_pid != -1 && cfg.context_switch_cost > 0 && last_pid != p->pid) {
            write_timeline(timeline_fp, t, t + cfg.context_switch_cost, -1, EV_CS);
            t += cfg.context_switch_cost;
        }

        if (p->first_response_time < 0) p->first_response_time = t;
        if (p->start_time < 0) p->start_time = t;

        int run = p->remaining;
        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;
        p->remaining = 0;
        p->finish_time = t;
        completed++;
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}
