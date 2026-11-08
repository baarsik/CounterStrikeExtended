cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseModuleAbiFixture LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
enable_testing()
foreach(fault RANGE 0 8)
  add_library(abi_module_${fault} SHARED module.cpp)
  target_compile_definitions(abi_module_${fault} PRIVATE CSE_ABI_FIXTURE_FAULT=${fault})
  target_compile_features(abi_module_${fault} PRIVATE cxx_std_17)
  if(WIN32)
    if(fault EQUAL 7)
      target_sources(abi_module_${fault} PRIVATE module_without_descriptor.def)
    else()
      target_sources(abi_module_${fault} PRIVATE module.def)
    endif()
  else()
    set_target_properties(abi_module_${fault} PROPERTIES CXX_VISIBILITY_PRESET hidden)
  endif()
endforeach()
add_executable(abi_loader loader.cpp)
target_compile_features(abi_loader PRIVATE cxx_std_17)
target_link_libraries(abi_loader PRIVATE ${CMAKE_DL_LIBS})
if(MSVC)
  foreach(target abi_loader abi_module_0 abi_module_1 abi_module_2 abi_module_3 abi_module_4 abi_module_5 abi_module_6
      abi_module_7 abi_module_8)
    target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
  endforeach()
endif()
add_test(NAME module-abi COMMAND abi_loader
  "$<TARGET_FILE:abi_module_0>" "$<TARGET_FILE:abi_module_1>" "$<TARGET_FILE:abi_module_2>"
  "$<TARGET_FILE:abi_module_3>" "$<TARGET_FILE:abi_module_4>" "$<TARGET_FILE:abi_module_5>"
  "$<TARGET_FILE:abi_module_6>" "$<TARGET_FILE:abi_module_7>" "$<TARGET_FILE:abi_module_8>")
add_executable(game_layout_tests game_layout_tests.cpp)
target_compile_features(game_layout_tests PRIVATE cxx_std_17)
target_include_directories(game_layout_tests PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/common")
add_test(NAME game-layouts COMMAND game_layout_tests)
add_executable(game_entry_tests game_entry_tests.cpp)
target_compile_features(game_entry_tests PRIVATE cxx_std_17)
target_include_directories(game_entry_tests PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/common")
add_test(NAME game-entry COMMAND game_entry_tests)
if(MSVC)
  foreach(target game_layout_tests game_entry_tests)
    target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
  endforeach()
endif()
# Explicit invocation only: accepts paths to the owned rebuilt game modules.
add_executable(game_module_probe game_module_probe.cpp)
target_compile_features(game_module_probe PRIVATE cxx_std_17)
target_include_directories(game_module_probe PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/common")
target_link_libraries(game_module_probe PRIVATE ${CMAKE_DL_LIBS})
if(MSVC)
  target_compile_options(game_module_probe PRIVATE /W4 /WX /EHsc)
endif()
if(WIN32)
  # Owned engine callbacks drive the rebuilt CS DLL's real C4 save/restore/think paths.
  # Execution is explicit; this is not an ordinary CTest module-loading fixture.
  add_executable(c4_module_probe c4_module_probe.cpp)
  target_compile_features(c4_module_probe PRIVATE cxx_std_17)
  target_compile_definitions(c4_module_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_include_directories(c4_module_probe PRIVATE
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/common"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/pm_shared"
    "${CSE_ROOT}/GoldSrc-SDK/common"
    "${CSE_ROOT}/GoldSrc-SDK/engine"
    "${CSE_ROOT}/GoldSrc-SDK/public")
  if(MSVC)
    target_compile_options(c4_module_probe PRIVATE /W3 /EHsc)
  endif()
endif()

if(WIN32)
  foreach(game hl cs)
    add_executable(world_module_probe_${game} world_module_probe.cpp)
    target_compile_features(world_module_probe_${game} PRIVATE cxx_std_17)
    target_compile_definitions(world_module_probe_${game} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(game STREQUAL cs)
      target_include_directories(world_module_probe_${game} PRIVATE
        "${CSE_ROOT}/GoldSrc-SDK/cstrike/dlls"
        "${CSE_ROOT}/GoldSrc-SDK/cstrike/pm_shared")
    endif()
    target_include_directories(world_module_probe_${game} PRIVATE
      "${CSE_ROOT}/GoldSrc-SDK/dlls"
      "${CSE_ROOT}/GoldSrc-SDK/pm_shared"
      "${CSE_ROOT}/GoldSrc-SDK/common"
      "${CSE_ROOT}/GoldSrc-SDK/engine"
      "${CSE_ROOT}/GoldSrc-SDK/public")
    if(MSVC)
      target_compile_options(world_module_probe_${game} PRIVATE /W3 /EHsc)
    endif()
  endforeach()
endif()
