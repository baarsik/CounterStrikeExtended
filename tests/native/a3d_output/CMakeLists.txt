cmake_minimum_required(VERSION 3.25)
project(A3dOutputOwnership LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(SDL3 CONFIG REQUIRED)
set(A3D_PLATFORM_SOURCE "${ROOT}/GoldSrc-SDK/a3dapi/native_platform.cpp" CACHE FILEPATH
    "Platform owner implementation (override only for isolated negative controls)")
set(A3D_OUTPUT_SOURCE "${ROOT}/GoldSrc-SDK/a3dapi/native_output.cpp" CACHE FILEPATH
    "Output owner implementation (override only for isolated negative controls)")
option(A3D_SANITIZERS "Instrument lifetime tests" OFF)
add_executable(a3d_output_tests output_tests.cpp fixture_mixer.cpp
    "${A3D_OUTPUT_SOURCE}"
    "${A3D_PLATFORM_SOURCE}")
add_executable(a3d_file_tests file_tests.cpp "${A3D_PLATFORM_SOURCE}")
foreach(target a3d_output_tests a3d_file_tests)
    target_include_directories(${target} PRIVATE "${ROOT}/GoldSrc-SDK/a3dapi")
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -frtti -ffp-model=precise
        -ffp-contract=off -Wno-overriding-option "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
    if(A3D_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()
enable_testing()
foreach(case partial_open_unwinds resume_failure double_close other_sdl_user_survives callback_quiescence
        retained_refill_cadence transported_pcm_counts_toward_low_water transient_put_recovers persistent_put_is_paced producer_poll_cadence
        producer_quiescence copied_guid unknown_device bounded_pcm put_failure render_failure dummy_stream enumerated_device)
    add_test(NAME ${case} COMMAND a3d_output_tests ${case})
    set_tests_properties(${case} PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" TIMEOUT 15)
endforeach()
foreach(case short_header short_data riff_overflow missing_file valid_wave padded_chunk invalid_format
        monotonic allocation worker wave_allocation_failure)
    add_test(NAME ${case} COMMAND a3d_file_tests ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 15)
endforeach()
# Explicit manual command only; never registered with CTest or run by automated builds.
add_custom_target(a3d_manual_device_probe
    COMMAND a3d_output_tests manual_device DEPENDS a3d_output_tests USES_TERMINAL)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/load_wave_body.inc"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_wave.py"
        "${ROOT}/GoldSrc-SDK/a3dapi/A3dSource.cpp" "${CMAKE_CURRENT_BINARY_DIR}/load_wave_body.inc"
    DEPENDS extract_wave.py "${ROOT}/GoldSrc-SDK/a3dapi/A3dSource.cpp")
add_executable(a3d_wave_caller_tests wave_caller_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/load_wave_body.inc"
    "${A3D_PLATFORM_SOURCE}")
target_include_directories(a3d_wave_caller_tests PRIVATE "${ROOT}/GoldSrc-SDK/a3dapi" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(a3d_wave_caller_tests PRIVATE SDL3::SDL3)
target_compile_features(a3d_wave_caller_tests PRIVATE cxx_std_17)
target_compile_definitions(a3d_wave_caller_tests PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_options(a3d_wave_caller_tests PRIVATE -Wall -Wextra -Werror -fno-exceptions -frtti
    -ffp-model=precise -ffp-contract=off -Wno-overriding-option "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
if(A3D_SANITIZERS)
    target_compile_options(a3d_wave_caller_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(a3d_wave_caller_tests PRIVATE -fsanitize=address,undefined)
endif()
foreach(case caller_valid caller_allocate_failure caller_lock_failure caller_unlock_failure)
    add_test(NAME ${case} COMMAND a3d_wave_caller_tests ${case})
endforeach()
