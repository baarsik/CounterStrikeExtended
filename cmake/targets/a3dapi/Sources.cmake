# Generated from projects/vs2026/a3dapi.vcxproj; see tools/translate_msbuild.py.
cse_a3d_geometry_source("geometry_objects.cpp" "A3D_GEOMETRY_OBJECTS")
cse_a3d_geometry_source("geometry_finalize.cpp" "A3D_GEOMETRY_FINALIZE")
cse_a3d_geometry_source("geometry_acoustics.cpp" "A3D_GEOMETRY_ACOUSTICS")
cse_a3d_geometry_source("geometry_submission.cpp" "A3D_GEOMETRY_SUBMISSION")
cse_a3d_geometry_source("geometry_state.cpp" "A3D_GEOMETRY_STATE")
target_sources(a3dapi PRIVATE
  "${CSE_SDK_DIR}/a3dapi/A3d3.cpp"
  "${CSE_SDK_DIR}/a3dapi/a3d_guids.cpp"
  "${CSE_SDK_DIR}/a3dapi/resman.cpp"
  "${CSE_SDK_DIR}/a3dapi/dal_a2d.cpp"
  "${CSE_SDK_DIR}/a3dapi/dalinfo.cpp"
  "${CSE_SDK_DIR}/a3dapi/A3dGeom.cpp"
  "${CSE_SDK_DIR}/a3dapi/A3dScene.cpp"
  "${CSE_SDK_DIR}/a3dapi/apimapper.cpp"
  "${CSE_SDK_DIR}/a3dapi/A3dSource.cpp"
  "${CSE_SDK_DIR}/a3dapi/matrix_inverse.cpp"
  "${CSE_SDK_DIR}/a3dapi/a2d.cpp"
  "${CSE_SDK_DIR}/a3dapi/source_reflections.cpp"
  "${CSE_SDK_DIR}/a3dapi/a2dbuffer.cpp"
  "${CSE_SDK_DIR}/a3dapi/source_geometry.cpp"
  "${CSE_SDK_DIR}/a3dapi/spatial_audio.cpp"
  "${CSE_SDK_DIR}/a3dapi/rotation.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/geometry_objects.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/geometry_finalize.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/geometry_acoustics.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/geometry_submission.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/geometry_state.cpp"
)
if(WIN32)
  target_sources(a3dapi PRIVATE
    "${CSE_SDK_DIR}/a3dapi/a3dapi.rc"
  )
endif()
