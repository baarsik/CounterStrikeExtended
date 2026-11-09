cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CsePanelVisibility LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS extract_visibility.py
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/controls/Panel.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/controls/HTML.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/VPanel.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_visibility.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(panel_visibility_tests visibility_tests.cpp)
target_compile_features(panel_visibility_tests PRIVATE cxx_std_17)
target_include_directories(panel_visibility_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
option(CSE_VISIBILITY_SANITIZERS "Instrument the extracted owning methods" OFF)
if(MSVC)
    target_compile_options(panel_visibility_tests PRIVATE /W4 /WX /EHsc /fp:precise
        "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
    target_compile_options(panel_visibility_tests PRIVATE -Wall -Wextra -Werror -fno-fast-math
        -ffp-contract=off "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    if(CSE_VISIBILITY_SANITIZERS)
        target_compile_options(panel_visibility_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(panel_visibility_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
add_test(NAME effective-browser-visibility COMMAND panel_visibility_tests)
