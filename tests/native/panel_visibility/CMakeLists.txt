cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CsePanelVisibility LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS extract_visibility.py
    "${CSE_ROOT}/src/vgui2/controls/Panel.cpp"
    "${CSE_ROOT}/src/vgui2/controls/HTML.cpp"
    "${CSE_ROOT}/src/vgui2/controls/HTML.h"
    "${CSE_ROOT}/src/cstrike/vgui2/controls/HTML.cpp"
    "${CSE_ROOT}/src/cstrike/public/vgui_controls/HTML.h"
    "${CSE_ROOT}/src/vgui2/src/VPanel.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_visibility.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_visibility.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}" cs COMMAND_ERROR_IS_FATAL ANY)
add_executable(panel_visibility_tests visibility_tests.cpp)
add_executable(panel_visibility_cs_tests visibility_tests.cpp)
target_compile_definitions(panel_visibility_cs_tests PRIVATE CSE_TEST_CS_HTML=1)
option(CSE_VISIBILITY_SANITIZERS "Instrument the extracted owning methods" OFF)
foreach(target panel_visibility_tests panel_visibility_cs_tests)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc /fp:precise
            "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-fast-math
            -ffp-contract=off "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
        if(CSE_VISIBILITY_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()
enable_testing()
add_test(NAME effective-browser-visibility COMMAND panel_visibility_tests)
add_test(NAME effective-cs-browser-visibility COMMAND panel_visibility_cs_tests)
