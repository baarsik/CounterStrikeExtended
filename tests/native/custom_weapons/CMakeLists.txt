cmake_minimum_required(VERSION 3.25)
project(CseCustomWeaponsTests LANGUAGES CXX)

get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
option(CSE_CUSTOM_WEAPONS_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

enable_testing()
add_executable(custom_weapon_protocol protocol_tests.cpp "${CSE_ROOT}/src/common/cse_custom_weapons.cpp")
target_compile_features(custom_weapon_protocol PRIVATE cxx_std_17)
target_include_directories(custom_weapon_protocol PRIVATE
    "${CSE_ROOT}/src/public"
    "${CSE_ROOT}/src/cstrike/cl_dll")
if(MSVC)
    target_compile_options(custom_weapon_protocol PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(custom_weapon_protocol PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    if(CSE_CUSTOM_WEAPONS_SANITIZERS)
        target_compile_options(custom_weapon_protocol PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(custom_weapon_protocol PRIVATE -fsanitize=address,undefined)
    endif()
endif()
add_test(NAME custom_weapon_protocol COMMAND custom_weapon_protocol
    "${CSE_ROOT}/tools/qualification/fixtures/custom-weapons/weapon_svdex.cfg")
