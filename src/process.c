#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_line(const char *line, Process *p) {
    // Expect: pid,arrival,burst,priority
    char buf[256];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char *tok = strtok(buf, ",");
    if (!tok) return -1; p->pid = atoi(tok);

    tok = strtok(NULL, ",");
    if (!tok) return -1; p->arrival = atoi(tok);

    tok = strtok(NULL, ",");
    if (!tok) return -1; p->burst = atoi(tok);

    tok = strtok(NULL, ",");
    p->priority = tok ? atoi(tok) : 0;

    p->remaining = p->burst;
    p->start_time = -1;
    p->finish_time = -1;
    p->first_response_time = -1;
    return 0;
}

int load_processes_csv(const char *path, Process **out_arr, int *out_n) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    // Count lines
    int cap = 16, n = 0;
    Process *arr = malloc(sizeof(Process) * cap);
    if (!arr) { fclose(f); return -2; }

    char line[512];
    // skip header
    if (!fgets(line, sizeof(line), f)) { fclose(f); free(arr); return -3; }

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0]=='\0') continue;
        if (n == cap) {
            cap *= 2;
            Process *tmp = realloc(arr, sizeof(Process) * cap);
            if (!tmp) { free(arr); fclose(f); return -4; }
            arr = tmp;
        }
        if (parse_line(line, &arr[n]) == 0) n++;
    }
    fclose(f);
    *out_arr = arr;
    *out_n = n;
    return 0;
}

static int cmp_arrival(const void *a, const void *b) {
    const Process *pa = (const Process *)a;
    const Process *pb = (const Process *)b;
    if (pa->arrival != pb->arrival) return pa->arrival - pb->arrival;
    return pa->pid - pb->pid;
}
void sort_by_arrival(Process *ps, int n) {
    qsort(ps, n, sizeof(Process), cmp_arrival);
}
