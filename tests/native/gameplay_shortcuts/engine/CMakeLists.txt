cmake_minimum_required(VERSION 3.25)
project(CseGameplayShortcutEngine LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CSE_SDL_INCLUDE "" CACHE PATH "Pinned SDL public headers")
option(CSE_SHORTCUT_ENGINE_SANITIZERS "Instrument actual owner/caller bodies" OFF)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS extract.py
    "${CSE_ROOT}/src/engine/sys_sdlwind.cpp"
    "${CSE_ROOT}/src/engine/vgui_int.cpp"
    "${CSE_ROOT}/src/engine/cdll_int.cpp"
    "${CSE_ROOT}/src/engine/sys_dll2.cpp"
    "${CSE_ROOT}/src/engine/snd_dma.cpp"
    "${CSE_ROOT}/src/engine/webm_video.cpp"
    "${CSE_ROOT}/src/launcher/native/MacRuntimeSession.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/bodies" COMMAND_ERROR_IS_FATAL ANY)
enable_testing()
foreach(mode hw sw)
    add_executable(shortcut_engine_${mode} engine_tests.cpp
        "${CSE_ROOT}/src/engine/native_mouse_window.cpp"
        "${CSE_ROOT}/src/engine/native_mouse_cocoa.cpp")
    target_include_directories(shortcut_engine_${mode} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/bodies"
        "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/common" "${CSE_ROOT}/src/public"
        "${CSE_SDL_INCLUDE}")
    target_compile_definitions(shortcut_engine_${mode} PRIVATE SDL_STATIC_LIB=1 CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD=1
        CSE_LOOP_BODY="${mode}_loop.inc")
    if(mode STREQUAL sw)
        target_compile_definitions(shortcut_engine_${mode} PRIVATE CSE_TEST_SOFTWARE=1)
    endif()
    target_compile_options(shortcut_engine_${mode} PRIVATE -Wall -Wextra -Werror -ffp-model=precise
        -ffp-contract=off -Wno-overriding-option -fno-exceptions -funwind-tables
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    set_target_properties(shortcut_engine_${mode} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(CSE_SHORTCUT_ENGINE_SANITIZERS)
        target_compile_options(shortcut_engine_${mode} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(shortcut_engine_${mode} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME shortcut-engine-${mode} COMMAND shortcut_engine_${mode})
endforeach()

foreach(mode hw sw)
    add_executable(shortcut_lifecycle_${mode} lifecycle_tests.cpp
        "${CSE_ROOT}/src/engine/native_mouse_window.cpp"
        "${CSE_ROOT}/src/engine/native_mouse_cocoa.cpp"
        "${CSE_ROOT}/src/engine/native_runtime_bootstrap.cpp")
    get_target_property(includes shortcut_engine_${mode} INCLUDE_DIRECTORIES)
    get_target_property(options shortcut_engine_${mode} COMPILE_OPTIONS)
    get_target_property(definitions shortcut_engine_${mode} COMPILE_DEFINITIONS)
    target_include_directories(shortcut_lifecycle_${mode} PRIVATE ${includes})
    target_compile_options(shortcut_lifecycle_${mode} PRIVATE ${options})
    target_compile_definitions(shortcut_lifecycle_${mode} PRIVATE ${definitions} CSE_SHORTCUT_LIFECYCLE_TEST=1
        CSE_SHUTDOWN_BODY="${mode}_shutdown.inc" CSE_LISTEN_BODY="${mode}_listen.inc")
    set_target_properties(shortcut_lifecycle_${mode} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(CSE_SHORTCUT_ENGINE_SANITIZERS)
        target_link_options(shortcut_lifecycle_${mode} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME shortcut-lifecycle-${mode} COMMAND shortcut_lifecycle_${mode})
endforeach()
