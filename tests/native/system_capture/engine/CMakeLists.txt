cmake_minimum_required(VERSION 3.25)
project(CseSystemCaptureEngine LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CSE_SDL_INCLUDE "" CACHE PATH "Pinned SDL public headers")
option(CSE_SHORTCUT_ENGINE_SANITIZERS "Instrument actual owner/caller bodies" OFF)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../gameplay_shortcuts/engine/extract.py
    "${CSE_ROOT}/GoldSrc-SDK/engine/sys_sdlwind.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/cdll_int.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/sys_dll2.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/snd_dma.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/webm_video.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/launcher/native/MacRuntimeSession.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../gameplay_shortcuts/engine/extract.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/bodies" COMMAND_ERROR_IS_FATAL ANY)
enable_testing()
foreach(mode hw sw)
    add_executable(capture_engine_${mode} capture_tests.cpp
        "${CSE_ROOT}/GoldSrc-SDK/engine/native_mouse_window.cpp"
        "${CSE_ROOT}/GoldSrc-SDK/engine/native_mouse_cocoa.cpp")
    target_include_directories(capture_engine_${mode} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/bodies"
        "${CSE_ROOT}/GoldSrc-SDK/engine" "${CSE_ROOT}/GoldSrc-SDK/common" "${CSE_ROOT}/GoldSrc-SDK/public"
        "${CSE_SDL_INCLUDE}")
    target_compile_definitions(capture_engine_${mode} PRIVATE SDL_STATIC_LIB=1 CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD=1
        CSE_LOOP_BODY="${mode}_loop.inc")
    if(mode STREQUAL sw)
        target_compile_definitions(capture_engine_${mode} PRIVATE CSE_TEST_SOFTWARE=1)
    endif()
    target_compile_options(capture_engine_${mode} PRIVATE -Wall -Wextra -Werror -ffp-model=precise
        -ffp-contract=off -Wno-overriding-option -fno-exceptions -funwind-tables
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    set_target_properties(capture_engine_${mode} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(CSE_SHORTCUT_ENGINE_SANITIZERS)
        target_compile_options(capture_engine_${mode} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(capture_engine_${mode} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME capture-engine-${mode} COMMAND capture_engine_${mode})
endforeach()

execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../native_mouse_window/extract_callers.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/bodies" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CSE_ROOT}/GoldSrc-SDK/engine/vgui2/BaseUISurface.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/VGUI_EngineSurfaceWrap.cpp")
add_executable(capture_callers caller_capture_tests.cpp "${CSE_ROOT}/GoldSrc-SDK/engine/native_mouse_window.cpp")
get_target_property(includes capture_engine_hw INCLUDE_DIRECTORIES)
get_target_property(options capture_engine_hw COMPILE_OPTIONS)
target_include_directories(capture_callers PRIVATE ${includes} "${CSE_ROOT}/GoldSrc-SDK/utils/vgui/include")
target_compile_options(capture_callers PRIVATE ${options})
target_compile_definitions(capture_callers PRIVATE SDL_STATIC_LIB=1 CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD=1)
set_target_properties(capture_callers PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CSE_SHORTCUT_ENGINE_SANITIZERS)
    target_link_options(capture_callers PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME capture-ui-callers COMMAND capture_callers)
