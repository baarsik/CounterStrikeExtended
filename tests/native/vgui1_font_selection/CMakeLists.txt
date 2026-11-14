cmake_minimum_required(VERSION 3.25)
project(Vgui1FontSelection LANGUAGES C CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(vgui "${CSE_ROOT}/src/utils/vgui/lib-src/vgui")
set(SELECTION_SOURCE "${CSE_ROOT}/src/engine/VGui1NativeFontSelection.cpp" CACHE FILEPATH
  "Default real component; controlled mutation source permitted only in this standalone fixture")
add_executable(font_selection component_tests.cpp fake_provider.cpp controlled_cursor.cpp actual_declaration_probe.cpp
  "${SELECTION_SOURCE}"
  "${vgui}/Scheme.cpp" "${vgui}/Cursor.cpp" "${vgui}/Font.cpp"
  "${vgui}/posix/Font.cpp" "${vgui}/posix/VGui1FontServices.cpp"
  "${vgui}/common/fileimage.cpp" "${vgui}/common/vfontdata.cpp")
target_include_directories(font_selection PRIVATE "${CSE_ROOT}/src/engine"
  "${vgui}/include" "${vgui}/posix" "${vgui}/common")
option(SELECTION_PUBLIC_HEADERS "Use actual consumer public VGUI declarations first" OFF)
if(SELECTION_PUBLIC_HEADERS)
  # Actual provider TUs keep their declaring owner context. Only the new
  # component and callers use the real public view; no header copy/rewrite.
  set_source_files_properties(component_tests.cpp actual_declaration_probe.cpp "${SELECTION_SOURCE}" PROPERTIES
    INCLUDE_DIRECTORIES "${CSE_ROOT}/src/utils/vgui/include")
endif()
set_target_properties(font_selection PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_options(font_selection PRIVATE -Wall -Wextra -Werror -fno-exceptions)
option(SELECTION_SANITIZERS "Instrument fake-service component" OFF)
if(SELECTION_SANITIZERS)
  target_compile_options(font_selection PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(font_selection PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
enable_testing()
add_test(NAME font-selection COMMAND font_selection)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME failure-children COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/failure_children.py"
  "$<TARGET_FILE:font_selection>")

# The actual font-atlas loader also accepts WebP.
if(TARGET font_selection)
    include("${CSE_ROOT}/cmake/WebPDependency.cmake")
    target_sources(font_selection PRIVATE "${CSE_ROOT}/src/common/image_codec.cpp")
    cse_link_webp(font_selection)
    set_property(SOURCE "${CSE_ROOT}/src/common/image_codec.cpp" APPEND PROPERTY COMPILE_OPTIONS
        "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>")
endif()
