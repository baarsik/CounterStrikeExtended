cmake_minimum_required(VERSION 3.25)
project(CseMacClientIntegration LANGUAGES CXX)
if(NOT APPLE)
  message(FATAL_ERROR "This fixture loads the actual macOS ARM64 client libraries")
endif()
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CSE_CLIENT_RUNTIME "" CACHE PATH "Built cse_client runtime directory")
foreach(name vgui vgui2)
  set(cse_${name} "${CSE_CLIENT_RUNTIME}/${name}.dylib")
  if(NOT EXISTS "${cse_${name}}")
    message(FATAL_ERROR "Build cse_client first: missing ${cse_${name}}")
  endif()
endforeach()
set(cse_SDL3 "${CSE_CLIENT_RUNTIME}/libSDL3.0.dylib")
if(NOT EXISTS "${cse_SDL3}")
  message(FATAL_ERROR "Build cse_client first: missing ${cse_SDL3}")
endif()
add_executable(mac_client_ui ui_tests.cpp)
target_compile_features(mac_client_ui PRIVATE cxx_std_17)
target_include_directories(mac_client_ui PRIVATE
  "${CSE_ROOT}/GoldSrc-SDK/utils/vgui/lib-src/vgui/include"
  "${CSE_ROOT}/GoldSrc-SDK/utils/vgui/lib-src/vgui/posix"
  "${CSE_ROOT}/GoldSrc-SDK/vgui2/src/public"
  "${CSE_ROOT}/GoldSrc-SDK/public"
  "${CSE_ROOT}/GoldSrc-SDK/external/SDL3/include")
target_compile_options(mac_client_ui PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
target_link_libraries(mac_client_ui PRIVATE "${cse_vgui}" "${cse_vgui2}" "${cse_SDL3}")
set_target_properties(mac_client_ui PROPERTIES BUILD_RPATH "${CSE_CLIENT_RUNTIME}")
enable_testing()
add_test(NAME mac-client-ui COMMAND mac_client_ui -cse-fixture "argument with spaces")
set_tests_properties(mac-client-ui PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME mac-client-images COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/load_images.py"
  "${CSE_CLIENT_RUNTIME}")
