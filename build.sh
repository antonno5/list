#!/bin/bash

echo "Building the project..."

if [ ! -d "./build" ]; then
    mkdir build
fi

if cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZER_ADDRESS=True -DENABLE_SANITIZER_UNDEFINED_BEHAVIOR=True $@; then
echo "calling make...";
else 
echo "CMake failed!";
exit 1 
fi

num_procs=$(nproc)
echo "building using ${num_procs} workers"
if cmake --build build --parallel ${num_procs}; then
echo "build ready";
else 
echo "build failed!";
exit 2
fi


