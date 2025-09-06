#!/usr/bin/env python3
import csv, sys, json, pathlib
from collections import defaultdict

def read_csv(p):
    with open(p, newline="") as f:
        r=csv.DictReader(f)
        return list(r)

def to_int(x): return None if x in (None,"","-") else int(x)

def validate(processes_csv, timeline_csv, metrics_csv, context_switch_cost=None):
    procs = {}
    for r in read_csv(processes_csv):
        pid=int(r["pid"]); procs[pid]={"arrival":int(r["arrival"]), "burst":int(r["burst"]), "priority":int(r.get("priority",0))}
    timeline = read_csv(timeline_csv)
    metrics  = read_csv(metrics_csv)

    # --- timeline checks
    last_end=-1
    total_busy=0; total_cs=0; total_idle=0
    run_time_by_pid=defaultdict(int)
    first_run_time={}
    last_run_end={}
    for e in timeline:
        s=int(e["start"]); en=int(e["end"]); ev=e["event"]; pid=e["pid"]
        assert en>=s, f"Bad segment duration: {s}->{en}"
        assert s>=last_end, f"Overlapping events: {s} < {last_end}"
        last_end=en
        dur=en-s
        if ev=="RUN":
            p=int(pid); run_time_by_pid[p]+=dur
            total_busy+=dur
            if p not in first_run_time: first_run_time[p]=s
            last_run_end[p]=en
        elif ev=="CS":
            total_cs+=dur
            if context_switch_cost is not None:
                assert dur==context_switch_cost, f"CS duration {dur} != configured {context_switch_cost}"
        elif ev=="IDLE":
            total_idle+=dur
        else:
            raise AssertionError(f"Unknown event {ev}")

    makespan = last_end
    sum_bursts = sum(p["burst"] for p in procs.values())
    assert sum(run_time_by_pid.values())==sum_bursts, "RUN time != sum of bursts"

    # --- metrics checks
    seen=set()
    for m in metrics:
        pid=int(m["pid"]); seen.add(pid)
        arrival=procs[pid]["arrival"]; burst=procs[pid]["burst"]
        waiting=int(m["waiting"]); turnaround=int(m["turnaround"]); finish=int(m["finish"])
        response=int(m["response"])
        assert turnaround==finish-arrival, f"TA mismatch for pid {pid}"
        assert waiting==turnaround-burst, f"Waiting mismatch for pid {pid}"
        assert waiting>=0, f"Negative waiting for pid {pid}"
        assert finish==last_run_end[pid], f"Finish != last RUN end for pid {pid}"
        if pid in first_run_time:
            assert response==first_run_time[pid]-arrival, f"Response mismatch for pid {pid}"
            assert first_run_time[pid]>=arrival, f"First run before arrival for pid {pid}"
        else:
            raise AssertionError(f"No RUN for pid {pid}")

    assert seen==set(procs.keys()), "Metrics missing/extra pids"

    utilization = (total_busy/makespan) if makespan>0 else 0.0
    throughput = (len(procs)/makespan) if makespan>0 else 0.0

    return {
        "makespan": makespan,
        "busy": total_busy,
        "idle": total_idle,
        "cs_time": total_cs,
        "cpu_utilization": round(utilization,6),
        "throughput": round(throughput,6)
    }

def main():
    if len(sys.argv)<4:
        print("Usage: validator.py <data/processes.csv> <out/timeline.csv> <out/metrics.csv> [cs_cost]")
        sys.exit(1)
    cs = int(sys.argv[4]) if len(sys.argv)>=5 else None
    res = validate(sys.argv[1], sys.argv[2], sys.argv[3], cs)
    print(json.dumps(res, indent=2))

if __name__=="__main__":
    main()
