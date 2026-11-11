cmake_minimum_required(VERSION 3.25)
project(A3dLiveHandoff LANGUAGES CXX)
if(NOT APPLE)
    message(FATAL_ERROR "This bounded live handoff targets macOS")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
if(NOT EXISTS "${A3D_GRAPH}/compile_commands.json")
    message(FATAL_ERROR "A3D_GRAPH must identify the fresh complete production graph")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
    "${ROOT}" "${CMAKE_CURRENT_BINARY_DIR}" "${A3D_GRAPH}/compile_commands.json"
    COMMAND_ERROR_IS_FATAL ANY)
set(runtime "${A3D_GRAPH}/runtime/${CMAKE_BUILD_TYPE}")
set(decoder "${A3D_GRAPH}/dependencies/mpg123/source/mpg123-1.33.7/src/include")
add_library(a3d_live_capture SHARED capture.cpp)
target_compile_features(a3d_live_capture PRIVATE cxx_std_17)
target_compile_options(a3d_live_capture PRIVATE -Wall -Wextra -Werror)
target_include_directories(a3d_live_capture PRIVATE "${ROOT}/GoldSrc-SDK/external/SDL3/include")
target_link_libraries(a3d_live_capture PRIVATE "${runtime}/libSDL3.0.dylib")
foreach(mode bare instrumented)
    add_executable(a3d_live_${mode} live_probe.cpp
        "${ROOT}/tests/native/a3d_module/enumeration_admission.cpp"
        "${ROOT}/GoldSrc-SDK/engine/mpg123_music.cpp"
        "${ROOT}/GoldSrc-SDK/engine/mpg123_decoder.cpp")
    target_compile_features(a3d_live_${mode} PRIVATE cxx_std_17)
    target_compile_options(a3d_live_${mode} PRIVATE -Wall -Wextra -Werror)
    target_compile_definitions(a3d_live_${mode} PRIVATE A3D_CAPTURE=$<STREQUAL:${mode},instrumented>)
    target_include_directories(a3d_live_${mode} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
        "${ROOT}/GoldSrc-SDK/public" "${ROOT}/GoldSrc-SDK/a3dapi"
        "${ROOT}/GoldSrc-SDK/engine" "${ROOT}/GoldSrc-SDK/external/SDL3/include" "${decoder}")
    target_link_libraries(a3d_live_${mode} PRIVATE "${runtime}/libSDL3.0.dylib"
        "${runtime}/libmpg123.dylib" Threads::Threads ${CMAKE_DL_LIBS})
    if(mode STREQUAL instrumented)
        target_link_libraries(a3d_live_${mode} PRIVATE a3d_live_capture)
    endif()
    set_target_properties(a3d_live_${mode} PROPERTIES BUILD_RPATH "@loader_path;${runtime}")
endforeach()
