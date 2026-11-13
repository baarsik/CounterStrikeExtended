cmake_minimum_required(VERSION 3.25)
project(CseNativeVguiFocus LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_VGUI_FOCUS_SANITIZERS "Instrument the isolated production focus path" OFF)
set(generated_sources
  "${CMAKE_CURRENT_BINARY_DIR}/focus_map.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/shared_bodies.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/cs_bodies.inc")
add_custom_command(OUTPUT ${generated_sources}
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_sources.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract_sources.py
    "${CSE_ROOT}/src/vgui2/controls/Panel.h"
    "${CSE_ROOT}/src/vgui2/controls/Panel.cpp"
    "${CSE_ROOT}/src/cstrike/vgui2/controls/Panel.cpp"
  VERBATIM)
enable_testing()
foreach(variant IN ITEMS shared cs public)
  if(variant STREQUAL "cs")
    set(keyvalues_source "${CSE_ROOT}/src/cstrike/public/KeyValues.cpp")
    set(buffer_source "${CSE_ROOT}/src/cstrike/public/UtlBuffer.cpp")
  elseif(variant STREQUAL "public")
    set(keyvalues_source "${CSE_ROOT}/src/vgui2/src/public/KeyValues.cpp")
    set(buffer_source "${CSE_ROOT}/src/vgui2/src/public/UtlBuffer.cpp")
  else()
    set(keyvalues_source "${CSE_ROOT}/src/engine/KeyValues.cpp")
    set(buffer_source "${CSE_ROOT}/src/engine/utlbuffer.cpp")
  endif()
  add_executable(vgui_focus_${variant} focus_tests.cpp ${generated_sources}
    "${keyvalues_source}" "${buffer_source}")
  set_target_properties(vgui_focus_${variant} PROPERTIES
    CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_include_directories(vgui_focus_${variant} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/public")
  if(variant STREQUAL "cs")
    target_include_directories(vgui_focus_${variant} BEFORE PRIVATE "${CSE_ROOT}/src/cstrike/public")
    target_compile_definitions(vgui_focus_${variant} PRIVATE CSE_FOCUS_CS)
  elseif(variant STREQUAL "public")
    target_include_directories(vgui_focus_${variant} BEFORE PRIVATE
      "${CSE_ROOT}/src/vgui2/src/public" "${CSE_ROOT}/src/vgui2/src")
    target_compile_definitions(vgui_focus_${variant} PRIVATE CSE_FOCUS_PUBLIC)
  endif()
  if(MSVC)
    target_compile_definitions(vgui_focus_${variant} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(vgui_focus_${variant} PRIVATE /W4 /EHsc)
  else()
    target_include_directories(vgui_focus_${variant} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat")
    target_compile_definitions(vgui_focus_${variant} PRIVATE _vsnprintf=vsnprintf)
    target_compile_options(vgui_focus_${variant} PRIVATE -Wall -Wextra -fshort-wchar)
    if(CSE_VGUI_FOCUS_SANITIZERS)
      target_compile_options(vgui_focus_${variant} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(vgui_focus_${variant} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
  foreach(scenario IN ITEMS forward null-default copied modern-map old-map pointer-copy)
    add_test(NAME vgui-focus-${variant}-${scenario} COMMAND vgui_focus_${variant} ${scenario})
  endforeach()
endforeach()
