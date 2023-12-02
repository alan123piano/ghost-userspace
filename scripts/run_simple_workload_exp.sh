#!/bin/bash

# very very bad security
# this is my VM user's password
PW=pass

restart_orca() {
    echo $PW | sudo -S pkill -KILL orca/orca
    echo $PW | sudo -S scripts/cleanup.sh
    echo $PW | sudo -S orca/orca 8000 &
    sleep 1
}

restart_orca

for i in {1..10}; do
    scripts/simple_workload_experiment.py \
        --orca_port 8000 \
        --out_file results${i}.txt |
        tee "stdout.txt" \
        &
    exp_pid=$!

    while kill -0 $exp_pid; do
        last_printed=$(stat -c %Y stdout.txt)
        now=$(date +%s)
        elapsed=$((now - last_printed))

        timeout=30
        if [ $elapsed -ge $timeout ]; then
            echo $PW | sudo -S kill -KILL $exp_pid
            break
        fi

        sleep 1
    done

    wait $exp_pid
done
