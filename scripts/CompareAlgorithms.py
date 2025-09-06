#!/usr/bin/env python3
import csv, subprocess, pathlib, sys
from statistics import mean
from tests.validator import validate, read_csv  # re-use validator helpers

ROOT = pathlib.Path(__file__).resolve().parents[1]
BIN  = ROOT/"bin"
EXE  = BIN/"os_sim"
if sys.platform.startswith("win") and (BIN/"os_sim.exe").exists():
    EXE = BIN/"os_sim.exe"

ALGORITHMS = [
    ("fcfs",  None),
    ("rr",    3),     # quantum for RR
    ("sjf",   None),
    ("srtf",  None),
    ("prio",  None),
]

def run_algo(algo, quantum, cs_cost, outdir):
    outdir.mkdir(parents=True, exist_ok=True)
    cmd = [str(EXE), "-i", str(ROOT/"data/processes.csv"), "-a", algo, "-cs", str(cs_cost), "-o", str(outdir)]
    if algo=="rr":
        q = quantum if quantum else 4
        cmd += ["-q", str(q)]
    print("Running:", " ".join(cmd))
    subprocess.check_call(cmd)

def avg(nums): return round(mean(nums), 6)

def metrics_from_csv(p):
    rows = read_csv(p)
    w = [int(r["waiting"]) for r in rows]
    t = [int(r["turnaround"]) for r in rows]
    r = [int(r["response"]) for r in rows]
    return avg(w), avg(t), avg(r)

def main():
    cs_cost = int(sys.argv[1]) if len(sys.argv)>=2 else 1
    q_rr    = int(sys.argv[2]) if len(sys.argv)>=3 else 3

    summary_rows = []
    for algo, q in ALGORITHMS:
        outdir = ROOT/"out"/algo
        run_algo(algo, q_rr if algo=="rr" else q, cs_cost, outdir)
        v = validate(ROOT/"data/processes.csv", outdir/"timeline.csv", outdir/"metrics.csv", cs_cost)
        aw, at, ar = metrics_from_csv(outdir/"metrics.csv")
        summary_rows.append({
            "algo": algo,
            "avg_waiting": aw,
            "avg_turnaround": at,
            "avg_response": ar,
            "cpu_utilization": v["cpu_utilization"],
            "throughput": v["throughput"],
            "makespan": v["makespan"]
        })

    # write CSV
    out_csv = ROOT/"out"/"summary.csv"
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with open(out_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(summary_rows[0].keys()))
        w.writeheader(); w.writerows(summary_rows)

    # write Markdown
    out_md = ROOT/"out"/"report.md"
    with open(out_md, "w") as f:
        f.write("# Scheduler Comparison Report\n\n")
        f.write(f"- Context switch cost: **{cs_cost}**; RR quantum: **{q_rr}**\n\n")
        f.write("| Algo | Avg Waiting | Avg Turnaround | Avg Response | CPU Util | Throughput | Makespan |\n")
        f.write("|------|-------------:|---------------:|-------------:|---------:|-----------:|---------:|\n")
        for r in summary_rows:
            f.write(f"| {r['algo']} | {r['avg_waiting']:.3f} | {r['avg_turnaround']:.3f} | {r['avg_response']:.3f} | {r['cpu_utilization']:.3f} | {r['throughput']:.3f} | {r['makespan']} |\n")
        f.write("\n")
        # simple winners
        best_wait = min(summary_rows, key=lambda x: x["avg_waiting"])["algo"]
        best_ta   = min(summary_rows, key=lambda x: x["avg_turnaround"])["algo"]
        best_resp = min(summary_rows, key=lambda x: x["avg_response"])["algo"]
        best_util = max(summary_rows, key=lambda x: x["cpu_utilization"])["algo"]
        f.write(f"**Best (lower is better)** → Waiting: `{best_wait}`, Turnaround: `{best_ta}`, Response: `{best_resp}`  \n")
        f.write(f"**Best (higher is better)** → CPU Utilization: `{best_util}`\n")

    print("\nWrote:")
    print(" -", out_csv)
    print(" -", out_md)

if __name__ == "__main__":
    main()
