cd /d ./externals/glfw

IF EXIST "./build" (
    cd /d ./build
    for /F "delims=" %%i in ('dir /b') do (rmdir "%%i" /s/q || del "%%i" /s/q)
    cd ..
)

cmake -S . -B ./build -G "MinGW Makefiles" 
cd ./build
mingw32-make
copy "src\libglfw3.a" "..\..\..\build\libs\libglfw3.a"