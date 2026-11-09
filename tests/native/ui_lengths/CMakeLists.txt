cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseUiLengths LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_UI_LENGTH_SANITIZERS "Instrument production length handling" OFF)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hud_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_sources.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract_sources.py
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/hud.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/hud_vgui2print.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/hud_spectator.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/saytext.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public/UtlBuffer.cpp" VERBATIM)
add_executable(ui_length_tests length_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/hud_bodies.inc")
set_target_properties(ui_length_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(ui_length_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(ui_length_tests PRIVATE "$<$<CONFIG:Debug>:_DEBUG>" "$<$<CONFIG:Release>:NDEBUG>")
if(MSVC)
  target_compile_definitions(ui_length_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(ui_length_tests PRIVATE /W4 /WX /EHsc "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
  target_compile_options(ui_length_tests PRIVATE -Wall -Wextra -Werror -Wshorten-64-to-32
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
  if(CSE_UI_LENGTH_SANITIZERS)
    target_compile_options(ui_length_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ui_length_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()
enable_testing()
foreach(case IN ITEMS basename font-length spectator-name chat-bounds buffer-envelope buffer-counts)
  add_test(NAME ui-length-${case} COMMAND ui_length_tests ${case})
endforeach()

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/localization_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_save.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract_save.py "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/LocalizedStringTable.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public/UnicodeFileHelpers.cpp" VERBATIM)
add_executable(localization_save_tests save_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/localization_bodies.inc")
set_target_properties(localization_save_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(localization_save_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(localization_save_tests PRIVATE "$<$<CONFIG:Debug>:_DEBUG>" "$<$<CONFIG:Release>:NDEBUG>")
if(MSVC)
  target_compile_definitions(localization_save_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(localization_save_tests PRIVATE /W4 /WX /EHsc "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
  target_compile_options(localization_save_tests PRIVATE -Wall -Wextra -Werror -Wshorten-64-to-32
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
  if(CSE_UI_LENGTH_SANITIZERS)
    target_compile_options(localization_save_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(localization_save_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()
foreach(case IN ITEMS localization-bytes localization-supplementary localization-chunks localization-failures)
  add_test(NAME ui-${case} COMMAND localization_save_tests ${case})
endforeach()

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hl_bodies.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_hl.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract_hl.py "${CSE_ROOT}/GoldSrc-SDK/cl_dll/hud.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cl_dll/hud_servers.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cl_dll/hud_spectator.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/cl_dll/text_message.cpp" VERBATIM)
add_executable(hl_ui_length_tests hl_length_tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/hl_bodies.inc")
set_target_properties(hl_ui_length_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(hl_ui_length_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
  target_compile_definitions(hl_ui_length_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(hl_ui_length_tests PRIVATE /W4 /WX /EHsc "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>")
else()
  target_compile_options(hl_ui_length_tests PRIVATE -Wall -Wextra -Werror -Wshorten-64-to-32
    -Wno-deprecated-declarations
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>")
  if(CSE_UI_LENGTH_SANITIZERS)
    target_compile_options(hl_ui_length_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(hl_ui_length_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()
foreach(case IN ITEMS basename newline server-response spectator-name mode-message)
  add_test(NAME hl-ui-${case} COMMAND hl_ui_length_tests ${case})
endforeach()
