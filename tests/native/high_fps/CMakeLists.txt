cmake_minimum_required(VERSION 3.25)
project(CseHighFps LANGUAGES C CXX)

get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(high_fps timing_tests.cpp)
target_compile_features(high_fps PRIVATE cxx_std_17)
target_include_directories(high_fps PRIVATE
    "${CSE_ROOT}/src/engine"
    "${CSE_ROOT}/src/game_shared"
    "${CSE_ROOT}/src/cstrike/cl_dll")

enable_testing()
add_test(NAME goldsrc-high-fps COMMAND high_fps)

add_executable(high_fps_movement movement_tests.c
    "${CSE_ROOT}/src/cstrike/pm_shared/pm_shared.c"
    "${CSE_ROOT}/src/cstrike/pm_shared/pm_math.c")
target_include_directories(high_fps_movement PRIVATE
    "${CSE_ROOT}/src/cstrike/pm_shared"
    "${CSE_ROOT}/src/pm_shared"
    "${CSE_ROOT}/src/common"
    "${CSE_ROOT}/src/engine"
    "${CSE_ROOT}/src/public"
    "${CSE_ROOT}/src/game_shared")
if(NOT MSVC)
    target_link_libraries(high_fps_movement PRIVATE m)
endif()
add_test(NAME goldsrc-high-fps-movement COMMAND high_fps_movement)
