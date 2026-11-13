include_guard(GLOBAL)

# One owner stages the complete text pack, even when several consumers build together.
function(cse_localization_assets target)
  set(source_root "${CSE_SDK_DIR}/deploy/localization")
  set(manifest "${source_root}/manifest.json")
  file(READ "${manifest}" catalogue)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
  string(JSON count LENGTH "${catalogue}" files)
  set(sources "${manifest}")
  if(count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
      string(JSON entry GET "${catalogue}" files ${index})
      string(JSON source GET "${entry}" source)
      string(JSON destination GET "${entry}" destination)
      list(APPEND sources "${source_root}/${source}")
      set(destinations "${destination}")
      string(JSON alias_count ERROR_VARIABLE alias_error LENGTH "${entry}" aliases)
      if(NOT alias_error AND alias_count GREATER 0)
        math(EXPR alias_last "${alias_count} - 1")
        foreach(alias_index RANGE 0 ${alias_last})
          string(JSON alias GET "${entry}" aliases ${alias_index})
          list(APPEND destinations "${alias}")
        endforeach()
      endif()
      foreach(destination IN LISTS destinations)
        # Keep authoritative source/destination ownership visible to metadata queries.
        set_property(TARGET ${target} APPEND PROPERTY CSE_QUERY_RESOURCE_SOURCES "${source_root}/${source}")
        set_property(TARGET ${target} APPEND PROPERTY CSE_QUERY_RESOURCE_DESTINATIONS "${destination}")
      endforeach()
    endforeach()
  endif()
  if(DEFINED CSE_QUERY_OS)
    return()
  endif()
  if(NOT TARGET cse_localization_runtime_assets)
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    set(generator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/build_localization.py")
    set(fallback_header "${CMAKE_BINARY_DIR}/localization/include/ServerBrowserEnglish.generated.inc")
    set(language_header "${CMAKE_BINARY_DIR}/localization/include/InterfaceLanguages.generated.inc")
    add_custom_target(cse_localization_runtime_assets
      COMMAND "${Python3_EXECUTABLE}" "${generator}"
        --source "${source_root}"
        --cache "${CMAKE_BINARY_DIR}/localization/$<CONFIG>"
        --output "${CSE_OUTPUT_DIRECTORY}"
        --fallback-header "${fallback_header}"
        --language-header "${language_header}"
      BYPRODUCTS "${fallback_header}" "${language_header}"
      DEPENDS "${generator}" ${sources}
      COMMENT "Staging bundled translations"
      VERBATIM)
  endif()
  get_target_property(target_type ${target} TYPE)
  if(NOT target_type STREQUAL "UTILITY")
    target_include_directories(${target} PRIVATE "${CMAKE_BINARY_DIR}/localization/include")
  endif()
  add_dependencies(${target} cse_localization_runtime_assets)
endfunction()
