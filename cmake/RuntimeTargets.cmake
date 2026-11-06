include_guard(GLOBAL)

function(cse_add_runtime target kind output)
  foreach(config Debug Release)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${config}/${target}"
      "${CMAKE_BINARY_DIR}/symbols/${config}/${target}")
  endforeach()
  if(kind STREQUAL "EXECUTABLE")
    add_executable(${target} WIN32)
    # All retained executables export CreateInterface; model their import products.
    set_property(TARGET ${target} PROPERTY ENABLE_EXPORTS ON)
  else()
    add_library(${target} SHARED)
  endif()
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${output}" PREFIX ""
    CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
    MSVC_RUNTIME_LIBRARY ""
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>/${target}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/${target}"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/${target}"
    COMPILE_PDB_NAME "${target}-compile")
endfunction()

function(cse_runtime_directory target relative)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>/${relative}")
  # Resource compiler file-backed inputs are resolved using the source directory.
  get_target_property(sources ${target} SOURCES)
  foreach(source IN LISTS sources)
    if(source MATCHES "\\.rc$")
      get_filename_component(resource_dir "${source}" DIRECTORY)
      set_property(SOURCE "${source}" APPEND PROPERTY INCLUDE_DIRECTORIES "${resource_dir}")
    endif()
  endforeach()
endfunction()

function(cse_link_import target config path)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "${target}: missing ${config} x86 library: ${path}")
  endif()
  if(path MATCHES "[/\\\\](x64|arm64|x64-windows)[/\\\\]")
    message(FATAL_ERROR "${target}: non-x86 library selected: ${path}")
  endif()
  string(SHA256 key "${path}")
  set(dependency "cse_import_${key}")
  if(NOT TARGET ${dependency})
    # UNKNOWN supports both static archives and import libraries. This is an
    # explicit link input, never a replacement runtime implementation target.
    add_library(${dependency} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(${dependency} PROPERTIES IMPORTED_LOCATION "${path}")
  endif()
  target_link_libraries(${target} PRIVATE "$<$<CONFIG:${config}>:${dependency}>")
endfunction()

function(cse_vgui_import_archive)
  # Must be called from vgui's directory: CMake's TARGET build events belong to
  # the directory that created the target.
  add_custom_command(TARGET vgui POST_BUILD
    COMMAND "${CMAKE_AR}" /nologo /machine:x86
      "/def:${CSE_SDK_DIR}/utils/vgui/vgui_imports.def"
      "/out:$<TARGET_LINKER_FILE:vgui>"
    VERBATIM COMMENT "Generate VGUI1 compatibility import library")
endfunction()

function(cse_a3d_geometry_source filename section)
  # Equivalent to the upstream PrepareGeometryCompilation target: each unique
  # translation unit selects a production section of A3dGeom.cpp. Per-file
  # settings are applied by the generated a3dapi target module.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/${filename}"
    CONTENT "#define ${section}\n#include \"${CSE_SDK_DIR}/a3dapi/A3dGeom.cpp\"\n" @ONLY)
endfunction()
