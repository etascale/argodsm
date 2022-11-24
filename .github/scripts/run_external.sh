#! /usr/bin/env bash

# Execute test_compiling_example on 2 nodes
mpirun -n 2 ./test_compiling_example > /dev/null
ret=$?

# Validate the outcome
if [ $ret -eq 0 ]; then
    echo "External run SUCCESSFUL"
else
    echo "External run FAILED"; exit $ret
fi
