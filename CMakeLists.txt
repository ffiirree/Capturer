cmake_minimum_required(VERSION 3.15)

project(Capturer VERSION 1.2.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Gui LinguistTools REQUIRED)

if(WIN32)
    # GUI/console
   set(CMAKE_WIN32_EXECUTABLE ON)
elseif(UNIX AND NOT APPLE)
    find_package(Qt5 COMPONENTS X11Extras REQUIRED)
endif()

set(QHOTKEY_INSTALL OFF CACHE BOOL "Disable QHotkey install rule" FORCE)
add_subdirectory(3rdparty/QHotkey)

########################################################################################################################
# Create the executable
########################################################################################################################
configure_file(src/version.h.in ${PROJECT_BINARY_DIR}/version.h @ONLY)

file(GLOB_RECURSE SOURCES src/*.cpp)

set(TS_FILES languages/capturer_en_US.ts languages/capturer_zh_CN.ts)
qt5_create_translation(QM_FILES ${SOURCES} ${TS_FILES})

add_executable(${PROJECT_NAME} ${SOURCES} src/capturer.qrc ${QM_FILES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        QHotkey::QHotkey Qt5::Widgets Qt5::Gui
        $<$<PLATFORM_ID:Windows>:dwmapi>
        $<$<PLATFORM_ID:Windows>:strmiids>
        $<$<PLATFORM_ID:Linux>:Qt5::X11Extras>
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        3rdparty
        src/core
        src/widgets
        src/menu
        src/snip
        src/video
        src/gif
        src/setting
        ${PROJECT_BINARY_DIR} # Generated header files
)

########################################################################################################################
# Install
########################################################################################################################

message(STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

# Binaries
if(WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}")
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
else(UNIX AND NOT APPLE)
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
    install(FILES ${QM_FILES} DESTINATION "/etc/capturer/translations")
endif()



########################################################################################################################
# Create DEB
########################################################################################################################

# Tell CPack to generate a .deb package
set(CPACK_GENERATOR "DEB")

# Set a Package Version
set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "ffmpeg")

# Set a Package Maintainer.
# This is required
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Zhang Liangqi(zhliangqi@gmail.com)")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Capturer")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Screenshots and recording screens.")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

include(CPack)