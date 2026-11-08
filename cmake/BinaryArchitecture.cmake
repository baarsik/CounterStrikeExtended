include_guard(GLOBAL)

# Inspect file contents on the builder. The builder's CPU and a library's path
# do not establish the target architecture of its object/import members.
function(cse_verify_binary path architecture)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Missing ${architecture} binary: ${path}")
  endif()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
  set(deployment_options)
  if(CSE_TARGET_OS STREQUAL macos AND CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND deployment_options --max-macos "${CMAKE_OSX_DEPLOYMENT_TARGET}")
  elseif(CSE_MAX_MACOS)
    list(APPEND deployment_options --max-macos "${CSE_MAX_MACOS}")
  endif()
  string(SHA256 key "${path}|${architecture}|${deployment_options}")
  get_property(verified GLOBAL PROPERTY "cse_binary_${key}")
  if(NOT verified)
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/binary_identity.py"
        --expect "${architecture}" ${deployment_options} "${path}"
      RESULT_VARIABLE status OUTPUT_VARIABLE identity ERROR_VARIABLE error)
    if(NOT status EQUAL 0)
      message(FATAL_ERROR "Binary architecture validation failed for ${path}: ${error}")
    endif()
    set_property(GLOBAL PROPERTY "cse_binary_${key}" TRUE)
  endif()
endfunction()

# Build-time validation runs in a fresh process, so manually changed producer
# files cannot reuse a configure-time result before being staged.
if(CMAKE_SCRIPT_MODE_FILE)
  if(NOT DEFINED CSE_BINARY_FILE OR NOT DEFINED CSE_EXPECTED_ARCH)
    message(FATAL_ERROR "Binary validation requires CSE_BINARY_FILE and CSE_EXPECTED_ARCH")
  endif()
  cse_verify_binary("${CSE_BINARY_FILE}" "${CSE_EXPECTED_ARCH}")
endif()
