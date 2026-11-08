cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseNativeEntityOffsets LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
enable_testing()
foreach(game hl cs particle)
  add_executable(entity_interface_${game} interface_tests.cpp)
  target_compile_features(entity_interface_${game} PRIVATE cxx_std_17)
  if(game STREQUAL cs)
    target_include_directories(entity_interface_${game} PRIVATE "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls")
  elseif(game STREQUAL particle)
    target_include_directories(entity_interface_${game} PRIVATE "${CSE_ROOT}/GoldSrc-SDK/particleman/compat")
  endif()
  target_include_directories(entity_interface_${game} PRIVATE
    "${CSE_ROOT}/GoldSrc-SDK/dlls"
    "${CSE_ROOT}/GoldSrc-SDK/common"
    "${CSE_ROOT}/GoldSrc-SDK/engine"
    "${CSE_ROOT}/GoldSrc-SDK/public")
  if(MSVC)
    target_compile_options(entity_interface_${game} PRIVATE /W4 /WX /EHsc)
  endif()
  add_test(NAME native-entity-interface-${game} COMMAND entity_interface_${game})
endforeach()

add_executable(engine_entity_tests engine_tests.cpp)
target_compile_features(engine_entity_tests PRIVATE cxx_std_17)
target_include_directories(engine_entity_tests PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/dlls"
  "${CSE_ROOT}/GoldSrc-SDK/common"
  "${CSE_ROOT}/GoldSrc-SDK/engine"
  "${CSE_ROOT}/GoldSrc-SDK/public")
if(MSVC)
  target_compile_options(engine_entity_tests PRIVATE /W4 /WX /EHsc)
endif()
add_test(NAME native-engine-entities COMMAND engine_entity_tests)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  try_compile(CSE_TRUNCATED_ENTITY_COMPILES
    "${CMAKE_CURRENT_BINARY_DIR}/rejected-entity-storage"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/wrong_entity_storage.cpp"
    CMAKE_FLAGS "-DCMAKE_CXX_STANDARD=17"
      "-DINCLUDE_DIRECTORIES=${CSE_ROOT}/GoldSrc-SDK/public;${CSE_ROOT}/GoldSrc-SDK/common"
    OUTPUT_VARIABLE CSE_TRUNCATED_ENTITY_DIAGNOSTIC)
  if(CSE_TRUNCATED_ENTITY_COMPILES OR NOT CSE_TRUNCATED_ENTITY_DIAGNOSTIC MATCHES
      "Saved entity offsets must use native EOFFSET storage")
    message(FATAL_ERROR "Native entity storage rejection fixture failed: ${CSE_TRUNCATED_ENTITY_DIAGNOSTIC}")
  endif()
endif()
