# The launcher does not link engine C++ code. It embeds hashes of the finished
# owning-build pair and checks them before either image can execute constructors.
set(native_cohort_header "${CMAKE_BINARY_DIR}/native-cohort/$<CONFIG>/cse_native_cohort_images.h")
set(native_cohort_engines hw_engine)
set(native_software_argument "")
if(TARGET swds)
  list(APPEND native_cohort_engines swds)
  set(native_software_argument "-DCSE_SOFTWARE_ENGINE=$<TARGET_FILE:swds>")
endif()
add_custom_command(OUTPUT "${native_cohort_header}"
  COMMAND "${CMAKE_COMMAND}" "-DCSE_ENGINE=$<TARGET_FILE:hw_engine>" "-DCSE_VGUI=$<TARGET_FILE:vgui>"
    ${native_software_argument} "-DCSE_HEADER=${native_cohort_header}" -P "${CSE_ROOT}/cmake/WriteNativeCohort.cmake"
  DEPENDS ${native_cohort_engines} vgui "${CSE_ROOT}/cmake/WriteNativeCohort.cmake"
  COMMENT "Bind native launcher to the finished engine/VGUI cohort" VERBATIM)
add_custom_target(cse_native_cohort DEPENDS "${native_cohort_header}")
add_dependencies(launcher cse_native_cohort)
target_sources(launcher PRIVATE "${native_cohort_header}")
target_include_directories(launcher PRIVATE "${CMAKE_BINARY_DIR}/native-cohort/$<CONFIG>")
target_compile_definitions(launcher PRIVATE CSE_NATIVE_RUNTIME_BOOTSTRAP=1)

if(TARGET hlds)
  add_dependencies(hlds cse_native_cohort filesystem_stdio vgui2)
  target_sources(hlds PRIVATE "${native_cohort_header}")
  target_include_directories(hlds PRIVATE "${CMAKE_BINARY_DIR}/native-cohort/$<CONFIG>")
  target_compile_definitions(hlds PRIVATE CSE_NATIVE_RUNTIME_BOOTSTRAP=1)
endif()

if(TARGET hlds AND TARGET swds)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set_target_properties(swds hlds PROPERTIES EXPORT_COMPILE_COMMANDS ON)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_custom_target(cse_dedicated_abi_evidence
    COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tools/dedicated_abi_evidence.py"
      --database "${CMAKE_BINARY_DIR}/compile_commands.json"
      --output "${CMAKE_BINARY_DIR}/dedicated-evidence/$<CONFIG>"
    COMMENT "Verify dedicated producer/host physical slots and include closure under owning policies"
    VERBATIM)
  add_dependencies(swds cse_dedicated_abi_evidence)
  add_dependencies(hlds cse_dedicated_abi_evidence)
endif()

if(TARGET hlds AND TARGET vgui2)
  set_target_properties(vgui2 PROPERTIES EXPORT_COMPILE_COMMANDS ON)
  add_custom_target(cse_standalone_vgui_abi_evidence
    COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tools/standalone_vgui_abi_evidence.py"
      --database "${CMAKE_BINARY_DIR}/compile_commands.json"
      --output "${CMAKE_BINARY_DIR}/standalone-vgui-evidence/$<CONFIG>"
    COMMENT "Verify complete standalone VGUI physical ABI in actual producer/control recipes"
    VERBATIM)
  add_dependencies(vgui2 cse_standalone_vgui_abi_evidence)
  add_dependencies(hlds cse_standalone_vgui_abi_evidence)
endif()

if(TARGET hlds AND TARGET vgui2)
  set(standalone_cohort_header "${CMAKE_BINARY_DIR}/native-cohort/$<CONFIG>/cse_standalone_cohort_images.h")
  add_custom_command(OUTPUT "${standalone_cohort_header}"
    COMMAND "${CMAKE_COMMAND}" "-DCSE_VGUI2=$<TARGET_FILE:vgui2>"
      "-DCSE_FILESYSTEM=$<TARGET_FILE:filesystem_stdio>" "-DCSE_HEADER=${standalone_cohort_header}"
      -P "${CSE_ROOT}/cmake/WriteStandaloneCohort.cmake"
    DEPENDS vgui2 filesystem_stdio "${CSE_ROOT}/cmake/WriteStandaloneCohort.cmake"
    COMMENT "Bind dedicated host to its finished standalone VGUI/filesystem cohort" VERBATIM)
  add_custom_target(cse_standalone_cohort DEPENDS "${standalone_cohort_header}")
  add_dependencies(hlds cse_standalone_cohort)
  target_sources(hlds PRIVATE "${standalone_cohort_header}")
endif()

if(TARGET hlds AND TARGET vgui2)
  add_custom_target(cse_dedicated_standalone_abi_evidence
    COMMAND "${Python3_EXECUTABLE}" "${CSE_ROOT}/tools/standalone_vgui_abi_evidence.py"
      --database "${CMAKE_BINARY_DIR}/compile_commands.json"
      --output "${CMAKE_BINARY_DIR}/dedicated-standalone-evidence/$<CONFIG>"
      --dedicated-consumers
    COMMENT "Verify standalone ABI in actual dedicated module and GUI consumers"
    VERBATIM)
  # Consumer sources include the real finished-image hash headers. Check them
  # after provider evidence/linking, without a circular provider dependency or fake hashes.
  add_dependencies(cse_dedicated_standalone_abi_evidence cse_native_cohort cse_standalone_cohort)
  add_dependencies(hlds cse_dedicated_standalone_abi_evidence)
endif()
