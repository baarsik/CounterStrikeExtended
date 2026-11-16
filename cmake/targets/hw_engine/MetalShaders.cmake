include_guard(GLOBAL)
get_filename_component(_CSE_METAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_METAL_COMPILER "" CACHE FILEPATH "Offline Metal compiler (xcrun --toolchain Metal --find metal)")
set(CSE_METAL_LINKER "" CACHE FILEPATH "Offline metallib linker")
function(cse_add_metal_shaders target)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(NOT EXISTS "${CSE_METAL_COMPILER}" OR NOT EXISTS "${CSE_METAL_LINKER}")
        message(FATAL_ERROR "Set CSE_METAL_COMPILER and CSE_METAL_LINKER to the offline Metal toolchain")
    endif()
    foreach(shader legacy_runtime radar)
        set(output "${CMAKE_CURRENT_BINARY_DIR}/metal-shaders/$<CONFIG>/${shader}_embedded.h")
        set(source "${_CSE_METAL_ROOT}/src/engine/render/native/shaders/${shader}.metal")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${Python3_EXECUTABLE}" "${_CSE_METAL_ROOT}/tools/compile_metal_shaders.py"
                --source "${source}" --output "${output}"
                --compiler "${CSE_METAL_COMPILER}" --linker "${CSE_METAL_LINKER}"
                --configuration "$<CONFIG>" --deployment-target 14.0
            DEPENDS "${source}" "${_CSE_METAL_ROOT}/tools/compile_metal_shaders.py"
            VERBATIM)
        target_sources(${target} PRIVATE "${output}")
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/metal-shaders/$<CONFIG>")
    endforeach()
endfunction()
