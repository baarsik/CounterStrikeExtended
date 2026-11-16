enable_language(C)
set(CSE_SDL3_LIBRARY "" CACHE FILEPATH "Existing SDL3 runtime library for real radar OpenGL tests")
if(NOT EXISTS "${CSE_SDL3_LIBRARY}")
    message(FATAL_ERROR "CSE_RADAR_GL_TESTS requires CSE_SDL3_LIBRARY pointing to the existing SDL3 runtime library")
endif()
set(shader_names radar.vert radar_backdrop.frag radar_mask.frag radar_prim.frag radar_prim.vert)
set(shader_index 0)
foreach(shader IN LISTS shader_names)
    file(READ "${RADAR_ROOT}/shaders/${shader}" CSE_RADAR_SHADER_${shader_index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RADAR_ROOT}/shaders/${shader}")
    math(EXPR shader_index "${shader_index} + 1")
endforeach()
configure_file("${RADAR_ROOT}/RadarShaders.h.in" "${CMAKE_CURRENT_BINARY_DIR}/RadarShaders.h" @ONLY)
add_executable(radar_gl_tests
    ../main.cpp ../RadarMapLayerFixtures.cpp renderer_tests.cpp label_tests.cpp
    "${RADAR_ROOT}/hud/radar/map/MapLayerRenderer.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapRender.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapRenderState.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapImageTone.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapGeometry.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapReachability.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/RadarMapLayers.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/MapLayerOverrides.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/MapLayerCache.cpp"
    "${RADAR_ROOT}/hud/radar/render/RadarDraw.cpp"
    "${RADAR_ROOT}/hud/radar/render/RadarDrawEffects.cpp"
    "${RADAR_ROOT}/hud/radar/render/text/HudTextBuffer.cpp"
    "${RADAR_ROOT}/hud/radar/render/text/HudTextZone.cpp"
    "${RADAR_ROOT}/hud/radar/render/text/RadarLabel.cpp"
    "${RADAR_ROOT}/hud/radar/integration/Performance.cpp"
    "${RADAR_ROOT}/ncl_gl/program.cpp"
    "${RADAR_ROOT}/ncl_gl/context.cpp"
    "${RADAR_ROOT}/ncl_gl/loader.cpp"
    "${RADAR_ROOT}/ncl_gl/glad.c")
get_target_property(radar_include_dirs radar_tests INCLUDE_DIRECTORIES)
target_include_directories(radar_gl_tests PRIVATE ${radar_include_dirs}
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}" "${CSE_ROOT}/src/external/SDL3/include")
target_compile_features(radar_gl_tests PRIVATE cxx_std_20)
set_target_properties(radar_gl_tests PROPERTIES CXX_EXTENSIONS NO)
target_link_libraries(radar_gl_tests PRIVATE "${CSE_SDL3_LIBRARY}")
if(MSVC)
    target_compile_definitions(radar_gl_tests PRIVATE NOMINMAX)
    target_compile_options(radar_gl_tests PRIVATE /W4 /fp:precise)
else()
    target_compile_options(radar_gl_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers -ffp-contract=off)
endif()
add_test(NAME radar-opengl-behavior COMMAND radar_gl_tests)
set_tests_properties(radar-opengl-behavior PROPERTIES TIMEOUT 45 LABELS gl)
