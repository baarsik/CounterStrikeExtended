cmake_minimum_required(VERSION 3.25)
project(CseRendererSelection LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(renderer_selection_tests selection_tests.cpp)
target_include_directories(renderer_selection_tests PRIVATE "${CSE_ROOT}/src/engine/render")
set_target_properties(renderer_selection_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
enable_testing()
add_test(NAME renderer-selection COMMAND renderer_selection_tests)
# The real device/backend lifecycle case lives in ../metal_renderer, keeping this policy suite GPU-independent.
