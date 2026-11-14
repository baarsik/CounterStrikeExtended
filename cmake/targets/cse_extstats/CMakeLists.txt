add_library(cse_extstats SHARED)
include("${CMAKE_CURRENT_LIST_DIR}/Sources.cmake")
set(sdk "${CSE_ROOT}/src/external/amxx-sdk")
set(own "${CSE_ROOT}/src/amxx/cse_extstats")
target_include_directories(cse_extstats PRIVATE "${own}" "${CSE_ROOT}/src/public")
target_include_directories(cse_extstats SYSTEM PRIVATE
  "${sdk}/amxmodx/public/sdk" "${sdk}/amxmodx/public" "${sdk}/metamod-hl1/metamod"
  "${sdk}/hlsdk/dlls" "${sdk}/hlsdk/engine" "${sdk}/hlsdk/common"
  "${sdk}/hlsdk/pm_shared" "${sdk}/hlsdk/public")
target_compile_features(cse_extstats PRIVATE cxx_std_17)
target_compile_definitions(cse_extstats PRIVATE HAVE_STDINT_H NOMINMAX _CRT_SECURE_NO_WARNINGS)
set(package "${CMAKE_BINARY_DIR}/package/addons/amxmodx")
set_target_properties(cse_extstats PROPERTIES PREFIX "" OUTPUT_NAME "cse_extstats_amxx"
  RUNTIME_OUTPUT_DIRECTORY "${package}/modules" LIBRARY_OUTPUT_DIRECTORY "${package}/modules"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES)
if(MSVC)
  target_compile_options(cse_extstats PRIVATE /W3 /EHsc)
  target_compile_definitions(cse_extstats PRIVATE WIN32)
  target_link_options(cse_extstats PRIVATE "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8")
  set_property(TARGET cse_extstats PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
else()
  target_compile_options(cse_extstats PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-register
    -Wno-missing-field-initializers -fno-strict-aliasing)
  target_compile_definitions(cse_extstats PRIVATE LINUX)
  target_link_options(cse_extstats PRIVATE -static-libgcc -static-libstdc++ -Wl,--no-undefined)
  set_property(TARGET cse_extstats PROPERTY OUTPUT_NAME "cse_extstats_amxx_i386")
endif()
add_custom_command(TARGET cse_extstats POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${package}/configs" "${package}/scripting/include"
    "${package}/scripting" "${CMAKE_BINARY_DIR}/package/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${own}/cse_extstats.cfg" "${package}/configs/"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${own}/cse_extstats.inc" "${package}/scripting/include/"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${own}/example.sma" "${package}/scripting/"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${sdk}/licenses" "${CMAKE_BINARY_DIR}/package/licenses"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${sdk}/lock.json" "${CMAKE_BINARY_DIR}/package/licenses/sdk-lock.json"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CSE_ROOT}/docs/extended-stats.md" "${CMAKE_BINARY_DIR}/package/"
  VERBATIM)
