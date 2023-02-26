if not exist build mkdir build
cd build
cmake -S ../ -B . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug --target clean
mingw32-make.exe -j5 && mingw32-make.exe Shaders
cd ..