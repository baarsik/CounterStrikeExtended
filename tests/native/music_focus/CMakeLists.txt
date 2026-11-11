cmake_minimum_required(VERSION 3.25)
project(CseMusicFocus LANGUAGES CXX)
if(NOT APPLE)
    message(FATAL_ERROR "This fixture qualifies the native Apple focus policy")
endif()
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_MUSIC_SOURCE_ROOT "${CSE_ROOT}" CACHE PATH "Exact music/controller owning source root")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_music.py"
    --source "${CSE_MUSIC_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/actual"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CSE_MUSIC_SOURCE_ROOT}/GoldSrc-SDK/engine/cd.cpp"
    "${CSE_MUSIC_SOURCE_ROOT}/GoldSrc-SDK/engine/sys_sdlwind.cpp")
option(CSE_MUSIC_SANITIZERS "Instrument actual controller bodies and real CD worker" OFF)
foreach(owner software hardware)
    add_executable(music_focus_${owner} focus_tests.cpp "${CSE_MUSIC_SOURCE_ROOT}/GoldSrc-SDK/engine/thread.cpp")
    target_include_directories(music_focus_${owner} PRIVATE "${CSE_MUSIC_SOURCE_ROOT}/GoldSrc-SDK/engine"
        "${CMAKE_CURRENT_BINARY_DIR}/actual")
    set_target_properties(music_focus_${owner} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_options(music_focus_${owner} PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(music_focus_${owner} PRIVATE Threads::Threads)
    if(owner STREQUAL "hardware")
        target_compile_definitions(music_focus_${owner} PRIVATE CSE_MUSIC_HARDWARE)
    endif()
    if(CSE_MUSIC_SANITIZERS)
        target_compile_options(music_focus_${owner} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(music_focus_${owner} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
endforeach()
enable_testing()
foreach(owner software hardware)
    foreach(case ordinary manual_pause manual_after_loss stopped finished disabled rapid manual_interleave stop_interleave new_play new_play_failure late_loss_after_play overflow_latest_gain
            queue_full failed_pause failed_resume suspend_init_failure suspend_open_failure suspend_start_failure
            suspend_file_failure suspend_manual_pause suspend_new_play_failure suspend_stop_after_failure suspend_stop_after_frame suspend_stop_after_frame_disabled shutdown shutdown_inflight)
        add_test(NAME ${owner}-${case} COMMAND music_focus_${owner} ${case})
    endforeach()
endforeach()
