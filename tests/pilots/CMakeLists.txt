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

    add_executable(cna_engine_pilot_tests CarSimulatorControlsTests.cpp)
    target_link_libraries(cna_engine_pilot_tests PRIVATE
        cna_engine_core cna_engine_graphics cna_engine_pilot_carsim_input GTest::gtest_main)
    gtest_discover_tests(cna_engine_pilot_tests
        TEST_PREFIX "cna_engine.pilot."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "pilot")
    message(STATUS "cna-engine: car-simulator pilot probes enabled (${CNA_ENGINE_CAR_SIMULATOR_DIR})")
else()
    message(STATUS "cna-engine: car-simulator not found; pilot probes skipped")
endif()
