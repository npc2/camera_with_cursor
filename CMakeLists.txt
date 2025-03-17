cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(test_qt LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "d:/Software/QT/6.7.2/mingw_64") # Qt Kit Dir

# Basic Qt settings
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt packages
find_package(Qt6 COMPONENTS 
    Widgets 
    Multimedia 
    MultimediaWidgets
    Core
    Gui
    REQUIRED
) # Qt COMPONENTS

# 源文件列表
set(SOURCES
    src/main.cpp
    src/test_qt.cpp
    src/test_qt.h
    src/test_qt.ui
    src/dbgout.cpp
    src/dbgout.h
    src/CameraControlDialog.cpp
    src/CameraControlDialog.h
    src/CameraDeviceInfo.h
    src/CameraUtils.cpp
    src/CameraUtils.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES}) 

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Widgets 
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Core
    Qt6::Gui
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