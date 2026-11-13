cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseClientAbi LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_CLIENT_ABI_SANITIZERS "Instrument actual client entry and loader bodies" OFF)
enable_testing()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hl_entries.inc" "${CMAKE_CURRENT_BINARY_DIR}/cs_entries.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/hl_stubs.inc" "${CMAKE_CURRENT_BINARY_DIR}/cs_stubs.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/engine_loader_bodies.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/fixture_module.def"
    "${CMAKE_CURRENT_BINARY_DIR}/fixture_module_without_descriptor.def"
    "${CMAKE_CURRENT_BINARY_DIR}/fixture_module_without_initializer.def"
    "${CMAKE_CURRENT_BINARY_DIR}/movement_diagnostics.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/engine_model_view.h" "${CMAKE_CURRENT_BINARY_DIR}/mutant_prototype_APIProxy.h"
    "${CMAKE_CURRENT_BINARY_DIR}/mutant_destination_APIProxy.h" "${CMAKE_CURRENT_BINARY_DIR}/mutant_event_api.h"
    "${CMAKE_CURRENT_BINARY_DIR}/mutant_mouse_source_APIProxy.h"
    "${CMAKE_CURRENT_BINARY_DIR}/mutant_mouse_destination_APIProxy.h"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_fixtures.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS prepare_fixtures.py "${CSE_ROOT}/src/engine/APIProxy.h"
    "${CSE_ROOT}/src/cl_dll/cdll_int.cpp" "${CSE_ROOT}/src/cstrike/cl_dll/cdll_int.cpp"
    "${CSE_ROOT}/src/cl_dll/GameStudioModelRenderer.cpp"
    "${CSE_ROOT}/src/cstrike/cl_dll/GameStudioModelRenderer.cpp"
    "${CSE_ROOT}/src/common/com_model.h" "${CSE_ROOT}/src/engine/client.h"
    "${CSE_ROOT}/src/common/event_api.h" "${CSE_ROOT}/tools/client_module_layouts.py" "${CSE_ROOT}/src/engine/cdll_int.cpp"
    "${CSE_ROOT}/src/common/client_module_required_exports.h" module.def VERBATIM)
add_custom_target(client_abi_extract DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/hl_entries.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/cs_entries.inc")
function(configure_client_fixture target)
  add_dependencies(${target} client_abi_extract)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/common"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/pm_shared")
  target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS
    "$<$<CONFIG:Debug>:_DEBUG>" "$<$<CONFIG:Release>:NDEBUG>")
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /EHsc
      "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wshorten-64-to-32
      "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
    if(CSE_CLIENT_ABI_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()
foreach(variant hl cs)
  add_library(client_module_${variant} SHARED module.cpp)
  configure_client_fixture(client_module_${variant})
  target_compile_definitions(client_module_${variant} PRIVATE CSE_CLIENT_FIXTURE_FAULT=0)
  if(variant STREQUAL cs)
    target_compile_definitions(client_module_${variant} PRIVATE CSE_CLIENT_ABI_CS=1)
  endif()
endforeach()
add_executable(client_descriptor_probe descriptor_probe.cpp)
configure_client_fixture(client_descriptor_probe)
target_link_libraries(client_descriptor_probe PRIVATE "${CMAKE_DL_LIBS}")
add_executable(client_catalog_tests catalog_tests.cpp)
configure_client_fixture(client_catalog_tests)
add_test(NAME client-catalog-validation COMMAND client_catalog_tests)
foreach(variant hl cs)
  add_test(NAME client-${variant}-descriptor COMMAND client_descriptor_probe "$<TARGET_FILE:client_module_${variant}>")
endforeach()
add_executable(client_abi_loader loader_tests.cpp)
configure_client_fixture(client_abi_loader)
target_link_libraries(client_abi_loader PRIVATE "${CMAKE_DL_LIBS}")
foreach(variant hl cs)
  if(WIN32)
    target_sources(client_module_${variant} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fixture_module.def")
  endif()
  add_test(NAME client-${variant}-actual-entry-exchange
    COMMAND client_abi_loader "$<TARGET_FILE:client_module_${variant}>" accept)
endforeach()
if(MSVC)
  set_property(TARGET client_module_hl PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_definitions(client_module_cs PRIVATE "$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>")
  target_compile_options(client_module_cs PRIVATE /GR-)
endif()
foreach(fault RANGE 1 14)
  add_library(client_fault_${fault} SHARED module.cpp)
  configure_client_fixture(client_fault_${fault})
  target_compile_definitions(client_fault_${fault} PRIVATE CSE_CLIENT_FIXTURE_FAULT=${fault})
  if(WIN32)
    if(fault EQUAL 8)
      target_sources(client_fault_${fault} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fixture_module_without_descriptor.def")
    elseif(fault EQUAL 14)
      target_sources(client_fault_${fault} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fixture_module_without_initializer.def")
    else()
      target_sources(client_fault_${fault} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fixture_module.def")
    endif()
  endif()
  if(fault EQUAL 7)
    set(expectation accept)
  else()
    set(expectation reject)
  endif()
  add_test(NAME client-metadata-${fault}
    COMMAND client_abi_loader "$<TARGET_FILE:client_fault_${fault}>" ${expectation})
endforeach()
foreach(mutation PROTOTYPE NESTED DESTINATION MOUSE_SOURCE MOUSE_DESTINATION)
  string(TOLOWER "${mutation}" identity)
  add_library(client_mutant_${identity} SHARED module.cpp)
  configure_client_fixture(client_mutant_${identity})
  target_compile_definitions(client_mutant_${identity} PRIVATE CSE_CLIENT_FIXTURE_FAULT=0 "CSE_ABI_${mutation}_MUTANT=1")
  if(WIN32)
    target_sources(client_mutant_${identity} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fixture_module.def")
  endif()
  add_test(NAME client-real-${identity}-mutation
    COMMAND client_abi_loader "$<TARGET_FILE:client_mutant_${identity}>" reject)
endforeach()

add_executable(client_engine_loader_tests engine_loader_tests.cpp)
configure_client_fixture(client_engine_loader_tests)
target_link_libraries(client_engine_loader_tests PRIVATE "${CMAKE_DL_LIBS}")
foreach(variant hl cs)
  foreach(mode F symbols)
    add_test(NAME client-engine-${variant}-${mode}
      COMMAND client_engine_loader_tests "$<TARGET_FILE:client_module_${variant}>" accept ${mode})
  endforeach()
endforeach()
foreach(fault RANGE 1 14)
  if(fault EQUAL 7)
    set(expectation accept)
  else()
    set(expectation reject)
  endif()
  foreach(mode F symbols)
    add_test(NAME client-engine-metadata-${fault}-${mode}
      COMMAND client_engine_loader_tests "$<TARGET_FILE:client_fault_${fault}>" ${expectation} ${mode})
  endforeach()
endforeach()
foreach(mutation prototype nested destination mouse_source mouse_destination)
  foreach(mode F symbols)
    add_test(NAME client-engine-${mutation}-${mode}
      COMMAND client_engine_loader_tests "$<TARGET_FILE:client_mutant_${mutation}>" reject ${mode})
  endforeach()
endforeach()

add_library(client_incomplete SHARED module.cpp)
configure_client_fixture(client_incomplete)
target_compile_definitions(client_incomplete PRIVATE CSE_CLIENT_FIXTURE_FAULT=15)
if(WIN32)
  target_sources(client_incomplete PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fixture_module.def")
endif()
add_test(NAME client-engine-incomplete-F
  COMMAND client_engine_loader_tests "$<TARGET_FILE:client_incomplete>" incomplete F)
add_test(NAME client-generated-contracts-current
  COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tools/client_module_layouts.py" --root "${CSE_ROOT}" --check)

foreach(variant hl cs)
  add_test(NAME client-${variant}-production-probe-path
    COMMAND client_descriptor_probe "$<TARGET_FILE:client_module_${variant}>" --entry-probe)
endforeach()
