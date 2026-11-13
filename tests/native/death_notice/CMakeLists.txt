cmake_minimum_required(VERSION 3.25)
project(CseDeathNotice LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_DEATH_NOTICE_SANITIZERS "Instrument the death notice fixture" OFF)
set(generated notices.inc bodies.inc source-manifest.json)
list(TRANSFORM generated PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
add_custom_command(OUTPUT ${generated}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_sources.py"
        "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS extract_sources.py
        "${CSE_ROOT}/src/cstrike/cl_dll/death.cpp"
        "${CSE_ROOT}/src/cstrike/cl_dll/cl_util.h"
    VERBATIM)
add_executable(death_notice death_notice_tests.cpp ${generated})
target_compile_features(death_notice PRIVATE cxx_std_17)
target_include_directories(death_notice PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(death_notice PRIVATE /W4 /WX)
else()
    target_compile_options(death_notice PRIVATE -Wall -Wextra -Werror)
    if(CSE_DEATH_NOTICE_SANITIZERS)
        target_compile_options(death_notice PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(death_notice PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
foreach(scenario IN ITEMS font-13 font-16 font-21 font-28 font-33 font-70 independent-heights
        taller-sprite no-headshot suicide non-player skull-fallback rows lifecycle gates spectator)
    add_test(NAME death-notice-${scenario} COMMAND death_notice ${scenario})
endforeach()
