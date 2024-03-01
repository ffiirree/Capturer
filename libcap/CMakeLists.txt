cmake_minimum_required(VERSION 3.21)

# Windows SDK >= 10.0.22621.0
if (WIN32 AND "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" VERSION_LESS "10.0.22621.0")
    # must be 10.0.22621.0, the behavior of winrt::com_ptr in 10.0.22621.0 has been changed
    message(WARNING "Capturer requires Windows SDK 10.0.22621.0 and above to compile.")
endif()

if(WIN32)
    # >= 6 required by nvenc for encoding d3d11 format
    find_package(FFmpeg 6 REQUIRED)
elseif(UNIX AND NOT APPLE)
    find_package(FFmpeg 4.2 REQUIRED)
    find_package(PulseAudio REQUIRED)
    find_package(Libv4l2 REQUIRED)
endif()

file(GLOB_RECURSE LIBCAP_SOURCES *.cpp)

add_library(libcap ${LIBCAP_SOURCES})
add_library(libcap::libcap ALIAS libcap)

target_compile_options(libcap
    PRIVATE 
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /Zc:preprocessor /Zc:__cplusplus /wd5054>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-deprecated-enum-enum-conversion>
)

target_link_libraries(libcap
    PRIVATE
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
        $<$<PLATFORM_ID:Linux>:Libv4l2::Libv4l2>
        $<$<PLATFORM_ID:Linux>:${PULSEAUDIO_LIBRARY}>
)

target_include_directories(libcap
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

    PRIVATE
        ${PROJECT_SOURCE_DIR}/3rdparty
        ${PROJECT_SOURCE_DIR}/src/common
        ${PULSEAUDIO_INCLUDE_DIR}
        ${PROJECT_BINARY_DIR} # Generated header files
)

set_target_properties(libcap
    PROPERTIES
        PUBLIC_HEADER "${LIBCAP_HEADERS}"

        RUNTIME_OUTPUT_DIRECTORY "bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY "lib/$<0:>"
        ARCHIVE_OUTPUT_DIRECTORY "lib/$<0:>"

        DEBUG_POSTFIX "d"
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(libcap PUBLIC CAPTURER_SHARED PRIVATE CAPTURER_BUILDING)
endif()
