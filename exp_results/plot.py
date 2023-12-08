import sys
import re
import matplotlib.pyplot as plt
import numpy as np
from enum import Enum


# with open(sys.argv[1]) as file:
class STAT(Enum):
    MEDIAN = 0
    MEAN = 1
    STDEV = 2


def populate_data(file_name: str):
    res = {
        "block_time_us": [],
        "runnable_time_us": [],
        "queued_time_us": [],
        "on_cpu_time_us": [],
        "yielding_time_us": [],
    }
    with open(file_name) as file:
        for line in file:
            # Define a regular expression pattern to extract median and mean
            pattern = r"median=([+-]?\d*\.\d+|\d+) mean=([+-]?\d*\.\d+|\d+) stdev=([+-]?\d*\.\d+|\d+)"

            # Use re.search to find the pattern in the line
            match = re.search(pattern, line)

            if match:
                # Extract the median and mean values from the match object
                median = float(match.group(1))
                mean = float(match.group(2))
                stdev = float(match.group(3))
                metric = line.split()[0]

                if metric in res.keys():
                    res[metric].append((mean, median, stdev))

    return res


def draw(data, stat: STAT, metric: str):
    print(stat.value)
    filtered_data = [item[stat.value] for item in data[metric]]
    plt.bar(
        ["0.0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0"],
        filtered_data,
        color="blue",
        width=0.5,
    )

    stat_name = "Median"
    if stat == STAT.MEAN:
        stat_name = "Mean"
    elif stat == STAT.STDEV:
        stat_name = "stdev"
    # # Add labels and title
    plt.xlabel("ratio of long tasks")
    plt.ylabel(f"{metric}")
    plt.title(f"{stat_name} {metric}")

    # Show the plot
    plt.show()
