import statistics as st
import sys

with open(sys.argv[1]) as file:
    stats = {}
    for line in file:
        if line.startswith('Received Metric.'):
            idx = len('Received Metric. ')
            datum = {v.split('=')[0]: v.split('=')[1] for v in line[idx:].strip().split(', ')}
            for k, v in datum.items():
                if k not in stats:
                    stats[k] = []
                stats[k].append(int(v))

    for stat, vals in stats.items():
        print(f"{stat} median={st.median(vals)} mean={st.mean(vals)} stdev={st.stdev(vals)}")
