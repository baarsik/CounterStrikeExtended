cmake_minimum_required(VERSION 3.25)
project(CseMotdLifetime LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_MOTD_SANITIZERS "Instrument the actual widget/owner method fixture" OFF)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/shared_lifetime.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/cs_lifetime.inc" "${CMAKE_CURRENT_BINARY_DIR}/owner.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/detach.inc")
add_custom_command(OUTPUT ${generated}
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_lifetime.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract_lifetime.py
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/controls/HTML.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/controls/Panel.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/vgui2/controls/HTML.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/vgui2/controls/Panel.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/vgui2/game_controls/clientmotd.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/html/native/browser_controller.cpp"
  VERBATIM)
enable_testing()
foreach(variant IN ITEMS shared cs)
  add_executable(motd_lifetime_${variant} lifetime_tests.cpp ${generated})
  set_target_properties(motd_lifetime_${variant} PROPERTIES
    CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_include_directories(motd_lifetime_${variant} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  if(variant STREQUAL "cs")
    target_compile_definitions(motd_lifetime_${variant} PRIVATE CSE_MOTD_CS)
  endif()
  if(MSVC)
    target_compile_definitions(motd_lifetime_${variant} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(motd_lifetime_${variant} PRIVATE /W4 /WX /EHsc)
  else()
    target_compile_definitions(motd_lifetime_${variant} PRIVATE _strdup=strdup)
    target_compile_options(motd_lifetime_${variant} PRIVATE -Wall -Wextra -Werror)
    if(CSE_MOTD_SANITIZERS)
      target_compile_options(motd_lifetime_${variant} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(motd_lifetime_${variant} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
  foreach(scenario IN ITEMS close-url close-html gui-stopped owner-destroy)
    add_test(NAME motd-${variant}-${scenario} COMMAND motd_lifetime_${variant} ${scenario})
  endforeach()
endforeach()
