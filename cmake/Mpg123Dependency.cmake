include_guard(GLOBAL)
add_subdirectory("${CSE_ROOT}/cmake/dependencies/mpg123" "${CMAKE_BINARY_DIR}/dependencies/mpg123")

function(cse_link_mpg123 target config)
  target_link_libraries(${target} PRIVATE "$<$<CONFIG:${config}>:libmpg123>")
  if(NOT TARGET cse_stage_mpg123)
    add_custom_target(cse_stage_mpg123
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:libmpg123>" "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>/$<TARGET_FILE_NAME:libmpg123>"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>/licenses/mpg123"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CSE_SDK_DIR}/external/mpg123/COPYING" "${CSE_SDK_DIR}/external/mpg123/AUTHORS"
        "${CSE_SDK_DIR}/external/mpg123/UPSTREAM.md"
        "${CSE_SDK_DIR}/external/mpg123/mpg123-1.33.7.tar.bz2"
        "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>/licenses/mpg123"
      DEPENDS libmpg123
      VERBATIM COMMENT "Stage source-built mpg123 shared decoder")
  endif()
  add_dependencies(${target} cse_stage_mpg123)
endfunction()
