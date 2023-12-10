import argparse
import csv
from dataclasses import dataclass
from decimal import Decimal
import os
from typing import Any
import matplotlib.pyplot as plt


@dataclass
class ExpResult:
    sched_type: str
    preemption_interval_us: int
    throughput: int
    proportion_long_jobs: Decimal
    latency: float


def read_csv(filename: str, latency_stat: str) -> list[ExpResult]:
    "Read CSV into data structure."

    results: list[ExpResult] = []

    with open(filename) as file:
        reader = csv.reader(file)
        rows = list(reader)

        idx_of_keys: dict[str, int] = {rows[0][i]: i for i in range(len(rows[0]))}
        for row in rows[1:]:
            results.append(
                ExpResult(
                    sched_type=row[idx_of_keys["sched_type"]],
                    preemption_interval_us=int(
                        row[idx_of_keys["preemption_interval_us"]]
                    ),
                    throughput=int(row[idx_of_keys["throughput"]]),
                    proportion_long_jobs=Decimal(
                        row[idx_of_keys["proportion_long_jobs"]]
                    ),
                    latency=float(row[idx_of_keys[latency_stat]]),
                )
            )

    return results


stats = [
    (f"{reqtype}_{pctile}_pct", f"{pctile}")
    for reqtype in ["short", "long"]
    for pctile in ["0", "25", "50", "75", "90", "99", "99.9", "1"]
]


parser = argparse.ArgumentParser()
parser.add_argument("results_dir", type=str, help="directory of results to plot")
parser.add_argument(
    "latency_stat",
    type=str,
    choices=[v[0] for v in stats],
    nargs="?",
    default="short_99.9_pct",
    help="which latency stat to plot",
)


def main() -> None:
    args = parser.parse_args()
    results_dir: str = args.results_dir
    latency_stat: str = args.latency_stat

    results: list[ExpResult] = []
    for fname in os.listdir(results_dir):
        fpath = os.path.join(results_dir, fname)
        if os.path.isfile(fpath) and fpath.endswith(".txt"):
            results += read_csv(fpath, latency_stat)

    # Want to plot  : throughput vs. short_99_9_pct (tail latency).
    # Multiple lines: sched_type.
    # Fixed vars    : preemption_interval_us, proportion_long_jobs.

    def plot(ax: Any, rows: list[ExpResult], title: str) -> None:
        # Ind vars: sched_type, throughput.
        # Dep vars: short_99_9_pct.

        # Filter for lowest tail latency per group.
        m: dict[tuple[str, int], list[float]] = {}
        for row in rows:
            k = (row.sched_type, row.throughput)
            v = row.latency
            if k not in m:
                m[k] = []
            m[k].append(v)

        # vals :: [(SchedType, Throughput, Latency)]
        vals: list[tuple[str, int, float]] = [
            (k[0], k[1], min(v)) for k, v in m.items()
        ]
        sched_types = ["dFCFS", "cFCFS", "cfs"]
        if "orca" in set([v[0] for v in vals]):
            sched_types.append("orca")

        for sched_type in sched_types:
            svals = [(v[1], v[2]) for v in vals if v[0] == sched_type]
            xs = [v[0] for v in svals]
            ys = [v[1] for v in svals]
            ax.plot(xs, ys, label=sched_type)

        ax.set_xlabel("Throughput (reqs/sec)")
        pctile = [v[1] for v in stats if v[0] == latency_stat][0]
        ax.set_ylabel(f"{pctile}% Latency (μs)")
        ax.legend()
        ax.set_title(title)

    results = [row for row in results]

    fig, axs = plt.subplots(2, 2)
    plot(
        axs[0][0],
        [row for row in results if row.proportion_long_jobs == Decimal("0")],
        "Scheduler Tail Latency (0-100 workload)",
    )
    plot(
        axs[0][1],
        [row for row in results if row.proportion_long_jobs == Decimal("0.01")],
        "Scheduler Tail Latency (1-99 workload)",
    )
    plot(
        axs[1][0],
        [row for row in results if row.proportion_long_jobs == Decimal("0.1")],
        "Scheduler Tail Latency (10-90 workload)",
    )
    plot(
        axs[1][1],
        [row for row in results if row.proportion_long_jobs == Decimal("0.5")],
        "Scheduler Tail Latency (50-50 workload)",
    )

    plt.show()


if __name__ == "__main__":
    main()
