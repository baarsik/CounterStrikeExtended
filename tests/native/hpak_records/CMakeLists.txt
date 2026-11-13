cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseHpakRecords LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CSE_HPAK_SDK_ROOT "${CSE_ROOT}/src" CACHE PATH "Pinned SDK for isolated archive fixtures")
set(CSE_HPAK_SOURCE "${CSE_HPAK_SDK_ROOT}/engine/hashpak.cpp" CACHE FILEPATH "Complete HPAK owner")
set(CSE_HPAK_MODE current CACHE STRING "current or preserved Win32 reference")
option(CSE_HPAK_SANITIZERS "Instrument the complete HPAK owner and CRC implementation" OFF)
add_executable(hpak_archive_tests archive_tests.cpp "${CSE_HPAK_SOURCE}"
    "${CSE_HPAK_SDK_ROOT}/engine/crc.cpp")
target_compile_features(hpak_archive_tests PRIVATE cxx_std_17)
set_target_properties(hpak_archive_tests PROPERTIES CXX_EXTENSIONS NO)
target_include_directories(hpak_archive_tests PRIVATE
    "${CSE_HPAK_SDK_ROOT}/engine"
    "${CSE_HPAK_SDK_ROOT}/common"
    "${CSE_HPAK_SDK_ROOT}/cl_dll"
    "${CSE_HPAK_SDK_ROOT}/public"
    "${CSE_HPAK_SDK_ROOT}/pm_shared"
    "${CSE_HPAK_SDK_ROOT}/utils/vgui/include")
if(MSVC)
    target_compile_definitions(hpak_archive_tests PRIVATE WIN32 _CRT_SECURE_NO_WARNINGS)
    target_compile_options(hpak_archive_tests PRIVATE /W4 /WX /EHsc /Gy /Gw
        "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
    # Preserve the owner's existing legacy diagnostics; the new fixture remains /WX.
    set_source_files_properties("${CSE_HPAK_SOURCE}" "${CSE_HPAK_SDK_ROOT}/engine/crc.cpp"
        PROPERTIES COMPILE_OPTIONS "/wd4100;/wd4244;/wd4267;/wd4127;/wd4706;/wd4189;/wd4996")
    target_link_options(hpak_archive_tests PRIVATE /OPT:REF /OPT:NOICF)
else()
    target_compile_definitions(hpak_archive_tests PRIVATE POSIX OSX)
    target_compile_options(hpak_archive_tests PRIVATE -Wall -Wextra -Werror
        -Werror=shorten-64-to-32 -ffp-model=precise -ffp-contract=off -Wno-overriding-option
        -ffunction-sections -fdata-sections
        -include "${CSE_HPAK_SDK_ROOT}/public/cse_posix_crt.h"
        "$<$<CONFIG:Release>:-Os>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    set_source_files_properties("${CSE_HPAK_SOURCE}" "${CSE_HPAK_SDK_ROOT}/engine/crc.cpp"
        PROPERTIES COMPILE_OPTIONS "-Wno-error=unused-parameter;-Wno-error=writable-strings;-Wno-error=sign-compare")
    target_link_options(hpak_archive_tests PRIVATE -Wl,-dead_strip)
    if(CSE_HPAK_SANITIZERS)
        target_compile_options(hpak_archive_tests PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(hpak_archive_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
endif()
set(work "${CMAKE_CURRENT_BINARY_DIR}/authored-owned")
file(MAKE_DIRECTORY "${work}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/make_fixture.py"
    "${work}/authored-win32.hpk" COMMAND_ERROR_IS_FATAL ANY)
enable_testing()
add_test(NAME complete-hpak-owner COMMAND hpak_archive_tests
    "${work}/authored-win32.hpk" "${work}" "${CSE_HPAK_MODE}")
set_tests_properties(complete-hpak-owner PROPERTIES TIMEOUT 20)
