cmake_minimum_required(VERSION 3.25)
project(CseVgui1MetricMatrix LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory("${CSE_ROOT}/tests/native/vgui1_matched_program" "${CMAKE_CURRENT_BINARY_DIR}/control")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_metrics.py"
    "${CSE_ROOT}/GoldSrc-SDK/utils/vgui/lib-src/vgui/posix/native/VGui1LegacyRaster.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/extracted" COMMAND_ERROR_IS_FATAL ANY)
add_executable(metric_matrix matrix.cpp)
target_include_directories(metric_matrix PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/control/extracted"
    "${CMAKE_CURRENT_BINARY_DIR}/extracted")
target_compile_definitions(metric_matrix PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_compile_options(metric_matrix PRIVATE /W4 /WX /EHsc /fp:strict)
target_link_libraries(metric_matrix PRIVATE matched_legacy_raster gdi32 user32 bcrypt)
