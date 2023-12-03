#!/bin/bash

for i in {1..10}; do
    scripts/simple_workload_experiment.py \
        --out_file results/results${i}.txt
done
