cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseCpuFixtures LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
find_package(Threads REQUIRED)
enable_testing()
add_executable(cpu_tests cpu_tests.cpp "${CSE_ROOT}/src/tier0/cpu.cpp"
  "${CSE_ROOT}/src/tier0/cpu_posix.cpp" "${CSE_ROOT}/src/tier0/fasttimer.cpp")
target_include_directories(cpu_tests PRIVATE "${CSE_ROOT}/src/public")
target_compile_features(cpu_tests PRIVATE cxx_std_14)
target_compile_definitions(cpu_tests PRIVATE TIER0_DLL NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_link_libraries(cpu_tests PRIVATE Threads::Threads)
if(MSVC)
  # Preserve tier0's asynchronous exceptions and FP defaults; the fixture uses the same source files.
  target_compile_options(cpu_tests PRIVATE /W4 /WX /EHa /fp:precise
    "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
  target_compile_options(cpu_tests PRIVATE -Wall -Wextra -Werror -ffp-model=precise -ffp-contract=off
    -Wno-overriding-option "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
endif()
add_test(NAME actual-cpu-and-profiler COMMAND cpu_tests)
