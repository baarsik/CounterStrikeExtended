cmake_minimum_required(VERSION 3.25)
project(CseDecalSave LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
  "${CSE_ROOT}/src/engine" "${CMAKE_CURRENT_BINARY_DIR}/decal_io.inc" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CSE_ROOT}/src/engine/host_cmd.cpp" "${CSE_ROOT}/src/engine/cl_parse.cpp")
add_executable(decal_save save_tests.cpp)
target_compile_features(decal_save PRIVATE cxx_std_17)
target_include_directories(decal_save PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
enable_testing()
add_test(NAME decal-legacy-save-roundtrip COMMAND decal_save)
