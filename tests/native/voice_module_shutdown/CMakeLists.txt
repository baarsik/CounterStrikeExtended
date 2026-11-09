cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseVoiceModuleShutdown LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS extract_shutdown.py
    "${CSE_ROOT}/GoldSrc-SDK/engine/voice.cpp" "${CSE_ROOT}/GoldSrc-SDK/engine/cd.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_shutdown.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
add_library(capture_close_boundary OBJECT capture_boundary.cpp)
target_compile_features(capture_close_boundary PRIVATE cxx_std_14)
target_include_directories(capture_close_boundary PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CSE_ROOT}/GoldSrc-SDK/engine")
add_executable(voice_module_shutdown_tests shutdown_tests.cpp $<TARGET_OBJECTS:capture_close_boundary>)
target_compile_features(voice_module_shutdown_tests PRIVATE cxx_std_14)
target_include_directories(voice_module_shutdown_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CSE_ROOT}/GoldSrc-SDK/engine")
option(CSE_VOICE_SHUTDOWN_SANITIZERS "Instrument owned close boundaries" OFF)
if(MSVC)
    target_compile_options(capture_close_boundary PRIVATE /W4 /WX /EHs-c-
        "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
    target_compile_options(voice_module_shutdown_tests PRIVATE /W4 /WX /EHsc
        "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>" "$<$<CONFIG:Debug>:/RTC1>")
else()
    target_compile_options(capture_close_boundary PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    target_compile_options(voice_module_shutdown_tests PRIVATE -Wall -Wextra -Werror
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    if(CSE_VOICE_SHUTDOWN_SANITIZERS)
        target_compile_options(capture_close_boundary PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_compile_options(voice_module_shutdown_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(voice_module_shutdown_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
add_test(NAME actual-voice-module-shutdown COMMAND voice_module_shutdown_tests)
