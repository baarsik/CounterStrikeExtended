cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseNativeStrings LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
enable_testing()
foreach(game hl cs particle)
  add_executable(string_offsets_${game} string_offset_tests.cpp)
  target_compile_features(string_offsets_${game} PRIVATE cxx_std_17)
  if(game STREQUAL cs)
    target_include_directories(string_offsets_${game} PRIVATE "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls")
  elseif(game STREQUAL particle)
    target_include_directories(string_offsets_${game} PRIVATE "${CSE_ROOT}/GoldSrc-SDK/particleman/compat")
  endif()
  target_include_directories(string_offsets_${game} PRIVATE
    "${CSE_ROOT}/GoldSrc-SDK/dlls"
    "${CSE_ROOT}/GoldSrc-SDK/common"
    "${CSE_ROOT}/GoldSrc-SDK/engine"
    "${CSE_ROOT}/GoldSrc-SDK/public")
  if(MSVC)
    target_compile_options(string_offsets_${game} PRIVATE /W4 /WX /EHsc)
    target_compile_definitions(string_offsets_${game} PRIVATE _CRT_NONSTDC_NO_WARNINGS)
  else()
    target_compile_definitions(string_offsets_${game} PRIVATE stricmp=strcasecmp)
  endif()
  add_test(NAME native-string-offsets-${game} COMMAND string_offsets_${game})
endforeach()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  try_compile(CSE_TRUNCATED_STRING_COMPILES
    "${CMAKE_CURRENT_BINARY_DIR}/rejected-string-storage"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/wrong_string_storage.cpp"
    CMAKE_FLAGS "-DCMAKE_CXX_STANDARD=17"
      "-DINCLUDE_DIRECTORIES=${CSE_ROOT}/GoldSrc-SDK/public;${CSE_ROOT}/GoldSrc-SDK/common"
    OUTPUT_VARIABLE CSE_TRUNCATED_STRING_DIAGNOSTIC)
  if(CSE_TRUNCATED_STRING_COMPILES OR NOT CSE_TRUNCATED_STRING_DIAGNOSTIC MATCHES
      "Saved engine strings must use native string_t storage")
    message(FATAL_ERROR "Native string storage rejection fixture failed: ${CSE_TRUNCATED_STRING_DIAGNOSTIC}")
  endif()
endif()

add_executable(worldspawn_value_tests worldspawn_value_tests.cpp)
target_compile_features(worldspawn_value_tests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(worldspawn_value_tests PRIVATE /W4 /WX /EHsc)
endif()
add_test(NAME worldspawn-string-recovery COMMAND worldspawn_value_tests)
