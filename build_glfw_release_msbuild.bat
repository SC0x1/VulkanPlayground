@echo off
cls

set rootDir=%~dp0%
set srcPath=%~dp0\externals\glfw
set buildDir=%srcPath%\build

cmake -S %srcPath% -B %buildDir%

set vswhereDir="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\"
cd /d %vswhereDir%
echo  %vswhereDir%

setlocal enabledelayedexpansion

rem https://github.com/microsoft/vswhere
for /f "usebackq tokens=*" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)

if exist "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat" (
   call "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat" %*
)

echo %InstallDir%

cd /d %buildDir%

msbuild GLFW.sln /target:GLFW3\glfw:Rebuild /property:Configuration=Release;OutDir="%rootDir%\build\libs" /t:Clean
