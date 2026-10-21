# SPDX-License-Identifier: MIT
#
# Pilot integration probes (PILOT-001/002). The pilots are not modified: their
# sources are compiled read-only next to engine code, and tests compare the
# engine facility with the pilot's own implementation on identical inputs.

# The pilots' own test suites, compiled read-only against this build's CNA:
# ADR-002's consumer checks for a pinned baseline. Slow (car-simulator's suite
# alone runs minutes in Debug), so off by default; build-probe/ turns it on.
option(CNA_ENGINE_PILOT_SUITES
    "Build and run car-simulator's and street-simulator's own test suites against this build's dependencies" OFF)

set(CNA_ENGINE_CAR_SIMULATOR_DIR "${PROJECT_SOURCE_DIR}/../../libcnalab/car-simulator"
    CACHE PATH "car-simulator checkout used by the pilot probes (read-only)")

if(EXISTS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Input/InputMapper.cpp")
    # car-simulator's input mapper only, with its own strict-XNA includes.
    add_library(cna_engine_pilot_carsim_input STATIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Input/InputMapper.cpp"
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/QualityTier.cpp")
    target_include_directories(cna_engine_pilot_carsim_input PUBLIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/include")
    # car-simulator runs on the engine since 2026-10-20: its sources include engine headers.
    target_link_libraries(cna_engine_pilot_carsim_input PUBLIC CNA::Math CNA::Input cna_engine_core)

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
    target_link_libraries(cna_engine_pilot_carsim_render PUBLIC CNA::Math CNA::GraphicsCore CNA::Content SharpRuntime::Text.Json
        cna_engine_graphics)

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

# --- Strict-profile scene comparison: all of car-simulator's libraries. --------
option(CNA_ENGINE_CARSIM_SCENE_COMPARISON
    "Build the car-simulator scene comparison tool (compiles car-simulator's libraries read-only)" ON)
if(CNA_ENGINE_CARSIM_SCENE_COMPARISON AND EXISTS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/WorldRenderer.cpp")
    file(GLOB_RECURSE _carsim_sources CONFIGURE_DEPENDS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/*.cpp")
    list(FILTER _carsim_sources EXCLUDE REGEX "/src/App/")
    add_library(cna_engine_pilot_carsim_full STATIC ${_carsim_sources})
    target_include_directories(cna_engine_pilot_carsim_full PUBLIC "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/include")
    # CNA's static modules, never the `CNA` umbrella next to them (UP-006).
    target_link_libraries(cna_engine_pilot_carsim_full PUBLIC
        CNA::Runtime CNA::GraphicsCore CNA::Content CNA::Media CNA::Audio CNA::Input CNA::Storage CNA::Diagnostics
        CNA::Core CNA::Math CNA::Platform SharpRuntime::Text.Json cna_engine_core cna_engine_graphics)

    add_executable(cna_engine_carsim_scene_comparison CarSimulatorSceneComparison.cpp)
    target_compile_definitions(cna_engine_carsim_scene_comparison PRIVATE
        CNA_ENGINE_CARSIM_CONTENT_DIR="${CNA_ENGINE_CAR_SIMULATOR_DIR}/content")
    target_link_libraries(cna_engine_carsim_scene_comparison PRIVATE
        cna_engine_core cna_engine_graphics cna_engine_pilot_carsim_full)
    add_test(NAME cna_engine.pilot.carsim_scene_comparison
        COMMAND cna_engine_carsim_scene_comparison "${CMAKE_CURRENT_BINARY_DIR}/carsim_scene_comparison")
    set_tests_properties(cna_engine.pilot.carsim_scene_comparison PROPERTIES LABELS pilot TIMEOUT 3600)

    if(CNA_ENGINE_PILOT_SUITES)
        file(GLOB_RECURSE _carsim_test_sources CONFIGURE_DEPENDS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/tests/*.cpp")
        add_executable(cna_engine_pilot_carsim_suite ${_carsim_test_sources})
        target_link_libraries(cna_engine_pilot_carsim_suite PRIVATE cna_engine_pilot_carsim_full GTest::gtest_main)
        target_compile_definitions(cna_engine_pilot_carsim_suite PRIVATE
            CARSIM_TEST_CONTENT_DIR="${CNA_ENGINE_CAR_SIMULATOR_DIR}/content")
        # SampleMap.LipovaLoadsAndIsFullyConnected asserts car-simulator's
        # production loading budget (6 s), which an unoptimized GCC/Clang build
        # cannot meet (upstream-issues.md UP-009); excluded by name there.
        set(_carsim_filter "")
        if(NOT CMAKE_BUILD_TYPE MATCHES "Rel")
            set(_carsim_filter "--gtest_filter=-SampleMap.LipovaLoadsAndIsFullyConnected")
        endif()
        add_test(NAME cna_engine.pilot.carsim_suite COMMAND cna_engine_pilot_carsim_suite ${_carsim_filter})
        set_tests_properties(cna_engine.pilot.carsim_suite PROPERTIES LABELS pilot TIMEOUT 3600)
    endif()

    # The probe of car-simulator's own vehicle step loop against
    # FixedStepClock was retired on 2026-10-20: car-simulator steps its
    # vehicle with FixedStepClock itself since then (docs/pilots/car-simulator.md).
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
    # street-simulator runs on the engine since 2026-10-20: its sources include engine headers.
    target_link_libraries(cna_engine_pilot_street PUBLIC CNA::Math CNA::Input CNA::GraphicsCore CNA::Core CNA::Content SharpRuntime::Text.Json
        cna_engine_graphics)

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
        CNA::GraphicsCore CNA::Diagnostics CNA::Core CNA::Math CNA::Platform SharpRuntime::Text.Json
        cna_engine_core cna_engine_graphics cna_engine_enhanced)

    if(CNA_ENGINE_PILOT_SUITES)
        # Street's tests are plain executables, one per file.
        file(GLOB _street_test_sources CONFIGURE_DEPENDS "${CNA_ENGINE_STREET_SIMULATOR_DIR}/tests/*Tests.cpp")
        foreach(_source IN LISTS _street_test_sources)
            get_filename_component(_name "${_source}" NAME_WE)
            add_executable(cna_engine_pilot_street_${_name} "${_source}")
            target_link_libraries(cna_engine_pilot_street_${_name} PRIVATE cna_engine_pilot_street_core)
            add_test(NAME cna_engine.pilot.street_suite.${_name} COMMAND cna_engine_pilot_street_${_name})
            set_tests_properties(cna_engine.pilot.street_suite.${_name} PROPERTIES LABELS pilot TIMEOUT 1800)
        endforeach()
    endif()

    add_executable(cna_engine_street_scene_comparison StreetSceneComparison.cpp)
    target_link_libraries(cna_engine_street_scene_comparison PRIVATE
        cna_engine_core cna_engine_graphics cna_engine_enhanced cna_engine_pilot_street_core)
    add_test(NAME cna_engine.pilot.street_scene_comparison
        COMMAND cna_engine_street_scene_comparison "${CMAKE_CURRENT_BINARY_DIR}/street_scene_comparison")
    # Building Street's procedural city in a Debug software-rendered build is slow.
    set_tests_properties(cna_engine.pilot.street_scene_comparison PROPERTIES LABELS pilot TIMEOUT 3600)
endif()

# easy-3d (galaxy-eggbert's former helper library): what the engine took over
# must keep producing exactly the retired implementation's vertices, indices
# and matrices. The minimal MIT-licensed oracle is frozen in this repository,
# so enabling pilot probes always creates these tests; no sibling checkout can
# disappear and silently skip them.
add_library(cna_engine_pilot_easy3d_legacy STATIC
    "${PROJECT_SOURCE_DIR}/tests/fixtures/easy3d_legacy/Easy3DLegacy.cpp")
target_include_directories(cna_engine_pilot_easy3d_legacy PUBLIC
    "${PROJECT_SOURCE_DIR}/tests/fixtures/easy3d_legacy")
target_link_libraries(cna_engine_pilot_easy3d_legacy PUBLIC CNA::Math)

add_executable(cna_engine_easy3d_equivalence_tests Easy3DEquivalenceTests.cpp)
target_link_libraries(cna_engine_easy3d_equivalence_tests PRIVATE
    cna_engine_graphics cna_engine_pilot_easy3d_legacy GTest::gtest_main)
gtest_discover_tests(cna_engine_easy3d_equivalence_tests
    TEST_PREFIX "cna_engine.pilot."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "pilot")

# The implementations car-, school-, train- and street-simulator replaced with
# the SHARE-001 facilities, frozen like easy-3d above: those games now call the
# engine, so compiling their current sources would compare the engine with
# itself.
add_library(cna_engine_pilot_simulator_oracles STATIC
    "${PROJECT_SOURCE_DIR}/tests/fixtures/simulator_oracles/MipOracles.cpp"
    "${PROJECT_SOURCE_DIR}/tests/fixtures/simulator_oracles/TrainSettings.cpp")
target_include_directories(cna_engine_pilot_simulator_oracles PUBLIC
    "${PROJECT_SOURCE_DIR}/tests/fixtures/simulator_oracles")
target_link_libraries(cna_engine_pilot_simulator_oracles PUBLIC CNA::Math CNA::GraphicsCore)

# Procedural images and noise (Graphics/Image.hpp, Core/Noise.hpp): the mip
# filters, alpha-test coverage, uploads and noise must match the games they
# were taken from -- the frozen oracles, and room-simulator compiled read-only
# from its checkout (left out when it is missing).
set(CNA_ENGINE_ROOM_SIMULATOR_DIR "${PROJECT_SOURCE_DIR}/../../libcnalab/room-simulator"
    CACHE PATH "room-simulator checkout used by the image probe (read-only)")

add_executable(cna_engine_image_equivalence_tests ImageEquivalenceTests.cpp)
target_link_libraries(cna_engine_image_equivalence_tests PRIVATE
    cna_engine_core cna_engine_graphics cna_engine_pilot_simulator_oracles GTest::gtest_main)
if(EXISTS "${CNA_ENGINE_ROOM_SIMULATOR_DIR}/src/Assets/Image.cpp")
    add_library(cna_engine_pilot_room_images STATIC "${CNA_ENGINE_ROOM_SIMULATOR_DIR}/src/Assets/Image.cpp")
    target_include_directories(cna_engine_pilot_room_images PUBLIC "${CNA_ENGINE_ROOM_SIMULATOR_DIR}/include")
    target_link_libraries(cna_engine_image_equivalence_tests PRIVATE cna_engine_pilot_room_images)
    target_compile_definitions(cna_engine_image_equivalence_tests PRIVATE CNA_ENGINE_PROBE_ROOM_SIMULATOR=1)
endif()
gtest_discover_tests(cna_engine_image_equivalence_tests
    TEST_PREFIX "cna_engine.pilot."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "pilot")

# Solar geometry (Core/SunPosition.hpp) against car-, room- and house-simulator,
# compiled read-only; within a tolerance (single precision, an ephemeris).
set(CNA_ENGINE_HOUSE_SIMULATOR_DIR "${PROJECT_SOURCE_DIR}/../../libcnalab/house-simulator"
    CACHE PATH "house-simulator checkout used by the sun probe (read-only)")
add_executable(cna_engine_sun_equivalence_tests SunEquivalenceTests.cpp)
target_link_libraries(cna_engine_sun_equivalence_tests PRIVATE cna_engine_core GTest::gtest_main)
if(EXISTS "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/LightingRig.cpp")
    add_library(cna_engine_pilot_carsim_lighting STATIC
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Render/LightingRig.cpp")
    target_include_directories(cna_engine_pilot_carsim_lighting PUBLIC "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/include")
    target_link_libraries(cna_engine_pilot_carsim_lighting PUBLIC CNA::Math CNA::GraphicsCore)
    target_link_libraries(cna_engine_sun_equivalence_tests PRIVATE cna_engine_pilot_carsim_lighting)
    target_compile_definitions(cna_engine_sun_equivalence_tests PRIVATE CNA_ENGINE_PROBE_CAR_SIMULATOR=1)
endif()
if(EXISTS "${CNA_ENGINE_ROOM_SIMULATOR_DIR}/src/Sim/TimeOfDay.cpp")
    add_library(cna_engine_pilot_room_sun STATIC "${CNA_ENGINE_ROOM_SIMULATOR_DIR}/src/Sim/TimeOfDay.cpp")
    target_include_directories(cna_engine_pilot_room_sun PUBLIC "${CNA_ENGINE_ROOM_SIMULATOR_DIR}/include")
    target_link_libraries(cna_engine_pilot_room_sun PUBLIC CNA::Math)
    target_link_libraries(cna_engine_sun_equivalence_tests PRIVATE cna_engine_pilot_room_sun)
    target_compile_definitions(cna_engine_sun_equivalence_tests PRIVATE CNA_ENGINE_PROBE_ROOM_SIMULATOR=1)
endif()
if(EXISTS "${CNA_ENGINE_HOUSE_SIMULATOR_DIR}/src/environment/SunModel.cpp")
    add_library(cna_engine_pilot_house_sun STATIC
        "${CNA_ENGINE_HOUSE_SIMULATOR_DIR}/src/environment/SunModel.cpp"
        "${CNA_ENGINE_HOUSE_SIMULATOR_DIR}/src/environment/SimClock.cpp"
        "${CNA_ENGINE_HOUSE_SIMULATOR_DIR}/src/environment/Season.cpp"
        "${CNA_ENGINE_HOUSE_SIMULATOR_DIR}/src/environment/Temperature.cpp")
    target_include_directories(cna_engine_pilot_house_sun PUBLIC "${CNA_ENGINE_HOUSE_SIMULATOR_DIR}/include")
    target_link_libraries(cna_engine_sun_equivalence_tests PRIVATE cna_engine_pilot_house_sun)
    target_compile_definitions(cna_engine_sun_equivalence_tests PRIVATE CNA_ENGINE_PROBE_HOUSE_SIMULATOR=1)
endif()
gtest_discover_tests(cna_engine_sun_equivalence_tests
    TEST_PREFIX "cna_engine.pilot."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "pilot")

# Settings files (Core/SettingsFile.hpp) against train-simulator's frozen
# Config::Settings: same values read, same bytes written.
add_executable(cna_engine_settings_equivalence_tests SettingsEquivalenceTests.cpp)
target_link_libraries(cna_engine_settings_equivalence_tests PRIVATE
    cna_engine_core cna_engine_pilot_simulator_oracles GTest::gtest_main)
gtest_discover_tests(cna_engine_settings_equivalence_tests
    TEST_PREFIX "cna_engine.pilot."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "pilot")
