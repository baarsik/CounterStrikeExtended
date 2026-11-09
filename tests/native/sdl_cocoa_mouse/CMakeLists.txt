cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseCocoaMouseBoundary LANGUAGES C CXX OBJCXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CSE_FROZEN_SAMPLER "" CACHE PATH "Frozen native-mouse-window source GoldSrc-SDK directory")
set(CSE_SDL_INCLUDE "" CACHE PATH "Pinned SDL3.4.20 include directory")
set(CSE_SDL_PROVIDER "" CACHE FILEPATH "Exact owned SDL provider image")
add_library(owned_sdl SHARED IMPORTED)
set_target_properties(owned_sdl PROPERTIES IMPORTED_LOCATION "${CSE_SDL_PROVIDER}")
add_library(old_objc_consumer SHARED consumer.cpp "${CSE_FROZEN_SAMPLER}/engine/native_mouse_window.cpp"
    "${CSE_FROZEN_SAMPLER}/engine/native_mouse_cocoa.mm")
target_include_directories(old_objc_consumer PRIVATE "${CSE_FROZEN_SAMPLER}/engine"
    "${CSE_FROZEN_SAMPLER}/common" "${CSE_SDL_INCLUDE}")
target_link_libraries(old_objc_consumer PRIVATE owned_sdl "-framework Cocoa")
set_source_files_properties("${CSE_FROZEN_SAMPLER}/engine/native_mouse_cocoa.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
add_executable(cocoa_module_host module_host.cpp)
target_include_directories(cocoa_module_host PRIVATE "${CSE_SDL_INCLUDE}")
add_executable(cocoa_exports_probe exports_red.cpp)
foreach(target old_objc_consumer cocoa_module_host cocoa_exports_probe)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES
        OBJCXX_STANDARD 14 OBJCXX_STANDARD_REQUIRED YES CXX_VISIBILITY_PRESET hidden)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables
        -ffp-model=precise -ffp-contract=off -Wno-overriding-option
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
endforeach()

add_library(cpp_consumer SHARED consumer.cpp "${CSE_ROOT}/GoldSrc-SDK/engine/native_mouse_window.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/engine/native_mouse_cocoa.cpp")
target_include_directories(cpp_consumer PRIVATE "${CSE_ROOT}/GoldSrc-SDK/engine" "${CSE_ROOT}/GoldSrc-SDK/common"
    "${CSE_SDL_INCLUDE}")
target_link_libraries(cpp_consumer PRIVATE owned_sdl)
set_target_properties(cpp_consumer PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_VISIBILITY_PRESET hidden)
target_compile_options(cpp_consumer PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables
    -ffp-model=precise -ffp-contract=off -Wno-overriding-option
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
add_executable(cocoa_c_import api_probe.c)
target_include_directories(cocoa_c_import PRIVATE "${CSE_ROOT}/GoldSrc-SDK/common")
target_link_libraries(cocoa_c_import PRIVATE owned_sdl)
set_target_properties(cocoa_c_import PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
target_compile_options(cocoa_c_import PRIVATE -Wall -Wextra -Werror)

add_executable(cocoa_provider_guards provider_guards.cpp)
target_include_directories(cocoa_provider_guards PRIVATE "${CSE_SDL_INCLUDE}" "${CSE_ROOT}/GoldSrc-SDK/common")
set_target_properties(cocoa_provider_guards PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
target_compile_options(cocoa_provider_guards PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")

set(CSE_COCOA_ADAPTER_SOURCE "${CSE_ROOT}/GoldSrc-SDK/engine/native_mouse_cocoa.cpp" CACHE FILEPATH
    "Actual C++ adapter or deliberate fixture mutant")
add_executable(cocoa_adapter_tests adapter_tests.cpp "${CSE_COCOA_ADAPTER_SOURCE}")
target_include_directories(cocoa_adapter_tests PRIVATE "${CSE_SDL_INCLUDE}" "${CSE_ROOT}/GoldSrc-SDK/engine"
    "${CSE_ROOT}/GoldSrc-SDK/common")
set_target_properties(cocoa_adapter_tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
target_compile_options(cocoa_adapter_tests PRIVATE -Wall -Wextra -Werror -fno-exceptions -funwind-tables
    -ffp-model=precise -ffp-contract=off -Wno-overriding-option
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
