cmake_minimum_required(VERSION 3.25)
project(CseEffectsSafety LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(inputs "${CSE_ROOT}/src/engine/r_efx.cpp" "${CSE_ROOT}/src/engine/r_part.cpp"
    "${CSE_ROOT}/src/engine/cl_tent.cpp" "${CSE_ROOT}/src/engine/cdll_int.cpp"
    "${CSE_ROOT}/src/engine/mathlib.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${inputs} extract.py)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/effects.inc" COMMAND_ERROR_IS_FATAL ANY)
add_executable(effects_tests effects_tests.cpp)
target_compile_features(effects_tests PRIVATE cxx_std_17)
target_include_directories(effects_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/common" "${CSE_ROOT}/src/public"
    "${CSE_ROOT}/src/cl_dll" "${CSE_ROOT}/src/pm_shared" "${CSE_ROOT}/src/external/SDL3/include")
if(MSVC)
    target_compile_options(effects_tests PRIVATE /W4 /WX /wd4100 /EHsc)
else()
    target_compile_options(effects_tests PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -ffp-contract=off)
endif()
option(CSE_EFFECTS_SANITIZERS "Enable ASan/UBSan" OFF)
if(CSE_EFFECTS_SANITIZERS)
    target_compile_options(effects_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(effects_tests PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
foreach(scenario api particles temporary visibility beams ramp reuse)
    add_test(NAME effects-safety-${scenario} COMMAND effects_tests ${scenario})
endforeach()
