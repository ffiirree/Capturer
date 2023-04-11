set(MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

message("Uninstall the project...")

if(NOT EXISTS ${MANIFEST})
    message(FATAL_ERROR "Cannot find install manifest: '${MANIFEST}'")
endif()

file(STRINGS ${MANIFEST} files)
foreach(file ${files})
    if(EXISTS ${file})
        message(STATUS "Uninstalling: ${file}")

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E remove "${file}"
                OUTPUT_VARIABLE stdout
                RESULT_VARIABLE result
        )
        
        if(NOT "${result}" EQUAL 0)
            message(FATAL_ERROR "Failed to remove file: ${file}.")
        endif()
    else()
        MESSAGE(STATUS "File '${file}' does not exist.")
    endif()
endforeach(file)