cmake_minimum_required(VERSION 3.15)

project(Capturer VERSION 1.2.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    find_package(Qt5 COMPONENTS Widgets Gui REQUIRED)
elseif(UNIX AND NOT APPLE)
    find_package(Qt5 COMPONENTS Widgets Gui X11Extras REQUIRED)
endif()

file(GLOB_RECURSE SOURCE_FILES src/*.h src/*.cpp src/*.qrc)

include_directories(
        3rdparty
        src/core
        src/widgets
        src/menu
        src/snip
        src/video
        src/gif
        src/setting
)

add_subdirectory(3rdparty/QHotkey)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

set(LINK_LIBS QHotkey::QHotkey Qt5::Widgets Qt5::Gui)

if(WIN32)
    list(APPEND LINK_LIBS dwmapi strmiids)
elseif(UNIX AND NOT APPLE)
    list(APPEND LINK_LIBS Qt5::X11Extras)
endif()

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})