cmake_minimum_required(VERSION 3.25)
project(CseLocalizationResources LANGUAGES NONE)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(NAME bundled-localization-resources
  COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_LIST_DIR}" -v)

get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CSE_SDK_DIR "${CSE_ROOT}/src")
set(CSE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>")
include("${CSE_ROOT}/cmake/Localization.cmake")
add_custom_target(localization_resources ALL)
cse_localization_assets(localization_resources)

set(CSE_LOCALIZATION_RUNTIME_ROOT "" CACHE PATH "Existing macOS runtime for actual VGUI loader checks")
if(APPLE AND CSE_LOCALIZATION_RUNTIME_ROOT)
  enable_language(CXX)
  add_executable(localization_loader_tests loader_tests.cpp)
  target_compile_features(localization_loader_tests PRIVATE cxx_std_17)
  target_include_directories(localization_loader_tests PRIVATE
    "${CSE_SDK_DIR}/public" "${CSE_SDK_DIR}/engine" "${CSE_SDK_DIR}/vgui2/src/public")
  add_dependencies(localization_loader_tests localization_resources)
  add_test(NAME actual-localization-loader
    COMMAND localization_loader_tests "${CSE_LOCALIZATION_RUNTIME_ROOT}" "${CSE_OUTPUT_DIRECTORY}")
  set_tests_properties(actual-localization-loader PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()
