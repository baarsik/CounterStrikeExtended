cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseNativeSaveFields LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
add_executable(save_integer_tests save_integer_tests.cpp)
target_compile_features(save_integer_tests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(save_integer_tests PRIVATE /W4 /WX /EHsc)
endif()
enable_testing()
add_test(NAME native-save-integers COMMAND save_integer_tests)
add_executable(save_metadata_tests metadata_tests.cpp)
target_compile_features(save_metadata_tests PRIVATE cxx_std_17)
target_include_directories(save_metadata_tests PRIVATE
  "${CSE_ROOT}/src/public"
  "${CSE_ROOT}/src/common")
if(MSVC)
  target_compile_options(save_metadata_tests PRIVATE /W4 /WX /EHsc)
endif()
add_test(NAME native-save-metadata COMMAND save_metadata_tests)
add_executable(save_string_tests save_string_tests.cpp)
target_compile_features(save_string_tests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(save_string_tests PRIVATE /W4 /WX /EHsc)
endif()
add_test(NAME native-save-string-bounds COMMAND save_string_tests)
add_executable(c4_beep_tests c4_beep_tests.cpp)
target_compile_features(c4_beep_tests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(c4_beep_tests PRIVATE /W4 /WX /EHsc)
endif()
add_test(NAME c4-saved-beep-phase COMMAND c4_beep_tests)
add_executable(save_pointer_tests pointer_storage_tests.cpp)
target_compile_features(save_pointer_tests PRIVATE cxx_std_17)
target_include_directories(save_pointer_tests PRIVATE
  "${CSE_ROOT}/src/dlls"
  "${CSE_ROOT}/src/common"
  "${CSE_ROOT}/src/engine"
  "${CSE_ROOT}/src/public")
add_test(NAME native-save-pointer-rejection COMMAND save_pointer_tests)

try_compile(CSE_RAW_POINTER_COMPILES
  "${CMAKE_CURRENT_BINARY_DIR}/rejected-pointer-storage"
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rejected_pointer_storage.cpp"
  CMAKE_FLAGS "-DCMAKE_CXX_STANDARD=17"
    "-DINCLUDE_DIRECTORIES=${CSE_ROOT}/src/public;${CSE_ROOT}/src/common"
  OUTPUT_VARIABLE CSE_RAW_POINTER_DIAGNOSTIC)
if(CSE_RAW_POINTER_COMPILES OR NOT CSE_RAW_POINTER_DIAGNOSTIC MATCHES
    "Raw pointers require a symbolic save representation")
  message(FATAL_ERROR "Raw pointer metadata rejection fixture failed: ${CSE_RAW_POINTER_DIAGNOSTIC}")
endif()
