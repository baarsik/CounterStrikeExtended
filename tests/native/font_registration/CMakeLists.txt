cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CSE_ROOT}/cmake/PlatformDefaults.cmake")
project(CseNativeFontRegistration LANGUAGES C CXX)
include("${CSE_ROOT}/cmake/TargetPlatform.cmake")
if(NOT APPLE)
  message(FATAL_ERROR "This standalone fixture exercises macOS process font registration")
endif()
include("${CSE_ROOT}/cmake/NativeThirdParty.cmake")
cse_load_native_third_party()
set(font_source "${CSE_ROOT}/GoldSrc-SDK/vgui2/vgui_surfacelib")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_file(CSE_FONT_FIXTURE_SOURCE NAMES Arial.ttf
  PATHS /System/Library/Fonts/Supplemental NO_DEFAULT_PATH REQUIRED)
set(unicode_fixture "${CMAKE_CURRENT_BINARY_DIR}/字体-registration-fixture.ttf")
add_custom_command(OUTPUT "${unicode_fixture}"
  COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tests/native/font_discovery/make_unicode_fixture.py"
    "${CSE_FONT_FIXTURE_SOURCE}" "${unicode_fixture}"
  DEPENDS "${CSE_ROOT}/tests/native/font_discovery/make_unicode_fixture.py" "${CSE_FONT_FIXTURE_SOURCE}" VERBATIM)
add_custom_target(unicode_registration_fixture DEPENDS "${unicode_fixture}")
set(collection_fixture "${CMAKE_CURRENT_BINARY_DIR}/mixed-registration-fixture.ttc")
add_custom_command(OUTPUT "${collection_fixture}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/make_collection_fixture.py"
    "${unicode_fixture}" "${CSE_FONT_FIXTURE_SOURCE}" "${collection_fixture}"
  DEPENDS make_collection_fixture.py "${unicode_fixture}" "${CSE_FONT_FIXTURE_SOURCE}" VERBATIM)
add_custom_target(collection_registration_fixture DEPENDS "${collection_fixture}")
set(surface_methods "${CMAKE_CURRENT_BINARY_DIR}/surface_font_methods.h")
add_custom_command(OUTPUT "${surface_methods}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_surface_methods.py"
    "${CSE_ROOT}" "${surface_methods}"
  DEPENDS extract_surface_methods.py "${CSE_ROOT}/GoldSrc-SDK/engine/vgui2/BaseUISurface.cpp"
    "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/Surface.cpp" VERBATIM)
add_custom_target(surface_font_methods DEPENDS "${surface_methods}")
include("${CSE_ROOT}/cmake/MacFontService.cmake")
set(CSE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime")
cse_define_mac_font_registration_service("${font_source}/native/MacFontRegistration.cpp")
target_include_directories(cse_mac_font_registration PUBLIC "${font_source}")
target_compile_options(cse_mac_font_registration PRIVATE -Wall -Wextra -Werror)
add_executable(font_registration_tests registration_tests.cpp "${font_source}/native/MacFontDiscovery.cpp")
add_dependencies(font_registration_tests unicode_registration_fixture collection_registration_fixture surface_font_methods)
target_include_directories(font_registration_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(font_registration_tests PRIVATE cxx_std_17)
target_compile_options(font_registration_tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(font_registration_tests PRIVATE CSE_FONT_REGISTRATION_FIXTURE="${unicode_fixture}"
  CSE_FONT_REGISTRATION_COLLECTION="${collection_fixture}")
target_link_libraries(font_registration_tests PRIVATE cse_mac_font_registration Freetype::Freetype
  "-framework CoreText" "-framework CoreFoundation")
foreach(owner a b)
  add_library(font_owner_${owner} SHARED owner_module.cpp)
  target_compile_features(font_owner_${owner} PRIVATE cxx_std_17)
  target_compile_options(font_owner_${owner} PRIVATE -Wall -Wextra -Werror)
  target_link_libraries(font_owner_${owner} PRIVATE cse_mac_font_registration)
endforeach()
target_compile_definitions(font_registration_tests PRIVATE
  CSE_FONT_OWNER_A="$<TARGET_FILE:font_owner_a>" CSE_FONT_OWNER_B="$<TARGET_FILE:font_owner_b>")
add_dependencies(font_registration_tests font_owner_a font_owner_b)
option(CSE_FONT_SANITIZERS "Run standalone font registration with ASan/UBSan" OFF)
if(CSE_FONT_SANITIZERS)
  foreach(target cse_mac_font_registration font_registration_tests font_owner_a font_owner_b)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()
enable_testing()
foreach(scenario lifecycle owners external failure collection resolver surfaces restart modules exit)
  add_test(NAME font-registration-${scenario} COMMAND font_registration_tests "${scenario}")
endforeach()
