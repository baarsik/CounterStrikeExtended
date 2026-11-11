include_guard(GLOBAL)

# Selected only by retained executable source metadata. Preserve all intended
# default-visible publications through Darwin LTO; this is not an export whitelist.
function(cse_require_apple_main_image_publisher target requirement)
  get_target_property(kind ${target} TYPE)
  if(NOT APPLE OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT kind STREQUAL EXECUTABLE
      OR NOT target MATCHES "^(launcher|hlds)$" OR NOT requirement STREQUAL socket-v1
      OR NOT CSE_TARGET_ARCH MATCHES "^(arm64|x64)$")
    message(FATAL_ERROR "Invalid Apple main-image publisher owner/requirement: ${target}/${requirement}")
  endif()
  get_target_property(existing ${target} CSE_APPLE_MAIN_IMAGE_PUBLISHER)
  if(existing)
    message(FATAL_ERROR "Duplicate Apple main-image publisher: ${target}")
  endif()
  set_property(TARGET ${target} PROPERTY CSE_APPLE_MAIN_IMAGE_PUBLISHER "${requirement}")
  target_link_options(${target} PRIVATE "-Wl,-export_dynamic")
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  set(checker "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/main_image_publication.py")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
    "${checker}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/binary_identity.py")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${checker}"
      --image "$<TARGET_FILE:${target}>" --owner "${target}" --architecture "${CSE_TARGET_ARCH}"
      --receipt "${CMAKE_BINARY_DIR}/main-image-publication/$<CONFIG>/${target}.json"
    COMMENT "Verify ${target} final executable dynamic publications" VERBATIM)
endfunction()
