cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0091 NEW)
project(CseWebPCaptureTests LANGUAGES C CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_SDK_DIR "${CSE_ROOT}/src")
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
include("${CSE_ROOT}/cmake/WebPDependency.cmake")
option(CSE_WEBP_CAPTURE_SANITIZERS "Instrument the production capture paths" OFF)
set(capture_bodies
  "${CMAKE_CURRENT_BINARY_DIR}/hardware.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/software.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/client.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/resources.inc")
add_custom_command(OUTPUT ${capture_bodies}
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract.py
    "${CSE_SDK_DIR}/engine/gl_screen.cpp"
    "${CSE_SDK_DIR}/engine/gl_vidnt.cpp"
    "${CSE_SDK_DIR}/engine/screen.cpp"
    "${CSE_SDK_DIR}/engine/vid_win.cpp"
    "${CSE_SDK_DIR}/engine/cl_main.cpp"
    "${CSE_SDK_DIR}/cstrike/dlls/client.cpp"
  VERBATIM)
add_executable(webp_capture_tests capture_tests.cpp ${capture_bodies} "${CSE_SDK_DIR}/common/image_codec.cpp")
target_compile_features(webp_capture_tests PRIVATE cxx_std_14)
target_include_directories(webp_capture_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_SDK_DIR}/engine" "${CSE_SDK_DIR}/public")
cse_link_webp(webp_capture_tests)
add_executable(webp_resource_tests resource_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/resources.inc")
target_compile_features(webp_resource_tests PRIVATE cxx_std_14)
target_include_directories(webp_resource_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
foreach(test_target IN ITEMS webp_capture_tests webp_resource_tests)
  if(MSVC)
    target_compile_definitions(${test_target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${test_target} PRIVATE /W4 /EHsc)
  else()
    target_compile_options(${test_target} PRIVATE -Wall -Wextra)
    if(CSE_WEBP_CAPTURE_SANITIZERS)
      target_compile_options(${test_target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${test_target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endforeach()
enable_testing()
foreach(scenario IN ITEMS metal-retained-size metal-no-presentation hardware-screenshot hardware-snapshot hardware-rect hardware-fullscreen hardware-movie
    software-movie software-screenshot software-565 software-555 client-numbering failed-write failed-flush failed-open
    software-invalid-stride numbering-exhausted invalid-dimensions client-long-name movie-long-name)
  add_test(NAME webp-capture-${scenario} COMMAND webp_capture_tests ${scenario})
endforeach()

foreach(scenario IN ITEMS tga-only webp-only both missing-both mixed)
  add_test(NAME webp-scope-resource-${scenario} COMMAND webp_resource_tests ${scenario})
endforeach()
