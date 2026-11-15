cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseBzip2DecoderFixtures LANGUAGES C CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")

option(CSE_BZIP2_SANITIZERS "Enable ASan and UBSan" OFF)
set(bzip2_source_dir "${CSE_ROOT}/src/HLTV/Core/private/utils/bzip2")
add_executable(bzip2_decoder_tests decoder_tests.cpp
    "${bzip2_source_dir}/blocksort.c"
    "${bzip2_source_dir}/bzlib.c"
    "${bzip2_source_dir}/compress.c"
    "${bzip2_source_dir}/crctable.c"
    "${bzip2_source_dir}/decompress.c"
    "${bzip2_source_dir}/huffman.c"
    "${bzip2_source_dir}/randtable.c")
target_compile_features(bzip2_decoder_tests PRIVATE cxx_std_17)
target_include_directories(bzip2_decoder_tests SYSTEM PRIVATE "${bzip2_source_dir}")
if(MSVC)
    target_compile_definitions(bzip2_decoder_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bzip2_decoder_tests PRIVATE /W4 "$<$<COMPILE_LANGUAGE:CXX>:/EHsc>"
        "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
    target_compile_options(bzip2_decoder_tests PRIVATE -Wall -Wextra
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
    if(CSE_BZIP2_SANITIZERS)
        target_compile_options(bzip2_decoder_tests PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(bzip2_decoder_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()

enable_testing()
foreach(scenario selector-zero selector-minimum selector-maximum selector-overflow roundtrip)
    add_test(NAME "bzip2-${scenario}" COMMAND bzip2_decoder_tests "${scenario}")
    set_tests_properties("bzip2-${scenario}" PROPERTIES TIMEOUT 30)
endforeach()
