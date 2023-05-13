#
# This module defines the following variables:
#
# FFMPEG_FOUND          - All required components and the core library were found
# FFMPEG_INCLUDE_DIRS   - Combined list of all components include dirs
# FFMPEG_LIBRARIES      - Combined list of all components libraries
# FFMPEG_VERSION        - Version defined in libavutil/ffversion.h
#
# ffmpeg::ffmpeg        - FFmpeg target
#
# For each requested component the following variables are defined:
#
# FFMPEG_<component>_FOUND          - The component was found
# FFMPEG_<component>_INCLUDE_DIRS   - The components include dirs
# FFMPEG_<component>_LIBRARIES      - The components libraries
# FFMPEG_<component>_VERSION        - The components version
# FFMPEG_<component>_VERSION_MAJOR  - The components major version
# FFMPEG_<component>_VERSION_MINOR  - The components minor version
# FFMPEG_<component>_VERSION_MICRO  - The components micro version
#
# ffmpeg::<component>               - The component target
#
# <component> is the uppercase name of the component
# 
# Usage:
#   find_package(FFmpeg REQUIRED)
#   find_package(FFmpeg 5.1.2 COMPONENTS avutil avcodec avformat avdevice avfilter REQUIRED)


find_package(PkgConfig QUIET)

# find the root dir for specified version
function(find_ffmpeg_root_by_version version)
    set(FFMPEG_FIND_PATHS $ENV{PATH} $ENV{FFMPEG_PATH} $ENV{FFMPEG_ROOT} /opt /usr /sw)

    set(_found_version)
    set(_found_root_dir)

    set(INDEX 1)

    foreach(path ${FFMPEG_FIND_PATHS})
        unset(FFMPEG_${INDEX}_HEADER_DIR CACHE)
        find_path(
            FFMPEG_${INDEX}_HEADER_DIR
            NAMES "libavutil/ffversion.h"
            PATHS ${path} ${path}/include/${CMAKE_LIBRARY_ARCHITECTURE}
            PATH_SUFFIXES ffmpeg include
            NO_DEFAULT_PATH
        )
        mark_as_advanced(FFMPEG_${INDEX}_HEADER_DIR)

        if(NOT "${FFMPEG_${INDEX}_HEADER_DIR}" STREQUAL "FFMPEG_${INDEX}_HEADER_DIR-NOTFOUND")
            # parse ffmpeg version
            set(_ffmpeg_version_header "${FFMPEG_${INDEX}_HEADER_DIR}/libavutil/ffversion.h")

            if(EXISTS "${_ffmpeg_version_header}")
                file(STRINGS "${_ffmpeg_version_header}" __ffmpeg_version_line REGEX "FFMPEG_VERSION")

                string(REGEX REPLACE ".*FFMPEG_VERSION[ \t]+\"[n]?([0-9\\.]*).*\"" "\\1" _ffmpeg_version "${__ffmpeg_version_line}")

                # if do not specified, return the fist one
                if("${version}" STREQUAL "")
                    set(_found_version ${_ffmpeg_version})
                    set(_found_root_dir ${path})
                    break()
                elseif(${_ffmpeg_version} VERSION_GREATER_EQUAL "${version}") # otherwise, the minimum one of suitable versions
                    if((NOT _found_version) OR(${_ffmpeg_version} VERSION_LESS "${_found_version}"))
                        set(_found_version ${_ffmpeg_version})
                        set(_found_root_dir ${path})
                    endif()
                endif()

            else()
                message(FATAL_ERROR "Can not find the version file: 'libavutil/ffversion.h'.")
            endif()

            unset(_ffmpeg_version_header)

            math(EXPR INDEX ${INDEX}+1)
        endif()
    endforeach()

    set(FFMPEG_VERSION ${_found_version} PARENT_SCOPE)
    set(FFMPEG_ROOT_DIR ${_found_root_dir} PARENT_SCOPE)
endfunction()

# find a ffmpeg component
function(find_ffmpeg_library root_dir component header)
    string(TOUPPER "${component}" component_u)

    set(FFMPEG_${component_u}_FOUND FALSE PARENT_SCOPE)
    set(FFmpeg_${component}_FOUND FALSE PARENT_SCOPE)

    unset(FFMPEG_${component}_INCLUDE_DIR CACHE)
    unset(FFMPEG_${component}_LIBRARY CACHE)

    find_path(
        FFMPEG_${component}_INCLUDE_DIR
        NAMES "lib${component}/${header}" "lib${component}/version.h"
        PATHS ${root_dir} ${root_dir}/include/${CMAKE_LIBRARY_ARCHITECTURE}
        PATH_SUFFIXES ffmpeg libav include
        NO_DEFAULT_PATH
    )

    find_library(
        FFMPEG_${component}_LIBRARY
        NAMES "${component}" "lib${component}"
        PATHS ${root_dir} ${root_dir}/lib/${CMAKE_LIBRARY_ARCHITECTURE}
        PATH_SUFFIXES lib lib64 bin bin64
        NO_DEFAULT_PATH
    )

    set(FFMPEG_${component_u}_INCLUDE_DIRS ${FFMPEG_${component}_INCLUDE_DIR} PARENT_SCOPE)
    set(FFMPEG_${component_u}_LIBRARIES ${FFMPEG_${component}_LIBRARY} PARENT_SCOPE)

    mark_as_advanced(FFMPEG_${component}_INCLUDE_DIR FFMPEG_${component}_LIBRARY)

    if(FFMPEG_${component}_INCLUDE_DIR AND FFMPEG_${component}_LIBRARY)
        set(FFMPEG_${component_u}_FOUND TRUE PARENT_SCOPE)
        set(FFmpeg_${component}_FOUND TRUE PARENT_SCOPE)

        # headers
        list(APPEND FFMPEG_INCLUDE_DIRS ${FFMPEG_${component}_INCLUDE_DIR})
        list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
        set(FFMPEG_INCLUDE_DIRS "${FFMPEG_INCLUDE_DIRS}" PARENT_SCOPE)

        # libs
        list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${component}_LIBRARY})
        list(REMOVE_DUPLICATES FFMPEG_LIBRARIES)
        set(FFMPEG_LIBRARIES "${FFMPEG_LIBRARIES}" PARENT_SCOPE)

        # version
        set(FFMPEG_${component_u}_VERSION "unknown" PARENT_SCOPE)

        set(_major_version_header "${FFMPEG_${component}_INCLUDE_DIR}/lib${component}/version_major.h")
        set(_version_header "${FFMPEG_${component}_INCLUDE_DIR}/lib${component}/version.h")

        if(EXISTS "${_version_header}")
            # major version
            if(EXISTS ${_major_version_header})
                file(STRINGS "${_major_version_header}" _major_version REGEX "^.*VERSION_MAJOR[ \t]+[0-9]+[ \t]*$")
            endif()

            # other
            file(STRINGS "${_version_header}" _version REGEX "^.*VERSION_(MAJOR|MINOR|MICRO)[ \t]+[0-9]+[ \t]*$")

            list(APPEND _version ${_major_version})

            string(REGEX REPLACE ".*VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" _major "${_version}")
            string(REGEX REPLACE ".*VERSION_MINOR[ \t]+([0-9]+).*" "\\1" _minor "${_version}")
            string(REGEX REPLACE ".*VERSION_MICRO[ \t]+([0-9]+).*" "\\1" _micro "${_version}")

            set(FFMPEG_${component_u}_VERSION_MAJOR "${_major}" PARENT_SCOPE)
            set(FFMPEG_${component_u}_VERSION_MINOR "${_minor}" PARENT_SCOPE)
            set(FFMPEG_${component_u}_VERSION_MICRO "${_micro}" PARENT_SCOPE)

            set(FFMPEG_${component_u}_VERSION "${_major}.${_minor}.${_micro}" PARENT_SCOPE)
        else()
            message(STATUS "Failed parsing FFmpeg ${component} version")
        endif()
    endif()
endfunction()

# start finding
if(NOT FFmpeg_FIND_COMPONENTS)
    list(APPEND FFmpeg_FIND_COMPONENTS avutil avcodec avdevice avfilter avformat swresample swscale postproc)
endif()

set(FFMPEG_VERSION)
find_ffmpeg_root_by_version("${FFmpeg_FIND_VERSION}")

if((NOT FFMPEG_VERSION) OR(NOT FFMPEG_ROOT_DIR))
    message(FATAL_ERROR "Can not find the suitable version.")
endif()

set(FFMPEG_INCLUDE_DIRS)
set(FFMPEG_LIBRARIES)

list(REMOVE_DUPLICATES FFmpeg_FIND_COMPONENTS)

foreach(component ${FFmpeg_FIND_COMPONENTS})
    if(component STREQUAL "avcodec")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "avcodec.h")
    elseif(component STREQUAL "avdevice")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "avdevice.h")
    elseif(component STREQUAL "avfilter")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "avfilter.h")
    elseif(component STREQUAL "avformat")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "avformat.h")
    elseif(component STREQUAL "avresample")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "avresample.h")
    elseif(component STREQUAL "avutil")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "avutil.h")
    elseif(component STREQUAL "postproc")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "postprocess.h")
    elseif(component STREQUAL "swresample")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "swresample.h")
    elseif(component STREQUAL "swscale")
        find_ffmpeg_library(${FFMPEG_ROOT_DIR} "${component}" "swscale.h")
    else()
        message(FATAL_ERROR "Unknown FFmpeg component requested: ${component}")
    endif()
endforeach()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    FFmpeg
    FOUND_VAR FFMPEG_FOUND
    REQUIRED_VARS FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES
    VERSION_VAR FFMPEG_VERSION
    HANDLE_COMPONENTS
)

if(FFMPEG_FOUND)
    foreach(component ${FFmpeg_FIND_COMPONENTS})
        if(NOT TARGET ffmpeg::${component})
            string(TOUPPER ${component} component_u)

            if(FFMPEG_${component_u}_FOUND)
                if(IS_ABSOLUTE "${FFMPEG_${component_u}_LIBRARIES}")
                    add_library(ffmpeg::${component} UNKNOWN IMPORTED)
                    set_target_properties(ffmpeg::${component} PROPERTIES IMPORTED_LOCATION "${FFMPEG_${component_u}_LIBRARIES}")
                else()
                    add_library(ffmpeg::${component} INTERFACE IMPORTED)
                    set_target_properties(ffmpeg::${component} PROPERTIES IMPORTED_LIBNAME "${FFMPEG_${component_u}_LIBRARIES}")
                endif()

                set_target_properties(ffmpeg::${component} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_${component_u}_INCLUDE_DIRS}")
            endif()
        endif()
    endforeach()

    if(NOT TARGET ffmpeg::ffmpeg)
        if(FFMPEG_FOUND)
            add_library(ffmpeg::ffmpeg INTERFACE IMPORTED)
            set_property(TARGET ffmpeg::ffmpeg PROPERTY INTERFACE_LINK_LIBRARIES ${FFMPEG_LIBRARIES})
            set_property(TARGET ffmpeg::ffmpeg PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_DIRS})
        endif()
    endif()
endif()
