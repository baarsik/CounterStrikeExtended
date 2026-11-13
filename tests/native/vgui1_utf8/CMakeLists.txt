cmake_minimum_required(VERSION 3.25)
project(CseVgui1Utf8 LANGUAGES CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../vgui1_native_provider" baseline-provider)
set(vgui_source "${CSE_ROOT}/src/utils/vgui/lib-src/vgui")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${vgui_source}/Font.cpp" "${vgui_source}/TextImage.cpp" "${vgui_source}/win32/Font.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
    "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/extracted" COMMAND_ERROR_IS_FATAL ANY)
add_executable(vgui1_utf8_text text.cpp)
target_include_directories(vgui1_utf8_text PRIVATE "${vgui_source}/common"
    "${CMAKE_CURRENT_BINARY_DIR}/extracted")
target_link_libraries(vgui1_utf8_text PRIVATE native_face)
if(MSVC)
    target_compile_options(vgui1_utf8_text PRIVATE /W4 /WX /fp:strict /utf-8)
else()
    target_compile_options(vgui1_utf8_text PRIVATE -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
endif()
if(CSE_PROVIDER_SANITIZERS)
    target_compile_options(vgui1_utf8_text PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer)
    target_link_options(vgui1_utf8_text PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
enable_testing()
add_executable(vgui1_utf8_windows_glyph windows_glyph.cpp)
target_include_directories(vgui1_utf8_windows_glyph PRIVATE "${vgui_source}/common"
    "${CMAKE_CURRENT_BINARY_DIR}/extracted")
if(MSVC)
    target_compile_options(vgui1_utf8_windows_glyph PRIVATE /W4 /WX /fp:strict /utf-8)
endif()
if(CSE_PROVIDER_SANITIZERS AND NOT MSVC)
    target_compile_options(vgui1_utf8_windows_glyph PRIVATE -fsanitize=address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(vgui1_utf8_windows_glyph PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
add_test(NAME vgui1-utf8-windows-glyph-receiver COMMAND vgui1_utf8_windows_glyph)
if(APPLE)
    add_test(NAME vgui1-utf8-real-font COMMAND vgui1_utf8_text
        "/System/Library/Fonts/Supplemental/Arial.ttf")
    set(CSE_ATLAS_SANITIZERS "${CSE_PROVIDER_SANITIZERS}" CACHE BOOL "Instrument actual native atlas" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../vgui1_native_atlas" baseline-atlas)
    get_target_property(atlas_sources atlas_root_owner SOURCES)
    list(REMOVE_ITEM atlas_sources component.cpp)
    set_property(TARGET atlas_root_owner PROPERTY SOURCES "${atlas_sources}")
    target_sources(atlas_root_owner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/atlas_component.cpp")
    add_executable(vgui1_utf8_atlas_host atlas_host.cpp)
    target_include_directories(vgui1_utf8_atlas_host PRIVATE "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/public")
    if(CSE_PROVIDER_SANITIZERS)
        target_compile_options(vgui1_utf8_atlas_host PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(vgui1_utf8_atlas_host PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
    add_test(NAME vgui1-utf8-actual-atlas COMMAND vgui1_utf8_atlas_host
        "$<TARGET_FILE:atlas_root_owner>" "$<TARGET_FILE:atlas_font_owner>")
    add_executable(vgui1_utf8_bitmap_owner bitmap_owner.cpp "${vgui_source}/posix/Font.cpp"
        "${vgui_source}/posix/VGui1FontServices.cpp" "${vgui_source}/Font.cpp"
        "${vgui_source}/common/fileimage.cpp" "${vgui_source}/common/vfontdata.cpp")
    target_include_directories(vgui1_utf8_bitmap_owner PRIVATE "${vgui_source}/common")
    target_link_libraries(vgui1_utf8_bitmap_owner PRIVATE native_face)
    target_compile_options(vgui1_utf8_bitmap_owner PRIVATE -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
    if(CSE_PROVIDER_SANITIZERS)
        target_compile_options(vgui1_utf8_bitmap_owner PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(vgui1_utf8_bitmap_owner PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
    set(CSE_HL_GAME "" CACHE PATH "Installed Half-Life directory for official bitmap glyph regression")
    if(CSE_HL_GAME)
        add_test(NAME vgui1-utf8-official-bitmap COMMAND vgui1_utf8_bitmap_owner
            "/System/Library/Fonts/Supplemental/Arial.ttf"
            "${CSE_HL_GAME}/valve/gfx/vgui/fonts/640_Primary Button Text.tga"
            "/System/Library/Fonts/Courier.ttc")
    endif()
endif()
