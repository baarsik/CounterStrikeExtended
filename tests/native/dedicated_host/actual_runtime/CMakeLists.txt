cmake_minimum_required(VERSION 3.25)
project(ActualDedicatedRuntime LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Native minimum")
get_filename_component(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(SDK "${SOURCE_ROOT}/GoldSrc-SDK")
if(NOT CSE_ACTUAL_RUNTIME)
  message(FATAL_ERROR "Require hash-bound actual owning runtime; execution is controller-owned")
endif()
set(cohort "${CMAKE_CURRENT_BINARY_DIR}/cohort/cse_native_cohort_images.h")
set(standalone "${CMAKE_CURRENT_BINARY_DIR}/cohort/cse_standalone_cohort_images.h")
add_custom_command(OUTPUT "${cohort}" "${standalone}"
  COMMAND "${CMAKE_COMMAND}" "-DCSE_ENGINE=${CSE_ACTUAL_RUNTIME}/hw.dylib"
    "-DCSE_SOFTWARE_ENGINE=${CSE_ACTUAL_RUNTIME}/swds.dylib" "-DCSE_VGUI=${CSE_ACTUAL_RUNTIME}/vgui.dylib"
    "-DCSE_HEADER=${cohort}" -P "${SOURCE_ROOT}/cmake/WriteNativeCohort.cmake"
  COMMAND "${CMAKE_COMMAND}" "-DCSE_VGUI2=${CSE_ACTUAL_RUNTIME}/vgui2.dylib"
    "-DCSE_FILESYSTEM=${CSE_ACTUAL_RUNTIME}/filesystem_stdio.dylib"
    "-DCSE_HEADER=${standalone}" -P "${SOURCE_ROOT}/cmake/WriteStandaloneCohort.cmake"
  DEPENDS "${CSE_ACTUAL_RUNTIME}/hw.dylib" "${CSE_ACTUAL_RUNTIME}/swds.dylib"
    "${CSE_ACTUAL_RUNTIME}/vgui.dylib" "${CSE_ACTUAL_RUNTIME}/vgui2.dylib"
    "${CSE_ACTUAL_RUNTIME}/filesystem_stdio.dylib")
set_source_files_properties("${SDK}/engine/mem.c" PROPERTIES LANGUAGE CXX)
add_executable(actual_dedicated_runtime main.cpp "${cohort}" "${standalone}"
  "${SDK}/dedicated/native/DedicatedSession.cpp" "${SDK}/dedicated/native/DedicatedModuleSet.cpp"
  "${SDK}/dedicated/native/Signals.cpp" "${SDK}/dedicated/native/TextConsolePosix.cpp"
  "${SDK}/launcher/native/MacRuntimeSession.cpp" "${SDK}/launcher/native/VGui1HostImageOwner.cpp"
  "${SDK}/launcher/native/SocketImageOwner.cpp"
  "${SDK}/engine/interface.cpp" "${SDK}/engine/mem.c" "${SDK}/common/ObjectList.cpp"
  "${SDK}/common/textconsole.cpp" "${SDK}/common/commandline.cpp"
  "${SOURCE_ROOT}/tests/native/dedicated_host/gui_boundary.cpp")
target_include_directories(actual_dedicated_runtime PRIVATE "${SDK}" "${SDK}/public" "${SDK}/engine"
  "${CMAKE_CURRENT_BINARY_DIR}/cohort")
target_compile_definitions(actual_dedicated_runtime PRIVATE CSE_NATIVE_RUNTIME_BOOTSTRAP=1)
target_compile_options(actual_dedicated_runtime PRIVATE -include "${SDK}/public/cse_posix_crt.h"
  "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Release>:-O2>" -fexceptions -frtti
  -ffp-model=precise -ffp-contract=off -Wno-overriding-option
  -Werror=shorten-64-to-32 -Werror=pointer-to-int-cast)
set_target_properties(actual_dedicated_runtime PROPERTIES ENABLE_EXPORTS ON)
# Deliberately no add_test: real Steam/game/GUI orchestration belongs to the controller.
