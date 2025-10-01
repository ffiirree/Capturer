cmake_minimum_required(VERSION 3.27)

project(capturer VERSION 1.7.3 LANGUAGES C CXX)

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
set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 6.8 COMPONENTS Core Gui Widgets PdfWidgets LinguistTools ShaderTools REQUIRED)
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

qt_add_shaders(${PROJECT_NAME} "capturer_shaders"
        PREFIX
        "/"
        FILES
        "src/resources/shaders/vertex.vert"
        "src/resources/shaders/gray.frag"
        "src/resources/shaders/yuv.frag"
        "src/resources/shaders/yuva.frag"
        "src/resources/shaders/yuv_p10.frag"
        "src/resources/shaders/yuv_p10_bt2020_pq.frag"
        "src/resources/shaders/yuyv.frag"
        "src/resources/shaders/uyvy.frag"
        "src/resources/shaders/nv12.frag"
        "src/resources/shaders/nv21.frag"
        "src/resources/shaders/nv12_bt2020_pq.frag"
        "src/resources/shaders/rgba.frag"
        "src/resources/shaders/bgra.frag"
        "src/resources/shaders/argb.frag"
        "src/resources/shaders/abgr.frag"
        "src/resources/shaders/gbrp.frag"
        "src/resources/shaders/pal8.frag"
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
        Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Widgets Qt6::PdfWidgets
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
# Package
# #######################################################################################################################

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

set(CPACK_PACKAGE_VENDOR "ffiirree")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ffiirree/Capturer")
set(CPACK_PACKAGE_DESCRIPTION "Capturer - Screenshot & Screen Recording")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

if (WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION . COMPONENT Applications)
    install(TARGETS ${PROJECT_NAME}
        COMPONENT Runtime
        RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll" "Qt.*.dll"
        DIRECTORIES $ENV{PATH}
        DESTINATION .
    )
    install(FILES ${QM_FILES} DESTINATION "translations")

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION . COMPONENT Runtime)

    qt_generate_deploy_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT qt_deploy_script
        CONTENT "
            qt_deploy_runtime_dependencies(
                EXECUTABLE \"$<TARGET_FILE:${PROJECT_NAME}>\"
                BIN_DIR .
                PLUGINS_DIR .
                NO_COMPILER_RUNTIME
                DEPLOY_TOOL_OPTIONS \"--no-opengl-sw\" \"--no-system-d3d-compiler\"
            )"
    )
    install(SCRIPT ${qt_deploy_script})

    set(CPACK_PACKAGE_INSTALL_DIRECTORY Capturer)
    set(CPACK_PACKAGE_CHECKSUM SHA512)

    # NSIS
    set(CPACK_GENERATOR NSIS)

    set(CPACK_NSIS_PACKAGE_NAME "Capturer ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${PROJECT_NAME}.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    # The display name string that appears in the Windows Apps & features in Control Panel
    set(CPACK_NSIS_DISPLAY_NAME Capturer)

    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/src/resources/icons/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/src/resources/icons/icon.ico")

    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_MODIFY_PATH ON)

    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${PROJECT_NAME}.exe")
    set(CPACK_PACKAGE_EXECUTABLES ${PROJECT_NAME} "Capturer")
    set(CPACK_CREATE_DESKTOP_LINKS ${PROJECT_NAME})

    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Windows-${CMAKE_HOST_SYSTEM_PROCESSOR}")

    include(CPack)
elseif (UNIX)
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
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/cmake/uninstall.cmake")

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
