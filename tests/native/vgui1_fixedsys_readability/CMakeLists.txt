cmake_minimum_required(VERSION 3.25)
project(CseVgui1FixedSysReadability LANGUAGES CXX)
if(NOT APPLE)
    message(FATAL_ERROR "This fixture requires the actual Apple owner and scoped font services")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_FREETYPE_PREFIX "" CACHE PATH "Pinned source-built FreeType prefix")
set(VCPKG_IMPORT_PREFIX "${CSE_FREETYPE_PREFIX}")
find_package(ZLIB CONFIG REQUIRED PATHS "${CSE_FREETYPE_PREFIX}/share/zlib" NO_DEFAULT_PATH)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS ZLIB::ZLIBSTATIC)
endif()
find_package(PNG CONFIG REQUIRED PATHS "${CSE_FREETYPE_PREFIX}/share/png" NO_DEFAULT_PATH)
# Consume the current parent-owned provider membership; do not duplicate it.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../vgui1_native_provider" baseline-provider)
set(vgui_source "${CSE_ROOT}/GoldSrc-SDK/utils/vgui/lib-src/vgui")
add_executable(fixedsys_owner_readability readability.cpp "${vgui_source}/posix/Font.cpp"
    "${vgui_source}/posix/VGui1FontServices.cpp" "${vgui_source}/Font.cpp"
    "${vgui_source}/common/fileimage.cpp" "${vgui_source}/common/vfontdata.cpp")
target_include_directories(fixedsys_owner_readability PRIVATE "${vgui_source}/common"
    "${CSE_ROOT}/tests/native/vgui1_legacy_raster")
target_link_libraries(fixedsys_owner_readability PRIVATE native_provider provider_discovery PNG::PNG)
target_compile_options(fixedsys_owner_readability PRIVATE -Wall -Wextra -Werror -fno-fast-math
    -ffp-contract=off -ffp-model=strict -fno-exceptions)
if(CSE_PROVIDER_SANITIZERS)
    target_compile_options(fixedsys_owner_readability PRIVATE -fsanitize=address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(fixedsys_owner_readability PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
# No automatic real-service CTest entry. The pinned runner supplies resources,
# fresh output directories and a 45-second external watchdog for each profile.
