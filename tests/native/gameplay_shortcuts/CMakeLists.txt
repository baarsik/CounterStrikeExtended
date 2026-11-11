cmake_minimum_required(VERSION 3.25)
project(CseGameplayShortcuts LANGUAGES C CXX OBJCXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CSE_SDL_INCLUDE "" CACHE PATH "Pinned SDL public headers")
set(CSE_SDL_CONFIG "" CACHE PATH "Qualified provider build_config directory")
set(CSE_SHORTCUT_PROVIDER_SOURCE "${CSE_ROOT}/GoldSrc-SDK/common/sdl3_native_cocoa_mouse.mm" CACHE FILEPATH
    "Complete actual owning Cocoa provider source")
option(CSE_SHORTCUT_SANITIZERS "Instrument actual source with ASan/UBSan" OFF)
add_library(shortcut_c_import OBJECT api_probe.c)
target_include_directories(shortcut_c_import PRIVATE "${CSE_ROOT}/GoldSrc-SDK/common")
set_target_properties(shortcut_c_import PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
add_executable(shortcut_tests shortcut_tests.mm fixture_sdl.cpp "${CSE_SHORTCUT_PROVIDER_SOURCE}")
target_include_directories(shortcut_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/common"
    "${CSE_SDL_INCLUDE}" "${CSE_SDL_CONFIG}")
target_compile_options(shortcut_tests PRIVATE -Wall -Wextra -Werror -fobjc-arc -fvisibility=hidden
    -fno-exceptions -funwind-tables -ffp-model=precise -ffp-contract=off -Wno-overriding-option
    "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
target_link_libraries(shortcut_tests PRIVATE "-framework Cocoa")
set_target_properties(shortcut_tests PROPERTIES OBJCXX_STANDARD 14 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
if(CSE_SHORTCUT_SANITIZERS)
    target_compile_options(shortcut_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    target_link_options(shortcut_tests PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME shortcut-absolute COMMAND shortcut_tests)
add_test(NAME shortcut-relative COMMAND shortcut_tests --relative)

# Recompile unchanged old provider suites against the complete new owning TU.
# Only additional SDL link seams are supplied; their bodies abort if reached.
foreach(suite cocoa interaction)
    if(suite STREQUAL cocoa)
        set(fixture "${CMAKE_CURRENT_LIST_DIR}/../native_mouse_window/cocoa_tests.mm")
    else()
        set(fixture "${CMAKE_CURRENT_LIST_DIR}/../system_capture/interaction_tests.mm")
    endif()
    add_executable(old_${suite}_tests "${fixture}" fixture_sdl.cpp "${CSE_SHORTCUT_PROVIDER_SOURCE}")
    target_include_directories(old_${suite}_tests PRIVATE "${CSE_ROOT}/GoldSrc-SDK/common"
        "${CSE_SDL_INCLUDE}" "${CSE_SDL_CONFIG}")
    target_compile_options(old_${suite}_tests PRIVATE -Wall -Wextra -Werror -fobjc-arc -fvisibility=hidden
        -fno-exceptions -funwind-tables -ffp-model=precise -ffp-contract=off -Wno-overriding-option
        "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g>")
    target_link_libraries(old_${suite}_tests PRIVATE "-framework Cocoa")
    set_target_properties(old_${suite}_tests PROPERTIES OBJCXX_STANDARD 14 OBJCXX_STANDARD_REQUIRED YES)
    if(CSE_SHORTCUT_SANITIZERS)
        target_compile_options(old_${suite}_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(old_${suite}_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME old-${suite}-absolute COMMAND old_${suite}_tests)
endforeach()
add_test(NAME old-interaction-relative COMMAND old_interaction_tests --relative)
