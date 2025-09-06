#include "scheduler.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

/* ---------------- FCFS ---------------- */
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

        int run = p->remaining;
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

/* ---------------- RR ---------------- */
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
            q_push(&rq, idx);
        }
        last_pid = p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp, &ps[i]);
    return finalize(ps, n, t, busy);
}

/* ---------------- SJF (non-preemptive) ---------------- */
static int pick_sjf(Process *ps, int n, int t){
    int best = -1, best_burst = INT_MAX;
    for (int i=0;i<n;i++){
        if (ps[i].remaining>0 && ps[i].arrival<=t){
            if (ps[i].burst < best_burst || (ps[i].burst==best_burst && ps[i].pid < ps[best].pid)){
                best = i; best_burst = ps[i].burst;
            }
        }
    }
    return best;
}

SimSummary simulate_sjf(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps, n);

    int t=0, completed=0, last_pid=-1, busy=0;

    while (completed<n){
        int idx = pick_sjf(ps,n,t);
        if (idx==-1){
            // idle until next arrival
            int next_t=t;
            for (int i=0;i<n;i++) if (ps[i].remaining>0) { if (next_t==t || ps[i].arrival<next_t) next_t=ps[i].arrival; }
            if (next_t>t){ write_timeline(timeline_fp,t,next_t,-1,EV_IDLE); t=next_t; }
            continue;
        }
        Process *p=&ps[idx];

        if (last_pid!=-1 && cfg.context_switch_cost>0 && last_pid!=p->pid){
            write_timeline(timeline_fp,t,t+cfg.context_switch_cost,-1,EV_CS); t+=cfg.context_switch_cost;
        }

        if (p->first_response_time<0) p->first_response_time=t;
        if (p->start_time<0) p->start_time=t;

        int run=p->remaining;
        write_timeline(timeline_fp,t,t+run,p->pid,EV_RUN);
        busy+=run; t+=run; p->remaining=0; p->finish_time=t; completed++;
        last_pid=p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp,&ps[i]);
    return finalize(ps,n,t,busy);
}

/* ---------------- SRTF (preemptive) ---------------- */
static int pick_srtf(Process *ps, int n, int t){
    int best=-1, best_rem=INT_MAX;
    for (int i=0;i<n;i++){
        if (ps[i].remaining>0 && ps[i].arrival<=t){
            if (ps[i].remaining < best_rem || (ps[i].remaining==best_rem && ps[i].pid < ps[best].pid)){
                best=i; best_rem=ps[i].remaining;
            }
        }
    }
    return best;
}

SimSummary simulate_srtf(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps,n);

    int t=0, completed=0, last_pid=-1, busy=0;

    // Find earliest arrival to start clock if needed
    for (int i=0;i<n;i++) if (ps[i].arrival < t) t = ps[i].arrival;

    while (completed<n){
        int idx = pick_srtf(ps,n,t);
        if (idx==-1){
            // Jump to next arrival
            int next_t = INT_MAX;
            for (int i=0;i<n;i++) if (ps[i].remaining>0 && ps[i].arrival>t && ps[i].arrival<next_t) next_t=ps[i].arrival;
            if (next_t==INT_MAX) break;
            write_timeline(timeline_fp,t,next_t,-1,EV_IDLE); t=next_t;
            continue;
        }

        Process *p=&ps[idx];

        if (last_pid!=-1 && cfg.context_switch_cost>0 && last_pid!=p->pid){
            write_timeline(timeline_fp,t,t+cfg.context_switch_cost,-1,EV_CS); t+=cfg.context_switch_cost;
        }

        if (p->first_response_time<0) p->first_response_time=t;
        if (p->start_time<0) p->start_time=t;

        // Next interesting time: next arrival or completion
        int next_arrival = INT_MAX;
        for (int i=0;i<n;i++) if (ps[i].remaining>0 && ps[i].arrival>t && ps[i].arrival<next_arrival) next_arrival=ps[i].arrival;

        int run;
        if (p->remaining <= (next_arrival==INT_MAX?INT_MAX:(next_arrival - t))) {
            run = p->remaining;
        } else {
            run = next_arrival - t;
        }

        write_timeline(timeline_fp,t,t+run,p->pid,EV_RUN);
        busy += run;
        t    += run;
        p->remaining -= run;

        if (p->remaining==0){
            p->finish_time=t; completed++;
        }
        // loop will re-pick best (preemption naturally occurs)
        last_pid=p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp,&ps[i]);
    return finalize(ps,n,t,busy);
}

/* ---------------- Priority (non-preemptive, lower number = higher priority) ---------------- */
static int pick_prio(Process *ps, int n, int t){
    int best=-1, best_pr=INT_MAX, best_arr=INT_MAX;
    for (int i=0;i<n;i++){
        if (ps[i].remaining>0 && ps[i].arrival<=t){
            if (ps[i].priority < best_pr ||
               (ps[i].priority==best_pr && (ps[i].arrival < best_arr ||
               (ps[i].arrival==best_arr && ps[i].pid < ps[best].pid)))){
                best=i; best_pr=ps[i].priority; best_arr=ps[i].arrival;
            }
        }
    }
    return best;
}

SimSummary simulate_prio(Process *ps, int n, SimConfig cfg, FILE *timeline_fp, FILE *metrics_fp){
    emit_header(timeline_fp, metrics_fp);
    sort_by_arrival(ps,n);

    int t=0, completed=0, last_pid=-1, busy=0;

    while (completed<n){
        int idx = pick_prio(ps,n,t);
        if (idx==-1){
            int next_t=t;
            for (int i=0;i<n;i++) if (ps[i].remaining>0) { if (next_t==t || ps[i].arrival<next_t) next_t=ps[i].arrival; }
            if (next_t>t){ write_timeline(timeline_fp,t,next_t,-1,EV_IDLE); t=next_t; }
            continue;
        }
        Process *p=&ps[idx];

        if (last_pid!=-1 && cfg.context_switch_cost>0 && last_pid!=p->pid){
            write_timeline(timeline_fp,t,t+cfg.context_switch_cost,-1,EV_CS); t+=cfg.context_switch_cost;
        }

        if (p->first_response_time<0) p->first_response_time=t;
        if (p->start_time<0) p->start_time=t;

        int run=p->remaining;
        write_timeline(timeline_fp,t,t+run,p->pid,EV_RUN);
        busy+=run; t+=run; p->remaining=0; p->finish_time=t; completed++;
        last_pid=p->pid;
    }

    for (int i=0;i<n;i++) write_metrics(metrics_fp,&ps[i]);
    return finalize(ps,n,t,busy);
}
