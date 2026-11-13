cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseCustomDecal LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CSE_CUSTOM_SDK "${CSE_ROOT}/src" CACHE PATH "Actual SDK headers and complete source bodies")
set(CSE_CUSTOM_BODY_SDK "${CSE_CUSTOM_SDK}" CACHE PATH "Frozen or current complete source bodies")
set(CSE_CUSTOM_SANITIZERS OFF CACHE BOOL "Instrument owned fake-service fixtures")
set(imported "${CMAKE_CURRENT_BINARY_DIR}/imported")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_custom_decal.py"
    --sdk "${CSE_CUSTOM_BODY_SDK}" --output "${imported}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/make_fixture.py"
    "${CMAKE_CURRENT_BINARY_DIR}/authored" COMMAND_ERROR_IS_FATAL ANY)
add_executable(custom_loader_tests loader_tests.cpp)
add_executable(custom_builder_hw pipeline_tests.cpp)
add_executable(custom_builder_sw pipeline_tests.cpp)
add_executable(custom_record_tests parser_tests.cpp)
add_executable(custom_pipeline_hw cache_pipeline_tests.cpp "${CSE_CUSTOM_BODY_SDK}/engine/com_custom.cpp")
add_executable(custom_pipeline_sw cache_pipeline_tests.cpp "${CSE_CUSTOM_BODY_SDK}/engine/com_custom.cpp")
target_compile_definitions(custom_builder_sw PRIVATE SW_RENDERER)
target_compile_definitions(custom_pipeline_sw PRIVATE SW_RENDERER)
foreach(target custom_loader_tests custom_builder_hw custom_builder_sw custom_record_tests custom_pipeline_hw custom_pipeline_sw)
    if(target STREQUAL "custom_record_tests")
        target_compile_features(${target} PRIVATE cxx_std_14)
    else()
        target_compile_features(${target} PRIVATE cxx_std_17)
    endif()
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS NO)
    target_include_directories(${target} PRIVATE "${imported}" "${CSE_CUSTOM_SDK}/engine"
        "${CSE_CUSTOM_SDK}/common" "${CSE_CUSTOM_SDK}/public" "${CSE_CUSTOM_SDK}/cl_dll"
        "${CSE_CUSTOM_SDK}/pm_shared" "${CSE_CUSTOM_SDK}/utils/vgui/include"
        "${CSE_CUSTOM_SDK}/external/SDL3/include")
    if(MSVC)
        target_compile_definitions(${target} PRIVATE WIN32 NOMINMAX _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE /W4 /WX /fp:precise /EHsc
            "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
    else()
        target_compile_definitions(${target} PRIVATE POSIX OSX)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wno-error=unused-parameter
            -Wno-error=writable-strings -Wno-error=sign-compare -fno-exceptions -funwind-tables
            -ffp-model=precise -ffp-contract=off -Wno-overriding-option
            -include "${CSE_CUSTOM_SDK}/public/cse_posix_crt.h"
            "$<$<CONFIG:Release>:-Os>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
        if(CSE_CUSTOM_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()
enable_testing()
add_test(NAME strict-heap-short-read COMMAND custom_loader_tests)
add_test(NAME native-hardware-cache-prefix COMMAND custom_builder_hw)
add_test(NAME native-software-cache-prefix COMMAND custom_builder_sw)
add_test(NAME bounded-WAD-records COMMAND custom_record_tests "${CMAKE_CURRENT_BINARY_DIR}/authored/canonical.wad")
add_test(NAME actual-hardware-customization COMMAND custom_pipeline_hw "${CMAKE_CURRENT_BINARY_DIR}/authored/canonical.wad")
add_test(NAME actual-software-customization COMMAND custom_pipeline_sw "${CMAKE_CURRENT_BINARY_DIR}/authored/canonical.wad")
foreach(renderer hw sw)
    foreach(case padded odd unaligned count0 count17-gradient)
        add_test(NAME "pipeline-${renderer}-${case}" COMMAND custom_pipeline_${renderer}
            "${CMAKE_CURRENT_BINARY_DIR}/authored/${case}.wad")
    endforeach()
    add_test(NAME "ordinary-builder-${renderer}" COMMAND custom_builder_${renderer}
        "${CMAKE_CURRENT_BINARY_DIR}/authored/ordinary.wad" "${CMAKE_CURRENT_BINARY_DIR}/ordinary-${renderer}"
        --ordinary)
    add_test(NAME "large-flags-${renderer}" COMMAND custom_pipeline_${renderer}
        "${CMAKE_CURRENT_BINARY_DIR}/authored/large.wad" --large-flags)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_test(NAME "reject-header-alias-${renderer}" COMMAND custom_pipeline_${renderer}
            "${CMAKE_CURRENT_BINARY_DIR}/authored/overlap.wad" --reject)
        add_test(NAME "logical-palette-${renderer}" COMMAND custom_builder_${renderer}
            --palette-span "${CMAKE_CURRENT_BINARY_DIR}/authored/count0.wad")
    endif()
endforeach()
set_tests_properties(strict-heap-short-read native-hardware-cache-prefix native-software-cache-prefix
    bounded-WAD-records actual-hardware-customization actual-software-customization PROPERTIES TIMEOUT 20)
