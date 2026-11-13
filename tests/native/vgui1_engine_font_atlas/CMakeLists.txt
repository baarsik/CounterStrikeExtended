cmake_minimum_required(VERSION 3.25)
project(CseVgui1EngineFontAtlasHeaders LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(atlas_header_probe OBJECT probe_headers.cpp)
target_include_directories(atlas_header_probe PRIVATE "${CSE_ROOT}/src/engine"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/include")
if(MSVC)
    target_compile_options(atlas_header_probe PRIVATE /W4 /WX /EHs-c-)
else()
    target_compile_options(atlas_header_probe PRIVATE -Wall -Wextra -Werror -fno-exceptions)
endif()
# Compile-only real declaration probe. No executable, test, provider or SDL link.

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CSE_ATLAS_SOURCE "${CSE_ROOT}/src/engine/VGUI_EngineSurfaceWrap.cpp" CACHE FILEPATH "Pinned method source")
set(CSE_ATLAS_MUTANT "none" CACHE STRING "Controlled generated-method mutation")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_methods.py"
    --source "${CSE_ATLAS_SOURCE}" --output "${CMAKE_CURRENT_BINARY_DIR}/generated" --mutant "${CSE_ATLAS_MUTANT}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(atlas_methods fixture.cpp "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/Font.cpp")
target_include_directories(atlas_methods PRIVATE "${CSE_ROOT}/src/engine"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
    target_compile_definitions(atlas_header_probe PRIVATE VGUI_EXPORTS)
    target_compile_definitions(atlas_methods PRIVATE VGUI_EXPORTS)
    target_compile_options(atlas_methods PRIVATE /W4 /WX /EHs-c- /fp:fast)
    target_compile_definitions(atlas_methods PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(atlas_methods PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-fast-math -ffp-contract=off)
endif()
option(CSE_ATLAS_SANITIZERS "Instrument owned fixture/methods" OFF)
if(CSE_ATLAS_SANITIZERS AND NOT MSVC)
    target_compile_options(atlas_methods PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer)
    target_link_options(atlas_methods PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
