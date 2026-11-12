cmake_minimum_required(VERSION 3.25)
project(vgui2_standalone LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()
set(CSE_OWNER_BUILD "" CACHE PATH "Actual retained runtime build directory")
if(NOT EXISTS "${CSE_OWNER_BUILD}/CMakeCache.txt")
  message(FATAL_ERROR "Set CSE_OWNER_BUILD to the retained source-SDL runtime build")
endif()
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(surface_tests surface_tests.cpp)
target_compile_features(surface_tests PRIVATE cxx_std_17)
target_include_directories(surface_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public"
  "${CSE_ROOT}/GoldSrc-SDK/public")
add_test(NAME standalone_surface_present COMMAND surface_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}/vgui2.dylib")
set(CSE_RESOURCE_ROOT "${CSE_ROOT}/build/vgui2-standalone-assets" CACHE PATH "Isolated retained platform resources")
set(owner_objects "${CSE_OWNER_BUILD}/targets/hlds/CMakeFiles/hlds.dir/__/__/__/GoldSrc-SDK")
file(GLOB control_objects "${owner_objects}/vgui2/controls/*.o")
foreach(source IN ITEMS controls.cpp interface.cpp mem.c KeyValues.cpp utlbuffer.cpp utlsymbol.cpp MemPool.cpp)
  list(APPEND control_objects "${owner_objects}/engine/${source}.o")
endforeach()
list(APPEND control_objects "${owner_objects}/common/TextEntryClipboard.cpp.o"
  "${owner_objects}/common/legacy_wide_format.cpp.o")
add_executable(control_tests control_tests.cpp ${control_objects})
target_compile_features(control_tests PRIVATE cxx_std_17)
target_compile_options(control_tests PRIVATE -include "${CSE_ROOT}/GoldSrc-SDK/public/cse_posix_crt.h")
target_include_directories(control_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/engine"
  "${CSE_ROOT}/GoldSrc-SDK/public" "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public"
  "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
target_link_libraries(control_tests PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib" objc)
foreach(case IN ITEMS mid_batch_resize_intent mid_batch_resize_intent_alternate command_edit_accelerators command_modifier_order callback_resize_retirement callback_scheme_retirement callback_think_retirement mouse_button_retirement shift_click_event_order popup_event_generation fractional_wheel_controls focus_capture_and_text resize_hidpi_clip glyph_texture_pixels surface_partial_failure_reopen)
  add_test(NAME ${case} COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
    "${CSE_RESOURCE_ROOT}" ${case})
  set_tests_properties(${case} PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software")
endforeach()
add_executable(lifetime_tests lifetime_tests.cpp
  "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/native/StandaloneWindow.cpp"
  "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/native/StandaloneRaster.cpp")
target_compile_features(lifetime_tests PRIVATE cxx_std_17)
target_include_directories(lifetime_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
target_compile_options(lifetime_tests PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(lifetime_tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(lifetime_tests PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
add_test(NAME renderer_lifetime_sanitizers COMMAND lifetime_tests)
set_tests_properties(renderer_lifetime_sanitizers PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software")
add_library(sdl_failures SHARED sdl_failures.cpp)
target_compile_features(sdl_failures PRIVATE cxx_std_17)
target_include_directories(sdl_failures PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
target_link_libraries(sdl_failures PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
set_tests_properties(surface_partial_failure_reopen PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:sdl_failures>")
foreach(case IN ITEMS resize_intent_sequences resize_external_clamp_density resize_backing_failure_recovery
    resize_boundary_outcomes
    resize_reentrant_size_callback resize_sync_reentrancy resize_sync_shutdown resize_old_id_and_pending_move)
  add_test(NAME ${case} COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
    "${CSE_RESOURCE_ROOT}" ${case})
  set_tests_properties(${case} PROPERTIES ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:sdl_failures>")
endforeach()
add_test(NAME retained_controls_resources COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
  "${CSE_RESOURCE_ROOT}" controls)
set_tests_properties(retained_controls_resources PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;CSE_TEST_PIXEL_RECEIPT=${CMAKE_CURRENT_BINARY_DIR}/retained-controls.bmp;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:sdl_failures>")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME callback_lifetime_sanitizers COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/sanitized_callbacks.py" --owner "${CSE_OWNER_BUILD}"
  --fixture "${CMAKE_CURRENT_BINARY_DIR}" --output "${CMAKE_CURRENT_BINARY_DIR}/callback-sanitizers"
  --resources "${CSE_RESOURCE_ROOT}" --config "${CMAKE_BUILD_TYPE}")
add_library(wrong_role SHARED wrong_role.cpp)
add_library(wrong_contract SHARED wrong_role.cpp)
target_compile_definitions(wrong_contract PRIVATE CSE_TEST_WRONG_CONTRACT)
add_executable(admission_tests admission_tests.cpp)
foreach(target IN ITEMS admission_tests wrong_role wrong_contract)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/src" "${CSE_ROOT}/GoldSrc-SDK/public"
    "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
endforeach()
target_link_libraries(admission_tests PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
add_test(NAME complete_factory_admission COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_admission.py"
  "$<TARGET_FILE:admission_tests>" "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}" "$<TARGET_FILE:wrong_role>" "$<TARGET_FILE:wrong_contract>")
add_library(glyph_reference SHARED glyph_reference.cpp)
target_compile_features(glyph_reference PRIVATE cxx_std_17)
target_include_directories(glyph_reference PRIVATE "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public"
  "${CSE_ROOT}/GoldSrc-SDK/public" "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
target_link_libraries(glyph_reference PRIVATE "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}/vgui2.dylib"
  "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")

add_test(NAME physical_abi_rejects_mutations COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/abi_negative.py" "${CSE_OWNER_BUILD}/compile_commands.json"
  "${CMAKE_CURRENT_BINARY_DIR}/abi-negative")

add_test(NAME ui_sound_lifetime COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
  "${CSE_RESOURCE_ROOT}" ui_sound_lifetime)
set_tests_properties(ui_sound_lifetime PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:sdl_failures>")

add_test(NAME native_help_handoff COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
  "${CSE_RESOURCE_ROOT}" native_help_handoff)
set_tests_properties(native_help_handoff PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:sdl_failures>")

add_library(present_observer SHARED present_observer.cpp)
target_compile_features(present_observer PRIVATE cxx_std_17)
target_include_directories(present_observer PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
target_link_libraries(present_observer PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
add_test(NAME focused_control_presentation COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
  "${CSE_RESOURCE_ROOT}" focused_control_presentation)
set_tests_properties(focused_control_presentation PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:present_observer>")

add_library(cursor_observer SHARED cursor_observer.cpp)
target_compile_features(cursor_observer PRIVATE cxx_std_17)
target_include_directories(cursor_observer PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/source/SDL3-3.4.20/include")
target_link_libraries(cursor_observer PRIVATE "${CSE_OWNER_BUILD}/dependencies/SDL3/lib/${CMAKE_BUILD_TYPE}/libSDL3.0.dylib")
add_test(NAME cursor_ownership COMMAND control_tests "${CSE_OWNER_BUILD}/runtime/${CMAKE_BUILD_TYPE}"
  "${CSE_RESOURCE_ROOT}" cursor_ownership)
set_tests_properties(cursor_ownership PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software;CSE_CURSOR_SIMULATED=1;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:cursor_observer>")
