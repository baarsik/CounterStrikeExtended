cmake_minimum_required(VERSION 3.25)
project(CseFileSystemPlatform LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_boundaries.py"
    "${CSE_ROOT}/GoldSrc-SDK/filesystem/FileSystem_Stdio.cpp" "${CMAKE_CURRENT_BINARY_DIR}/actual_low_level.h"
    COMMAND_ERROR_IS_FATAL ANY)
if(WIN32)
    add_executable(filesystem_reference reference.cpp)
    target_link_libraries(filesystem_reference PRIVATE bcrypt)
    target_include_directories(filesystem_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_options(filesystem_reference PRIVATE /W4 /WX /EHsc)
    target_compile_definitions(filesystem_reference PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE NOMINMAX)
endif()
if(APPLE)
    include(CTest)
    option(CSE_FS_PLATFORM_SANITIZERS "Instrument actual module and platform fixtures" OFF)
    set(CSE_FILESYSTEM_SANITIZERS "${CSE_FS_PLATFORM_SANITIZERS}" CACHE BOOL "" FORCE)
    add_subdirectory("${CSE_ROOT}/tests/native/filesystem" actual_module)
    add_executable(filesystem_platform_tests native_tests.cpp)
    target_include_directories(filesystem_platform_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/filesystem")
    target_compile_options(filesystem_platform_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wsign-conversion)
    target_link_libraries(filesystem_platform_tests PRIVATE filesystem_stdio)
    if(CSE_FS_PLATFORM_SANITIZERS)
        target_compile_options(filesystem_platform_tests PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(filesystem_platform_tests PRIVATE -fsanitize=address,undefined)
    endif()
    foreach(scenario positions sizes times format warning)
        add_test(NAME filesystem-platform-${scenario} COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_native.py" "$<TARGET_FILE:filesystem_platform_tests>"
            "${scenario}" "${CMAKE_CURRENT_BINARY_DIR}/platform-data/${scenario}")
        set_tests_properties(filesystem-platform-${scenario} PROPERTIES TIMEOUT 20)
    endforeach()
endif()
if(APPLE)
    set(CSE_FS_PLATFORM_DATABASE "" CACHE FILEPATH "Actual owning filesystem compile_commands.json")
    if(NOT EXISTS "${CSE_FS_PLATFORM_DATABASE}")
        message(FATAL_ERROR "The actual owning filesystem compiler database is required")
    endif()
    set(san_args)
    if(CSE_FS_PLATFORM_SANITIZERS)
        set(san_args --sanitizers)
    endif()
    set(clock_object "${CMAKE_CURRENT_BINARY_DIR}/scalar-clock/dbg.cpp.o")
    add_custom_command(OUTPUT "${clock_object}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compile_tus.py"
            --database "${CSE_FS_PLATFORM_DATABASE}" --output "${CMAKE_CURRENT_BINARY_DIR}/scalar-clock"
            --file dbg.cpp --controlled-clock ${san_args}
        DEPENDS "${CSE_ROOT}/GoldSrc-SDK/filesystem/dbg.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/compile_tus.py"
        DEPFILE "${clock_object}.d" VERBATIM)
    set(path_object "${CMAKE_CURRENT_BINARY_DIR}/scalar-path/pathmatch.cpp.o")
    add_custom_command(OUTPUT "${path_object}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compile_tus.py"
            --database "${CSE_FS_PLATFORM_DATABASE}" --output "${CMAKE_CURRENT_BINARY_DIR}/scalar-path"
            --file pathmatch.cpp --source "${CMAKE_CURRENT_SOURCE_DIR}/path_bridge.cpp" ${san_args}
        DEPENDS "${CSE_ROOT}/GoldSrc-SDK/filesystem/pathmatch.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/path_bridge.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/compile_tus.py"
        DEPFILE "${path_object}.d" VERBATIM)
    set_source_files_properties("${clock_object}" "${path_object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    add_executable(filesystem_scalar_tests scalar_tests.cpp "${clock_object}" "${path_object}")
    target_include_directories(filesystem_scalar_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/filesystem")
    target_compile_options(filesystem_scalar_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wsign-conversion)
    if(CSE_FS_PLATFORM_SANITIZERS)
        target_compile_options(filesystem_scalar_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(filesystem_scalar_tests PRIVATE -fsanitize=address,undefined)
    endif()
    foreach(scenario high-clock zero-clock diagnostics)
        add_test(NAME filesystem-platform-${scenario} COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_native.py" "$<TARGET_FILE:filesystem_scalar_tests>"
            "${scenario}" "${CMAKE_CURRENT_BINARY_DIR}/platform-data/${scenario}" --scalar)
    endforeach()
endif()

if(APPLE)
    set(CSE_FS_REFERENCE_LOG "" CACHE FILEPATH "Actual pinned Win32 probe log with UTC fmt rows")
    if(NOT EXISTS "${CSE_FS_REFERENCE_LOG}")
        message(FATAL_ERROR "The actual Win32 formatting log is required")
    endif()
    target_compile_definitions(filesystem_platform_tests PRIVATE CSE_FS_REFERENCE_LOG="${CSE_FS_REFERENCE_LOG}")
    add_executable(filesystem_formatter_failure_tests formatter_failure_tests.cpp)
    target_include_directories(filesystem_formatter_failure_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_options(filesystem_formatter_failure_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wsign-conversion)
    if(CSE_FS_PLATFORM_SANITIZERS)
        target_compile_options(filesystem_formatter_failure_tests PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(filesystem_formatter_failure_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME filesystem-platform-formatter-failure COMMAND filesystem_formatter_failure_tests)
endif()
