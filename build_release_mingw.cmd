if not exist build mkdir build
cd build
cmake -S ../ -B . -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE=Release
cmake --build . --target clean 
mingw32-make.exe && mingw32-make.exe Shaders
cd ..