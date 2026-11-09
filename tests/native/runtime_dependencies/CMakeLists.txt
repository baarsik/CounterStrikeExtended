cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseNativeRuntimeDependencyFixtures LANGUAGES C CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
set(CSE_SDK_DIR "${CSE_ROOT}/GoldSrc-SDK")
include("${CSE_ROOT}/cmake/RuntimeTargets.cmake")
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(CSE_RUNTIME_REJECTION_CASE "" CACHE STRING "One intentionally invalid input in a child configure")

# The logical manifest root has poisoned configured headers. These files must
# never be used by the compiler after the production include mapper runs.
set(CSE_SDK_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixture-sdk")
set(reference_prefix "${CSE_SDK_DIR}/external/installed/x86-windows-static-md")
file(MAKE_DIRECTORY "${reference_prefix}/include/freetype/config" "${CSE_SDK_DIR}/public")
file(WRITE "${reference_prefix}/include/ft2build.h" "#error Retained x86 FreeType headers shadowed native headers\n")
file(WRITE "${reference_prefix}/include/freetype/config/ftconfig.h" "#error Retained x86 ftconfig selected\n")
file(WRITE "${reference_prefix}/include/zconf.h" "#error Retained x86 zconf selected\n")
file(WRITE "${CSE_SDK_DIR}/public/runtime_sdk_marker.h" "constexpr int RuntimeSdkMarker = 73;\n")

add_executable(runtime_probe probe.cpp)
if(CSE_RUNTIME_REJECTION_CASE)
  include(rejection_cases.cmake)
  message(FATAL_ERROR "Rejection fixture unexpectedly accepted: ${CSE_RUNTIME_REJECTION_CASE}")
endif()

# Independent expectations for all 24 retained logical imports. The last
# column is the concrete archive behind a package's public interface target.
set(imports
  "freetype|freetype|freetyped|Freetype::Freetype|freetype"
  "bzip2|bz2|bz2d|BZip2::BZip2|BZip2::BZip2"
  "expat|libexpatMD|libexpatdMD|expat::expat|expat::expat"
  "png|libpng16|libpng16d|PNG::PNG|PNG::png_static"
  "zlib|zs|zsd|ZLIB::ZLIB|ZLIB::ZLIB"
  "brotlicommon|brotlicommon|brotlicommon|unofficial::brotli::brotlicommon|unofficial::brotli::brotlicommon"
  "brotlidec|brotlidec|brotlidec|unofficial::brotli::brotlidec|unofficial::brotli::brotlidec"
  "ogg|ogg|ogg|Ogg::ogg|Ogg::ogg"
  "vorbis|vorbis|vorbis|Vorbis::vorbis|Vorbis::vorbis"
  "vorbisfile|vorbisfile|vorbisfile|Vorbis::vorbisfile|Vorbis::vorbisfile"
  "vpx|vpx|vpx|unofficial::libvpx::libvpx|unofficial::libvpx::libvpx"
  "webm|webm|webm|unofficial::libwebm::libwebm|unofficial::libwebm::libwebm")
cse_native_runtime_include("${reference_prefix}/include" native_include)
cse_native_runtime_include("${reference_prefix}/include/webm" native_webm_include)
cse_native_runtime_include("${reference_prefix}/include/freetype/config" native_ft_config_include)
cse_native_runtime_include("${CSE_SDK_DIR}/public" sdk_include)
file(REAL_PATH "${CSE_NATIVE_THIRD_PARTY_PREFIX}" native_prefix)
if(NOT native_include STREQUAL "${native_prefix}/include"
    OR NOT native_webm_include STREQUAL "${native_prefix}/include/webm"
    OR NOT native_ft_config_include STREQUAL "${native_prefix}/include/freetype/config"
    OR NOT sdk_include STREQUAL "${CSE_SDK_DIR}/public")
  message(FATAL_ERROR "Native header mapping changed the selected include tree or another SDK include")
endif()
if(UNIX)
  set(sdk_alias "${CMAKE_CURRENT_BINARY_DIR}/fixture-sdk-alias")
  file(REMOVE "${sdk_alias}")
  file(CREATE_LINK "${CSE_SDK_DIR}" "${sdk_alias}" SYMBOLIC)
  set(original_sdk "${CSE_SDK_DIR}")
  set(CSE_SDK_DIR "${sdk_alias}")
  cse_native_runtime_include("${sdk_alias}/external/installed/x86-windows-static-md/include" alias_include)
  cse_native_runtime_include("${sdk_alias}/public" alias_sdk_include)
  set(CSE_SDK_DIR "${original_sdk}")
  if(NOT alias_include STREQUAL native_include OR NOT alias_sdk_include STREQUAL "${sdk_alias}/public")
    message(FATAL_ERROR "SDK root alias changed logical include routing or an ordinary SDK include")
  endif()
endif()

enable_testing()
add_test(NAME runtime-native-dependency-record-parser COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
  -s "${CMAKE_CURRENT_SOURCE_DIR}" -p test_verify_build_inputs.py -v)
set(selection_evidence "")
foreach(entry IN LISTS imports)
  string(REPLACE "|" ";" parts "${entry}")
  list(GET parts 0 family)
  list(GET parts 1 release_name)
  list(GET parts 2 debug_name)
  list(GET parts 3 package)
  list(GET parts 4 archive)
  set(target "runtime_${family}")
  add_executable(${target} probe.cpp)
  string(TOUPPER "${family}" probe_name)
  target_compile_definitions(${target} PRIVATE "CSE_PROBE_${probe_name}=1")
  target_compile_features(${target} PRIVATE cxx_std_17)
  foreach(config Release Debug)
    cse_target_dependency_include(${target} ${config} "${reference_prefix}/include")
    cse_source_dependency_include("${CMAKE_CURRENT_SOURCE_DIR}/probe.cpp" ${config}
      "${reference_prefix}/include/webm")
  endforeach()
  target_include_directories(${target} PRIVATE "${sdk_include}")
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
  cse_link_import(${target} Release "${reference_prefix}/lib/${release_name}.lib")
  cse_link_import(${target} Debug "${reference_prefix}/debug/lib/${debug_name}.lib")
  get_target_property(linked ${target} LINK_LIBRARIES)
  if(NOT linked STREQUAL "$<$<CONFIG:Release>:${package}>;$<$<CONFIG:Debug>:${package}>")
    message(FATAL_ERROR "${family}: unexpected package routing: ${linked}")
  endif()
  string(APPEND selection_evidence "${family}|$<TARGET_FILE:${archive}>\n")
  add_test(NAME "runtime-import-${family}" COMMAND ${target})
endforeach()
set_target_properties(runtime_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/selected-imports-$<CONFIG>.txt" CONTENT "${selection_evidence}")
add_test(NAME runtime-native-build-inputs COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/verify_build_inputs.py" "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_MAKE_PROGRAM}" "${native_prefix}" "$<CONFIG>" "${CSE_TARGET_ARCH}")
set(rejection_cases unknown-import wrong-root import-escape wrong-config unsupported-config
    include-escape invalid-include missing-include missing-prefix foreign-provider interface-provider
    configuration-fallback cached-provider-change wrong-architecture)
if(UNIX)
  list(APPEND rejection_cases sdk-internal-symlink sdk-escape-symlink)
endif()
foreach(case IN LISTS rejection_cases)
  add_test(NAME "runtime-reject-${case}" COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/expect_rejection.py" "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/reject-${case}" "${case}" "${CSE_NATIVE_THIRD_PARTY_PREFIX}"
    "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}" "${CSE_TARGET_ARCH}")
endforeach()
