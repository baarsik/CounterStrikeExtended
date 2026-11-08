include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/RuntimeTargets.cmake")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/dependencies/SDL3" "${CMAKE_BINARY_DIR}/dependencies/SDL3")

function(cse_link_sdl3 target config)
  target_link_libraries(${target} PRIVATE "$<$<CONFIG:${config}>:SDL3::SDL3-shared>")
  if(NOT TARGET cse_stage_sdl3)
    set(validation_commands)
    if(DEFINED CSE_TARGET_ARCH)
      set(deployment_argument)
      if(CSE_TARGET_OS STREQUAL macos)
        set(deployment_argument "-DCSE_MAX_MACOS=${CMAKE_OSX_DEPLOYMENT_TARGET}")
      endif()
      list(APPEND validation_commands
        COMMAND "${CMAKE_COMMAND}" "-DCSE_BINARY_FILE=$<TARGET_FILE:SDL3-shared>"
          "-DCSE_EXPECTED_ARCH=${CSE_TARGET_ARCH}" ${deployment_argument}
          -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BinaryArchitecture.cmake")
    endif()
    set(platform_commands)
    if(UNIX)
      # Preserve the SONAME/install-name file used by consumers, in addition to
      # the real versioned library. A versioned file alone is not loadable closure.
      list(APPEND platform_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:SDL3-shared>" "${CSE_OUTPUT_DIRECTORY}/$<TARGET_SONAME_FILE_NAME:SDL3-shared>")
    endif()
    set(distribution_commands)
    if(CSE_STAGE_DISTRIBUTION_MATERIALS)
      list(APPEND distribution_commands
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CSE_OUTPUT_DIRECTORY}/licenses/SDL3"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${CSE_SDK_DIR}/external/SDL3/LICENSE.txt"
          "${CSE_SDK_DIR}/external/SDL3-source/UPSTREAM.md"
          "${CSE_SDK_DIR}/external/SDL3-source/SDL3-3.4.20.tar.gz"
          "${CSE_OUTPUT_DIRECTORY}/licenses/SDL3")
    endif()
    add_custom_target(cse_stage_sdl3
      ${validation_commands}
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CSE_OUTPUT_DIRECTORY}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:SDL3-shared>" "${CSE_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:SDL3-shared>"
      ${platform_commands} ${distribution_commands}
      DEPENDS SDL3-shared
      VERBATIM COMMENT "Stage source-built SDL3 shared provider")
  endif()
  add_dependencies(${target} cse_stage_sdl3)
endfunction()
