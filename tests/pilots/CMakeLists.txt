# SPDX-License-Identifier: MIT
#
# Pilot integration probes (PILOT-001/002). The pilots are not modified: their
# sources are compiled read-only next to engine code, and tests compare the
# engine facility with the pilot's own implementation on identical inputs.

set(CNA_ENGINE_CAR_SIMULATOR_DIR "${PROJECT_SOURCE_DIR}/../../libcnalab/car-simulator"
    CACHE PATH "car-simulator checkout used by the pilot probes (read-only)")

if(EXISTS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Input/InputMapper.cpp")
    # car-simulator's input mapper only, with its own strict-XNA includes.
    add_library(cna_engine_pilot_carsim_input STATIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Input/InputMapper.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/QualityTier.cpp")
    target_include_directories(cna_engine_pilot_carsim_input PUBLIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/include")
    target_link_libraries(cna_engine_pilot_carsim_input PUBLIC CNA::Math CNA::Input)

    # Camera poses and procedural meshes / GPU upload. Camera.cpp uses only
    # VehicleState data, so no simulation sources are needed.
    add_library(cna_engine_pilot_carsim_render STATIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/Camera.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/MeshData.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/GpuMesh.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/Screenshot.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/BitmapFont.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/Image.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/ImageText.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/PlateRenderer.cpp")
    target_include_directories(cna_engine_pilot_carsim_render PUBLIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/include")
    target_link_libraries(cna_engine_pilot_carsim_render PUBLIC CNA::Math CNA::GraphicsCore CNA::Content SharpRuntime::Text.Json)

    add_executable(cna_engine_pilot_tests CarSimulatorControlsTests.cpp CarSimulatorRenderTests.cpp CarSimulatorAssetsTests.cpp
        CarSimulatorQualityTests.cpp)
    target_link_libraries(cna_engine_pilot_tests PRIVATE
        cna_engine_core cna_engine_graphics cna_engine_pilot_carsim_input cna_engine_pilot_carsim_render
        GTest::gtest_main)
    gtest_discover_tests(cna_engine_pilot_tests
        TEST_PREFIX "cna_engine.pilot."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "pilot")
    # ADR-001 contract: the pilot's own checker accepts the strict engine
    # surface given only the documented include allowance.
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND AND EXISTS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/scripts/check_xna_only.py")
        add_test(NAME cna_engine.pilot.car_simulator_checker_allowance
            COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/run_pilot_checker.py"
                    "${CNA_ENGINE_CAR_SIMULATOR_DIR}" "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(cna_engine.pilot.car_simulator_checker_allowance PROPERTIES LABELS pilot)
    endif()
    message(STATUS "cna-engine: car-simulator pilot probes enabled (${CNA_ENGINE_CAR_SIMULATOR_DIR})")
else()
    message(STATUS "cna-engine: car-simulator not found; pilot probes skipped")
endif()

set(CNA_ENGINE_STREET_SIMULATOR_DIR "${PROJECT_SOURCE_DIR}/../../libcnalab/street-simulator"
    CACHE PATH "street-simulator checkout used by the pilot probes (read-only)")

if(EXISTS "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/Camera.cpp")
    # Street's camera, fly/walk controller and tangent-frame geometry/upload.
    add_library(cna_engine_pilot_street STATIC
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/Camera.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/CameraController.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Geometry/MeshBuilder.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/GpuMesh.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/InstancedMesh.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/RenderSettings.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/MaterialLibrary.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Assets/SignFactory.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Assets/TextureFactory.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Assets/Canvas.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Assets/Image.cpp"
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Core/Rng.cpp")
    target_include_directories(cna_engine_pilot_street PUBLIC
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/include")
    target_link_libraries(cna_engine_pilot_street PUBLIC CNA::Math CNA::Input CNA::GraphicsCore CNA::Core CNA::Content SharpRuntime::Text.Json)

    add_executable(cna_engine_pilot_street_tests StreetSimulatorTests.cpp StreetQualityTests.cpp StreetAssetsTests.cpp)
    target_link_libraries(cna_engine_pilot_street_tests PRIVATE
        cna_engine_core cna_engine_graphics cna_engine_pilot_street GTest::gtest_main)
    gtest_discover_tests(cna_engine_pilot_street_tests
        TEST_PREFIX "cna_engine.pilot."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "pilot")
    message(STATUS "cna-engine: street-simulator pilot probes enabled (${CNA_ENGINE_STREET_SIMULATOR_DIR})")
else()
    message(STATUS "cna-engine: street-simulator not found; Street pilot probes skipped")
endif()

# --- MILESTONE-2 scene comparison: all of Street's core, read-only. -----------
option(CNA_ENGINE_STREET_SCENE_COMPARISON
    "Build the Street scene comparison tool (compiles street-simulator's whole core read-only)" ON)
# Street's sky uses CNA's CNAEXT shader packages, so the comparison needs a
# build with -DCNA_ENGINE_CNA_CNAEXT=ON (build-probe/ is used for that).
if(CNA_ENGINE_STREET_SCENE_COMPARISON AND CNA_CNAEXT
   AND EXISTS "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/SceneRenderer.cpp")
    file(GLOB_RECURSE _street_core_sources CONFIGURE_DEPENDS "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/*.cpp")
    list(FILTER _street_core_sources EXCLUDE REGEX "/src/Main\\.cpp$")
    add_library(cna_engine_pilot_street_core STATIC ${_street_core_sources})
    target_include_directories(cna_engine_pilot_street_core PUBLIC "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/include")
    target_compile_definitions(cna_engine_pilot_street_core PUBLIC
        CNA_STREET_VERSION="probe" CNA_STREET_RENDERER_NAME="${CNA_GRAPHICS_RENDERER}"
        CNA_STREET_DEFAULT_ASSET_DIR="${CNA_ENGINE_STREET_SIMULATOR_DIR}/assets")
    # Street itself links the `CNA` umbrella, which is libcna.so when
    # CNA_SHARED_LIBRARY is ON (CNA's default), while the engine links CNA's
    # static modules: both in one process means two CNA runtimes
    # (upstream-issues.md UP-006). Link the same static modules instead.
    target_link_libraries(cna_engine_pilot_street_core PUBLIC
        CNA::Runtime CNA::GraphicsExt CNA::DevicesExt CNA::Storage CNA::Content CNA::Media CNA::Audio CNA::Input
        CNA::GraphicsCore CNA::Diagnostics CNA::Core CNA::Math CNA::Platform SharpRuntime::Text.Json)

    add_executable(cna_engine_street_scene_comparison StreetSceneComparison.cpp)
    target_link_libraries(cna_engine_street_scene_comparison PRIVATE
        cna_engine_core cna_engine_graphics cna_engine_enhanced cna_engine_pilot_street_core)
    add_test(NAME cna_engine.pilot.street_scene_comparison
        COMMAND cna_engine_street_scene_comparison "${CMAKE_CURRENT_BINARY_DIR}/street_scene_comparison")
    # Building Street's procedural city in a Debug software-rendered build is slow.
    set_tests_properties(cna_engine.pilot.street_scene_comparison PROPERTIES LABELS pilot TIMEOUT 3600)
endif()
