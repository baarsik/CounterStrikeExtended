cmake_minimum_required(VERSION 3.25)
project(CseVgui1NativeAtlas LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_NATIVE_SOURCE_ROOT "${CSE_ROOT}" CACHE PATH "Frozen production cohort root override")
set(VGUI "${CSE_ROOT}/GoldSrc-SDK/utils/vgui/lib-src/vgui")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(atlas_font_owner SHARED provider.cpp provider_boundary.cpp
    "${VGUI}/Font.cpp" "${VGUI}/posix/Font.cpp" "${VGUI}/posix/VGui1FontServices.cpp"
    "${VGUI}/Scheme.cpp" "${VGUI}/Cursor.cpp"
    "${CSE_ROOT}/tests/native/vgui1_font_selection/controlled_cursor.cpp"
    "${VGUI}/common/fileimage.cpp" "${VGUI}/common/vfontdata.cpp")
add_library(atlas_root_owner SHARED component.cpp boundary_probe.cpp unrelated_bridge.cpp allocation_faults.cpp
    "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/VGui1NativeRuntime.cpp"
    "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/VGui1NativeAtlas.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/VGui1NativeFontSelection.cpp")
target_compile_definitions(atlas_font_owner PRIVATE CSE_VGUI1_ATLAS_FIXTURE_PROVIDER)
# The real VGUI owner target already selects synchronous exceptions.
get_target_property(font_owner_sources atlas_font_owner SOURCES)
set_source_files_properties(${font_owner_sources} PROPERTIES COMPILE_OPTIONS -fexceptions)
target_link_libraries(atlas_root_owner PRIVATE atlas_font_owner)
add_executable(atlas_owned_host host.cpp)
add_library(atlas_benign_insertion SHARED
    "${CSE_ROOT}/tests/native/vgui1_host_image_owner/benign_insertion.cpp")
foreach(target atlas_font_owner atlas_root_owner atlas_owned_host)
    set_target_properties(${target} PROPERTIES VISIBILITY_INLINES_HIDDEN YES)
    target_include_directories(${target} PRIVATE "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine"
        "${CSE_ROOT}/GoldSrc-SDK/engine"
        "${CSE_ROOT}/GoldSrc-SDK/public" "${VGUI}/include" "${VGUI}/posix" "${VGUI}/common")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-fast-math -ffp-contract=off)
endforeach()
# Explicit per-file EH only. Existing wrapper/Selection code stays NoEH.
set_source_files_properties(allocation_faults.cpp host.cpp retirement.cpp PROPERTIES COMPILE_OPTIONS -fexceptions)
option(CSE_ATLAS_NOEH_RED "Controlled old NoEH policy fault observation" OFF)
if(NOT CSE_ATLAS_NOEH_RED)
    set_source_files_properties("${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/VGui1NativeRuntime.cpp"
        "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/VGui1NativeAtlas.cpp" PROPERTIES COMPILE_OPTIONS -fexceptions)
endif()
option(CSE_ATLAS_SANITIZERS "Instrument actual owned component and dylibs" OFF)
if(CSE_ATLAS_SANITIZERS)
    foreach(target atlas_font_owner atlas_root_owner atlas_owned_host)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endforeach()
endif()
enable_testing()
add_test(NAME actual-owner-one-unload COMMAND atlas_owned_host "$<TARGET_FILE:atlas_root_owner>"
    "$<TARGET_FILE:atlas_font_owner>" 1)
add_test(NAME actual-owner-v2-one-unload COMMAND atlas_owned_host "$<TARGET_FILE:atlas_root_owner>"
    "$<TARGET_FILE:atlas_font_owner>" 1 --binding-v2)
foreach(binding v1 v2)
    set(binding_argument)
    if(binding STREQUAL "v2")
        set(binding_argument --binding-v2)
    endif()
    add_test(NAME inserted-runtime-${binding} COMMAND "${CMAKE_COMMAND}" -E env
        "DYLD_INSERT_LIBRARIES=$<TARGET_FILE:atlas_benign_insertion>" CSE_OWNER_EXPECT_INSERTION=1
        "$<TARGET_FILE:atlas_owned_host>" "$<TARGET_FILE:atlas_root_owner>"
        "$<TARGET_FILE:atlas_font_owner>" 1 ${binding_argument})
endforeach()
if(NOT CSE_ATLAS_SANITIZERS)
    add_test(NAME actual-owner-reloads COMMAND atlas_owned_host "$<TARGET_FILE:atlas_root_owner>"
        "$<TARGET_FILE:atlas_font_owner>" 4)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/vgui_EngineSurfaceHW.cpp"
    "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/vgui_EngineSurfaceSW.cpp"
    "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/VGui1NativeRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/extract_retirement.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_retirement.py"
    --sdk "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK" --output "${CMAKE_CURRENT_BINARY_DIR}/retirement"
    COMMAND_ERROR_IS_FATAL ANY)
foreach(mode hw sw sw_win32)
    add_executable(retirement_${mode} retirement.cpp)
    set_target_properties(retirement_${mode} PROPERTIES CXX_STANDARD 17)
    target_include_directories(retirement_${mode} PRIVATE "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine"
        "${CSE_ROOT}/GoldSrc-SDK/engine"
        "${CSE_ROOT}/GoldSrc-SDK/public" "${CSE_ROOT}/GoldSrc-SDK/external/SDL3/include"
        "${CMAKE_CURRENT_BINARY_DIR}/retirement")
    target_compile_options(retirement_${mode} PRIVATE -Wall -Wextra -Werror -fno-exceptions)
    if(mode STREQUAL "hw")
        target_compile_definitions(retirement_${mode} PRIVATE CSE_RETIRE_HW)
    endif()
    if(mode STREQUAL "sw_win32")
        target_compile_definitions(retirement_${mode} PRIVATE CSE_SW_WIN32_BRANCH)
    endif()
    if(CSE_ATLAS_SANITIZERS)
        target_compile_options(retirement_${mode} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(retirement_${mode} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
    add_test(NAME actual-${mode}-retirement COMMAND retirement_${mode})
endforeach()

add_test(NAME allocation-no-propagation COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/check_faults.py" "$<TARGET_FILE:atlas_owned_host>"
    "$<TARGET_FILE:atlas_root_owner>" "$<TARGET_FILE:atlas_font_owner>")
set(CSE_ADAPTER_SOURCE "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine/VGui1NativeRuntime.cpp" CACHE FILEPATH
    "Complete real Runtime adapter source; old frozen source allowed for safe RED")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_adapter.py"
    --source "${CSE_ADAPTER_SOURCE}" --output "${CMAKE_CURRENT_BINARY_DIR}/adapter"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CSE_ADAPTER_SOURCE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/extract_adapter.py")
add_executable(embedded_adapter adapter.cpp)
target_include_directories(embedded_adapter PRIVATE "${CSE_NATIVE_SOURCE_ROOT}/GoldSrc-SDK/engine"
    "${CSE_ROOT}/GoldSrc-SDK/engine"
    "${CSE_ROOT}/GoldSrc-SDK/public" "${CMAKE_CURRENT_BINARY_DIR}/adapter")
target_compile_options(embedded_adapter PRIVATE -Wall -Wextra -Werror -fno-exceptions)
if(CSE_ATLAS_SANITIZERS)
    target_compile_options(embedded_adapter PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    target_link_options(embedded_adapter PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
add_test(NAME actual-embedded-adapter COMMAND embedded_adapter)

option(CSE_ATLAS_TSAN "Controlled fixed-source concurrent owner/callback race check" OFF)
if(CSE_ATLAS_TSAN)
    if(CSE_ATLAS_SANITIZERS)
        message(FATAL_ERROR "Run TSan separately from ASan/UBSan")
    endif()
    foreach(target atlas_font_owner atlas_root_owner atlas_owned_host retirement_hw retirement_sw embedded_adapter)
        target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=thread)
    endforeach()
endif()
