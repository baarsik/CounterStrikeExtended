cmake_minimum_required(VERSION 3.25)
project(A3dNativeNotifications LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Threads REQUIRED)
option(A3D_SANITIZERS "Instrument event lifetime and undefined behavior" OFF)
add_executable(a3d_event_tests event_tests.cpp "${ROOT}/src/a3dapi/native_notifications.cpp")
target_include_directories(a3d_event_tests PRIVATE "${ROOT}/src/a3dapi" "${ROOT}/src/public")
target_link_libraries(a3d_event_tests PRIVATE Threads::Threads)
target_compile_features(a3d_event_tests PRIVATE cxx_std_17)
target_compile_definitions(a3d_event_tests PRIVATE A3D_EVENT_TESTING "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_options(a3d_event_tests PRIVATE -Wall -Wextra -Werror -fno-exceptions -frtti -ffp-model=precise
    -ffp-contract=off -Wno-overriding-option "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
if(A3D_SANITIZERS)
    target_compile_options(a3d_event_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(a3d_event_tests PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
foreach(case notification_event_reset_wait notification_cancel_close notification_event_identity)
    add_test(NAME ${case} COMMAND a3d_event_tests ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 30)
endforeach()

find_package(SDL3 CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(A3D "${ROOT}/src/a3dapi")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tests/native/a3d_float_state/prepare_sources.py"
    "${ROOT}" "${A3D}/a2d.cpp" "${CMAKE_BUILD_TYPE}" arm64 "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/multiply.cpp"
    "#include \"platform_types.h\"\n#include \"multiply_fixed_body.inc\"\n")
add_executable(a3d_notification_tests notification_tests.cpp "${A3D}/native_buffer.cpp"
    "${A3D}/native_mix.cpp" "${A3D}/native_notifications.cpp" "${A3D}/native_output.cpp" "${A3D}/a2d.cpp"
    "${A3D}/a2dbuffer.cpp" "${A3D}/native_platform.cpp" "${CMAKE_CURRENT_BINARY_DIR}/multiply.cpp")
target_include_directories(a3d_notification_tests PRIVATE "${A3D}" "${ROOT}/src/public"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(a3d_notification_tests PRIVATE SDL3::SDL3 Threads::Threads)
target_compile_features(a3d_notification_tests PRIVATE cxx_std_17)
target_compile_definitions(a3d_notification_tests PRIVATE A3D_EVENT_TESTING "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_options(a3d_notification_tests PRIVATE -Wall -Wextra -Werror -fno-exceptions -frtti
    -ffp-model=precise -ffp-contract=off -Wno-overriding-option
    "SHELL:-include ${ROOT}/src/public/cse_posix_crt.h"
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
target_link_options(a3d_notification_tests PRIVATE -Wl,-dead_strip)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tests/native/a3d_buffers/prepare_policy.py"
    "${ROOT}" "${CMAKE_BUILD_TYPE}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_CURRENT_BINARY_DIR}/subject-policy.cmake")
if(A3D_SANITIZERS)
    target_compile_options(a3d_notification_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(a3d_notification_tests PRIVATE -fsanitize=address,undefined)
endif()
foreach(case notification_crossings notification_replace_remove notification_waiter_reentry)
    add_test(NAME ${case} COMMAND a3d_notification_tests ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 15)
endforeach()

set_tests_properties(notification_waiter_reentry PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
