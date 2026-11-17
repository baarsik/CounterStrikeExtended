cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseWavLoading LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CSE_WAV_SOURCE "${CSE_ROOT}/src/engine/snd_mem.cpp" CACHE FILEPATH "Complete production WAV implementation")
option(CSE_WAV_SANITIZERS "Instrument WAV parsing, resampling and ownership" OFF)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_helpers.py"
    "${CSE_ROOT}/src/engine" "${CMAKE_CURRENT_BINARY_DIR}/owned_helpers.inc" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/extract_helpers.py"
    "${CSE_ROOT}/src/engine/common.cpp" "${CSE_ROOT}/src/engine/snd_mix.cpp")
add_executable(wav_loading_tests wav_tests.cpp "${CSE_WAV_SOURCE}")
target_compile_features(wav_loading_tests PRIVATE cxx_std_17)
target_include_directories(wav_loading_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(wav_loading_tests SYSTEM PRIVATE
    "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/common" "${CSE_ROOT}/src/public"
    "${CSE_ROOT}/src/pm_shared" "${CSE_ROOT}/src/cl_dll" "${CSE_ROOT}/src/utils/vgui/include")
if(MSVC)
    target_compile_definitions(wav_loading_tests PRIVATE WIN32 NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(wav_loading_tests PRIVATE /W4 /EHsc /fp:precise)
else()
    target_compile_definitions(wav_loading_tests PRIVATE POSIX OSX)
    target_compile_options(wav_loading_tests PRIVATE -Wall -Wextra -fno-fast-math -ffp-contract=off
        -include "${CSE_ROOT}/src/public/cse_posix_crt.h")
    if(CSE_WAV_SANITIZERS)
        target_compile_options(wav_loading_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(wav_loading_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
foreach(case pcm truncated metadata invalid-arguments chunk-bounds loops loop-overflow loop-length
        stream-prefix odd-padding resample-valid
        resample-short resample-clear resample-product resample-fraction
        load-invalid load-stereo load-cache-failure load-success load-fallback load-temp-failure
        stream-invalid stream-stereo stream-width stream-rate stream-cache-failure
        stream-success stream-refill-failure stream-temp-failure stream-reopen-move stream-reopen-evict
        stream-reload-pcm mix-reload-failure mix-stream-move mix-stream-evict mix-stream-temp-failure
        mix-stream-reload-failure mix-stream-eof vox-empty-text vox-empty-words)
    add_test(NAME wav-${case} COMMAND wav_loading_tests ${case})
endforeach()
add_test(NAME wav-shipped-bot-sounds COMMAND wav_loading_tests corpus "${CSE_ROOT}/src/deploy/zbot/cstrike/sound")
