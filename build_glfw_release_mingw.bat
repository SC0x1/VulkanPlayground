cd ./externals/glfw
cmake -S . -B ./build -G "MinGW Makefiles"
cd ./build
mingw32-make
copy "src\libglfw3.a" "..\..\..\build\libs\libglfw3.a"