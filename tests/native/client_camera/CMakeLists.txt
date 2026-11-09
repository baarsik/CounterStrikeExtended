cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseClientCamera LANGUAGES CXX)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CSE_CAMERA_SANITIZERS "Instrument actual camera bodies with controlled services" OFF)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hl_camera.inc" "${CMAKE_CURRENT_BINARY_DIR}/cs_camera.inc"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_camera.py"
        "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS extract_camera.py "${CSE_ROOT}/GoldSrc-SDK/cl_dll/in_camera.cpp"
        "${CSE_ROOT}/GoldSrc-SDK/cstrike/cl_dll/in_camera.cpp" VERBATIM)
add_custom_target(camera_extract DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/hl_camera.inc")
enable_testing()
foreach(game hl cs)
    add_executable(camera_${game} ownership_tests.cpp)
    add_dependencies(camera_${game} camera_extract)
    target_include_directories(camera_${game} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
        "${CSE_ROOT}/GoldSrc-SDK/common" "${CSE_ROOT}/GoldSrc-SDK/cl_dll")
    if(game STREQUAL cs)
        target_compile_definitions(camera_${game} PRIVATE CSE_CAMERA_CS=1)
    else()
        target_compile_definitions(camera_${game} PRIVATE CSE_CAMERA_CS=0)
    endif()
    set_target_properties(camera_${game} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(camera_${game} PRIVATE /W4 /WX /fp:precise /EHs-c-
            $<$<CONFIG:Release>:/O2> $<$<CONFIG:Debug>:/Od>)
    else()
        target_compile_options(camera_${game} PRIVATE -Wall -Wextra -Werror -ffp-model=precise -ffp-contract=off
            -Wno-overriding-option -fno-exceptions -funwind-tables $<$<CONFIG:Release>:-O2> $<$<CONFIG:Debug>:-O0>)
        if(CSE_CAMERA_SANITIZERS)
            target_compile_options(camera_${game} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(camera_${game} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    add_test(NAME camera-${game}-ownership COMMAND camera_${game})
    set_tests_properties(camera-${game}-ownership PROPERTIES TIMEOUT 15)
endforeach()
