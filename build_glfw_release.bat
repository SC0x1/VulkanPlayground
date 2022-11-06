rem @echo off
cls

set rootDir=%~dp0%
set srcPath=%~dp0\Externals\glfw
set solutionDir=%srcPath%\build

cmake -S %srcPath% -B %solutionDir%

set vswhereDir=C:\Program Files (x86)\Microsoft Visual Studio\Installer
cd /d %vswhereDir%
echo  %vswhereDir%

rem https://github.com/microsoft/vswhere
for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)

if exist "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat" (
   call "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat" %*
)

echo %InstallDir%

cd /d %solutionDir%

msbuild GLFW.sln /target:GLFW3\glfw:Rebuild /property:Configuration=Release;OutDir="%rootDir%\Binaries\Lib" /t:Clean
