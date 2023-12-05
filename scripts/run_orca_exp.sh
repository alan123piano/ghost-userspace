#!/bin/bash

for i in {1..10}; do
    scripts/orca_experiment.py \
        --out_file results/results${i}.txt
done
