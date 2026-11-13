cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CsePortableLauncherEntry LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
if(WIN32)
  message(FATAL_ERROR "This bounded fixture qualifies the POSIX launcher entry")
endif()
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(manifest_sources "${CMAKE_CURRENT_BINARY_DIR}/manifest_sources.cmake")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/manifest_sources.py"
  "${CSE_ROOT}" "${CSE_TARGET_OS}" "${manifest_sources}" "${CSE_TARGET_ARCH}" COMMAND_ERROR_IS_FATAL ANY)
include("${manifest_sources}")
include("${CSE_ROOT}/tests/native/CMakeInventoryDependencies.cmake")
cse_fixture_inventory_dependencies("${CSE_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}/manifest_sources.py"
  "${CSE_ROOT}/tests/native/filesystem/manifest_sources.py")
add_executable(launcher_entry ${CSE_LAUNCHER_SOURCES})
set_target_properties(launcher_entry PROPERTIES CXX_STANDARD "${CSE_LAUNCHER_CXX_STANDARD}"
  CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(launcher_entry PRIVATE "${CSE_ROOT}/src/launcher"
  "${CSE_ROOT}/src/public" "${CSE_ROOT}/src")
target_compile_options(launcher_entry PRIVATE -Wall -Wextra -Wno-unknown-pragmas
  "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
target_link_libraries(launcher_entry PRIVATE ${CMAKE_DL_LIBS})
option(CSE_LAUNCHER_ENTRY_SANITIZERS "Instrument the actual portable launcher entry" OFF)
if(CSE_LAUNCHER_ENTRY_SANITIZERS)
  target_compile_options(launcher_entry PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(launcher_entry PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME launcher-entry-bounded-arguments COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/entry_tests.py" "$<TARGET_FILE:launcher_entry>")
