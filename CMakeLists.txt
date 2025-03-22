cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(qt_camera_control LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "d:/Software/QT/6.7.2/mingw_64") # Qt Kit Dir

# Basic Qt settings
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets)

# 源文件列表
set(PROJECT_SOURCES
    src/main.cpp
    src/cam_qt.cpp
    src/cam_qt.h
    src/cam_qt.ui
    src/CameraControlDialog.cpp
    src/CameraControlDialog.h
    src/CameraDeviceInfo.h
    src/CameraUtils.cpp
    src/CameraUtils.h
    src/dbgout.cpp
    src/dbgout.h
    src/AudioManager.cpp
    src/AudioManager.h
    src/AudioPanel.cpp
    src/AudioPanel.h
)

# Create executable
add_executable(qt_camera_control ${PROJECT_SOURCES}) 

# Link libraries
target_link_libraries(qt_camera_control PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    ole32
    oleaut32
    strmiids
    uuid
    setupapi
)

# Windows specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        setupapi
        ole32
        oleaut32
        strmiids
        uuid
    )
endif()