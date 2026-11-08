cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseLegacyGraphFixtures LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
add_executable(graph_records_tests graph_records_tests.cpp)
target_compile_features(graph_records_tests PRIVATE cxx_std_17)
target_include_directories(graph_records_tests PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/dlls"
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/common"
  "${CSE_ROOT}/GoldSrc-SDK/engine"
  "${CSE_ROOT}/GoldSrc-SDK/pm_shared")
if(MSVC)
  target_compile_options(graph_records_tests PRIVATE /W4 /WX /EHsc)
endif()
enable_testing()
add_test(NAME legacy-navigation-records COMMAND graph_records_tests)
add_executable(nav_reference_tests nav_reference_tests.cpp)
target_compile_features(nav_reference_tests PRIVATE cxx_std_17)
target_include_directories(nav_reference_tests PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/game_shared/bot"
  "${CSE_ROOT}/GoldSrc-SDK/dlls"
  "${CSE_ROOT}/GoldSrc-SDK/game_shared"
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/common"
  "${CSE_ROOT}/GoldSrc-SDK/engine"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/pm_shared")
if(MSVC)
  target_compile_options(nav_reference_tests PRIVATE /W4 /WX /EHsc)
endif()
add_test(NAME native-navigation-references COMMAND nav_reference_tests)

add_executable(nav_file_tests nav_file_tests.cpp
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/game_shared/bot/nav_file.cpp"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/game_shared/bot/nav_area.cpp"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/game_shared/bot/nav_node.cpp")
target_compile_features(nav_file_tests PRIVATE cxx_std_17)
target_include_directories(nav_file_tests PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/engine"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls/bot"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls/hostage"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/common"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/pm_shared"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/game_shared"
  "${CSE_ROOT}/GoldSrc-SDK/cstrike/game_shared/bot"
  "${CSE_ROOT}/GoldSrc-SDK/dlls"
  "${CSE_ROOT}/GoldSrc-SDK/pm_shared"
  "${CSE_ROOT}/GoldSrc-SDK/game_shared"
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/common"
  "${CSE_ROOT}/GoldSrc-SDK/engine")
if(MSVC)
  target_compile_definitions(nav_file_tests PRIVATE WIN32)
  target_compile_options(nav_file_tests PRIVATE /EHsc /Gy /Gw)
  target_link_options(nav_file_tests PRIVATE /OPT:REF /OPT:NOICF)
else()
  target_compile_definitions(nav_file_tests PRIVATE stricmp=strcasecmp)
  target_compile_options(nav_file_tests PRIVATE -ffunction-sections -fdata-sections)
  if(APPLE)
    target_link_options(nav_file_tests PRIVATE -Wl,-dead_strip)
  else()
    target_link_options(nav_file_tests PRIVATE -Wl,--gc-sections)
  endif()
endif()
add_test(NAME native-navigation-files COMMAND nav_file_tests "${CMAKE_CURRENT_BINARY_DIR}/round-trip.nav")
