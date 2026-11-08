cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseSurfaceLightingFixtures LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
set(CSE_SURFACE_IMPLEMENTATION "${CSE_ROOT}/GoldSrc-SDK/engine/surface_block16_packed_impl.h" CACHE FILEPATH
  "Production packed lighting function or explicit frozen Win32 reference")
option(CSE_SURFACE_REFERENCE_MMX "Compile the frozen MMX implementation (Win32 only)" OFF)
configure_file(surface_header.h.in surface_header.h @ONLY)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_executable(surface_lighting_tests surface_tests.cpp)
target_include_directories(surface_lighting_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/GoldSrc-SDK/common" "${CSE_ROOT}/GoldSrc-SDK/engine" "${CSE_ROOT}/GoldSrc-SDK/public")
target_compile_features(surface_lighting_tests PRIVATE cxx_std_17)
target_compile_definitions(surface_lighting_tests PRIVATE SW_RENDERER _CRT_SECURE_NO_WARNINGS
  CSE_SURFACE_REFERENCE_MMX=$<BOOL:${CSE_SURFACE_REFERENCE_MMX}>)
if(MSVC)
  target_compile_options(surface_lighting_tests PRIVATE /W4 /WX /EHsc /fp:precise
    "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
  target_compile_options(surface_lighting_tests PRIVATE -Wall -Wextra -Werror -ffp-model=precise -ffp-contract=off
    -Wno-overriding-option "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
endif()
add_test(NAME actual-packed-surface-lighting COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_LIST_DIR}/verify_pixels.py" "$<TARGET_FILE:surface_lighting_tests>"
  "${CMAKE_CURRENT_BINARY_DIR}/surface-pixels.bin" "${CMAKE_CURRENT_LIST_DIR}/expected_pixels.json")
add_executable(surface_dispatch_tests dispatch_tests.cpp)
target_compile_features(surface_dispatch_tests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(surface_dispatch_tests PRIVATE /W4 /WX /EHa)
else()
  target_compile_options(surface_dispatch_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME packed-surface-dispatch COMMAND surface_dispatch_tests)
