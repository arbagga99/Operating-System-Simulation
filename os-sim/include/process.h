#ifndef PROCESS_H
#define PROCESS_H

typedef struct {
    int pid;
    int arrival;
    int burst;
    int remaining;
    int priority;

    int start_time;            // first time scheduled (may include CS before)
    int finish_time;           // completion time
    int first_response_time;   // time of first CPU run
} Process;

int load_processes_csv(const char *path, Process **out_arr, int *out_n);
void sort_by_arrival(Process *ps, int n);

#endif
