#! /usr/bin/env bash

module load $COMPILER
mkdir build
cd build
cmake ../ -DTEST_WITH_SLURM=ON -DCMAKE_BUILD_TYPE=Release -DYGM_BUILD_TESTS=On
make VERBOSE=1
ctest -VV
