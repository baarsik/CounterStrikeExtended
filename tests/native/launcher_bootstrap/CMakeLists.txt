cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseLauncherBootstrap LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
if(NOT APPLE)
  message(FATAL_ERROR "This fixture executes the owned macOS bootstrap only")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CSE_BOOTSTRAP_FILESYSTEM "" CACHE FILEPATH "Qualified actual owned filesystem dylib")
set(CSE_BOOTSTRAP_RUNTIME "" CACHE PATH "Optional complete actual runtime for pre-Run startup checks")
option(CSE_BOOTSTRAP_SANITIZERS "Instrument actual launcher and fixture-owned command handlers" OFF)
option(CSE_BOOTSTRAP_HIDE_INLINES "Match approved Mac launcher private inline symbol isolation" ON)
enable_testing()
if(NOT EXISTS "${CSE_BOOTSTRAP_FILESYSTEM}")
  message(FATAL_ERROR "Actual filesystem provider is required")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}"
  "${CSE_ROOT}/tests/native/launcher_entry/manifest_sources.py" "${CSE_ROOT}" "${CSE_TARGET_OS}"
  "${CMAKE_CURRENT_BINARY_DIR}/manifest_sources.cmake" "${CSE_TARGET_ARCH}" COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_CURRENT_BINARY_DIR}/manifest_sources.cmake")
include("${CSE_ROOT}/tests/native/CMakeInventoryDependencies.cmake")
cse_fixture_inventory_dependencies("${CSE_ROOT}"
  "${CSE_ROOT}/tests/native/launcher_entry/manifest_sources.py"
  "${CSE_ROOT}/tests/native/filesystem/manifest_sources.py")
add_executable(bootstrap_launcher ${CSE_LAUNCHER_SOURCES})
add_executable(bootstrap_probe ${CSE_LAUNCHER_SOURCES} probe.cpp)
target_compile_definitions(bootstrap_probe PRIVATE main=CseActualLauncherMain)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_menu_restart.py"
  "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/extract_menu_restart.py"
  "${CSE_ROOT}/src/launcher/launcher.cpp"
  "${CSE_ROOT}/src/engine/host.cpp" "${CSE_ROOT}/src/engine/host_cmd.cpp" "${CSE_ROOT}/src/engine/sys_getmodes.cpp"
  "${CSE_ROOT}/src/engine/registry.cpp" "${CSE_ROOT}/src/gameui/OptionsSubVideo.cpp")
add_library(bootstrap_engine SHARED dummy_engine.cpp menu_restart.cpp)
add_executable(commandline_checks commandline_checks.cpp "${CSE_ROOT}/src/launcher/common/commandline.cpp")
add_executable(menu_restart_checks menu_restart.cpp)
add_executable(menu_restart_glonly_checks menu_restart.cpp)
add_executable(menu_restart_win32_checks menu_restart.cpp)
target_compile_definitions(bootstrap_engine PRIVATE CSE_ENABLE_METAL=1)
target_compile_definitions(menu_restart_checks PRIVATE CSE_MENU_CHECKS=1 CSE_ENABLE_METAL=1)
target_compile_definitions(menu_restart_glonly_checks PRIVATE CSE_MENU_CHECKS=1)
target_compile_definitions(menu_restart_win32_checks PRIVATE CSE_MENU_CHECKS=1 CSE_MENU_WIN32_BEHAVIOR=1)
foreach(target bootstrap_engine menu_restart_checks menu_restart_glonly_checks menu_restart_win32_checks commandline_checks)
  target_compile_options(${target} PRIVATE -UNDEBUG)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()

if(CSE_BOOTSTRAP_RUNTIME)
  set(cohort_header "${CMAKE_CURRENT_BINARY_DIR}/runtime-cohort/cse_native_cohort_images.h")
  add_custom_command(OUTPUT "${cohort_header}"
    COMMAND "${CMAKE_COMMAND}" "-DCSE_ENGINE=${CSE_BOOTSTRAP_RUNTIME}/hw.dylib"
      "-DCSE_SOFTWARE_ENGINE=${CSE_BOOTSTRAP_RUNTIME}/swds.dylib"
      "-DCSE_VGUI=${CSE_BOOTSTRAP_RUNTIME}/vgui.dylib" "-DCSE_HEADER=${cohort_header}"
      -P "${CSE_ROOT}/cmake/WriteNativeCohort.cmake"
    DEPENDS "${CSE_BOOTSTRAP_RUNTIME}/hw.dylib" "${CSE_BOOTSTRAP_RUNTIME}/swds.dylib"
      "${CSE_BOOTSTRAP_RUNTIME}/vgui.dylib" "${CSE_ROOT}/cmake/WriteNativeCohort.cmake" VERBATIM)
  set(runtime_probe_sources ${CSE_LAUNCHER_SOURCES})
  list(REMOVE_ITEM runtime_probe_sources "${CSE_ROOT}/src/launcher/launcher.cpp")
  add_executable(bootstrap_runtime_probe ${runtime_probe_sources}
    "${CMAKE_CURRENT_BINARY_DIR}/runtime_launcher.cpp" runtime_probe.cpp "${cohort_header}")
  target_compile_features(bootstrap_runtime_probe PRIVATE cxx_std_17)
  target_compile_definitions(bootstrap_runtime_probe PRIVATE CSE_NATIVE_RUNTIME_BOOTSTRAP=1
    main=CseActualLauncherMain _NSGetExecutablePath=CseFixtureExecutablePath
    realpath=CseFixtureRealpath chdir=CseFixtureChdir)
  target_compile_options(bootstrap_runtime_probe PRIVATE -UNDEBUG -fvisibility-inlines-hidden)
  target_include_directories(bootstrap_runtime_probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/runtime-cohort" "${CSE_ROOT}/src/launcher"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src")
  if(CSE_BOOTSTRAP_SANITIZERS)
    target_compile_options(bootstrap_runtime_probe PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(bootstrap_runtime_probe PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME launcher-runtime-working-directory COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime_tests.py" "$<TARGET_FILE:bootstrap_runtime_probe>"
    "${CSE_BOOTSTRAP_RUNTIME}")
  set_tests_properties(launcher-runtime-working-directory PROPERTIES TIMEOUT 120)
endif()
add_library(bootstrap_mismatch SHARED mismatch_filesystem.cpp)
add_library(bootstrap_nullfs SHARED mismatch_filesystem.cpp)
target_compile_definitions(bootstrap_nullfs PRIVATE CSE_BOOTSTRAP_NULL_FACTORY=1)
set(targets bootstrap_launcher bootstrap_probe bootstrap_engine bootstrap_mismatch bootstrap_nullfs
  menu_restart_checks menu_restart_glonly_checks menu_restart_win32_checks commandline_checks)
foreach(target IN LISTS targets)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_include_directories(${target} PRIVATE "${CSE_ROOT}/src/launcher"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src")
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unknown-pragmas
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
  target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
endforeach()
set_target_properties(bootstrap_launcher bootstrap_probe PROPERTIES CXX_STANDARD "${CSE_LAUNCHER_CXX_STANDARD}")
if(CSE_BOOTSTRAP_HIDE_INLINES)
  foreach(target bootstrap_launcher bootstrap_probe)
    target_compile_options(${target} PRIVATE -fvisibility-inlines-hidden)
  endforeach()
endif()
if(CSE_BOOTSTRAP_SANITIZERS)
  foreach(target bootstrap_launcher bootstrap_probe bootstrap_engine menu_restart_checks menu_restart_glonly_checks menu_restart_win32_checks commandline_checks)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()
add_test(NAME launcher-canonical-exact-parameters COMMAND commandline_checks)
add_test(NAME launcher-native-menu-intent COMMAND menu_restart_checks)
add_test(NAME launcher-glonly-menu-intent COMMAND menu_restart_glonly_checks)
add_test(NAME launcher-retained-win32-menu-intent COMMAND menu_restart_win32_checks)
add_test(NAME launcher-bootstrap-entry-limits COMMAND "${Python3_EXECUTABLE}"
  "${CSE_ROOT}/tests/native/launcher_entry/entry_tests.py" "$<TARGET_FILE:bootstrap_launcher>")
foreach(case normal explicit-game alias relative-argv long-path software restart borrowed retained mismatch null-interface
    metal-cached-software restart-metal-to-gl restart-gl-to-metal restart-options restart-software restart-to-software restart-to-hardware restart-exec-failure
    menu-software menu-to-software menu-to-hardware menu-metal-to-gl menu-gl-to-metal command-size-only command-renderer-only)
  add_test(NAME launcher-bootstrap-${case} COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap_tests.py" --launcher "$<TARGET_FILE:bootstrap_launcher>"
    --probe "$<TARGET_FILE:bootstrap_probe>" --engine "$<TARGET_FILE:bootstrap_engine>"
    --mismatch "$<TARGET_FILE:bootstrap_mismatch>" --nullfs "$<TARGET_FILE:bootstrap_nullfs>"
    --filesystem "${CSE_BOOTSTRAP_FILESYSTEM}" --case ${case})
  set_tests_properties(launcher-bootstrap-${case} PROPERTIES TIMEOUT 20)
endforeach()
