include_guard(GLOBAL)

# This standalone helper does not opt any retained target into publication.
# Root integration must supply real constructor requirements and the reviewed
# preload binding before production mode can succeed. FIXTURE_ONLY is explicit
# and its generated header cannot be consumed by an ordinary publisher.
function(cse_add_engine_bootstrap_evidence)
  cmake_parse_arguments(EVIDENCE "FIXTURE_ONLY"
    "TARGET;CONTEXTS;FOUNDATION;SCHEMA;OUTPUT_DIRECTORY;PYTHON_EXECUTABLE" "" ${ARGN})
  foreach(required TARGET CONTEXTS FOUNDATION SCHEMA OUTPUT_DIRECTORY)
    if(NOT EVIDENCE_${required})
      message(FATAL_ERROR "Bootstrap evidence requires ${required}")
    endif()
  endforeach()
  if(NOT TARGET ${EVIDENCE_TARGET})
    message(FATAL_ERROR "Bootstrap publisher target does not exist: ${EVIDENCE_TARGET}")
  endif()
  if(EVIDENCE_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Unknown bootstrap evidence arguments: ${EVIDENCE_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT EVIDENCE_PYTHON_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(EVIDENCE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
  endif()
  get_filename_component(evidence_output "${EVIDENCE_OUTPUT_DIRECTORY}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  set(evidence_tool "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/engine_bootstrap_evidence.py")
  set(evidence_header "${evidence_output}/evidence.h")
  set(evidence_stamp "${evidence_output}/accepted.stamp")
  set(evidence_arguments generate
    --schema "${EVIDENCE_SCHEMA}" --contexts "${EVIDENCE_CONTEXTS}"
    --foundation "${EVIDENCE_FOUNDATION}" --output "${evidence_output}"
    --depfile "${evidence_output}/evidence.d")
  if(EVIDENCE_FIXTURE_ONLY)
    list(APPEND evidence_arguments --fixture-only)
    target_compile_definitions(${EVIDENCE_TARGET} PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:CSE_ENGINE_BOOTSTRAP_FIXTURE_EVIDENCE=1>")
  endif()
  # Validation deliberately runs at every owning build. Its content cache checks
  # effective options, response files, compiler components, and include closure;
  # unchanged evidence.h is not rewritten. Timestamp-only dependency shortcuts
  # must not bypass this prerequisite when the environment/options change.
  set(evidence_target "${EVIDENCE_TARGET}__bootstrap_evidence")
  add_custom_target(${evidence_target}
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONDONTWRITEBYTECODE=1
      "${EVIDENCE_PYTHON_EXECUTABLE}" "${evidence_tool}" ${evidence_arguments}
    BYPRODUCTS "${evidence_header}" "${evidence_stamp}" "${evidence_output}/metadata.json"
      "${evidence_output}/receipt.json" "${evidence_output}/evidence.d"
    DEPENDS "${evidence_tool}" "${EVIDENCE_SCHEMA}" "${EVIDENCE_CONTEXTS}" "${EVIDENCE_FOUNDATION}"
    VERBATIM)
  add_dependencies(${EVIDENCE_TARGET} ${evidence_target})
  set_source_files_properties("${evidence_header}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
  target_sources(${EVIDENCE_TARGET} PRIVATE "${evidence_header}")
  if(MSVC)
    target_compile_options(${EVIDENCE_TARGET} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/FI${evidence_header}>")
  else()
    target_compile_options(${EVIDENCE_TARGET} PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:-include>" "$<$<COMPILE_LANGUAGE:CXX>:${evidence_header}>")
  endif()
  set(${EVIDENCE_TARGET}_BOOTSTRAP_EVIDENCE_HEADER "${evidence_header}" PARENT_SCOPE)
  set(${EVIDENCE_TARGET}_BOOTSTRAP_EVIDENCE_TARGET "${evidence_target}" PARENT_SCOPE)
endfunction()
