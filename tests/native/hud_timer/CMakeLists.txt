cmake_minimum_required(VERSION 3.25)
project(CseHudTimer LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_HUD_TIMER_SANITIZERS "Instrument the timer fixture" OFF)
set(generated classes.inc bodies.inc scale_colors.inc fonts.inc metrics.inc source-manifest.json)
list(TRANSFORM generated PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
add_custom_command(OUTPUT ${generated}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_sources.py"
        "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS extract_sources.py
        "${CSE_ROOT}/src/cstrike/cl_dll/health.cpp"
        "${CSE_ROOT}/src/cstrike/cl_dll/health.h"
        "${CSE_ROOT}/src/cstrike/cl_dll/message.cpp"
        "${CSE_ROOT}/src/cstrike/cl_dll/hud.cpp"
        "${CSE_ROOT}/src/cstrike/cl_dll/hud_redraw.cpp"
        "${CSE_ROOT}/src/cstrike/cl_dll/cl_util.h"
        "${CSE_ROOT}/src/engine/vgui2/text_draw.cpp"
    VERBATIM)
add_executable(hud_timer timer_tests.cpp ${generated})
target_compile_features(hud_timer PRIVATE cxx_std_17)
target_include_directories(hud_timer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CSE_ROOT}/src/public")
if(MSVC)
    target_compile_options(hud_timer PRIVATE /W4 /WX)
else()
    target_compile_options(hud_timer PRIVATE -Wall -Wextra -Werror)
    if(CSE_HUD_TIMER_SANITIZERS)
        target_compile_options(hud_timer PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(hud_timer PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
foreach(scenario IN ITEMS font-round-480 font-career-480 font-round-1080 font-career-1080
        font-round-1440 font-career-1440 width hidden panic career-fade current-font)
    add_test(NAME hud-timer-${scenario} COMMAND hud_timer ${scenario})
endforeach()
if(MSVC)
    add_test(NAME hud-timer-msvc-diagnostic-boundary
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_msvc_policy.py"
            "${CMAKE_CXX_COMPILER}" "${CMAKE_CURRENT_BINARY_DIR}")
endif()
