#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
  #include <sys/stat.h>
#else
  #include <direct.h>
#endif

static void usage(void){
    fprintf(stderr, "Usage: os_sim -i <input.csv> -a <fcfs|rr|sjf|srtf|prio> [-q <quantum>] [-cs <cost>] [-o <outdir>]\n");
}

static void ensure_dir(const char *path){
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

int main(int argc, char **argv){
    const char *in = NULL, *algo = NULL, *outdir = "out";
    int quantum = 4, cs = 0;

    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"-i") && i+1<argc) in=argv[++i];
        else if (!strcmp(argv[i],"-a") && i+1<argc) algo=argv[++i];
        else if (!strcmp(argv[i],"-q") && i+1<argc) quantum=atoi(argv[++i]);
        else if (!strcmp(argv[i],"-cs") && i+1<argc) cs=atoi(argv[++i]);
        else if (!strcmp(argv[i],"-o") && i+1<argc) outdir=argv[++i];
        else { usage(); return 1; }
    }
    if (!in || !algo) { usage(); return 1; }

    Process *ps = NULL; int n = 0;
    if (load_processes_csv(in, &ps, &n) != 0 || n<=0) {
        fprintf(stderr, "Failed to read input CSV: %s\n", in);
        return 2;
    }

    ensure_dir(outdir);
    char tpath[256], mpath[256];
    snprintf(tpath, sizeof(tpath), "%s/timeline.csv", outdir);
    snprintf(mpath, sizeof(mpath), "%s/metrics.csv",  sizeof(mpath));

    FILE *tf = fopen(tpath, "w");
    FILE *mf = fopen(mpath, "w");
    if (!tf || !mf) { fprintf(stderr, "Cannot open out files in %s\n", outdir); return 3; }

    SimConfig cfg = { .context_switch_cost = cs, .quantum = quantum, .algo = algo };
    SimSummary s;

    if      (!strcmp(algo,"fcfs")) s = simulate_fcfs(ps, n, cfg, tf, mf);
    else if (!strcmp(algo,"rr"))   s = simulate_rr  (ps, n, cfg, tf, mf);
    else if (!strcmp(algo,"sjf"))  s = simulate_sjf (ps, n, cfg, tf, mf);
    else if (!strcmp(algo,"srtf")) s = simulate_srtf(ps, n, cfg, tf, mf);
    else if (!strcmp(algo,"prio")) s = simulate_prio(ps, n, cfg, tf, mf);
    else { fprintf(stderr, "Unknown algo: %s\n", algo); fclose(tf); fclose(mf); free(ps); return 4; }

    fclose(tf); fclose(mf);

    printf("== Summary ==\n");
    printf("Algo        : %s\n", algo);
    printf("Processes   : %d\n", n);
    printf("Makespan    : %d\n", s.total_time);
    printf("CPU Util    : %.3f\n", s.cpu_utilization);
    printf("Throughput  : %.3f proc/unit\n", s.throughput);

    free(ps);
    return 0;
}
