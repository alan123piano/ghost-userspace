#!/bin/bash

ARGS=("$@")
make clean -C orca
make -C orca
sudo ./orca/orca_client ${ARGS[@]}
