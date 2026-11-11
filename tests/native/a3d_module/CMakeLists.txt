cmake_minimum_required(VERSION 3.25)
project(A3dNativeModule LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT EXISTS "${A3D_MODULE}")
    message(FATAL_ERROR "A3D_MODULE must identify the actual owning production dylib")
endif()
find_package(Threads REQUIRED)
add_executable(a3d_module_tests module_tests.cpp)
target_compile_features(a3d_module_tests PRIVATE cxx_std_17)
target_include_directories(a3d_module_tests PRIVATE "${ROOT}/GoldSrc-SDK/public" "${ROOT}/GoldSrc-SDK/a3dapi")
target_compile_options(a3d_module_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(a3d_module_tests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
enable_testing()
foreach(case reject_abi_before_factory class_iid_aggregation canonical_unknown qi_failure_cleanup
        source_buffer_playback held_interface_unload_refusal factory_lock_balance repeated_load_unload
        notification_busy_unload notification_public_admission callable_operation_matrix)
    add_test(NAME ${case} COMMAND a3d_module_tests "${A3D_MODULE}" ${case} "${A3D_FOREIGN_MODULE}")
    set_tests_properties(${case} PROPERTIES TIMEOUT 30 ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME abi_slot_declarations COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/check_contract.py" "${ROOT}")

add_executable(a3d_enumeration_tests enumeration_tests.cpp enumeration_admission.cpp)
target_compile_features(a3d_enumeration_tests PRIVATE cxx_std_17)
target_include_directories(a3d_enumeration_tests PRIVATE "${ROOT}/GoldSrc-SDK/public" "${ROOT}/GoldSrc-SDK/a3dapi")
target_compile_options(a3d_enumeration_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(a3d_enumeration_tests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
foreach(case public_selected_initialize copied_record_name sized_output_validation empty_snapshot
        held_enumerator_unload shared_audio_ownership concurrent_snapshot_reads snapshot_failure_cleanup
        stale_foreign_tokens enumeration_admission_mutations repeated_enumerator_reload)
    add_test(NAME ${case} COMMAND a3d_enumeration_tests "${A3D_MODULE}" ${case} "${A3D_FOREIGN_MODULE}")
    set_tests_properties(${case} PROPERTIES TIMEOUT 30 ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
endforeach()

if(APPLE)
    get_filename_component(A3D_MODULE_DIRECTORY "${A3D_MODULE}" DIRECTORY)
    set(A3D_SDL_LIBRARY "${A3D_MODULE_DIRECTORY}/libSDL3.0.dylib" CACHE FILEPATH "Actual staged SDL provider")
    add_library(a3d_enumeration_faults SHARED enumeration_faults.cpp)
    target_compile_features(a3d_enumeration_faults PRIVATE cxx_std_17)
    target_compile_options(a3d_enumeration_faults PRIVATE -Wall -Wextra -Werror -fno-optimize-sibling-calls)
    target_link_libraries(a3d_enumeration_faults PRIVATE "${A3D_SDL_LIBRARY}")
    foreach(fault init devices negative name malformed surrogate truncated out_of_range continuation
            empty long multibyte empty_name alloc1 alloc2 alloc3)
        add_test(NAME enumeration_fault_${fault} COMMAND a3d_enumeration_tests "${A3D_MODULE}" "fault_${fault}")
        set_tests_properties(enumeration_fault_${fault} PROPERTIES TIMEOUT 30
            ENVIRONMENT "SDL_AUDIO_DRIVER=dummy;DYLD_INSERT_LIBRARIES=$<TARGET_FILE:a3d_enumeration_faults>")
    endforeach()
endif()

if(A3D_BASELINE_MODULE)
    add_test(NAME enumeration_baseline_rejection COMMAND a3d_enumeration_tests "${A3D_MODULE}"
        enumeration_baseline_rejection "${A3D_BASELINE_MODULE}")
    set_tests_properties(enumeration_baseline_rejection PROPERTIES TIMEOUT 30 ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
endif()
