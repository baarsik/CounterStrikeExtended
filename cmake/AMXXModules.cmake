project(CseAmxxModule LANGUAGES C CXX)
if(NOT (WIN32 AND MSVC) AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "AMXX module targets are Windows/MSVC x86 and Linux x86")
endif()
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
if(NOT CSE_TARGET_ARCH STREQUAL x86)
  message(FATAL_ERROR "AMXX modules require an x86 compiler (x86 MSVC environment or GCC multilib -m32)")
endif()

set(CSE_AMXX_SDK "${CSE_ROOT}/src/external/amxx-sdk")
set(CSE_AMXX_PACKAGE "${CMAKE_BINARY_DIR}/package/addons/amxmodx")
set(CSE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/package")
set(CSE_STAGE_DISTRIBUTION_MATERIALS ON)
# Forms use small icons, and the legacy Linux x86 compiler lacks a WebP AVX2 intrinsic.
set(WEBP_ENABLE_SIMD OFF)
include("${CSE_ROOT}/cmake/WebPDependency.cmake")
add_custom_target(cse_amxx_package_notices
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/package/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CSE_AMXX_SDK}/licenses" "${CMAKE_BINARY_DIR}/package/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CSE_AMXX_SDK}/lock.json" "${CMAKE_BINARY_DIR}/package/lock.json"
  VERBATIM COMMENT "Stage AMXX SDK notices and dependency lock")

function(cse_add_amxx_module target)
  add_library(${target} SHARED)
  add_dependencies(${target} cse_amxx_package_notices)
  target_include_directories(${target} PRIVATE "${CSE_ROOT}/src/amxx/${target}" "${CSE_ROOT}/src/public")
  target_include_directories(${target} SYSTEM PRIVATE
    "${CSE_AMXX_SDK}/amxmodx/public/sdk" "${CSE_AMXX_SDK}/amxmodx/public" "${CSE_AMXX_SDK}/metamod-hl1/metamod"
    "${CSE_AMXX_SDK}/hlsdk/dlls" "${CSE_AMXX_SDK}/hlsdk/engine" "${CSE_AMXX_SDK}/hlsdk/common"
    "${CSE_AMXX_SDK}/hlsdk/pm_shared" "${CSE_AMXX_SDK}/hlsdk/public")
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_definitions(${target} PRIVATE HAVE_STDINT_H NOMINMAX _CRT_SECURE_NO_WARNINGS)
  set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "${target}_amxx"
    RUNTIME_OUTPUT_DIRECTORY "${CSE_AMXX_PACKAGE}/modules" LIBRARY_OUTPUT_DIRECTORY "${CSE_AMXX_PACKAGE}/modules"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W3 /EHsc)
    target_compile_definitions(${target} PRIVATE WIN32)
    target_link_options(${target} PRIVATE "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8")
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-register
      -Wno-missing-field-initializers -fno-strict-aliasing)
    target_compile_definitions(${target} PRIVATE LINUX)
    target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++ -Wl,--no-undefined)
    set_property(TARGET ${target} PROPERTY OUTPUT_NAME "${target}_amxx_i386")
  endif()
endfunction()

add_subdirectory("${CSE_ROOT}/cmake/targets/csextended" "${CMAKE_BINARY_DIR}/targets/csextended")
