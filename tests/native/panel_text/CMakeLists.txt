cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CsePanelTextUnits LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(source "${CSE_ROOT}/GoldSrc-SDK/vgui2/controls/Panel.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CSE_ROOT}/GoldSrc-SDK/engine/vgui2/BaseUISurface.cpp"
  "${CSE_ROOT}/GoldSrc-SDK/engine/unicode_strtools.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/extract_sdl_text.py"
  "${CMAKE_CURRENT_LIST_DIR}/extract_keytyped.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_sdl_text.py"
  "${CSE_ROOT}/GoldSrc-SDK" "${CMAKE_CURRENT_BINARY_DIR}/actual_sdl_text.inc" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_keytyped.py"
  "${source}" "${CMAKE_CURRENT_BINARY_DIR}/actual_keytyped.inc" COMMAND_ERROR_IS_FATAL ANY)
add_executable(panel_text_tests panel_text_tests.cpp)
target_compile_features(panel_text_tests PRIVATE cxx_std_17)
target_include_directories(panel_text_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
  "${CSE_ROOT}/GoldSrc-SDK/external/SDL3/include")
option(CSE_ENABLE_SANITIZERS "Instrument the actual key dispatch fixture" OFF)
if(MSVC)
  target_compile_options(panel_text_tests PRIVATE /W4 /WX /EHsc
    "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
  target_compile_options(panel_text_tests PRIVATE -Wall -Wextra -Werror
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
  if(CSE_ENABLE_SANITIZERS)
    target_compile_options(panel_text_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(panel_text_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()
enable_testing()
add_test(NAME actual_utf16_unit_dispatch COMMAND panel_text_tests)
