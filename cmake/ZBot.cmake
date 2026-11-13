include_guard(GLOBAL)

option(CSE_ENABLE_ZBOT "Enable Counter-Strike ZBot support and bundle its runtime resources" ON)

function(cse_zbot_assets target)
  if(NOT CSE_ENABLE_ZBOT)
    return()
  endif()
  set(source_root "${CSE_SDK_DIR}/deploy/zbot")
  file(GLOB_RECURSE resources CONFIGURE_DEPENDS RELATIVE "${source_root}" "${source_root}/cstrike/*")
  foreach(resource IN LISTS resources)
    set_property(TARGET ${target} APPEND PROPERTY CSE_QUERY_RESOURCE_SOURCES "${source_root}/${resource}")
    set_property(TARGET ${target} APPEND PROPERTY CSE_QUERY_RESOURCE_DESTINATIONS "${resource}")
  endforeach()
  if(DEFINED CSE_QUERY_OS)
    return()
  endif()
  if(NOT TARGET cse_zbot_runtime_assets)
    add_custom_target(cse_zbot_runtime_assets
      COMMAND "${CMAKE_COMMAND}" "-DCSE_ZBOT_SOURCE=${source_root}"
        "-DCSE_ZBOT_OUTPUT=${CSE_OUTPUT_DIRECTORY}"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StageZBotResources.cmake"
      COMMENT "Staging ZBot profiles, chatter, navigation and UI resources"
      VERBATIM)
  endif()
  add_dependencies(${target} cse_zbot_runtime_assets)
endfunction()
