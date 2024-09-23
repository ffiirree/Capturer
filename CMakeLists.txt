cmake_minimum_required(VERSION 3.27)

project(capturer VERSION 1.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default build type: Release." FORCE)
endif ()

# #######################################################################################################################
# dependencies
# #######################################################################################################################
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/3rdparty/cmake-modules")

# 3rdparty: QHotkey glog
set(BUILD_SHARED_LIBS           OFF CACHE BOOL "Build static libraries")
set(WITH_GFLAGS                 OFF CACHE BOOL "Use gflags for google/glog")
set(PROBE_BUILD_WITH_QT         ON  CACHE BOOL "Build probe with Qt")
set(JSON_BuildTests             OFF CACHE BOOL "Build nlohmann/json tests")
set(QT_DEFAULT_MAJOR_VERSION    6   CACHE STRING "Qt major version to use (5 or 6), default: 6.")

add_subdirectory(3rdparty/QHotkey   EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/glog      EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/fmt       EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/probe     EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/json      EXCLUDE_FROM_ALL)
add_subdirectory(libcap             EXCLUDE_FROM_ALL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.7 COMPONENTS Core Gui Widgets LinguistTools ShaderTools REQUIRED)
find_package(FFmpeg 7.0 REQUIRED)
find_package(Libass REQUIRED)

if (WIN32)
    # GUI/console
    set(CMAKE_WIN32_EXECUTABLE $<IF:$<CONFIG:DEBUG>,OFF,ON>)

elseif (UNIX AND NOT APPLE)
    find_package(Qt6 6.7 COMPONENTS DBus REQUIRED)
endif ()

# #######################################################################################################################
# Create the executable
# #######################################################################################################################
configure_file(src/version.h.in ${PROJECT_BINARY_DIR}/version.h @ONLY)

file(GLOB_RECURSE SOURCES src/*.cpp)

set(TS_FILES languages/capturer_en_US.ts languages/capturer_zh_CN.ts)
qt6_create_translation(QM_FILES ${SOURCES} ${TS_FILES} OPTIONS -locations none -no-obsolete)

add_executable(${PROJECT_NAME}
        ${SOURCES}
        src/capturer.qrc
        ${QM_FILES}
        $<$<PLATFORM_ID:Windows>:src/ico_win32.rc>
)

if (WIN32)
   # Build HLSL shaders
   add_custom_target(shaders)

   set(HLSL_SHADER_FILES src/resources/d3d11/ResizingVertexShader.hlsl src/resources/d3d11/ResizingPixelShader.hlsl)

   set_source_files_properties(src/resources/d3d11/ResizingVertexShader.hlsl   PROPERTIES ShaderType "vs")
   set_source_files_properties(src/resources/d3d11/ResizingPixelShader.hlsl    PROPERTIES ShaderType "ps")

   set_source_files_properties(${HLSL_SHADER_FILES}                            PROPERTIES ShaderModel "5_0")

   foreach (FILE ${HLSL_SHADER_FILES})
       get_filename_component(FILE_WE ${FILE} NAME_WE)
       list(APPEND CSO_SHADER_FILES ${PROJECT_BINARY_DIR}/${FILE_WE}.cso)
       get_source_file_property(shadertype ${FILE} ShaderType)
       get_source_file_property(shadermodel ${FILE} ShaderModel)
       add_custom_command(TARGET shaders
               COMMAND fxc.exe /nologo /E${shadertype}_main /T${shadertype}_${shadermodel} $<IF:$<CONFIG:DEBUG>,/Od,/O1> /Zi /Fh ${PROJECT_BINARY_DIR}/${FILE_WE}.h ${FILE}
               MAIN_DEPENDENCY ${FILE}
               COMMENT "HLSL ${FILE}"
               WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
               VERBATIM
       )
   endforeach ()

   add_dependencies(${PROJECT_NAME} shaders)
endif ()

qt_add_shaders(${PROJECT_NAME} "capturer_shaders"
        PREFIX
        "/"
        FILES
        "src/resources/shaders/vertex.vert"
        "src/resources/shaders/yuv.frag"
        "src/resources/shaders/rgba.frag"
        "src/resources/shaders/bgra.frag"
        "src/resources/shaders/argb.frag"
        "src/resources/shaders/abgr.frag"
        "src/resources/shaders/rgbx.frag"
        "src/resources/shaders/bgrx.frag"
        "src/resources/shaders/xrgb.frag"
        "src/resources/shaders/xbgr.frag"
        "src/resources/shaders/nv12.frag"
        "src/resources/shaders/nv21.frag"
        "src/resources/shaders/yuv420p10le.frag"
        "src/resources/shaders/yuyv422.frag"
        "src/resources/shaders/gbr.frag"
        "src/resources/shaders/ass.frag"
)

target_compile_options(${PROJECT_NAME}
        PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /Zc:preprocessor /Zc:__cplusplus /wd5054>
        $<$<CXX_COMPILER_ID:MSVC>:/D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR> # VS 17.10: https://github.com/microsoft/STL/issues/4730
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-deprecated-enum-enum-conversion>
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        QHotkey::QHotkey
        Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Widgets
        glog::glog
        fmt::fmt
        ffmpeg::ffmpeg
        probe::probe
        nlohmann_json::nlohmann_json
        ass::ass
        libcap::libcap
        $<$<PLATFORM_ID:Windows>:Winmm>
        $<$<PLATFORM_ID:Linux>:Qt6::DBus>
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        src/
        src/common
        src/widgets
        src/preview
        src/snipping
        src/player
        src/setting
        ${PROJECT_BINARY_DIR} # Generated header files
)

# #######################################################################################################################
# Install
# #######################################################################################################################
message(STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

# Binaries
if (WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}")
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
elseif (UNIX AND NOT APPLE)
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
    # install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/capturer/capturer /usr/local/bin/capturer)")
    # install(IMPORTED_RUNTIME_ARTIFACTS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets DESTINATION "${CMAKE_INSTALL_PREFIX}/capturer")
    install(FILES ${QM_FILES} DESTINATION "/usr/local/etc/capturer/translations")
    install(FILES "${PROJECT_SOURCE_DIR}/capturer.desktop" DESTINATION "/usr/share/applications")
    install(FILES "${PROJECT_SOURCE_DIR}/src/resources/icons/capturer.png" DESTINATION "/usr/share/pixmaps")
    install(FILES "${PROJECT_SOURCE_DIR}/src/resources/icons/capturer.png" DESTINATION "/usr/share/icons/hicolor/128x128/apps")
endif ()

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/cmake/uninstall.cmake")

# #######################################################################################################################
# Create DEB
# #######################################################################################################################
if (UNIX)
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

    if (EXISTS "/etc/lsb-release")
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

        if (${HOST_SYSTEM_NAME} MATCHES "Ubuntu")
            string(CONCAT HOST_SYSTEM_VER "ubuntu_" ${HOST_SYSTEM_VER})

            # the ubuntu need to modify the architecture name
            if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
                set(CMAKE_HOST_SYSTEM_PROCESSOR "amd64")
            endif ()
        endif ()

        set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${HOST_SYSTEM_VER}-${CMAKE_HOST_SYSTEM_PROCESSOR})
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${CMAKE_HOST_SYSTEM_PROCESSOR})
    else ()
        set(HOST_SYSTEM_VER "Unknown")
    endif ()

    include(CPack)
endif ()
