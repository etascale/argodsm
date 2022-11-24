#! /usr/bin/env bash

# Build argo_example.cpp using the installed ArgoDSM libraries
mpic++ -o test_compiling_example -O3 -std=c++17 -L$(pwd)/install/lib -I$(pwd)/install/include/argo -Wl,-rpath,$(pwd)/install/lib docs/_includes/argo_example.cpp -largo -largobackend-mpi
ret=$?

# Validate the outcome
if [ $ret -eq 0 ]; then
    echo "External make SUCCESSFUL"
else
    echo "External make FAILED"; exit $ret
fi
