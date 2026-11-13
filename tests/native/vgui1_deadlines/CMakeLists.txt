cmake_minimum_required(VERSION 3.25)
project(Vgui1Deadlines LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(VGUI "${CSE_ROOT}/src/utils/vgui/lib-src/vgui")
set(HEADER_VIEW private CACHE STRING "Actual widget declaring header view")
option(DEADLINE_SANITIZERS "Instrument fake-service actual-method component" OFF)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(fixture "${CMAKE_CURRENT_BINARY_DIR}/controlled-bindings")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/prepare_fixture.py"
  --root "${CSE_ROOT}" --output "${fixture}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${VGUI}/TextEntry.cpp" "${VGUI}/EditPanel.cpp" "${VGUI}/Panel.cpp" "${VGUI}/posix/App.cpp"
  "${VGUI}/win32/App.cpp" "${CMAKE_CURRENT_LIST_DIR}/prepare_fixture.py")
add_executable(deadline_reference deadline_reference.cpp controlled_dependencies.cpp
  "${fixture}/TextEntry.cpp" "${fixture}/EditPanel.cpp" "${fixture}/Panel-controlled-unreachable-property.cpp"
  "${VGUI}/App.cpp" "${VGUI}/Color.cpp" "${fixture}/App-controlled-clock.cpp" "${VGUI}/posix/VGui1AppPlatform.cpp")
target_include_directories(deadline_reference PRIVATE "${fixture}/headers-${HEADER_VIEW}"
  "${VGUI}/include" "${VGUI}" "${VGUI}/posix" "${CMAKE_CURRENT_LIST_DIR}")
set_target_properties(deadline_reference PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
  target_compile_definitions(deadline_reference PRIVATE VGUI_EXPORTS)
  target_compile_options(deadline_reference PRIVATE /W4 /WX /EHsc /fp:precise
    "/FI${CMAKE_CURRENT_LIST_DIR}/visibility_neutral_fixture.h")
  set_property(TARGET deadline_reference PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  target_compile_options(deadline_reference PRIVATE -Wall -Wextra -Werror -Wshorten-64-to-32
    -Wno-error=deprecated-declarations -Wno-error=inconsistent-missing-override)
endif()
if(DEADLINE_SANITIZERS)
  target_compile_options(deadline_reference PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(deadline_reference PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME deadline-methods COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_and_check.py"
  "$<TARGET_FILE:deadline_reference>" "${CMAKE_CURRENT_BINARY_DIR}/actual-methods.jsonl")
