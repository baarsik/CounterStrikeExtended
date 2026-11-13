cmake_minimum_required(VERSION 3.25)
project(CseMacCursorPolicy LANGUAGES CXX OBJCXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CSE_CLIENT_RUNTIME "" CACHE PATH "Actual native client runtime")
set(CSE_KEYBOARD_POLICY_HEADER "${CSE_ROOT}/src/common/sdl3_keyboard.h" CACHE FILEPATH
    "Actual keyboard policy or frozen pre-fix header for RED qualification")
add_executable(mac_cursor_policy policy.mm
    "${CSE_ROOT}/src/engine/native_mouse_window.cpp"
    "${CSE_ROOT}/src/engine/native_mouse_cocoa.cpp")
target_include_directories(mac_cursor_policy PRIVATE
    "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/common"
    "${CSE_ROOT}/src/external/SDL3/include")
target_link_libraries(mac_cursor_policy PRIVATE "${CSE_CLIENT_RUNTIME}/libSDL3.0.dylib" "-framework Cocoa")
target_compile_definitions(mac_cursor_policy PRIVATE CSE_KEYBOARD_POLICY_HEADER="${CSE_KEYBOARD_POLICY_HEADER}")
target_compile_options(mac_cursor_policy PRIVATE -Wall -Wextra -Werror -fno-exceptions
    "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
set_target_properties(mac_cursor_policy PROPERTIES CXX_STANDARD 14 OBJCXX_STANDARD 14
    BUILD_RPATH "${CSE_CLIENT_RUNTIME}")
enable_testing()
add_test(NAME mac-gameplay-cursor-policy COMMAND mac_cursor_policy)
add_test(NAME mac-gameplay-cursor-image COMMAND mac_cursor_policy --cursor-image)
set_tests_properties(mac-gameplay-cursor-image PROPERTIES RUN_SERIAL TRUE)
add_test(NAME mac-engine-keyboard-startup COMMAND mac_cursor_policy "${CSE_CLIENT_RUNTIME}/hw.dylib")
set_tests_properties(mac-engine-keyboard-startup PROPERTIES WORKING_DIRECTORY "${CSE_CLIENT_RUNTIME}")
