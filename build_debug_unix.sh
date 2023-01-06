#!/bin/bash
mkdir -p build
cd build
cmake -S ../ -B . -D CMAKE_BUILD_TYPE=Debug
cmake --build . --target clean
make && make Shaders
cd ..