#include "scheduler.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>

typedef enum { EV_RUN, EV_CS, EV_IDLE } EventType;

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

static void enqueue_arrivals(Queue *rq, Process *ps, int n, int *next_i, int t){
    while (*next_i < n && ps[*next_i].arrival <= t) {
        q_push(rq, *next_i);
        (*next_i)++;
    }
}

static SimSummary finalize(Process *ps, int n, int total_time, int busy_time){
    SimSummary s;
    s.total_time = total_time;
    s.cpu_utilization = (total_time>0)? ((double)busy_time)/total_time : 0.0;
    s.throughput = (total_time>0)? ((double)n)/total_time : 0.0;
    return s;
}

SimSummary simulate_fcfs(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps, n);

    Queue rq; q_init(&rq);
    int next_i = 0, t = 0, completed = 0, last_pid = -1, busy = 0;

    enqueue_arrivals(&rq, ps, n, &next_i, t);

    while (completed < n) {
        if (q_empty(&rq)) {
            int next_t = (next_i < n) ? ps[next_i].arrival : t;
            if (next_t > t) { write_timeline(timeline_fp, t, next_t, -1, EV_IDLE); t = next_t; }
            enqueue_arrivals(&rq, ps, n, &next_i, t);
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

        int run = p->remaining; // FCFS: run to completion
        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;
        p->remaining = 0;
        p->finish_time = t;
        completed++;

        enqueue_arrivals(&rq, ps, n, &next_i, t);
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}

SimSummary simulate_rr(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps, n);

    Queue rq; q_init(&rq);
    int next_i = 0, t = 0, completed = 0, last_pid = -1, busy = 0;
    int q = (cfg.quantum>0)? cfg.quantum : 4;

    enqueue_arrivals(&rq, ps, n, &next_i, t);

    while (completed < n) {
        if (q_empty(&rq)) {
            int next_t = (next_i < n) ? ps[next_i].arrival : t;
            if (next_t > t) { write_timeline(timeline_fp, t, next_t, -1, EV_IDLE); t = next_t; }
            enqueue_arrivals(&rq, ps, n, &next_i, t);
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

        int run = (p->remaining < q) ? p->remaining : q;
        write_timeline(timeline_fp, t, t + run, p->pid, EV_RUN);
        busy += run;
        t += run;
        p->remaining -= run;

        enqueue_arrivals(&rq, ps, n, &next_i, t);

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
