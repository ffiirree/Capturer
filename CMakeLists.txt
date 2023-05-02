cmake_minimum_required(VERSION 3.16)

project(capturer VERSION 1.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default build type: Release." FORCE)
endif()

# #######################################################################################################################
# dependencies
# #######################################################################################################################
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# 3rdparty: QHotkey glog
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "build static libraries.")
set(WITH_GFLAGS         OFF CACHE BOOL "Use gflags")
set(PROBE_BUILD_WITH_QT ON  CACHE BOOL "Build probe with Qt")

add_subdirectory(3rdparty/QHotkey   EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/glog      EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/fmt       EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/probe     EXCLUDE_FROM_ALL)

# Qt5
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Gui LinguistTools REQUIRED)

if(WIN32)
    # GUI/console
    set(CMAKE_WIN32_EXECUTABLE $<IF:$<CONFIG:DEBUG>,OFF,ON>)
    # >= 6 required by nvenc for encoding d3d11 format
    find_package(FFmpeg 6 REQUIRED)
elseif(UNIX AND NOT APPLE)
    find_package(FFmpeg 3.4 REQUIRED)
    find_package(Qt5 COMPONENTS X11Extras REQUIRED)
    find_package(PulseAudio REQUIRED)
    find_package(Libv4l2 REQUIRED)
    find_package(X11 REQUIRED)
    if(NOT X11_Xrandr_FOUND)
        message(FATAL_ERROR "Xrandr is required")
    endif()
endif()

# #######################################################################################################################
# Create the executable
# #######################################################################################################################
configure_file(src/version.h.in ${PROJECT_BINARY_DIR}/version.h @ONLY)

file(GLOB_RECURSE SOURCES src/*.cpp)

set(TS_FILES languages/capturer_en_US.ts languages/capturer_zh_CN.ts)
qt5_create_translation(QM_FILES ${SOURCES} ${TS_FILES})

add_executable(${PROJECT_NAME}
    ${SOURCES}
    src/capturer.qrc
    ${QM_FILES}
    $<$<PLATFORM_ID:Windows>:src/ico_win32.rc>
)

target_compile_options(${PROJECT_NAME} 
    PRIVATE 
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /Zc:preprocessor /Zc:__cplusplus>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-deprecated-enum-enum-conversion>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        QHotkey::QHotkey
        Qt5::Widgets Qt5::Gui
        glog::glog
        fmt::fmt
        probe::probe
        ffmpeg::ffmpeg
        $<$<PLATFORM_ID:Windows>:dwmapi>
        $<$<PLATFORM_ID:Windows>:strmiids>
        $<$<PLATFORM_ID:Windows>:ntdll>
        $<$<PLATFORM_ID:Windows>:Shcore> 
        $<$<PLATFORM_ID:Windows>:DXGI>
        $<$<PLATFORM_ID:Windows>:D3D11>
        $<$<PLATFORM_ID:Linux>:Qt5::X11Extras>
        $<$<PLATFORM_ID:Linux>:X11::Xrandr>
        $<$<PLATFORM_ID:Linux>:Libv4l2::Libv4l2>
        $<$<PLATFORM_ID:Linux>:${PULSEAUDIO_LIBRARY}>
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        3rdparty
        src/core
        src/media
        src/widgets
        src/menu
        src/pinned
        src/snip
        src/record
        src/setting
        ${PULSEAUDIO_INCLUDE_DIR}
        ${PROJECT_BINARY_DIR} # Generated header files
)

# #######################################################################################################################
# Install
# #######################################################################################################################
message(STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

# Binaries
if(WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}")
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
elseif(UNIX AND NOT APPLE)
    # installed: /usr/local/bin/capturer
    # installed: /usr/local/etc/capturer/translations/capturer_en_US.ts
    # installed: /usr/local/etc/capturer/translations/capturer_zh_CN.ts
    # installed: /usr/share/applications/capturer.desktop
    # installed: /usr/share/pixmaps/capturer.png
    # installed: /usr/share/icons/hicolor/128x128/apps/capturer.png
    # installed: ~/.config/capturer/config.json
    # installed: ~/.config/autostart/capturer.desktop
    # installed: ~/.local/logs/capturer/capturer-*.log
    install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/etc/capturer/translations")
    install(FILES "${PROJECT_SOURCE_DIR}/capturer.desktop" DESTINATION "/usr/share/applications")
    install(FILES "${PROJECT_SOURCE_DIR}/src/res/capturer.png" DESTINATION "/usr/share/pixmaps")
    install(FILES "${PROJECT_SOURCE_DIR}/src/res/capturer.png" DESTINATION "/usr/share/icons/hicolor/128x128/apps")
endif()

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/cmake/uninstall.cmake")

# #######################################################################################################################
# Create DEB
# #######################################################################################################################
if(UNIX)
    set(CPACK_PACKAGE_NAME ${PROJECT_NAME})

    # Set a Package Version
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

    set(CPACK_PACKAGE_DESCRIPTION "Screen snipping and recording")

    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

    # Tell CPack to generate a .deb package
    set(CPACK_GENERATOR "DEB")

    # Set a Package Maintainer.
    # This is required
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Liangqi Zhang(zhliangqi@gmail.com)")

    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")

    if(EXISTS "/etc/lsb-release")
        file(STRINGS "/etc/lsb-release" SYSTEM_NAME)
        execute_process(
            COMMAND echo "${SYSTEM_NAME}"
            COMMAND cut -d ";" -f 1
            COMMAND cut -d "=" -f 2
            OUTPUT_VARIABLE HOST_SYSTEM_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND echo "${SYSTEM_NAME}"
            COMMAND cut -d ";" -f 2
            COMMAND cut -d "=" -f 2
            OUTPUT_VARIABLE HOST_SYSTEM_VER
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(${HOST_SYSTEM_NAME} MATCHES "Ubuntu")
            string(CONCAT HOST_SYSTEM_VER "ubuntu_" ${HOST_SYSTEM_VER})

            # the ubuntu need to modify the architecture name
            if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
                set(CMAKE_HOST_SYSTEM_PROCESSOR "amd64")
            endif()
        endif()

        set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${HOST_SYSTEM_VER}-${CMAKE_HOST_SYSTEM_PROCESSOR})
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${CMAKE_HOST_SYSTEM_PROCESSOR})
    else()
        set(HOST_SYSTEM_VER "Unknown")
    endif()

    include(CPack)
endif()
