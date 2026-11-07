cmake_minimum_required(VERSION 4.2)
cmake_policy(SET CMP0091 NEW)
project(CseMpg123 LANGUAGES C)

if(NOT DEFINED CSE_SDK_DIR)
  get_filename_component(CSE_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../GoldSrc-SDK" ABSOLUTE)
endif()
set(archive "${CSE_SDK_DIR}/external/mpg123/mpg123-1.33.7.tar.bz2")
file(SHA256 "${archive}" archive_hash)
if(NOT archive_hash STREQUAL "31d0e35a4ca567ec9b5ebda6c3062bb4435d6d3eacd6ef0d95cadd7854dc03ee")
  message(FATAL_ERROR "mpg123 source archive hash mismatch")
endif()
set(source_root "${CMAKE_CURRENT_BINARY_DIR}/source")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${source_root}")
set(CSE_MPG123_SOURCE "${source_root}/mpg123-1.33.7" CACHE INTERNAL "Pinned mpg123 source")

# Scope upstream options and include settings to this directory and its children.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS ON)
set(BUILD_LIBOUT123 OFF)
set(BUILD_PROGRAMS OFF)
set(PORTABLE_API OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
# The runtime intentionally clears CMake defaults; upstream DLL exports require WIN32.
add_compile_definitions("$<$<BOOL:${WIN32}>:WIN32>")
add_subdirectory("${CSE_MPG123_SOURCE}/ports/cmake" upstream EXCLUDE_FROM_ALL)
set_target_properties(libmpg123 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>/mpg123"
  PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/mpg123"
  COMPILE_PDB_NAME mpg123-compile
  COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/mpg123")
target_compile_definitions(libmpg123 INTERFACE "$<$<BOOL:${WIN32}>:LINK_MPG123_DLL>")
target_include_directories(libmpg123 INTERFACE "$<BUILD_INTERFACE:${CSE_MPG123_SOURCE}/src/include>")

if(MSVC)
  foreach(dependency libmpg123 compat)
    target_compile_options(${dependency} PRIVATE /Zi
      "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
    target_compile_definitions(${dependency} PRIVATE "$<$<CONFIG:Release>:NDEBUG>")
  endforeach()
  target_link_options(libmpg123 PRIVATE /DEBUG)
endif()

# A root-level producer also supports CMake/VS builds of this wrapper on its own.
add_custom_target(cse_mpg123 ALL DEPENDS libmpg123)
