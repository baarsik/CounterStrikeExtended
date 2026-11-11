cmake_minimum_required(VERSION 3.25)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Dedicated native minimum macOS")
project(DedicatedHostTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
get_filename_component(SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../GoldSrc-SDK" ABSOLUTE)
add_executable(host_tests host_tests.cpp "${SDK}/common/commandline.cpp")
target_include_directories(host_tests PRIVATE "${SDK}")
add_test(NAME argv_limit_before_init COMMAND host_tests argv)
add_test(NAME console_flag_without_value COMMAND host_tests flag)
add_executable(console_probe console_probe.cpp "${SDK}/dedicated/native/TextConsolePosix.cpp"
  "${SDK}/dedicated/native/Signals.cpp" "${SDK}/common/textconsole.cpp" "${SDK}/common/ObjectList.cpp")
target_include_directories(console_probe PRIVATE "${SDK}" "${SDK}/public")
target_compile_definitions(console_probe PRIVATE _snprintf=snprintf)
foreach(case pipe_eof_continues pty_restore_signals closed_descriptors repeated_open)
  add_test(NAME ${case} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/console_tests.py"
    "$<TARGET_FILE:console_probe>" ${case})
endforeach()
target_compile_options(host_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h")
target_include_directories(console_probe PRIVATE "${SDK}/engine")
set_source_files_properties("${SDK}/engine/mem.c" PROPERTIES LANGUAGE CXX)
target_sources(console_probe PRIVATE "${SDK}/engine/mem.c")
add_executable(handle_tests handle_tests.cpp)
target_include_directories(handle_tests PRIVATE "${SDK}")
add_test(NAME inherited_high_handles COMMAND handle_tests)
add_library(session_owner OBJECT "${SDK}/dedicated/native/DedicatedSession.cpp")
target_include_directories(session_owner PRIVATE "${SDK}/public" "${SDK}/engine")
add_library(fixture_engine SHARED provider.cpp)
set_target_properties(fixture_engine PROPERTIES PREFIX "" OUTPUT_NAME swds)
add_library(fixture_font SHARED font.cpp)
set_target_properties(fixture_font PROPERTIES PREFIX "" OUTPUT_NAME vgui)
add_library(fixture_filesystem SHARED filesystem.cpp)
set_target_properties(fixture_filesystem PROPERTIES PREFIX "" OUTPUT_NAME filesystem_stdio)
foreach(target fixture_engine fixture_filesystem)
  target_include_directories(${target} PRIVATE "${SDK}")
endforeach()
set(cohort "${CMAKE_CURRENT_BINARY_DIR}/cohort/cse_native_cohort_images.h")
add_custom_command(OUTPUT "${cohort}"
  COMMAND "${CMAKE_COMMAND}" "-DCSE_ENGINE=$<TARGET_FILE:fixture_engine>"
    "-DCSE_SOFTWARE_ENGINE=$<TARGET_FILE:fixture_engine>" "-DCSE_VGUI=$<TARGET_FILE:fixture_font>"
    "-DCSE_HEADER=${cohort}" -P "${SDK}/../cmake/WriteNativeCohort.cmake"
  DEPENDS fixture_engine fixture_font)
add_executable(session_tests session_tests.cpp "${cohort}"
  "${SDK}/dedicated/native/DedicatedSession.cpp" "${SDK}/dedicated/native/DedicatedModuleSet.cpp"
  "${SDK}/dedicated/native/Signals.cpp" "${SDK}/dedicated/native/TextConsolePosix.cpp"
  "${SDK}/launcher/native/MacRuntimeSession.cpp" "${SDK}/launcher/native/VGui1HostImageOwner.cpp"
  "${SDK}/engine/interface.cpp" "${SDK}/engine/mem.c" "${SDK}/common/ObjectList.cpp"
  "${SDK}/common/textconsole.cpp" "${SDK}/common/commandline.cpp")
target_include_directories(session_tests PRIVATE "${SDK}" "${SDK}/public" "${SDK}/engine"
  "${CMAKE_CURRENT_BINARY_DIR}/cohort")
target_compile_definitions(session_tests PRIVATE CSE_NATIVE_RUNTIME_BOOTSTRAP=1)
target_compile_options(session_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h")
set_target_properties(session_tests PROPERTIES ENABLE_EXPORTS ON)
add_dependencies(session_tests fixture_filesystem)
foreach(case cohort_reject_before_callbacks partial_init_retry shutdown_order)
  add_test(NAME ${case} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py"
    "$<TARGET_FILE:session_tests>" ${case})
endforeach()
target_sources(session_tests PRIVATE gui_boundary.cpp)
add_executable(signal_tests signal_tests.cpp "${SDK}/dedicated/native/Signals.cpp")
target_include_directories(signal_tests PRIVATE "${SDK}")
add_test(NAME signal_partial_install_rollback COMMAND signal_tests)
add_test(NAME actual_engine_partial_init_retirement COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../software_video/run_dedicated_owner_test.py"
  "${CMAKE_CURRENT_BINARY_DIR}/actual-owner" "$<IF:$<CONFIG:Release>,-Os,-O0>")
add_test(NAME physical_slot_negative_control COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/physical_slots.py")
add_test(NAME preconstructor_cohort_reject COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py" "$<TARGET_FILE:session_tests>" preconstructor_cohort_reject)
add_library(fixture_missing_retire SHARED provider.cpp)
target_include_directories(fixture_missing_retire PRIVATE "${SDK}")
target_compile_definitions(fixture_missing_retire PRIVATE CSE_TEST_MISSING_RETIRE=1)
set_target_properties(fixture_missing_retire PROPERTIES PREFIX "" OUTPUT_NAME swds
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/missing-runtime")
set(missing_cohort "${CMAKE_CURRENT_BINARY_DIR}/missing-cohort/cse_native_cohort_images.h")
add_custom_command(OUTPUT "${missing_cohort}"
  COMMAND "${CMAKE_COMMAND}" "-DCSE_ENGINE=$<TARGET_FILE:fixture_missing_retire>"
    "-DCSE_SOFTWARE_ENGINE=$<TARGET_FILE:fixture_missing_retire>" "-DCSE_VGUI=$<TARGET_FILE:fixture_font>"
    "-DCSE_HEADER=${missing_cohort}" -P "${SDK}/../cmake/WriteNativeCohort.cmake"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:fixture_font>" "${CMAKE_CURRENT_BINARY_DIR}/missing-runtime/vgui.dylib"
  DEPENDS fixture_missing_retire fixture_font)
get_target_property(session_sources session_tests SOURCES)
list(REMOVE_ITEM session_sources "${cohort}")
add_executable(missing_retire_tests ${session_sources} "${missing_cohort}")
target_include_directories(missing_retire_tests PRIVATE "${SDK}" "${SDK}/public" "${SDK}/engine"
  "${CMAKE_CURRENT_BINARY_DIR}/missing-cohort")
target_compile_definitions(missing_retire_tests PRIVATE CSE_NATIVE_RUNTIME_BOOTSTRAP=1)
target_compile_options(missing_retire_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h")
set_target_properties(missing_retire_tests PROPERTIES ENABLE_EXPORTS ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/missing-runtime")
add_test(NAME missing_retirement_export COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py" "$<TARGET_FILE:missing_retire_tests>" missing_retirement_export)
add_test(NAME retirement_refuses_unload COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py" "$<TARGET_FILE:session_tests>" retirement_refuses_unload)
add_test(NAME wrong_actual_view COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py" "$<TARGET_FILE:session_tests>" wrong_actual_view)

foreach(target host_tests console_probe handle_tests session_owner fixture_engine fixture_font fixture_filesystem
    session_tests signal_tests fixture_missing_retire missing_retire_tests)
  target_compile_options(${target} PRIVATE "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Release>:-O2>"
    -fexceptions -frtti -ffp-model=precise -ffp-contract=off -Wno-overriding-option
    -Werror=shorten-64-to-32 -Werror=pointer-to-int-cast)
endforeach()

add_test(NAME engine_argv_roundtrip COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_engine_arguments.py" "${CMAKE_CURRENT_BINARY_DIR}/actual-arguments"
  "$<IF:$<CONFIG:Release>,-Os,-O0>")
add_test(NAME retirement_restores_pty COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py" "$<TARGET_FILE:session_tests>" retirement_restores_pty)
include("${CMAKE_CURRENT_SOURCE_DIR}/WindowsResult.cmake")
add_test(NAME excess_arguments_before_effects COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/session_tests.py" "$<TARGET_FILE:session_tests>" excess_arguments_before_effects)

add_test(NAME actual_startup_command_roundtrip COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_startup_commands.py" "${CMAKE_CURRENT_BINARY_DIR}/actual-startup"
  "$<IF:$<CONFIG:Release>,-Os,-O0>")
