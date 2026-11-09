cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseClientMouseCallback LANGUAGES CXX)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(CSE_SDL_INCLUDE "" CACHE PATH "Pinned SDL3.4.20 headers; provider is not linked")
option(CSE_MOUSE_CALLBACK_SANITIZERS "Instrument owned callback and fake services" OFF)
if(NOT EXISTS "${CSE_SDL_INCLUDE}/SDL3/SDL.h")
    message(FATAL_ERROR "Provide pinned SDL headers")
endif()
set(CSE_MOUSE_WINDOW_HEADER "${CSE_ROOT}/GoldSrc-SDK/engine/sdl3_window.h")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/actual_callback.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/actual_destination.inc" "${CMAKE_CURRENT_BINARY_DIR}/actual_engine_table.inc"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_fixtures.py"
        "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS prepare_fixtures.py "${CSE_ROOT}/GoldSrc-SDK/engine/cdll_int.cpp"
        "${CSE_ROOT}/GoldSrc-SDK/engine/module.cpp" "${CSE_ROOT}/GoldSrc-SDK/engine/APIProxy.h"
        "${CSE_ROOT}/tools/client_module_layouts.py"
        "${CSE_ROOT}/tests/native/client_abi/prepare_fixtures.py"
        "${CSE_ROOT}/GoldSrc-SDK/engine/client.h" "${CSE_ROOT}/GoldSrc-SDK/common/com_model.h" VERBATIM)
add_executable(client_mouse_callback_tests callback_tests.cpp
    ../client_mouse_coordinates/controlled_window.cpp "${CMAKE_CURRENT_BINARY_DIR}/actual_callback.inc")
target_include_directories(client_mouse_callback_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CSE_SDL_INCLUDE}" "${CSE_ROOT}/GoldSrc-SDK/engine" "${CSE_ROOT}/GoldSrc-SDK/common"
    "${CSE_ROOT}/GoldSrc-SDK/public" "${CSE_ROOT}/GoldSrc-SDK/pm_shared")
target_compile_definitions(client_mouse_callback_tests PRIVATE
    CSE_MOUSE_WINDOW_HEADER="${CSE_MOUSE_WINDOW_HEADER}" _CRT_SECURE_NO_WARNINGS NOMINMAX)
set_target_properties(client_mouse_callback_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(client_mouse_callback_tests PRIVATE /W4 /WX /fp:precise /EHs-c-
        $<$<CONFIG:Release>:/O2> $<$<CONFIG:Debug>:/Od>)
else()
    target_compile_options(client_mouse_callback_tests PRIVATE -Wall -Wextra -Werror -ffp-model=precise
        -ffp-contract=off -Wno-overriding-option -fno-exceptions -funwind-tables
        $<$<CONFIG:Release>:-O2> $<$<CONFIG:Debug>:-O0>)
    if(CSE_MOUSE_CALLBACK_SANITIZERS)
        target_compile_options(client_mouse_callback_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(client_mouse_callback_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
add_test(NAME owned-mouse-callback COMMAND client_mouse_callback_tests)
set_tests_properties(owned-mouse-callback PROPERTIES TIMEOUT 15)
