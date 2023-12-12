# Orca: A Dynamic Scheduling Framework

EECS 582 Final Project

Yong Seung Lee (leeyongs), Johnathan Schwartz (johnschw), Alan Yang (alanyang)

## Requirements

1. [ghOSt kernel](https://github.com/google/ghost-kernel) must be installed.
2. Follow the directions in [ghOSt userspace](https://github.com/google/ghost-userspace) to install the necessary headers and to install Bazel (the build tool used for the project).
3. Run `bazel build -c opt ...` to build all ghOSt userspace components, including our Orca components.

## Running the project

You can run the experiments from our paper by running `scripts/run_simple_workload_exp.sh`.

## Our changes

This repo is a fork of the [ghOSt userspace](https://github.com/google/ghost-userspace) libraries.

Here is a rough list of changes which we made as part of our project:

1. Added `orca` directory, which includes the Orchestrator code.
2. Added `plot` and `stats` directories, which include scripts for generating the graphs from our report.
3. Added instrumentation code to the provided `schedulers/fifo/centralized` and `schedulers/fifo/per_cpu` ghOSt userspace schedulers.
4. Added `scripts` directory, which includes scripts we wrote during development.
5. Added `tests/custom/simple_workload.cc`, which represents the synthetic workload we ran experiments on.
6. Changed Bazel `BUILD` file to include all of our code when compiling the library.
