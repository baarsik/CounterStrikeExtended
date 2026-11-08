include_guard(GLOBAL)
add_subdirectory("${CSE_ROOT}/cmake/dependencies/mpg123" "${CMAKE_BINARY_DIR}/dependencies/mpg123")

function(cse_link_mpg123 target config)
  target_link_libraries(${target} PRIVATE "$<$<CONFIG:${config}>:libmpg123>")
  if(NOT TARGET cse_stage_mpg123)
    set(validation_commands)
    if(DEFINED CSE_TARGET_ARCH)
      set(deployment_argument)
      if(CSE_TARGET_OS STREQUAL macos)
        set(deployment_argument "-DCSE_MAX_MACOS=${CMAKE_OSX_DEPLOYMENT_TARGET}")
      endif()
      list(APPEND validation_commands
        COMMAND "${CMAKE_COMMAND}" "-DCSE_BINARY_FILE=$<TARGET_FILE:libmpg123>"
          "-DCSE_EXPECTED_ARCH=${CSE_TARGET_ARCH}" ${deployment_argument}
          -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BinaryArchitecture.cmake")
    endif()
    set(distribution_commands)
    if(CSE_STAGE_DISTRIBUTION_MATERIALS)
      list(APPEND distribution_commands
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CSE_OUTPUT_DIRECTORY}/licenses/mpg123"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${CSE_SDK_DIR}/external/mpg123/COPYING" "${CSE_SDK_DIR}/external/mpg123/AUTHORS"
          "${CSE_SDK_DIR}/external/mpg123/UPSTREAM.md"
          "${CSE_SDK_DIR}/external/mpg123/mpg123-1.33.7.tar.bz2"
          "${CSE_OUTPUT_DIRECTORY}/licenses/mpg123")
    endif()
    add_custom_target(cse_stage_mpg123
      ${validation_commands}
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CSE_OUTPUT_DIRECTORY}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:libmpg123>" "${CSE_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:libmpg123>"
      ${distribution_commands}
      DEPENDS libmpg123
      VERBATIM COMMENT "Stage source-built mpg123 shared decoder")
  endif()
  add_dependencies(${target} cse_stage_mpg123)
endfunction()
