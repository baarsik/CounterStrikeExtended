cmake_minimum_required(VERSION 3.25)
include("${CMAKE_CURRENT_LIST_DIR}/../../RuntimeTargets.cmake")
set(archive "${CSE_SDK_DIR}/external/SDL3-source/SDL3-3.4.20.tar.gz")
file(SHA256 "${archive}" archive_hash)
if(NOT archive_hash STREQUAL "f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f")
  message(FATAL_ERROR "SDL3 source archive hash mismatch")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${archive}")
set(source_root "${CMAKE_CURRENT_BINARY_DIR}/source")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${source_root}")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(SDL_SHARED ON)
set(SDL_STATIC OFF)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_EXAMPLES OFF)
set(SDL_INSTALL OFF)
set(SDL_FRAMEWORK OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_subdirectory("${source_root}/SDL3-3.4.20" upstream EXCLUDE_FROM_ALL)
set_target_properties(SDL3-shared PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>/SDL3"
  PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/SDL3"
  COMPILE_PDB_NAME SDL3-compile
  COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/SDL3")
