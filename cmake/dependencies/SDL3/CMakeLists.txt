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
if(APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT CMAKE_OBJCXX_COMPILER_LOADED)
  if(NOT DEFINED CMAKE_USER_MAKE_RULES_OVERRIDE_OBJCXX)
    set(CMAKE_USER_MAKE_RULES_OVERRIDE_OBJCXX "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
  endif()
  enable_language(OBJCXX)
endif()
add_subdirectory("${source_root}/SDL3-3.4.20" upstream EXCLUDE_FROM_ALL)
# The runtime clears CMake's configuration defaults for its CMake-owned code.
# Supply the dependency policy privately, without changing assertions, CRT or FP.
if(MSVC)
  target_compile_options(SDL3-shared PRIVATE
    "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>"
    "$<$<OR:$<CONFIG:Debug>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/Zi>")
  target_link_options(SDL3-shared PRIVATE
    "$<$<OR:$<CONFIG:Debug>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>"
    "$<$<CONFIG:Debug>:/ILK:${CMAKE_CURRENT_BINARY_DIR}/SDL3-$<CONFIG>.ilk>"
    "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
elseif(CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
  target_compile_options(SDL3-shared PRIVATE
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>"
    "$<$<OR:$<CONFIG:Debug>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:-g>")
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND (WIN32 OR APPLE))
  # SDL3.4.20+CSE mouse-query V1: an owned additive export, with upstream bytes/features retained.
  target_sources(SDL3-shared PRIVATE "${CSE_SDK_DIR}/common/sdl3_native_mouse_bridge.c")
endif()
if(APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(cse_cocoa_mouse "${CSE_SDK_DIR}/common/sdl3_native_cocoa_mouse.mm")
  target_sources(SDL3-shared PRIVATE "${cse_cocoa_mouse}")
  # No C SDL_internal PCH/dynapi aliases enter this owned ObjC++ boundary.
  set_source_files_properties("${cse_cocoa_mouse}" TARGET_DIRECTORY SDL3-shared PROPERTIES
    LANGUAGE OBJCXX SKIP_PRECOMPILE_HEADERS TRUE)
  set_property(SOURCE "${cse_cocoa_mouse}" TARGET_DIRECTORY SDL3-shared APPEND PROPERTY COMPILE_OPTIONS
    -fobjc-arc -fno-exceptions -funwind-tables -ffp-model=precise -ffp-contract=off
    -Wno-overriding-option -fvisibility=hidden)
  set_target_properties(SDL3-shared PROPERTIES OBJCXX_STANDARD 14 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
endif()
set_target_properties(SDL3-shared PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>/SDL3"
  PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/SDL3"
  COMPILE_PDB_NAME SDL3-compile
  COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/SDL3")
