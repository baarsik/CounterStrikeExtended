cmake_minimum_required(VERSION 3.25)
project(CseVgui1FontComponent LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(vgui_source "${CSE_ROOT}/src/utils/vgui/lib-src/vgui")
add_library(font_component SHARED "${vgui_source}/posix/Font.cpp" "${vgui_source}/posix/VGui1FontServices.cpp"
    "${vgui_source}/Font.cpp" "${vgui_source}/common/fileimage.cpp" "${vgui_source}/common/vfontdata.cpp")
option(CSE_FONT_FIXTURE_BINDINGS "Link only fixture-owned provider/Surface bindings" ON)
if(CSE_FONT_FIXTURE_BINDINGS)
    target_sources(font_component PRIVATE fake_provider.cpp)
endif()
target_include_directories(font_component PUBLIC "${vgui_source}/include" "${vgui_source}/posix"
    PRIVATE "${vgui_source}/common")
add_executable(font_component_tests component_tests.cpp)
target_link_libraries(font_component_tests PRIVATE font_component)
add_executable(font_prepare_tests prepare_tests.cpp)
target_link_libraries(font_prepare_tests PRIVATE font_component)
add_executable(font_misuse_tests misuse_tests.cpp)
target_link_libraries(font_misuse_tests PRIVATE font_component)
foreach(client a b)
    add_library(font_consumer_${client} SHARED consumer.cpp)
    target_link_libraries(font_consumer_${client} PRIVATE font_component)
    target_compile_definitions(font_consumer_${client} PRIVATE CSE_FONT_CONSUMER_NAME="${client}")
    set_target_properties(font_consumer_${client} PROPERTIES VISIBILITY_INLINES_HIDDEN YES)
endforeach()
add_executable(font_module_lifecycle module_lifecycle_tests.cpp)
target_include_directories(font_module_lifecycle PRIVATE "${vgui_source}/include" "${vgui_source}/posix")
add_library(font_reload_leaf SHARED asan_reload_leaf.cpp)
add_library(font_reload_virtual SHARED asan_reload_leaf.cpp)
target_compile_definitions(font_reload_virtual PRIVATE CSE_ASAN_RELOAD_VIRTUAL=1)
foreach(client a b)
    add_library(font_reload_client_${client} SHARED asan_reload_consumer.cpp)
    target_link_libraries(font_reload_client_${client} PRIVATE font_reload_virtual)
endforeach()
add_executable(font_reload_control asan_reload_control.cpp)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(component_targets font_component font_component_tests font_prepare_tests font_misuse_tests
    font_consumer_a font_consumer_b
    font_module_lifecycle font_reload_leaf font_reload_virtual font_reload_control
    font_reload_client_a font_reload_client_b)
foreach(target IN LISTS component_targets)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
endforeach()
target_compile_options(font_component PRIVATE -fno-exceptions)
set_target_properties(font_component PROPERTIES VISIBILITY_INLINES_HIDDEN YES)
option(CSE_FONT_SANITIZERS "Instrument the actual component and clients" OFF)
option(CSE_FONT_FAST_FP "Exercise inherited fast-FP/NoEH policy" OFF)
foreach(target IN LISTS component_targets)
    if(CSE_FONT_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
    if(CSE_FONT_FAST_FP)
        target_compile_options(${target} PRIVATE -ffast-math)
    endif()
endforeach()

string(REPEAT "p" 140 long_component)
set(owned_inputs "${CMAKE_CURRENT_BINARY_DIR}/owned_字体/${long_component}/${long_component}")
file(MAKE_DIRECTORY "${owned_inputs}")
file(WRITE "${owned_inputs}/lifecycle-face" "Owned Face")
enable_testing()
add_test(NAME font-component-contract COMMAND font_component_tests "${owned_inputs}")
add_test(NAME font-scoped-preparation COMMAND font_prepare_tests "${owned_inputs}")
set(lifecycle_arguments $<TARGET_FILE:font_component> $<TARGET_FILE:font_consumer_a>
    $<TARGET_FILE:font_consumer_b> "${owned_inputs}/lifecycle-face")
add_test(NAME font-module-lifecycle COMMAND font_module_lifecycle ${lifecycle_arguments}
    "${CMAKE_CURRENT_BINARY_DIR}/module-teardown.txt" once)
add_test(NAME font-process-exit COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_process_exit.py"
    $<TARGET_FILE:font_module_lifecycle> ${lifecycle_arguments} "${CMAKE_CURRENT_BINARY_DIR}/exit-teardown.txt")
add_test(NAME font-module-repeated COMMAND font_module_lifecycle ${lifecycle_arguments}
    "${CMAKE_CURRENT_BINARY_DIR}/repeat-teardown.txt" repeat)
add_test(NAME font-unexpected-use-guards COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_misuse.py"
    $<TARGET_FILE:font_misuse_tests> "${owned_inputs}/lifecycle-face")
set_tests_properties(font-component-contract font-scoped-preparation font-module-lifecycle font-process-exit
    font-module-repeated
    font-unexpected-use-guards
    PROPERTIES TIMEOUT 20)
