#!/bin/bash
cd ./Externals/glfw
cmake -S . -B ./build -D GLFW_BUILD_EXAMPLES=OFF
cd ./build
cmake --build . --target clean
make
mkdir -p ../../../build/libs/
cp ./src/libglfw3.a ../../../build/libs/libglfw3.a