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
        "${CNA_ENGINE_CAR_SIMULATOR_DIR}/simulator/src/Input/InputMapper.cpp")
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

    add_executable(cna_engine_pilot_tests CarSimulatorControlsTests.cpp CarSimulatorRenderTests.cpp CarSimulatorAssetsTests.cpp)
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
        set_tests_properties(cna_engine.pilot.car_simulator_checker_allowance PROPERTIES LABELS "pilot;boundary")
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
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/src/Render/GpuMesh.cpp")
    target_include_directories(cna_engine_pilot_street PUBLIC
        "${CNA_ENGINE_STREET_SIMULATOR_DIR}/street/include")
    target_link_libraries(cna_engine_pilot_street PUBLIC CNA::Math CNA::Input CNA::GraphicsCore)

    add_executable(cna_engine_pilot_street_tests StreetSimulatorTests.cpp)
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
