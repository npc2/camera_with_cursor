@echo off
REM 设置代码页为UTF-8
chcp 65001 > nul

echo === Starting build and run Qt project ===
echo Using CMake to generate build files...

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles"

echo Building project...
mingw32-make

echo Running program...
.\qt_camera_control.exe
