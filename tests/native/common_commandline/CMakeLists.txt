cmake_minimum_required(VERSION 3.25)
project(CseCommonCommandLine LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
option(CSE_COMMANDLINE_SANITIZERS "Instrument the actual common command-line implementation" OFF)
if(WIN32)
    message(FATAL_ERROR "The argv overload is POSIX-only; Windows is verified through the actual owning build")
endif()
add_executable(common_commandline_tests
    "${CMAKE_CURRENT_LIST_DIR}/../launcher_commandline/commandline_tests.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/common/commandline.cpp")
set_target_properties(common_commandline_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(common_commandline_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/common")
target_compile_options(common_commandline_tests PRIVATE -include "${CSE_ROOT}/GoldSrc-SDK/public/cse_posix_crt.h"
    -Wall -Wextra -Werror -Wno-unknown-pragmas
    -Wno-error=sign-compare -Wno-error=deprecated-declarations)
if(CSE_COMMANDLINE_SANITIZERS)
    target_compile_options(common_commandline_tests PRIVATE -fsanitize=address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(common_commandline_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
enable_testing()
foreach(scenario argv argv-boundary argv-overflow)
    add_test(NAME common-commandline-${scenario} COMMAND common_commandline_tests "${scenario}")
endforeach()
