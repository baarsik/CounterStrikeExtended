cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseLegacyGraphFixtures LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
add_executable(graph_records_tests graph_records_tests.cpp)
target_compile_features(graph_records_tests PRIVATE cxx_std_17)
target_include_directories(graph_records_tests PRIVATE
  "${CSE_ROOT}/src/dlls"
  "${CSE_ROOT}/src/public"
  "${CSE_ROOT}/src/common"
  "${CSE_ROOT}/src/engine"
  "${CSE_ROOT}/src/pm_shared")
if(MSVC)
  target_compile_options(graph_records_tests PRIVATE /W4 /WX /EHsc)
endif()
enable_testing()
add_test(NAME legacy-navigation-records COMMAND graph_records_tests)
