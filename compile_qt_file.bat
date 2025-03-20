@echo off
setlocal enabledelayedexpansion

REM 设置Qt路径
set QT_DIR=D:\Software\QT\6.7.2\mingw_64
set QT_TOOLS_DIR=D:\Software\QT\Tools\mingw1120_64
set PATH=%QT_DIR%\bin;%QT_TOOLS_DIR%\bin;%PATH%

REM 检查参数
if "%~1"=="" (
    echo 请提供源文件路径作为参数
    exit /b 1
)

if "%~2"=="" (
    echo 请提供输出可执行文件名作为第二个参数
    exit /b 1
)

set SOURCE_FILE=%~1
set OUTPUT_FILE=%~2

REM 创建build目录
if not exist build mkdir build

echo 编译Qt项目到 %OUTPUT_FILE%...

REM 生成UI头文件
echo 生成UI头文件...
%QT_DIR%\bin\uic.exe src\cam_qt.ui -o build\ui_cam_qt.h

REM 运行Qt元对象编译器
echo 运行Qt元对象编译器...
%QT_DIR%\bin\moc.exe src\cam_qt.h -o build\moc_cam_qt.cpp
%QT_DIR%\bin\moc.exe src\CameraControlDialog.h -o build\moc_CameraControlDialog.cpp

REM 编译所有源文件
echo 编译所有源文件...
g++ -c src\dbgout.cpp -o build\dbgout.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE
g++ -c src\CameraUtils.cpp -o build\CameraUtils.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE
g++ -c src\CameraControlDialog.cpp -o build\CameraControlDialog.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE
g++ -c src\cam_qt.cpp -o build\cam_qt.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE
g++ -c src\main.cpp -o build\main.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE
g++ -c build\moc_cam_qt.cpp -o build\moc_cam_qt.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE
g++ -c build\moc_CameraControlDialog.cpp -o build\moc_CameraControlDialog.o -I%QT_DIR%\include -I%QT_DIR%\include\QtCore -I%QT_DIR%\include\QtGui -I%QT_DIR%\include\QtWidgets -I%QT_DIR%\include\QtMultimedia -I%QT_DIR%\include\QtMultimediaWidgets -I. -Ibuild -DUNICODE

REM 链接
g++ build\dbgout.o build\CameraUtils.o build\CameraControlDialog.o build\cam_qt.o build\main.o build\moc_cam_qt.o build\moc_CameraControlDialog.o -o %OUTPUT_FILE% -L%QT_DIR%\lib -lQt6Core -lQt6Gui -lQt6Widgets -lQt6Multimedia -lQt6MultimediaWidgets -lole32 -loleaut32 -lstrmiids -luuid -lsetupapi -mwindows

if %errorlevel% equ 0 (
    echo 编译成功！
    %OUTPUT_FILE%
) else (
    echo 编译失败！
) 