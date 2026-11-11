get_filename_component(result_fixture_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(result_source "${CMAKE_CURRENT_BINARY_DIR}/actual_windows_result.cpp")
add_custom_command(OUTPUT "${result_source}"
  COMMAND "${Python3_EXECUTABLE}" "${result_fixture_dir}/prepare_windows_result.py" "${result_source}"
  DEPENDS "${SDK}/dedicated/sys_ded.cpp" "${result_fixture_dir}/prepare_windows_result.py"
  VERBATIM)
add_executable(windows_result "${result_source}")
target_include_directories(windows_result PRIVATE "${result_fixture_dir}" "${SDK}/dedicated")
add_test(NAME windows_main_result COMMAND windows_result)
