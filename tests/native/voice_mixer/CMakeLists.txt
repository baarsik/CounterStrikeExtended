cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseVoiceMixerReference LANGUAGES CXX)
if(WIN32)
    add_executable(voice_mixer_windows_reference windows_reference.cpp)
    target_compile_definitions(voice_mixer_windows_reference PRIVATE WIN32
        CSE_MIXER_SOURCE="${CSE_ROOT}/GoldSrc-SDK/engine/voice_mixer_controls.cpp")
    target_compile_options(voice_mixer_windows_reference PRIVATE /W4 /WX /EHsc /fp:precise
        $<$<CONFIG:Release>:/O2> $<$<CONFIG:Debug>:/Od> $<$<CONFIG:Debug>:/Zi>)
    target_compile_definitions(voice_mixer_windows_reference PRIVATE
        $<$<CONFIG:Release>:NDEBUG> $<$<CONFIG:Debug>:_DEBUG>)
    set_target_properties(voice_mixer_windows_reference PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    enable_testing()
    add_test(NAME whole-retained-winmm-source COMMAND voice_mixer_windows_reference)
    set_tests_properties(whole-retained-winmm-source PROPERTIES TIMEOUT 15)
elseif(APPLE)
    add_library(voice_mixer_coreaudio_compile OBJECT coreaudio_compile_probe.cpp)
    target_compile_options(voice_mixer_coreaudio_compile PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables)
    set_target_properties(voice_mixer_coreaudio_compile PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
    option(CSE_MIXER_MISSING_BASELINE "Build the explicitly failing missing-provider baseline" OFF)
    if(CSE_MIXER_MISSING_BASELINE)
        set(mixer_source missing_coreaudio_baseline.cpp)
    else()
        set(mixer_source "${CSE_ROOT}/GoldSrc-SDK/engine/voice_mixer_controls_coreaudio.cpp")
    endif()
    add_library(coreaudio_mixer_component OBJECT ${mixer_source})
    target_include_directories(coreaudio_mixer_component PRIVATE "${CSE_ROOT}/GoldSrc-SDK/engine")
    target_compile_options(coreaudio_mixer_component PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables
        -ffp-model=precise -ffp-contract=off -Wno-overriding-option
        $<$<CONFIG:Release>:-O2> $<$<CONFIG:Debug>:-O0> $<$<CONFIG:Debug>:-g>)
    set_target_properties(coreaudio_mixer_component PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    add_executable(coreaudio_mixer_tests coreaudio_tests.cpp fake_coreaudio.cpp forbidden_native_hal.cpp
        $<TARGET_OBJECTS:coreaudio_mixer_component>)
    target_include_directories(coreaudio_mixer_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/engine")
    target_compile_options(coreaudio_mixer_tests PRIVATE -Wall -Wextra -Werror -ffp-model=precise -ffp-contract=off
        -Wno-overriding-option $<$<CONFIG:Release>:-O2> $<$<CONFIG:Debug>:-O0> $<$<CONFIG:Debug>:-g>)
    target_link_libraries(coreaudio_mixer_tests PRIVATE "-framework CoreFoundation")
    set_target_properties(coreaudio_mixer_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    enable_testing()
    add_test(NAME coreaudio-mixer-factory COMMAND coreaudio_mixer_tests factory)
    add_test(NAME coreaudio-mixer-volume COMMAND coreaudio_mixer_tests volume)
    add_test(NAME coreaudio-mixer-binding COMMAND coreaudio_mixer_tests binding)
    add_test(NAME coreaudio-mixer-monitor-boost COMMAND coreaudio_mixer_tests monitor-boost)
    add_test(NAME coreaudio-mixer-selection COMMAND coreaudio_mixer_tests selection)
    add_test(NAME coreaudio-mixer-failures COMMAND coreaudio_mixer_tests failures)
    add_test(NAME coreaudio-mixer-topology COMMAND coreaudio_mixer_tests topology)
    add_test(NAME coreaudio-mixer-owners COMMAND coreaudio_mixer_tests owners)
    set_tests_properties(coreaudio-mixer-factory coreaudio-mixer-volume coreaudio-mixer-binding
        coreaudio-mixer-monitor-boost coreaudio-mixer-selection coreaudio-mixer-failures
        coreaudio-mixer-topology coreaudio-mixer-owners PROPERTIES TIMEOUT 15)
    option(CSE_MIXER_SANITIZERS "Instrument only the fake-service mixer component" OFF)
    if(CSE_MIXER_SANITIZERS)
        foreach(target coreaudio_mixer_component coreaudio_mixer_tests)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
endif()
