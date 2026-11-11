cmake_minimum_required(VERSION 3.25)
project(SoftwareLifetimeOwnerTests LANGUAGES NONE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
set(CSE_TEST_SDL "" CACHE FILEPATH "Exact source-built SDL for isolated process-owner test")
foreach(profile IN ITEMS debug release)
  set(profile_argument "")
  if(profile STREQUAL "release")
    set(profile_argument --release)
  endif()
  foreach(owner IN ITEMS gate bootstrap terminal_adapter remote_access steam_callbacks provider_close completion_owner vgui_completion)
    add_test(NAME software-lifetime-${owner}-${profile}
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_${owner}.py"
        "${CMAKE_CURRENT_BINARY_DIR}/${profile}/${owner}" ${profile_argument})
    set_tests_properties(software-lifetime-${owner}-${profile} PROPERTIES RUN_SERIAL TRUE)
  endforeach()
  if(CSE_TEST_SDL)
    add_test(NAME software-lifetime-process-owner-${profile}
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_controller_completion.py"
        "${CMAKE_CURRENT_BINARY_DIR}/${profile}/process" "${CSE_TEST_SDL}" ${profile_argument})
    set_tests_properties(software-lifetime-process-owner-${profile} PROPERTIES RUN_SERIAL TRUE)
  endif()
endforeach()
add_test(NAME software-lifetime-metadata
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/metadata_policy.py")
set_tests_properties(software-lifetime-metadata PROPERTIES RUN_SERIAL TRUE)
