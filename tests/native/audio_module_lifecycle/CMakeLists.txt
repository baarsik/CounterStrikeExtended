cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseAudioModuleLifecycle LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
option(CSE_AUDIO_LIFECYCLE_SANITIZERS "Instrument owned audio lifecycle sources" OFF)
find_package(Threads REQUIRED)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(CSE_LIFECYCLE_PROVIDER_DIRECTORY "" CACHE PATH "Frozen actual SDL3 and mpg123 images/import libraries")
set(archive "${CSE_ROOT}/GoldSrc-SDK/external/mpg123/mpg123-1.33.7.tar.bz2")
file(SHA256 "${archive}" archive_hash)
if(NOT archive_hash STREQUAL "31d0e35a4ca567ec9b5ebda6c3062bb4435d6d3eacd6ef0d95cadd7854dc03ee")
  message(FATAL_ERROR "Pinned mpg123 source hash mismatch")
endif()
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/decoder-source")
set(decoder_source "${CMAKE_CURRENT_BINARY_DIR}/decoder-source/mpg123-1.33.7")
foreach(provider SDL3 mpg123)
  add_library(lifecycle_${provider} SHARED IMPORTED)
  if(WIN32)
    set_target_properties(lifecycle_${provider} PROPERTIES
      IMPORTED_LOCATION "${CSE_LIFECYCLE_PROVIDER_DIRECTORY}/${provider}.dll"
      IMPORTED_IMPLIB "${CSE_LIFECYCLE_PROVIDER_DIRECTORY}/${provider}.lib")
  else()
    if(provider STREQUAL SDL3)
      set(filename libSDL3.0.dylib)
    else()
      set(filename libmpg123.dylib)
    endif()
    set_target_properties(lifecycle_${provider} PROPERTIES
      IMPORTED_LOCATION "${CSE_LIFECYCLE_PROVIDER_DIRECTORY}/${filename}")
  endif()
endforeach()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/production_audio_boundary.inc"
    "${CMAKE_CURRENT_BINARY_DIR}/production_dma_close.inc" "${CMAKE_CURRENT_BINARY_DIR}/production_run.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_lifecycle.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS prepare_lifecycle.py "${CSE_ROOT}/GoldSrc-SDK/engine/cd.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/snd_sdl.cpp" "${CSE_ROOT}/GoldSrc-SDK/engine/sys_dll2.cpp" VERBATIM)
add_custom_target(audio_lifecycle_extract DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/production_audio_boundary.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/production_dma_close.inc" "${CMAKE_CURRENT_BINARY_DIR}/production_run.inc")
add_library(cse_audio_lifecycle_module SHARED fixture_module.cpp worker_fault_injection.cpp
  production_boundary.cpp production_dma.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/production_audio_boundary.inc"
  "${CSE_ROOT}/GoldSrc-SDK/engine/mpg123_music.cpp"
  "${CSE_ROOT}/GoldSrc-SDK/engine/mpg123_decoder.cpp"
  "${CSE_ROOT}/GoldSrc-SDK/engine/thread.cpp")
add_dependencies(cse_audio_lifecycle_module audio_lifecycle_extract)
add_executable(cse_audio_lifecycle_probe fixture_loader.cpp)
add_executable(cse_audio_worker_faults worker_fault_tests.cpp worker_fault_injection.cpp
  "${CSE_ROOT}/GoldSrc-SDK/engine/thread.cpp")
foreach(target cse_audio_lifecycle_module cse_audio_lifecycle_probe cse_audio_worker_faults)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CSE_ROOT}/GoldSrc-SDK/external/SDL3/include" "${decoder_source}/src/include")
  target_compile_definitions(${target} PRIVATE CSE_SOUND_USE_SDL=1
    "$<$<CONFIG:Debug>:_DEBUG>" "$<$<CONFIG:Release>:NDEBUG>")
  if(MSVC)
    target_compile_definitions(${target} PRIVATE LINK_MPG123_DLL)
    target_compile_options(${target} PRIVATE /W4 /WX /Zi
      "$<$<CONFIG:Debug>:/Od>" "$<$<CONFIG:Release>:/O2>")
    target_link_options(${target} PRIVATE /DEBUG)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wshorten-64-to-32
      "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Release>:-O2>")
  endif()
  if(CSE_AUDIO_LIFECYCLE_SANITIZERS AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
  target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()
foreach(target cse_audio_lifecycle_module cse_audio_lifecycle_probe)
  target_link_libraries(${target} PRIVATE lifecycle_SDL3 lifecycle_mpg123 "${CMAKE_DL_LIBS}")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:lifecycle_SDL3>" "$<TARGET_FILE_DIR:${target}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:lifecycle_mpg123>" "$<TARGET_FILE_DIR:${target}>")
endforeach()
set(worker "${CSE_ROOT}/GoldSrc-SDK/engine/thread.cpp")
set_property(SOURCE "${worker}" APPEND PROPERTY COMPILE_DEFINITIONS CSE_INJECT_WORKER_CALLS=1)
if(MSVC)
  set_property(SOURCE "${worker}" APPEND PROPERTY COMPILE_OPTIONS
    "/FI${CMAKE_CURRENT_SOURCE_DIR}/worker_fault_injection.h")
  # Preserve Run /EHa, audio /EHsc and the worker's absent EH flag / precise FP policy.
  set_property(SOURCE "${worker}" APPEND PROPERTY COMPILE_OPTIONS /fp:precise)
  set_property(SOURCE fixture_module.cpp APPEND PROPERTY COMPILE_OPTIONS /EHa /fp:precise)
  set_property(SOURCE production_boundary.cpp fixture_loader.cpp worker_fault_injection.cpp worker_fault_tests.cpp
    "${CSE_ROOT}/GoldSrc-SDK/engine/mpg123_music.cpp" "${CSE_ROOT}/GoldSrc-SDK/engine/mpg123_decoder.cpp"
    APPEND PROPERTY COMPILE_OPTIONS /EHsc)
else()
  set_property(SOURCE "${worker}" APPEND PROPERTY COMPILE_OPTIONS
    -include "${CMAKE_CURRENT_SOURCE_DIR}/worker_fault_injection.h")
endif()
enable_testing()
add_test(NAME audio-module-explicit-unload
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_fixture.py"
    --probe "$<TARGET_FILE:cse_audio_lifecycle_probe>" --module "$<TARGET_FILE:cse_audio_lifecycle_module>"
    --track "${decoder_source}/src/tests/sweep.mp3" --evidence "${CMAKE_CURRENT_BINARY_DIR}/evidence")
set(faults 1 3)
if(WIN32)
  list(APPEND faults 2)
else()
  list(APPEND faults 4 5 6)
endif()
foreach(fault IN LISTS faults)
  add_test(NAME audio-worker-init-failure-${fault} COMMAND cse_audio_worker_faults ${fault})
endforeach()
