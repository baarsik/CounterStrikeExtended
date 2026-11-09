cmake_minimum_required(VERSION 3.25)
project(CseCsSmokeContract LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CSE_SMOKE_SOURCE "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/ev_hldm.cpp" CACHE FILEPATH "Complete candidate source")
set(CSE_SMOKE_REFERENCE "${CMAKE_CURRENT_LIST_DIR}/reference_ev_hldm.cpp.txt" CACHE FILEPATH "Frozen old source")
set(queue_source "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/hud_redraw.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CSE_SMOKE_SOURCE}" "${CSE_SMOKE_REFERENCE}" "${queue_source}" extract.py)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract.py"
    --source "${CSE_SMOKE_SOURCE}" --reference "${CSE_SMOKE_REFERENCE}" --queue-source "${queue_source}"
    --output "${CMAKE_CURRENT_BINARY_DIR}/bodies" COMMAND_ERROR_IS_FATAL ANY)
add_library(smoke_reference OBJECT reference.cpp)
add_library(smoke_candidate OBJECT candidate.cpp)
add_executable(smoke_cases cases.cpp queue.cpp $<TARGET_OBJECTS:smoke_reference> $<TARGET_OBJECTS:smoke_candidate>)
foreach(target smoke_reference smoke_candidate smoke_cases)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/bodies"
        "${CSE_ROOT}/GoldSrc-SDK/cstrike/engine" "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls"
        "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll" "${CSE_ROOT}/GoldSrc-SDK/cstrike/common"
        "${CSE_ROOT}/GoldSrc-SDK/cstrike/pm_shared" "${CSE_ROOT}/GoldSrc-SDK/cstrike/public"
        "${CSE_ROOT}/GoldSrc-SDK/dlls" "${CSE_ROOT}/GoldSrc-SDK/common"
        "${CSE_ROOT}/GoldSrc-SDK/pm_shared" "${CSE_ROOT}/GoldSrc-SDK/public"
        "${CSE_ROOT}/GoldSrc-SDK/engine" "${CSE_ROOT}/GoldSrc-SDK/game_shared")
    target_compile_definitions(${target} PRIVATE CLIENT_DLL CLIENT_WEAPONS HL_DLL)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE WIN32 _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc /fp:precise)
    else()
        target_compile_definitions(${target} PRIVATE POSIX OSX)
        target_compile_options(${target} PRIVATE -Wall -Wextra -fexceptions -fno-fast-math -ffp-contract=off
            -include "${CSE_ROOT}/GoldSrc-SDK/public/cse_posix_crt.h"
            # Exact retained Apple owning-recipe warning policies; still emit
            # legacy header/body diagnostics, never suppress model diagnostics.
            -Wno-error=unknown-pragmas -Wno-error=deprecated-copy-with-user-provided-copy
            -Wno-error=writable-strings)
        if(NOT target STREQUAL "smoke_reference")
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endforeach()
# The old reference emits its actual Clang uninitialized warning; it is never
# run with unsupported inputs. No production policy or warning suppression.
option(CSE_SMOKE_SANITIZERS "Instrument complete-body CPU fixture" OFF)
if(CSE_SMOKE_SANITIZERS)
    foreach(target smoke_reference smoke_candidate smoke_cases)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
    endforeach()
    target_link_options(smoke_cases PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
enable_testing()
add_test(NAME actual-valid-bodies COMMAND smoke_cases --valid)
if(APPLE)
    add_test(NAME native-invalid-no-effects COMMAND smoke_cases --invalid)
endif()
