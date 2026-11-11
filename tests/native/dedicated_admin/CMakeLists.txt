cmake_minimum_required(VERSION 3.25)
project(DedicatedAdminTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
get_filename_component(SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../GoldSrc-SDK" ABSOLUTE)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/admin_actual.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_admin.py" "${SDK}"
    "${CMAKE_CURRENT_BINARY_DIR}/admin_actual.inc"
  DEPENDS "${SDK}/dedicated/AdminServer.cpp" "${SDK}/dedicated/AdminServer.h" extract_admin.py)
add_executable(admin_tests admin_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/admin_actual.inc" "${SDK}/engine/utlbuffer.cpp")
target_include_directories(admin_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${SDK}/engine" "${SDK}/public")
target_compile_options(admin_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h"
  -fexceptions -frtti -ffp-model=precise -ffp-contract=off)
foreach(case admin_bind_failure_retry response_self_next_removal old_generation_revoked close_during_response admin_command_roundtrip malformed_packet_rejected)
  add_test(NAME ${case} COMMAND admin_tests ${case})
endforeach()
add_executable(provider_tests provider_tests.cpp "${SDK}/engine/utlbuffer.cpp" "${SDK}/engine/interface.cpp")
target_include_directories(provider_tests PRIVATE "${SDK}/engine" "${SDK}/public" "${SDK}/common" "${SDK}/cl_dll" "${SDK}/pm_shared")
target_compile_options(provider_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h" -fexceptions -frtti
  -ffp-model=precise -ffp-contract=off)
add_test(NAME provider_malformed COMMAND provider_tests provider_malformed)
add_test(NAME provider_operations COMMAND provider_tests provider_operations)
add_executable(files_tests files_tests.cpp)
target_include_directories(files_tests PRIVATE "${SDK}")
if(EXISTS "${SDK}/dedicated/native/AdminFiles.cpp")
  target_sources(files_tests PRIVATE "${SDK}/dedicated/native/AdminFiles.cpp")
endif()
add_test(NAME native_file_dialog_paths COMMAND files_tests)
add_executable(commands_tests commands_tests.cpp)
target_include_directories(commands_tests PRIVATE "${SDK}")
add_test(NAME startup_command_quoting COMMAND commands_tests)
add_executable(provider_state_tests provider_state_tests.cpp)
target_include_directories(provider_state_tests PRIVATE "${SDK}" "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME provider_state_not_checkbox COMMAND provider_state_tests)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/provider_state_actual.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_provider_state.py"
    "${SDK}/engine/sv_steam3.cpp" "${CMAKE_CURRENT_BINARY_DIR}/provider_state_actual.inc"
  DEPENDS "${SDK}/engine/sv_steam3.cpp" extract_provider_state.py)
target_sources(provider_state_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/provider_state_actual.inc")
add_executable(roundtrip_tests roundtrip_tests.cpp provider_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/admin_actual.inc"
  "${SDK}/engine/utlbuffer.cpp" "${SDK}/engine/interface.cpp")
target_compile_definitions(roundtrip_tests PRIVATE CSE_PROVIDER_NO_MAIN=1)
target_include_directories(roundtrip_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${SDK}/engine" "${SDK}/public"
  "${SDK}/common" "${SDK}/cl_dll" "${SDK}/pm_shared")
target_compile_options(roundtrip_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h" -fexceptions -frtti
  -ffp-model=precise -ffp-contract=off)
add_test(NAME actual_admin_provider_roundtrip COMMAND roundtrip_tests)

foreach(target admin_tests provider_tests files_tests commands_tests provider_state_tests roundtrip_tests)
  target_compile_options(${target} PRIVATE "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Release>:-O2>")
endforeach()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gui_stop_actual.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_gui_stop.py"
    "${SDK}/dedicated/native/NativeGui.cpp" "${CMAKE_CURRENT_BINARY_DIR}/gui_stop_actual.inc"
  DEPENDS "${SDK}/dedicated/native/NativeGui.cpp" extract_gui_stop.py)
add_executable(gui_stop_tests gui_stop_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/gui_stop_actual.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/admin_actual.inc" "${SDK}/engine/utlbuffer.cpp")
target_include_directories(gui_stop_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${SDK}/engine" "${SDK}/public")
target_compile_options(gui_stop_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h" -fexceptions -frtti
  "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Release>:-O2>" -ffp-model=precise -ffp-contract=off)
add_test(NAME retired_gui_preserves_new_session COMMAND gui_stop_tests)
set(CSE_ADMIN_OWNER_BUILD "" CACHE PATH "Optional completed actual hlds owning graph for retained GUI fixture")
set(CSE_ADMIN_RESOURCE_ROOT "" CACHE PATH "Copied isolated retained platform resources")
if(CSE_ADMIN_OWNER_BUILD)
  set(owner_objects "${CSE_ADMIN_OWNER_BUILD}/targets/hlds/CMakeFiles/hlds.dir/__/__/__/GoldSrc-SDK")
  file(GLOB control_objects "${owner_objects}/vgui2/controls/*.o")
  foreach(source controls.cpp interface.cpp mem.c KeyValues.cpp utlbuffer.cpp utlsymbol.cpp MemPool.cpp)
    list(APPEND control_objects "${owner_objects}/engine/${source}.o")
  endforeach()
  list(APPEND control_objects "${owner_objects}/common/TextEntryClipboard.cpp.o"
    "${owner_objects}/common/legacy_wide_format.cpp.o" "${owner_objects}/dedicated/AdminServer.cpp.o"
    "${owner_objects}/dedicated/native/AdminFiles.cpp.o")
  add_executable(gui_tests gui_tests.cpp provider_tests.cpp ${control_objects})
  target_compile_definitions(gui_tests PRIVATE CSE_PROVIDER_NO_MAIN=1)
  target_compile_options(gui_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h" -fexceptions -frtti)
  target_include_directories(gui_tests PRIVATE "${SDK}" "${SDK}/engine" "${SDK}/public"
    "${SDK}/vgui2/controls" "${SDK}/vgui2/src/public" "${SDK}/common" "${SDK}/cl_dll" "${SDK}/pm_shared"
    "${CSE_ADMIN_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
  target_link_libraries(gui_tests PRIVATE
    "${CSE_ADMIN_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
  add_test(NAME actual_admin_frame_close_reopen COMMAND gui_tests "${CSE_ADMIN_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
    "${CSE_ADMIN_RESOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/isolated-config")
  set_tests_properties(actual_admin_frame_close_reopen PROPERTIES ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;CSE_DEDICATED_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/isolated-config")
endif()

add_test(NAME retained_runtime_resources COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/resource_tests.py")

add_test(NAME oversized_response_queue COMMAND provider_tests oversized_response_queue)

if(CSE_ADMIN_OWNER_BUILD)
  set(setup_control_objects ${control_objects})
  list(FILTER setup_control_objects EXCLUDE REGEX "/dedicated/native/AdminFiles.cpp.o$")
  add_executable(setup_tests setup_tests.cpp provider_tests.cpp ${setup_control_objects}
    "${SDK}/dedicated/native/AdminFiles.cpp"
    "${SDK}/dedicated/CreateMultiplayerGameServerPage.cpp" "${SDK}/dedicated/native/NativeGui.cpp"
    "${SDK}/dedicated/native/DedicatedSession.cpp" "${SDK}/dedicated/native/Signals.cpp"
    "${SDK}/dedicated/native/TextConsolePosix.cpp" "${SDK}/launcher/native/SocketImageOwner.cpp"
    "${SDK}/common/textconsole.cpp" "${SDK}/common/ObjectList.cpp" "${SDK}/common/commandline.cpp")
  target_compile_definitions(setup_tests PRIVATE CSE_PROVIDER_NO_MAIN=1)
  target_compile_options(setup_tests PRIVATE -include "${SDK}/public/cse_posix_crt.h" -fexceptions -frtti)
  target_include_directories(setup_tests PRIVATE "${SDK}/dedicated" "${SDK}/cstrike/common"
    "${SDK}/cstrike/public" "${SDK}/vgui2/controls" "${SDK}/cl_dll" "${SDK}/engine"
    "${SDK}/vgui2/src/public" "${SDK}/common" "${SDK}/public" "${SDK}" "${SDK}/pm_shared"
    "${CSE_ADMIN_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
  target_link_libraries(setup_tests PRIVATE
    "${CSE_ADMIN_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
  foreach(case selection text config bounds)
    add_test(NAME actual_setup_${case} COMMAND setup_tests "${CSE_ADMIN_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
      "${CSE_ADMIN_RESOURCE_ROOT}" ${case})
    set_tests_properties(actual_setup_${case} PROPERTIES ENVIRONMENT
      "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software")
  endforeach()
endif()
