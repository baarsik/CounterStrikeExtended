cmake_minimum_required(VERSION 3.25)
project(CseVgui1AppColdAdmission LANGUAGES C CXX)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(reader_source "${CSE_ROOT}/src/launcher/native/VGui1AppColdAdmission.cpp")
if(EXISTS "${reader_source}")
    add_library(app_cold_reader STATIC "${reader_source}")
else()
    add_library(app_cold_reader STATIC missing.cpp)
endif()
target_include_directories(app_cold_reader PUBLIC "${CSE_ROOT}/src/launcher/native"
    "${CSE_ROOT}/src/public")
add_executable(reader_tests reader_tests.cpp)
target_link_libraries(reader_tests PRIVATE app_cold_reader)
add_executable(app_cold_inspect inspect.cpp)
target_link_libraries(app_cold_inspect PRIVATE app_cold_reader)
foreach(target app_cold_reader reader_tests app_cold_inspect)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
endforeach()
enable_testing()
add_test(NAME bounded-reader COMMAND reader_tests)

option(CSE_APP_COLD_CONSTRUCTORS "Build actual complete cold-constructor controlled fixtures" OFF)
if(CSE_APP_COLD_CONSTRUCTORS)
    if(NOT EXISTS "${CSE_APP_COLD_METADATA_HEADER}")
        message(FATAL_ERROR "Fresh actual metadata header must be supplied explicitly")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CSE_ROOT}/src/utils/vgui/include/VGUI_Panel.h"
        "${CSE_ROOT}/src/engine/VGUI_EngineSurfaceWrap.h" generate_abort_bindings.py)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/generate_abort_bindings.py"
        "${CSE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/abort_bindings.inc" COMMAND_ERROR_IS_FATAL ANY)
    set(vgui "${CSE_ROOT}/src/utils/vgui/lib-src/vgui")
    add_library(cold_counter SHARED counter.cpp)
    add_library(cold_vgui SHARED cold_provider.cpp cold_font_binding.cpp
        "${vgui}/App.cpp" "${vgui}/posix/App.cpp" "${vgui}/posix/VGui1AppPlatform.cpp"
        "${vgui}/Scheme.cpp" "${vgui}/Font.cpp" "${vgui}/posix/Font.cpp"
        "${vgui}/posix/VGui1FontServices.cpp" "${vgui}/Cursor.cpp"
        "${vgui}/Bitmap.cpp" "${vgui}/Image.cpp" "${vgui}/Color.cpp"
        "${vgui}/common/fileimage.cpp" "${vgui}/common/vfontdata.cpp")
    add_library(cold_root SHARED cold_root.cpp root_abort.cpp)
    target_link_libraries(cold_vgui PRIVATE cold_counter)
    target_link_libraries(cold_root PRIVATE cold_vgui cold_counter)
    foreach(target cold_vgui cold_root)
        target_sources(${target} PRIVATE "${CSE_ROOT}/src/common/vgui1_app_cold_publisher.cpp")
        target_compile_definitions(${target} PRIVATE CSE_APP_COLD_FIXTURE)
        target_include_directories(${target} PRIVATE "${vgui}/posix" "${vgui}/common")
        set_target_properties(${target} PROPERTIES VISIBILITY_INLINES_HIDDEN YES)
    endforeach()
    target_compile_definitions(cold_vgui PRIVATE CSE_APP_COLD_CURSOR_SOURCE="${vgui}/DefaultCursor.cpp")
    target_compile_definitions(cold_root PRIVATE CSE_APP_COLD_ROOT_SOURCE="${CSE_ROOT}/src/engine/vgui_intwrap.cpp")
    target_compile_definitions(cold_root PRIVATE POSIX OSX HL25_HW_ENGINE)
    target_compile_options(cold_root PRIVATE -Wno-error=delete-non-abstract-non-virtual-dtor -include "${CSE_ROOT}/src/public/cse_posix_crt.h")
    target_include_directories(cold_vgui PRIVATE "${vgui}/include")
    target_include_directories(cold_root PRIVATE "${CSE_ROOT}/src/utils/vgui/include"
        "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/public"
        "${CSE_ROOT}/src/common" "${CSE_ROOT}/src/pm_shared"
        "${CSE_ROOT}/src/cl_dll"
        "${CSE_ROOT}/src/external/SDL3/include" "${CMAKE_CURRENT_BINARY_DIR}")
    # Unique ABSOLUTE provider identity; the reader refuses @rpath resolution.
    set_target_properties(cold_vgui cold_counter PROPERTIES BUILD_WITH_INSTALL_NAME_DIR YES
        INSTALL_NAME_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    set_property(SOURCE "${CSE_ROOT}/src/common/vgui1_app_cold_publisher.cpp"
        TARGET_DIRECTORY cold_vgui APPEND PROPERTY COMPILE_DEFINITIONS
        "CSE_VGUI1_APP_COLD_GENERATED_HEADER=\"${CSE_APP_COLD_METADATA_HEADER}\"" CSE_VGUI1_APP_COLD_OFFER=1)
    # Source properties are directory-scoped, so the requirement publisher gets
    # its own wrapper TU (exact publisher included once, no duplicated record).
    set_property(TARGET cold_root PROPERTY SOURCES cold_root.cpp root_abort.cpp requirement_publisher.cpp)
    target_compile_definitions(cold_root PRIVATE CSE_VGUI1_APP_COLD_GENERATED_HEADER="${CSE_APP_COLD_METADATA_HEADER}")
    add_executable(cold_host cold_host.cpp)
    target_link_libraries(cold_host PRIVATE cold_counter app_cold_reader)
    foreach(target cold_counter cold_vgui cold_root cold_host)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-fast-math -ffp-contract=off)
    endforeach()
    add_test(NAME actual-cold-constructors COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/run_cold.py" --build "${CMAKE_CURRENT_BINARY_DIR}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/cold-controls")
    set_tests_properties(actual-cold-constructors PROPERTIES TIMEOUT 45)
    get_target_property(vgui_sources cold_vgui SOURCES)
    set_source_files_properties(${vgui_sources} PROPERTIES COMPILE_OPTIONS -fexceptions)
endif()

option(CSE_APP_COLD_SANITIZERS "Instrument actual controlled constructors and bounded reader" OFF)
if(CSE_APP_COLD_SANITIZERS)
    set(sanitized_targets app_cold_reader reader_tests app_cold_inspect)
    if(CSE_APP_COLD_CONSTRUCTORS)
        list(APPEND sanitized_targets cold_counter cold_vgui cold_root cold_host)
    endif()
    foreach(target IN LISTS sanitized_targets)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endforeach()
endif()

# The actual font-atlas loader also accepts WebP.
if(TARGET cold_vgui)
    include("${CSE_ROOT}/cmake/WebPDependency.cmake")
    target_sources(cold_vgui PRIVATE "${CSE_ROOT}/src/common/image_codec.cpp")
    cse_link_webp(cold_vgui)
    set_property(SOURCE "${CSE_ROOT}/src/common/image_codec.cpp" APPEND PROPERTY COMPILE_OPTIONS
        "$<$<CXX_COMPILER_ID:MSVC>:/EHsc>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>")
endif()
