include_guard(GLOBAL)

# This module is also the standalone configure/build-time validation script.
set(CSE_SDL3_ROOT "${CSE_SDK_DIR}/external/SDL3")
set(CSE_SDL3_MANIFEST "${CSE_SDL3_ROOT}/SHA256SUMS")
if(NOT EXISTS "${CSE_SDL3_MANIFEST}")
  message(FATAL_ERROR "Missing SDL3 payload manifest: ${CSE_SDL3_MANIFEST}")
endif()
file(SHA256 "${CSE_SDL3_MANIFEST}" manifest_hash)
if(NOT manifest_hash STREQUAL "75af41d550d677be367a481d95f1adc5145a331cc2c08abcf05fdb83908ddc9a")
  message(FATAL_ERROR "SDL3 payload manifest hash mismatch")
endif()
file(STRINGS "${CSE_SDL3_MANIFEST}" payload_hashes)
foreach(entry IN LISTS payload_hashes)
  if(NOT entry MATCHES "^([0-9a-f]+)[|]([a-zA-Z0-9_./-]+)$")
    message(FATAL_ERROR "Invalid SDL3 payload manifest entry: ${entry}")
  endif()
  set(expected "${CMAKE_MATCH_1}")
  set(relative "${CMAKE_MATCH_2}")
  string(LENGTH "${expected}" hash_length)
  if(NOT hash_length EQUAL 64 OR relative MATCHES "(^/|[.][.])")
    message(FATAL_ERROR "Invalid SDL3 payload manifest entry: ${entry}")
  endif()
  set(path "${CSE_SDL3_ROOT}/${relative}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Missing SDL3 payload: ${path}")
  endif()
  file(SHA256 "${path}" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "SDL3 hash mismatch: ${path}")
  endif()
endforeach()
foreach(required include/SDL3/SDL.h include/SDL3/SDL_version.h lib/x86/SDL3.lib lib/x86/SDL3.dll
    cmake/SDL3Config.cmake cmake/SDL3ConfigVersion.cmake cmake/sdlcpu.cmake LICENSE.txt README.md INSTALL.md)
  if(NOT EXISTS "${CSE_SDL3_ROOT}/${required}")
    message(FATAL_ERROR "Missing SDL3 payload: ${CSE_SDL3_ROOT}/${required}")
  endif()
endforeach()
if(CMAKE_SCRIPT_MODE_FILE)
  return()
endif()

# Ignore installed packages, registries, and SDL3_DIR left in a user's cache.
set(SDL3_DIR "${CSE_SDL3_ROOT}/cmake")
find_package(SDL3 3.4.20 EXACT CONFIG REQUIRED COMPONENTS SDL3-shared
  PATHS "${CSE_SDL3_ROOT}/cmake" NO_DEFAULT_PATH)
get_target_property(sdl3_implib SDL3::SDL3-shared IMPORTED_IMPLIB)
get_target_property(sdl3_dll SDL3::SDL3-shared IMPORTED_LOCATION)
if(NOT sdl3_implib STREQUAL "${CSE_SDL3_ROOT}/lib/x86/SDL3.lib" OR
    NOT sdl3_dll STREQUAL "${CSE_SDL3_ROOT}/lib/x86/SDL3.dll")
  message(FATAL_ERROR "SDL3 must resolve to the pinned x86 shared provider")
endif()

function(cse_link_sdl3 target config)
  target_link_libraries(${target} PRIVATE "$<$<CONFIG:${config}>:SDL3::SDL3-shared>")
  if(NOT TARGET cse_stage_sdl3)
    # A single always-run target restores a deleted DLL on an up-to-date build,
    # including independent builds of DLLs in nested game directories.
    add_custom_target(cse_stage_sdl3
      COMMAND "${CMAKE_COMMAND}" "-DCSE_SDK_DIR=${CSE_SDK_DIR}" -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CSE_SDL3_ROOT}/lib/x86/SDL3.dll" "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>/SDL3.dll"
      DEPENDS "${CSE_SDL3_ROOT}/lib/x86/SDL3.dll" "${CSE_SDL3_MANIFEST}"
      VERBATIM COMMENT "Validate and stage pinned SDL3 runtime")
  endif()
  add_dependencies(${target} cse_stage_sdl3)
endfunction()
