cmake_minimum_required(VERSION 3.25)
project(CseExtendedStatsTests LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
option(CSE_EXTSTATS_SANITIZERS "Enable address and undefined behavior sanitizers" ON)
add_executable(extended_stats_tests protocol_tests.cpp)
target_compile_features(extended_stats_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_tests PRIVATE "${CSE_ROOT}/src/public")
if(MSVC)
  target_compile_options(extended_stats_tests PRIVATE /W4 /WX)
else()
  target_compile_options(extended_stats_tests PRIVATE -Wall -Wextra -Werror)
  if(CSE_EXTSTATS_SANITIZERS)
    target_compile_options(extended_stats_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(extended_stats_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()
enable_testing()
add_test(NAME conversion_boundaries COMMAND extended_stats_tests conversion)
add_test(NAME wire_contract COMMAND extended_stats_tests wire)
add_executable(extended_stats_server_tests server_tests.cpp)
target_compile_features(extended_stats_server_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_server_tests PRIVATE "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/amxx/cse_extstats")
if(EXISTS "${CSE_ROOT}/src/amxx/cse_extstats/stats_state.cpp")
  target_sources(extended_stats_server_tests PRIVATE "${CSE_ROOT}/src/amxx/cse_extstats/stats_state.cpp")
endif()
foreach(scenario IN ITEMS native_setters identity_vs_protocol query_timeout reused_slot_reply completed_query_ids nested_publication
    automatic_changes manual_mode live_mode_toggle lifecycle_force recipient_permissions hltv_stable_snapshot
    deferred_native_operations publication_reentrancy legacy_manual_refresh)
  add_test(NAME ${scenario} COMMAND extended_stats_server_tests ${scenario})
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(draw_inputs extract_draw.py)
if(EXISTS "${CSE_ROOT}/src/cstrike/cl_dll/hud_stat_numbers.cpp")
  list(APPEND draw_inputs "${CSE_ROOT}/src/cstrike/cl_dll/hud_stat_numbers.cpp")
endif()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/draw_bodies.inc" "${CMAKE_CURRENT_BINARY_DIR}/spectator_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_draw.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/draw_bodies.inc"
  DEPENDS ${draw_inputs} "${CSE_ROOT}/src/cstrike/cl_dll/VGUI/cstrikespectatorgui.cpp" VERBATIM)
add_executable(extended_stats_draw_tests draw_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/draw_bodies.inc")
target_compile_features(extended_stats_draw_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_draw_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/cstrike/cl_dll")
add_test(NAME stat_digit_sprites COMMAND extended_stats_draw_tests digits)
add_test(NAME stat_layout COMMAND extended_stats_draw_tests layout)
add_executable(extended_stats_spectator_tests spectator_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/spectator_bodies.inc")
target_compile_features(extended_stats_spectator_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_spectator_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/cstrike/cl_dll")
add_test(NAME spectator_stats COMMAND extended_stats_spectator_tests spectator)
add_test(NAME localized_stats COMMAND extended_stats_spectator_tests localized)
add_executable(money_hud_layout_tests money_hud_layout_tests.cpp)
target_compile_features(money_hud_layout_tests PRIVATE cxx_std_17)
target_include_directories(money_hud_layout_tests PRIVATE "${CSE_ROOT}/src/cstrike/cl_dll")
add_test(NAME money_six_digit_layout COMMAND money_hud_layout_tests)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bridge_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_bridge.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/bridge_bodies.inc"
  DEPENDS extract_bridge.py "${CSE_ROOT}/src/cstrike/cl_dll/extended_stats.cpp"
    "${CSE_ROOT}/src/cstrike/cl_dll/health.cpp" "${CSE_ROOT}/src/cstrike/cl_dll/battery.cpp"
    "${CSE_ROOT}/src/cstrike/cl_dll/view.cpp" "${CSE_ROOT}/src/cstrike/cl_dll/hl/hl_weapons.cpp" VERBATIM)
add_executable(extended_stats_bridge_tests bridge_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/bridge_bodies.inc")
target_compile_features(extended_stats_bridge_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_bridge_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/cstrike/cl_dll")
add_test(NAME legacy_local_isolation COMMAND extended_stats_bridge_tests legacy)
add_test(NAME healthinfo_local_fallback COMMAND extended_stats_bridge_tests healthinfo)
add_test(NAME fractional_health_minimum_bridge COMMAND extended_stats_bridge_tests fractional_health)
add_test(NAME spectator_refresh COMMAND extended_stats_bridge_tests spectator)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/registration_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_registration.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/registration_bodies.inc"
  DEPENDS extract_registration.py "${CSE_ROOT}/src/amxx/cse_extstats/module.cpp" VERBATIM)
add_executable(extended_stats_registration_tests registration_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/registration_bodies.inc")
target_compile_features(extended_stats_registration_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_registration_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CSE_ROOT}/src/public")
add_test(NAME message_hook_interoperability COMMAND extended_stats_registration_tests normal)
add_test(NAME conflicting_message_registration COMMAND extended_stats_registration_tests conflict)
add_executable(extended_stats_legacy_tests legacy_tests.cpp)
target_compile_features(extended_stats_legacy_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_legacy_tests PRIVATE "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/amxx/cse_extstats")
add_test(NAME legacy_normalization COMMAND extended_stats_legacy_tests)
add_executable(extended_stats_client_tests client_tests.cpp)
target_compile_features(extended_stats_client_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_client_tests PRIVATE "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/cstrike/cl_dll")
foreach(scenario IN ITEMS message_validation legacy_precedence invalidation_fallback local_alive_state
    scoreboard_health_fallback fractional_health_minimum client_lifecycle)
  add_test(NAME ${scenario} COMMAND extended_stats_client_tests ${scenario})
endforeach()

add_test(NAME legacy_trailing_payload COMMAND extended_stats_bridge_tests trailing)
if(NOT MSVC AND CSE_EXTSTATS_SANITIZERS)
  foreach(test_target IN ITEMS extended_stats_server_tests extended_stats_client_tests extended_stats_legacy_tests
      extended_stats_draw_tests extended_stats_spectator_tests extended_stats_bridge_tests extended_stats_registration_tests)
    target_compile_options(${test_target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${test_target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()

add_test(NAME qualification_evidence COMMAND "${Python3_EXECUTABLE}" -m unittest discover
  -s "${CMAKE_CURRENT_SOURCE_DIR}" -p test_probe.py)

add_test(NAME late_legacy_registration COMMAND extended_stats_registration_tests late)
add_test(NAME demo_bridge_reversal COMMAND extended_stats_bridge_tests demo)

add_test(NAME spectator_language_replay COMMAND extended_stats_spectator_tests replay)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/legacy_hook_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_legacy_hooks.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/legacy_hook_bodies.inc"
  DEPENDS extract_legacy_hooks.py "${CSE_ROOT}/src/amxx/cse_extstats/legacy_messages.cpp" VERBATIM)
add_executable(extended_stats_legacy_hook_tests legacy_hook_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/legacy_hook_bodies.inc"
  "${CSE_ROOT}/src/amxx/cse_extstats/stats_state.cpp")
target_compile_features(extended_stats_legacy_hook_tests PRIVATE cxx_std_17)
target_include_directories(extended_stats_legacy_hook_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/amxx/cse_extstats")
if(NOT MSVC AND CSE_EXTSTATS_SANITIZERS)
  target_compile_options(extended_stats_legacy_hook_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(extended_stats_legacy_hook_tests PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME buffered_legacy_hooks COMMAND extended_stats_legacy_hook_tests)

add_test(NAME wide_predicted_life COMMAND extended_stats_bridge_tests wide_life)
add_test(NAME wide_dead_view_roll COMMAND extended_stats_bridge_tests wide_view)
add_test(NAME hud_startup_reset COMMAND extended_stats_bridge_tests startup)
