cmake_minimum_required(VERSION 3.25)
get_filename_component(CSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseNativeRadar LANGUAGES CXX)
find_package(Threads REQUIRED)
option(CSE_RADAR_SANITIZERS "Instrument native radar tests with address and undefined sanitizers" OFF)
set(RADAR_ROOT "${CSE_ROOT}/src/cstrike/cl_dll/nextclient")
add_executable(radar_tests
    main.cpp worker_tests.cpp GraphicsGenerationTests.cpp RadarMapReadinessTests.cpp RadarMapLayerFixtures.cpp MapGeometryTests.cpp MapReachabilityTests.cpp
    RadarPolicyTests.cpp RadarLayerSelectionTests.cpp RadarMapLayersTests.cpp MapLayerOverridesTests.cpp
    RadarPolylineTests.cpp RadarUtilTests.cpp RadarViewTests.cpp
    "${RADAR_ROOT}/hud/radar/integration/LayerAnalysis.cpp"
    "${RADAR_ROOT}/hud/radar/integration/Performance.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapGeometry.cpp"
    "${RADAR_ROOT}/hud/radar/map/MapReachability.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/MapLayerOverrides.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/RadarMapLayers.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/RadarLayerSelection.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/RadarLayerContext.cpp"
    "${RADAR_ROOT}/hud/radar/map/layers/RadarLayerAnalysisJob.cpp"
    "${RADAR_ROOT}/hud/radar/policy/RadarPolicy.cpp"
    "${RADAR_ROOT}/hud/radar/render/RadarView.cpp")
target_include_directories(radar_tests PRIVATE "${RADAR_ROOT}" "${RADAR_ROOT}/include"
    "${CSE_ROOT}/src/common" "${CSE_ROOT}/src/engine" "${CSE_ROOT}/src/pm_shared"
    "${CSE_ROOT}/src/cstrike/dlls" "${CSE_ROOT}/src/dlls"
    "${CSE_ROOT}/src/cstrike/game_shared" "${CSE_ROOT}/src/game_shared"
    "${CSE_ROOT}/src/external" "${CSE_ROOT}/src/external/steamworks"
    "${CSE_ROOT}/src/public" "${CSE_ROOT}/src/cstrike/cl_dll")
target_compile_features(radar_tests PRIVATE cxx_std_20)
set_target_properties(radar_tests PROPERTIES CXX_EXTENSIONS NO)
target_link_libraries(radar_tests PRIVATE Threads::Threads)
if(MSVC)
    target_compile_definitions(radar_tests PRIVATE NOMINMAX)
    target_compile_options(radar_tests PRIVATE /W4 /fp:precise)
else()
    target_compile_options(radar_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers -ffp-contract=off)
    if(CSE_RADAR_SANITIZERS)
        target_compile_options(radar_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(radar_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()
enable_testing()
add_test(NAME radar-behavior COMMAND radar_tests)
set_tests_properties(radar-behavior PROPERTIES TIMEOUT 30)

option(CSE_RADAR_GL_TESTS "Build real SDL3/OpenGL radar rendering tests" OFF)
if(CSE_RADAR_GL_TESTS)
    add_subdirectory(render)
endif()

if(NOT MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME radar-scoreboard-retired-root COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scoreboard_lifecycle.py" "${CSE_ROOT}" "${CMAKE_CXX_COMPILER}")
endif()
