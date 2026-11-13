cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseRenderDispatchFixture LANGUAGES CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
set(native "${CSE_ROOT}/src/engine/render/native")
add_library(render_dispatch STATIC "${native}/legacy_state.cpp" "${native}/legacy_commands.cpp"
    "${native}/legacy_texture.cpp" "${native}/frame_stream.cpp")
# The initial RED snapshot has no dispatch implementation/header. Do not provide a test substitute.
# After the API checkpoint this target compiles the actual production implementation.
if(EXISTS "${native}/legacy_dispatch.cpp")
    target_sources(render_dispatch PRIVATE "${native}/legacy_dispatch.cpp")
endif()
target_include_directories(render_dispatch PUBLIC "${native}" "${CSE_ROOT}/src/external/SDL3/include")
add_executable(render_dispatch_tests dispatch_tests.cpp)
target_link_libraries(render_dispatch_tests PRIVATE render_dispatch)
option(CSE_RENDER_GL_ORACLE "Reserved for separately admitted actual GL comparison" OFF)
if(CSE_RENDER_GL_ORACLE)
    message(FATAL_ERROR "Use the Root-owned real-gl-run-packet.md; this fixture must remain CPU-only")
endif()
option(CSE_RENDER_SANITIZERS "Instrument CPU layer and fixture" OFF)
if(CSE_RENDER_SANITIZERS AND MSVC)
    message(FATAL_ERROR "This fixture's ASan/UBSan mode requires Clang/GNU")
endif()
foreach(target render_dispatch render_dispatch_tests)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc /fp:precise
            "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Debug>:/Od>" "$<$<CONFIG:Debug>:/Zi>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -ffp-contract=off
            "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    endif()
    if(CSE_RENDER_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()
enable_testing()
foreach(case lifecycle mixed-owner matrices-queries shadow-failure capture-refusal refusal-gate extension-aliases hud-trace
    getfloatv-begin-end isenabled-begin-end)
    add_test(NAME render-dispatch-${case} COMMAND render_dispatch_tests ${case})
endforeach()

# T2b additive CPU cases; no compiler/runtime admission is implied by registration.
foreach(case t2b-slot-lifecycle t2b-mixed-texture t2b-texture-mapping t2b-unpack-owned t2b-ui-lightmap-trace
    t2b-array-copy-time t2b-array-refusal t2b-list-routing t2b-list-immediate-state t2b-list-refusal
    t2b-generated-names t2b-extension-ownership t2b-shadow-after-refusal t2b-state-no-frame)
    add_test(NAME render-dispatch-${case} COMMAND render_dispatch_tests ${case})
endforeach()
