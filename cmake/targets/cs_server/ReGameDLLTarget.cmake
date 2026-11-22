cse_add_runtime(cs_server SHARED "mp")
include("${CMAKE_CURRENT_LIST_DIR}/ReGameDLLSources.cmake")

set(regamedll_root "${CSE_REGAMEDLL_STAGED_SOURCE_ROOT}/regamedll")
if(MSVC)
  set(platform_sources public/tier0/assert_dialog.cpp public/tier0/platform_win32.cpp public/tier0/assert_dialog.rc)
else()
  set(platform_sources public/tier0/platform_posix.cpp)
endif()

set(wrapper_root "${CMAKE_CURRENT_BINARY_DIR}/regamedll-wrappers")
set_property(TARGET cs_server PROPERTY CSE_REGAMEDLL_WRAPPER_ROOT "${wrapper_root}")
set(sources "")
foreach(relative IN LISTS CSE_REGAMEDLL_GAME_SOURCES platform_sources CSE_REGAMEDLL_SUPPORT_SOURCES)
  set(input "${regamedll_root}/${relative}")
  if(relative IN_LIST CSE_REGAMEDLL_CALLBACK_SOURCES)
    # Some callback classes are defined in the upstream .cpp. Compile the
    # registration in the same translation unit without changing that file.
    set(wrapper "${wrapper_root}/regamedll/${relative}")
    string(REGEX REPLACE "[.]cpp$" "" registration "${relative}")
    string(REPLACE "/" "_" registration "${registration}")
    file(CONFIGURE OUTPUT "${wrapper}" CONTENT [=[
#include "@input@"
#include "regamedll_saved_callbacks.h"

CSE_DEFINE_REGAMEDLL_CALLBACKS(@registration@)
]=] @ONLY NEWLINE_STYLE LF)
    list(APPEND sources "${wrapper}")
  else()
    list(APPEND sources "${input}")
  endif()
endforeach()

set(saved_callbacks "${CSE_REGAMEDLL_INTEGRATION}/regamedll_saved_callbacks.cpp")
target_sources(cs_server PRIVATE ${sources} "${CSE_REGAMEDLL_INTEGRATION}/game_module_entry.cpp" "${saved_callbacks}")
target_include_directories(cs_server PRIVATE
  "${regamedll_root}" "${regamedll_root}/engine" "${regamedll_root}/common" "${regamedll_root}/dlls"
  "${regamedll_root}/game_shared" "${regamedll_root}/pm_shared" "${regamedll_root}/regamedll"
  "${regamedll_root}/unittests" "${CSE_REGAMEDLL_STAGED_SOURCE_ROOT}/dep/cppunitlite/include"
  "${regamedll_root}/public" "${regamedll_root}/public/regamedll"
  "${CSE_SDK_DIR}/common" "${CSE_SDK_DIR}/public" "${CSE_REGAMEDLL_INTEGRATION}")
set_target_properties(cs_server PROPERTIES CXX_STANDARD 17)
target_compile_definitions(cs_server PRIVATE
  REGAMEDLL_FIXES REGAMEDLL_API REGAMEDLL_ADD UNICODE_FIXES BUILD_LATEST CLIENT_WEAPONS USE_QSTRING
  CSE_SHARED_GAME_CONTRACT $<$<CONFIG:Release>:NDEBUG> CSE_ENABLE_ZBOT=$<BOOL:${CSE_ENABLE_ZBOT}>)
if(MSVC)
  target_compile_definitions(cs_server PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
  set_property(TARGET cs_server PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # The typed save catalog requires a distinct address for each registered callback.
  target_link_options(cs_server PRIVATE /OPT:NOICF /INCREMENTAL:NO
    "$<$<OR:$<CONFIG:Debug>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>")
  target_compile_options(cs_server PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:/EHsc;/GR;/Zc:__cplusplus;/fp:precise>"
    "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>>:/Od;/Zi>"
    "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/O2>"
    "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>,$<COMPILE_LANGUAGE:CXX>>:/Zi>")
  # Retain the stock public C exports and their ordinals. ReGameDLL has its own
  # private C++ implementation and exports additional entities through DLLEXPORT.
  set(export_input "${CSE_ROOT}/cmake/exports/cs_server.def")
  set_target_properties(cs_server PROPERTIES
    CSE_WIN32_EXPORT_DEFINITION "${CSE_ROOT}/cmake/exports/win32/cstrikedll.def"
    CSE_NATIVE_EXPORT_DEFINITION "${export_input}")
  # Inventory can be queried to regenerate the native definition itself.
  set(exports "")
  if(NOT CSE_BUILD_INVENTORY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${export_input}")
    file(READ "${export_input}" exports)
    if(CSE_TARGET_ARCH STREQUAL "x86")
      string(REPLACE "GiveFnptrsToDll @1" "GiveFnptrsToDll=_GiveFnptrsToDll@8 @1" exports "${exports}")
    endif()
    string(REPLACE "g_flTimeLimit @2" "g_flTimeLimit @2 DATA" exports "${exports}")
  endif()
  set(definition "${CSE_REGAMEDLL_STAGED_SOURCE_ROOT}/regamedll/cse-exports.def")
  file(CONFIGURE OUTPUT "${definition}" CONTENT "${exports}" @ONLY)
  target_sources(cs_server PRIVATE "${definition}")
else()
  target_compile_definitions(cs_server PRIVATE
    _LINUX LINUX _GLIBCXX_USE_CXX11_ABI=0
    _stricmp=strcasecmp _strnicmp=strncasecmp _strdup=strdup _unlink=unlink _snprintf=snprintf
    _vsnprintf=vsnprintf _write=write _close=close _access=access _vsnwprintf=vswprintf)
  target_compile_options(cs_server PRIVATE -U_FORTIFY_SOURCE -Wall -fno-exceptions -fno-builtin
    -Wno-unknown-pragmas -ffunction-sections -fdata-sections)
  target_link_libraries(cs_server PRIVATE dl)
  target_link_options(cs_server PRIVATE -Wl,-no_deduplicate)
  set_property(SOURCE "${saved_callbacks}" APPEND PROPERTY COMPILE_OPTIONS -fexceptions)
  target_compile_options(cs_server PRIVATE
    "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>>:-O0;-g>"
    "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-O3>"
    "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>,$<COMPILE_LANGUAGE:CXX>>:-g>")
endif()
if(CSE_STAGE_DISTRIBUTION_MATERIALS)
  cse_runtime_assets(cs_server "licenses/ReGameDLL_CS"
    "${CSE_REGAMEDLL_STAGED_SOURCE_ROOT}/LICENSE"
    "${CSE_REGAMEDLL_STAGED_SOURCE_ROOT}/LICENSE-TRANSITION.md")
endif()
cse_zbot_assets(cs_server)
cse_runtime_directory(cs_server "cstrike/dlls")
cse_native_module_alias(cs_server "cs")
