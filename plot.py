import csv, sys
import matplotlib.pyplot as plt

rows = list(csv.DictReader(open(sys.argv[1])))

def plot(metric, out, ylabel, title):
    by = {}
    for r in rows:
        by.setdefault(r["impl"], []).append((int(r["iter"]), float(r[metric])))
    plt.figure()
    for impl, pts in by.items():
        pts.sort()
        plt.plot([x for x,_ in pts], [y for _,y in pts], label=impl)
    plt.xlabel("ITER"); plt.ylabel(ylabel); plt.title(title); plt.legend()
    plt.tight_layout(); plt.savefig(out); plt.close()

plot("latency_ns", "results/latency.png", "Average response time (ns)", "Latency vs ITER")
plot("throughput", "results/throughput.png", "Average throughput (clients/s)", "Throughput vs ITER")
print("Wrote: latency.png, throughput.png")
