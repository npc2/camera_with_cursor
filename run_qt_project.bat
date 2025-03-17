@echo off
REM 设置代码页为UTF-8
chcp 65001 > nul
REM 一键编译并运行Qt项目的批处理文件

echo === 开始编译并运行Qt项目 ===
echo 使用CMake构建项目...

REM 创建build目录
if not exist build mkdir build
cd build

REM 使用CMake生成构建文件
cmake .. -G "MinGW Makefiles"

REM 使用MinGW编译
echo 编译项目...
mingw32-make

REM 运行程序
echo 运行程序...
.\test_qt.exe

cd ..

echo === 批处理执行完成 === 