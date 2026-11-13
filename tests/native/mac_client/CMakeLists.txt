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
  "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/include"
  "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/posix"
  "${CSE_ROOT}/src/vgui2/src/public"
  "${CSE_ROOT}/src/public"
  "${CSE_ROOT}/src/external/SDL3/include")
target_compile_options(mac_client_ui PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
target_link_libraries(mac_client_ui PRIVATE "${cse_vgui}" "${cse_vgui2}" "${cse_SDL3}")
set_target_properties(mac_client_ui PROPERTIES BUILD_RPATH "${CSE_CLIENT_RUNTIME}")
enable_testing()
add_test(NAME mac-client-ui COMMAND mac_client_ui -all_languages -cse-fixture "argument with spaces"
  -literal "player \"bob\" smith")
add_test(NAME mac-client-restart-system-arguments COMMAND mac_client_ui --cse-resume-command-line
  "fixture -all_languages -cse-fixture \"argument with spaces\" -novid"
  fixture -all_languages -cse-fixture "argument with spaces" -literal "player \"bob\" smith")
set_tests_properties(mac-client-ui mac-client-restart-system-arguments PROPERTIES
  ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
add_executable(mac_client_surface_selection surface_selection.cpp)
target_compile_features(mac_client_surface_selection PRIVATE cxx_std_17)
target_include_directories(mac_client_surface_selection PRIVATE
  "${CSE_ROOT}/src/vgui2/controls"
  "${CSE_ROOT}/src/vgui2/src"
  "${CSE_ROOT}/src/vgui2/src/public"
  "${CSE_ROOT}/src/public"
  "${CSE_ROOT}/src/external/SDL3/include")
target_compile_options(mac_client_surface_selection PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
target_link_libraries(mac_client_surface_selection PRIVATE "${cse_SDL3}" "${CMAKE_DL_LIBS}")
set_target_properties(mac_client_surface_selection PROPERTIES BUILD_RPATH "${CSE_CLIENT_RUNTIME}")
add_test(NAME mac-client-surface-selection COMMAND mac_client_surface_selection "${CSE_CLIENT_RUNTIME}")
set_tests_properties(mac-client-surface-selection PROPERTIES
  WORKING_DIRECTORY "${CSE_CLIENT_RUNTIME}"
  ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME mac-client-images COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/load_images.py"
  "${CSE_CLIENT_RUNTIME}")
add_executable(mac_client_module_paths module_paths.cpp)
target_compile_features(mac_client_module_paths PRIVATE cxx_std_17)
target_include_directories(mac_client_module_paths PRIVATE "${CSE_ROOT}/src/public")
target_compile_options(mac_client_module_paths PRIVATE -Wall -Wextra -Werror)
target_link_libraries(mac_client_module_paths PRIVATE "${CMAKE_DL_LIBS}")
add_test(NAME mac-client-vfs-module-paths COMMAND mac_client_module_paths "${CSE_CLIENT_RUNTIME}")
add_executable(mac_client_bootstrap bootstrap.cpp
  "${CSE_ROOT}/src/launcher/native/VGui1HostImageOwner.cpp"
  "${CSE_ROOT}/src/launcher/native/SocketImageOwner.cpp")
target_compile_features(mac_client_bootstrap PRIVATE cxx_std_17)
target_include_directories(mac_client_bootstrap PRIVATE "${CSE_ROOT}/src/public"
  "${CSE_ROOT}/src/launcher/native")
target_compile_options(mac_client_bootstrap PRIVATE -Wall -Wextra -Werror -fno-exceptions)
target_link_libraries(mac_client_bootstrap PRIVATE "${CMAKE_DL_LIBS}")
add_test(NAME mac-client-root-bootstrap COMMAND mac_client_bootstrap "${CSE_CLIENT_RUNTIME}")
set_tests_properties(mac-client-root-bootstrap PROPERTIES WORKING_DIRECTORY "${CSE_CLIENT_RUNTIME}")
add_executable(mac_client_consumer_contract consumer_contract.cpp)
target_compile_features(mac_client_consumer_contract PRIVATE cxx_std_17)
target_include_directories(mac_client_consumer_contract PRIVATE
  "${CSE_ROOT}/src/utils/vgui/include"
  "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/posix")
target_link_libraries(mac_client_consumer_contract PRIVATE "${cse_vgui}")
set_target_properties(mac_client_consumer_contract PROPERTIES BUILD_RPATH "${CSE_CLIENT_RUNTIME}")
add_test(NAME mac-client-consumer-font-contract COMMAND mac_client_consumer_contract font)
add_test(NAME mac-client-consumer-grid-contract COMMAND mac_client_consumer_contract grid)
add_test(NAME mac-client-cohort-rejection COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cohort_rejection.py" "${CSE_CLIENT_RUNTIME}")
add_test(NAME mac-client-game-export-isolation COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/game_exports.py" "${CSE_CLIENT_RUNTIME}")

if(EXISTS "${CSE_CLIENT_RUNTIME}/swds.dylib")
  add_executable(mac_software_font software_font.cpp
    "${CSE_ROOT}/src/launcher/native/VGui1HostImageOwner.cpp"
  "${CSE_ROOT}/src/launcher/native/SocketImageOwner.cpp")
  target_compile_features(mac_software_font PRIVATE cxx_std_17)
  target_compile_options(mac_software_font PRIVATE -UNDEBUG)
  target_include_directories(mac_software_font PRIVATE "${CSE_ROOT}/src/engine"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/launcher/native"
    "${CSE_ROOT}/src/utils/vgui/lib-src/vgui/include"
    "${CSE_ROOT}/src/external/SDL3/include")
  target_link_libraries(mac_software_font PRIVATE "${CSE_CLIENT_RUNTIME}/swds.dylib" "${cse_vgui}")
  set_target_properties(mac_software_font PROPERTIES BUILD_RPATH "${CSE_CLIENT_RUNTIME}")
  add_test(NAME mac-software-font COMMAND mac_software_font "${CSE_CLIENT_RUNTIME}")
  set_tests_properties(mac-software-font PROPERTIES WORKING_DIRECTORY "${CSE_CLIENT_RUNTIME}")
  add_test(NAME mac-software-root-bootstrap COMMAND mac_client_bootstrap "${CSE_CLIENT_RUNTIME}" swds.dylib)
  set_tests_properties(mac-software-root-bootstrap PROPERTIES WORKING_DIRECTORY "${CSE_CLIENT_RUNTIME}")
endif()

add_test(NAME mac-client-cohort-admission COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cohort_admission.py")

add_test(NAME mac-client-cohort-alias-reuse COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cohort_alias_reuse.py")

set_target_properties(mac_client_bootstrap PROPERTIES ENABLE_EXPORTS ON)
if(TARGET mac_software_font)
  set_target_properties(mac_software_font PROPERTIES ENABLE_EXPORTS ON)
endif()
