cmake_minimum_required(VERSION 3.25)
project(CseVgui1FixedSysReference LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Console memory-GDI reference only; no Mac font/provider execution")
endif()
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tests/native/vgui1_legacy_raster/extract_reference.py"
    "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/win32/Font.cpp" "${CMAKE_CURRENT_BINARY_DIR}/extracted"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tests/native/vgui1_matched_program/extract_owner.py"
    "${CSE_ROOT}/tests/native/vgui1_legacy_raster/gdi_reference.cpp" "${CMAKE_CURRENT_BINARY_DIR}/extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(fixedsys_reference reference.cpp)
target_include_directories(fixedsys_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/extracted")
target_compile_definitions(fixedsys_reference PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(fixedsys_reference PRIVATE /W4 /WX /EHsc /fp:precise)
target_link_libraries(fixedsys_reference PRIVATE gdi32 user32 bcrypt)
