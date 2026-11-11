cmake_minimum_required(VERSION 3.25)
project(CseBotLifetime LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls/bot/cs_bot_init.cpp" "${CMAKE_CURRENT_BINARY_DIR}/install.inc"
  COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls/bot/cs_bot_init.cpp")
add_executable(bot_lifecycle lifecycle.cpp)
target_compile_features(bot_lifecycle PRIVATE cxx_std_17)
target_include_directories(bot_lifecycle PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
enable_testing()
add_test(NAME bot-derived-lifetime COMMAND bot_lifecycle)
