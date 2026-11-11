include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/MainImagePublication.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BinaryArchitecture.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/NativeRuntimeDependencies.cmake")

option(CSE_RELEASE_DIAGNOSTICS "Generate Release debug symbols and linker maps" OFF)
option(CSE_STAGE_DISTRIBUTION_MATERIALS "Stage dependency license/source materials for distribution" OFF)
# Independent test/dependency projects keep their outputs in their own build tree.
if(NOT DEFINED CSE_OUTPUT_DIRECTORY)
  set(CSE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/$<CONFIG>")
endif()

function(cse_add_runtime target kind output)
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
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/${target}"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>/${target}"
    COMPILE_PDB_NAME "${target}-compile")
  if(APPLE)
    # Mach-O otherwise coalesces weak inline registry functions across images,
    # binding an owned module's factory to the executable's private registry.
    set_property(TARGET ${target} PROPERTY VISIBILITY_INLINES_HIDDEN YES)
    if(target MATCHES "^(cs_client|cs_server|hl_client|hl_server)$")
      # Client prediction and server entities reuse C++ names with different
      # definitions. Mach-O must never coalesce their private templates/RTTI.
      # The SDK's explicit EXPORT declarations retain the public module surface.
      set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    endif()
  endif()
  if(MSVC)
    target_link_options(${target} PRIVATE
      "$<$<CONFIG:Debug>:/ILK:${CMAKE_CURRENT_BINARY_DIR}/${target}-$<CONFIG>.ilk>")
  endif()
endfunction()

function(cse_runtime_directory target relative)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CSE_OUTPUT_DIRECTORY}/${relative}$<0:>"
    LIBRARY_OUTPUT_DIRECTORY "${CSE_OUTPUT_DIRECTORY}/${relative}$<0:>")
  if(APPLE)
    # The staged dependency directory is shared by root and game-relative images.
    # Do not encode the owning build tree into relocatable runtime load commands.
    file(RELATIVE_PATH runtime_root "${CSE_OUTPUT_DIRECTORY}/${relative}" "${CSE_OUTPUT_DIRECTORY}")
    if(runtime_root)
      set(runtime_rpath "@loader_path/${runtime_root}")
    else()
      set(runtime_rpath "@loader_path")
    endif()
    set_target_properties(${target} PROPERTIES
      INSTALL_RPATH "${runtime_rpath}" BUILD_WITH_INSTALL_RPATH YES)
  endif()
  # Resource compiler file-backed inputs are resolved using the source directory.
  get_target_property(sources ${target} SOURCES)
  foreach(source IN LISTS sources)
    if(source MATCHES "\\.rc$")
      get_filename_component(resource_dir "${source}" DIRECTORY)
      set_property(SOURCE "${source}" APPEND PROPERTY INCLUDE_DIRECTORIES "${resource_dir}")
    endif()
  endforeach()
endfunction()

# Required runtime assets are restored even when the binary itself is already current.
# The retained manifest/translator supplies the explicit inventory, independently of optional distribution materials.
function(cse_runtime_assets target relative)
  set(destination "${CSE_OUTPUT_DIRECTORY}/${relative}")
  add_custom_target(${target}_runtime_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${ARGN} "${destination}"
    DEPENDS ${ARGN}
    VERBATIM COMMENT "Stage required ${target} runtime resources")
  add_dependencies(${target} ${target}_runtime_assets)
endfunction()

function(cse_native_module_alias target name)
  if(WIN32)
    return()
  endif()
  get_target_property(kind ${target} TYPE)
  if(NOT kind STREQUAL "SHARED_LIBRARY" OR NOT name MATCHES "^[A-Za-z_][A-Za-z0-9_-]*$")
    message(FATAL_ERROR "Invalid native module alias for ${target}: ${name}")
  endif()
  get_target_property(output_directory ${target} LIBRARY_OUTPUT_DIRECTORY)
  set(alias_path "${output_directory}/${name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DCSE_MODULE_ALIAS=${alias_path}"
      "-DCSE_MODULE_TARGET=$<TARGET_FILE_NAME:${target}>"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RuntimeModuleAlias.cmake"
    BYPRODUCTS "${alias_path}"
    VERBATIM COMMENT "Create metadata-compatible ${name} module alias")
endfunction()

function(cse_link_import target config path)
  if(NOT DEFINED CSE_TARGET_ARCH)
    message(FATAL_ERROR "${target}: CSE_TARGET_ARCH must identify the compiler target before importing libraries")
  endif()
  set(legacy_prefix "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/")
  string(FIND "${path}" "${legacy_prefix}" retained_dependency)
  if(NOT CSE_TARGET_ARCH STREQUAL x86 AND DEFINED CSE_SDK_DIR AND retained_dependency EQUAL 0)
    cse_link_native_runtime_import(${target} "${config}" "${path}")
    return()
  endif()
  cse_verify_binary("${path}" "${CSE_TARGET_ARCH}")
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

function(cse_target_dependency_include target configuration reference)
  if(CSE_TARGET_ARCH AND NOT CSE_TARGET_ARCH STREQUAL x86)
    cse_native_runtime_include("${reference}" resolved)
  else()
    set(resolved "${reference}")
  endif()
  target_include_directories(${target} PRIVATE "$<$<CONFIG:${configuration}>:${resolved}>")
endfunction()

function(cse_source_dependency_include source configuration reference)
  if(CSE_TARGET_ARCH AND NOT CSE_TARGET_ARCH STREQUAL x86)
    cse_native_runtime_include("${reference}" resolved)
  else()
    set(resolved "${reference}")
  endif()
  set_property(SOURCE "${source}" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:${configuration}>:${resolved}>")
endfunction()

function(cse_vgui_import_archive)
  # Must be called from vgui's directory: CMake's TARGET build events belong to
  # the directory that created the target.
  if(NOT WIN32 OR NOT CSE_TARGET_ARCH STREQUAL x86)
    # Native consumers use the compiler-generated import archive and decorated names.
    return()
  endif()
  add_custom_command(TARGET vgui POST_BUILD
    COMMAND "${CMAKE_AR}" /nologo /machine:x86
      "/def:${CSE_SDK_DIR}/utils/vgui/vgui_imports.def"
      "/out:$<TARGET_LINKER_FILE:vgui>"
    VERBATIM COMMENT "Generate VGUI1 compatibility import library")
endfunction()

function(cse_win32_module_definition target definition)
  if(WIN32 AND CSE_TARGET_ARCH STREQUAL x86)
    target_sources(${target} PRIVATE "${definition}")
  endif()
endfunction()

set(CSE_BROWSER_HOST_RELATIVE_PATH "" CACHE STRING "Native browser host path relative to SDL_GetBasePath")
function(cse_configure_native_browser target)
  set(host_path "${CSE_BROWSER_HOST_RELATIVE_PATH}")
  if(NOT host_path)
    if(WIN32)
      set(host_path "cse_cef_host.exe")
    else()
      message(FATAL_ERROR "Set CSE_BROWSER_HOST_RELATIVE_PATH for the staged native application layout")
    endif()
  endif()
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_definitions(${target} PRIVATE "CSE_BROWSER_HOST_RELATIVE_PATH=\"${host_path}\""
    "CSE_BROWSER_DEBUG_PROFILE=$<IF:$<CONFIG:Debug>,1,0>")
endfunction()

function(cse_game_module_exports target win32_definition native_definition)
  if(WIN32)
    if(CSE_TARGET_ARCH STREQUAL x86)
      target_sources(${target} PRIVATE "${win32_definition}")
    else()
      # C++ EXPORT declarations emit the selected compiler's native decorated names.
      target_sources(${target} PRIVATE "${native_definition}")
    endif()
  endif()
endfunction()

function(cse_a3d_geometry_source filename section)
  # Equivalent to the upstream PrepareGeometryCompilation target: each unique
  # translation unit selects a production section of A3dGeom.cpp. Per-file
  # settings are applied by the generated a3dapi target module.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/${filename}"
    CONTENT "#define ${section}\n#include \"${CSE_SDK_DIR}/a3dapi/A3dGeom.cpp\"\n" @ONLY)
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/SteamDependency.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MacFontService.cmake")

function(cse_link_native_voice_mixer target)
  if(NOT APPLE)
    message(FATAL_ERROR "Native CoreAudio mixer services are a macOS target policy")
  endif()
  target_link_libraries(${target} PRIVATE "-framework CoreAudio" "-framework CoreFoundation")
endfunction()

# macOS discovers native font faces through CoreText and rasterizes with the
# pinned source-built FreeType provider. Runtime widgets/layouts remain owned by VGUI.
function(cse_link_native_font_services target)
  if(NOT APPLE)
    message(FATAL_ERROR "Native CoreText font services are a macOS target policy")
  endif()
  include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NativeThirdParty.cmake")
  cse_load_native_third_party()
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/native")
  if(NOT TARGET cse_mac_font_registration)
    message(FATAL_ERROR "The manifest-owned font registration service must precede its consumers")
  endif()
  target_link_libraries(${target} PRIVATE cse_mac_font_registration)
  target_link_options(${target} PRIVATE "-Wl,-rpath,@loader_path")
  target_link_libraries(${target} PRIVATE Freetype::Freetype "-framework CoreText" "-framework CoreFoundation")
endfunction()
