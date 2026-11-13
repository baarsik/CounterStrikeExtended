cmake_minimum_required(VERSION 3.25)
project(CseEntityApi LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
  "${CSE_ROOT}/src/cstrike/dlls/cbase.cpp" "${CMAKE_CURRENT_BINARY_DIR}/entity_api.inc"
  COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CSE_ROOT}/src/cstrike/dlls/cbase.cpp")
add_executable(entity_api api_tests.cpp)
target_compile_features(entity_api PRIVATE cxx_std_17)
target_include_directories(entity_api PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/src/engine")
enable_testing()
add_test(NAME entity-api-initialization COMMAND entity_api)
