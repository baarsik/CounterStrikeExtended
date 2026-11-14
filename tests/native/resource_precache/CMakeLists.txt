cmake_minimum_required(VERSION 3.25)
project(CseResourcePrecache LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_RESOURCE_ENGINE "${CSE_ROOT}/src/engine" CACHE PATH "Production precache and decal source directory")
option(CSE_RESOURCE_SANITIZERS "Instrument the precache component with ASan/UBSan" OFF)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CSE_RESOURCE_ENGINE}/cl_main.cpp" "${CSE_RESOURCE_ENGINE}/decals.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract.py"
    "${CSE_RESOURCE_ENGINE}" "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)

foreach(renderer hw sw)
    add_executable(resource_precache_${renderer} precache_tests.cpp)
    target_compile_features(resource_precache_${renderer} PRIVATE cxx_std_17)
    target_include_directories(resource_precache_${renderer} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories(resource_precache_${renderer} SYSTEM PRIVATE
        "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/common" "${CSE_ROOT}/src/public"
        "${CSE_ROOT}/src/pm_shared" "${CSE_ROOT}/src/cl_dll" "${CSE_ROOT}/src/utils/vgui/include")
    if(renderer STREQUAL "sw")
        target_compile_definitions(resource_precache_${renderer} PRIVATE SW_RENDERER)
    endif()
    if(MSVC)
        target_compile_definitions(resource_precache_${renderer} PRIVATE WIN32 NOMINMAX _CRT_SECURE_NO_WARNINGS)
        target_compile_options(resource_precache_${renderer} PRIVATE /W4 /WX /EHsc /fp:precise)
    else()
        target_compile_definitions(resource_precache_${renderer} PRIVATE POSIX OSX)
        target_compile_options(resource_precache_${renderer} PRIVATE -Wall -Wextra -Werror
            -include "${CSE_ROOT}/src/public/cse_posix_crt.h")
        if(CSE_RESOURCE_SANITIZERS)
            target_compile_options(resource_precache_${renderer} PRIVATE
                -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(resource_precache_${renderer} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()

enable_testing()
foreach(renderer hw sw)
    foreach(case sound missing-sound model lazy-model inline-model event bsp decal mixed)
        add_test(NAME ${renderer}-${case} COMMAND resource_precache_${renderer} ${case})
    endforeach()
endforeach()
