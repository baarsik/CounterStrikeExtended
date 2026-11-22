function(cse_check_regamedll_declaration source label first last expected_hash)
  string(FIND "${source}" "${first}" start)
  string(FIND "${source}" "${last}" end)
  if(start LESS 0 OR end LESS start)
    message(FATAL_ERROR "Could not locate the upstream ReGameDLL ${label} in regamedll/engine/eiface.h.")
  endif()

  string(LENGTH "${last}" last_length)
  math(EXPR length "${end} - ${start} + ${last_length}")
  string(SUBSTRING "${source}" ${start} ${length} declaration)
  string(SHA256 hash "${declaration}")
  if(NOT hash STREQUAL expected_hash)
    message(FATAL_ERROR
      "The upstream ReGameDLL ${label} changed in regamedll/engine/eiface.h.\n"
      "Review the old and new declarations before updating the expected SHA-256 to ${hash}.\n"
      "See docs/build-system.md for the upgrade procedure.")
  endif()
endfunction()

function(cse_check_regamedll_contract source)
  string(REPLACE "\r\n" "\n" source "${source}")
  # These upstream declarations are overridden by CSE headers in cse.patch.
  cse_check_regamedll_declaration("${source}" "engine callback table"
    "typedef struct enginefuncs_s" "} enginefuncs_t;"
    "69859252b6833851c5c0c04e931f0c8be7f2480b9d488c7ad74d6b10f3d24a10")
  cse_check_regamedll_declaration("${source}" "save field declarations"
    "typedef enum _fieldtypes" "} TYPEDESCRIPTION;"
    "3a4857b2dd2c3e44e72578d7f2314c205fd8020b78b98b881d19707417957131")
endfunction()
