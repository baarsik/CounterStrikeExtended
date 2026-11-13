include_guard(GLOBAL)

function(cse_verify_steam_provider path target_os architecture)
  if(target_os STREQUAL windows AND architecture STREQUAL x64)
    set(profile windows_x64)
  elseif(target_os STREQUAL macos AND architecture STREQUAL arm64)
    set(profile macos_arm64)
  else()
    message(FATAL_ERROR "No qualified Steam provider pin for ${target_os}-${architecture}")
  endif()
  set(lock "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/qualification/fixtures/steam/lock.json")
  file(READ "${lock}" manifest)
  string(JSON expected_hash GET "${manifest}" "${profile}" runtime sha256)
  string(JSON expected_size GET "${manifest}" "${profile}" runtime bytes)
  string(JSON source_name GET "${manifest}" "${profile}" runtime path)
  get_filename_component(runtime_name "${source_name}" NAME)
  if(NOT IS_ABSOLUTE "${path}" OR NOT EXISTS "${path}")
    message(FATAL_ERROR "Pinned Steam provider requires an existing absolute CSE_STEAM_PROVIDER_FILE: ${path}")
  endif()
  file(SIZE "${path}" actual_size)
  file(SHA256 "${path}" actual_hash)
  if(NOT actual_size EQUAL expected_size OR NOT actual_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Pinned Steam provider hash/size mismatch: ${path}")
  endif()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  set(platform_options)
  if(target_os STREQUAL macos)
    # This exact Valve payload is universal; dyld selects the measured ARM64 slice.
    list(APPEND platform_options --allow-universal --max-macos 14.0)
  endif()
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/binary_identity.py"
    --expect "${architecture}" ${platform_options} "${path}"
    RESULT_VARIABLE status OUTPUT_VARIABLE identity ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Pinned Steam provider architecture validation failed: ${error}")
  endif()
  if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}" "${lock}")
  endif()
  set(CSE_STEAM_RUNTIME_NAME "${runtime_name}" PARENT_SCOPE)
endfunction()

function(cse_force_steam_initialization target)
  set(header "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/public/steam_module_initializer.h")
  if(MSVC)
    # cl treats a joined /FI//UNC path as another switch; use native backslashes.
    file(TO_NATIVE_PATH "${header}" native_header)
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/FI${native_header}>")
  else()
    # Debug must not coalesce inline loader/provider code across independently unloaded consumers.
    set_property(TARGET ${target} PROPERTY VISIBILITY_INLINES_HIDDEN YES)
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${header}\">")
  endif()
endfunction()

function(cse_link_steam target configuration reference_import)
  if(CSE_TARGET_ARCH STREQUAL x86)
    cse_link_import(${target} "${configuration}" "${reference_import}")
    return()
  endif()
  set(CSE_STEAM_PROVIDER_FILE "" CACHE FILEPATH "Pinned native Valve Steam API runtime provider")
  cse_verify_steam_provider("${CSE_STEAM_PROVIDER_FILE}" "${CSE_TARGET_OS}" "${CSE_TARGET_ARCH}")
  target_compile_definitions(${target} PRIVATE "$<$<CONFIG:${configuration}>:CSE_OWNED_STEAM_PROVIDER=1>")
  cse_force_steam_initialization(${target})
  if(NOT TARGET cse_stage_native_steam)
    add_custom_target(cse_stage_native_steam
      COMMAND "${CMAKE_COMMAND}" "-DCSE_STEAM_PROVIDER_FILE=${CSE_STEAM_PROVIDER_FILE}"
        "-DCSE_TARGET_OS=${CSE_TARGET_OS}" "-DCSE_TARGET_ARCH=${CSE_TARGET_ARCH}"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CSE_OUTPUT_DIRECTORY}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CSE_STEAM_PROVIDER_FILE}" "${CSE_OUTPUT_DIRECTORY}/${CSE_STEAM_RUNTIME_NAME}"
      DEPENDS "${CSE_STEAM_PROVIDER_FILE}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/qualification/fixtures/steam/lock.json"
      VERBATIM COMMENT "Validate and stage pinned native Steam provider")
  endif()
  add_dependencies(${target} cse_stage_native_steam)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
  cse_verify_steam_provider("${CSE_STEAM_PROVIDER_FILE}" "${CSE_TARGET_OS}" "${CSE_TARGET_ARCH}")
endif()
