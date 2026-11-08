# Included by the root before the Win32 runtime's platform gate. This entry point
# builds real native providers and their bounded ABI probe, not a partial game.
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CounterStrikeExtendedDependencies VERSION 0.1 LANGUAGES C CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug)$")
  message(FATAL_ERROR "Select a native dependency Release or Debug preset")
endif()
set(CSE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime")
include("${CSE_ROOT}/cmake/RuntimeTargets.cmake")
set(CSE_SDL3_FROM_SOURCE ON)
include("${CSE_ROOT}/cmake/SDL3Dependency.cmake")
include("${CSE_ROOT}/cmake/Mpg123Dependency.cmake")
add_executable(cse_dependency_probe "${CSE_ROOT}/tests/native/dependency_probe.c")
cse_link_sdl3(cse_dependency_probe "${CMAKE_BUILD_TYPE}")
cse_link_mpg123(cse_dependency_probe "${CMAKE_BUILD_TYPE}")
add_executable(cse_decoder_fixture "${CSE_ROOT}/tests/music/decoder_tests.cpp"
  "${CSE_SDK_DIR}/engine/mpg123_decoder.cpp")
target_compile_features(cse_decoder_fixture PRIVATE cxx_std_17)
cse_link_sdl3(cse_decoder_fixture "${CMAKE_BUILD_TYPE}")
cse_link_mpg123(cse_decoder_fixture "${CMAKE_BUILD_TYPE}")
foreach(fixture cse_dependency_probe cse_decoder_fixture)
  set_target_properties(${fixture} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/${fixture}"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/${fixture}")
  if(MSVC)
    target_compile_options(${fixture} PRIVATE /W4 /WX)
    if(fixture STREQUAL cse_decoder_fixture)
      target_compile_options(${fixture} PRIVATE /EHsc)
    endif()
  endif()
endforeach()
# This deliberately does not register a test that automatically loads providers.
# Run the probe only in an authorized provider fixture, with runtime DLL paths.
add_custom_target(cse_native_dependencies ALL DEPENDS cse_dependency_probe cse_decoder_fixture)
message(STATUS "Dependency compilation only: no native game/provider/runtime qualification is implied")
