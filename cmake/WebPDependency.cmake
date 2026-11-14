include_guard(GLOBAL)
if(NOT DEFINED CSE_SDK_DIR)
  set(CSE_SDK_DIR "${CSE_ROOT}/src")
endif()
add_subdirectory("${CSE_ROOT}/cmake/dependencies/WebP" "${CMAKE_BINARY_DIR}/dependencies/WebP")

function(cse_link_webp target)
  get_target_property(runtime ${target} MSVC_RUNTIME_LIBRARY)
  set(provider cse_webp_md)
  if(MSVC AND runtime AND NOT runtime MATCHES "DLL")
    set(provider cse_webp_mt)
  endif()
  target_link_libraries(${target} PRIVATE ${provider})
  target_include_directories(${target} PRIVATE "${CSE_ROOT}/src/common")
  if(CSE_STAGE_DISTRIBUTION_MATERIALS)
    if(NOT TARGET cse_stage_webp_materials)
      set(materials "${CSE_ROOT}/src/external/libwebp-source")
      add_custom_target(cse_stage_webp_materials
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CSE_OUTPUT_DIRECTORY}/licenses/libwebp"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${materials}/COPYING" "${materials}/PATENTS" "${materials}/UPSTREAM.md"
          "${materials}/libwebp-1.6.0.tar.gz" "${CSE_OUTPUT_DIRECTORY}/licenses/libwebp"
        VERBATIM COMMENT "Stage libwebp distribution materials")
    endif()
    add_dependencies(${target} cse_stage_webp_materials)
  endif()
endfunction()
