cmake_minimum_required(VERSION 3.25)
project(A3dNativeBuffers LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(A3D "${ROOT}/GoldSrc-SDK/a3dapi")
find_package(SDL3 CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
option(A3D_SANITIZERS "Instrument actual native buffers and retained kernels" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Only the stock integer helper is extracted; actual mixer, table/control TU and
# native owners are compiled directly. No fixture mixer implementation is linked.
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tests/native/a3d_float_state/prepare_sources.py"
    "${ROOT}" "${A3D}/a2d.cpp" "${CMAKE_BUILD_TYPE}" arm64 "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/multiply.cpp"
    "#include \"platform_types.h\"\n#include \"multiply_fixed_body.inc\"\n")
add_library(a3d_buffer_subject STATIC "${A3D}/native_buffer.cpp" "${A3D}/native_mix.cpp"
    "${A3D}/a2d.cpp" "${A3D}/a2dbuffer.cpp" "${A3D}/native_platform.cpp" "${A3D}/native_notifications.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/multiply.cpp")
target_link_libraries(a3d_buffer_subject PUBLIC SDL3::SDL3)
target_include_directories(a3d_buffer_subject PUBLIC "${A3D}" "${ROOT}/GoldSrc-SDK/public"
    "${CMAKE_CURRENT_BINARY_DIR}")
add_executable(a3d_buffer_tests buffer_tests.cpp)
add_executable(a3d_mix_tests mix_tests.cpp)
foreach(target a3d_buffer_subject a3d_buffer_tests a3d_mix_tests)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE "$<$<CONFIG:Debug>:_DEBUG>" "$<$<CONFIG:Release>:NDEBUG>")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -frtti
        -ffp-model=precise -ffp-contract=off -Wno-overriding-option
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
    if(A3D_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()
target_compile_options(a3d_buffer_subject PRIVATE "SHELL:-include ${ROOT}/GoldSrc-SDK/public/cse_posix_crt.h")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_policy.py"
    "${ROOT}" "${CMAKE_BUILD_TYPE}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_CURRENT_BINARY_DIR}/subject-policy.cmake")
foreach(target a3d_buffer_tests a3d_mix_tests)
    target_link_libraries(${target} PRIVATE a3d_buffer_subject)
    target_link_options(${target} PRIVATE -Wl,-dead_strip)
endforeach()
enable_testing()
foreach(case public_buffer_roundtrip split_lock_wrap duplicate_lifetime stopped_loop_tail_cursor
        high_address_storage format_size_overflow release_while_playing parent_lifetime spatial_parameters duplicate_shared_lock
        duplicate_initial_state duplicate_pending_frequency duplicate_effective_rate duplicate_rejected_control
        duplicate_allocation_failure)
    add_test(NAME ${case} COMMAND a3d_buffer_tests ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 30)
endforeach()
foreach(case chunk_boundaries owned_logical_samples stock_unread_control multiple_instance_scratch exact_owned_pcm stock_control_flags stock_scalar_compatibility consumed_control_domain consumed_control_pcm unsafe_rate_transition)
    add_test(NAME ${case} COMMAND a3d_mix_tests ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 30)
endforeach()

# The 32 original receipt files cover the retained direct and filtered component
# kernels; native Render intentionally selects only the evidenced A2D FALSE path.
add_executable(a3d_retained_pcm_tests "${ROOT}/tests/native/a3d_float_state/float_tests.cpp")
target_link_libraries(a3d_retained_pcm_tests PRIVATE a3d_buffer_subject)
target_compile_features(a3d_retained_pcm_tests PRIVATE cxx_std_17)
target_compile_options(a3d_retained_pcm_tests PRIVATE -ffp-contract=off -frounding-math)
target_link_options(a3d_retained_pcm_tests PRIVATE -Wl,-dead_strip)
if(A3D_SANITIZERS)
    target_compile_options(a3d_retained_pcm_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(a3d_retained_pcm_tests PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME retained_pcm_receipts COMMAND a3d_retained_pcm_tests pcm
    "${ROOT}/tests/native/a3d_float_state/fixtures")
add_test(NAME retained_float_state COMMAND a3d_retained_pcm_tests state)

add_test(NAME qualified_conversion_edges COMMAND a3d_retained_pcm_tests conversion)
