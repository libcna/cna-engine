# SPDX-License-Identifier: MIT

# GoogleTest comes from an existing checkout (Rule 4: no per-build clones).
# Sharp Runtime vendors it; any other checkout can be selected explicitly.
set(CNA_ENGINE_GOOGLETEST_DIR "${CNA_ENGINE_SHARP_RUNTIME_DIR}/vendor/googletest"
    CACHE PATH "GoogleTest source tree used by cna-engine tests")

if(NOT TARGET GTest::gtest_main)
    if(NOT EXISTS "${CNA_ENGINE_GOOGLETEST_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "cna-engine: no GoogleTest at '${CNA_ENGINE_GOOGLETEST_DIR}'. "
            "Set -DCNA_ENGINE_GOOGLETEST_DIR=... or -DCNA_ENGINE_BUILD_TESTS=OFF.")
    endif()
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CNA_ENGINE_GOOGLETEST_DIR}" "${CMAKE_BINARY_DIR}/_googletest" EXCLUDE_FROM_ALL)
endif()

include(GoogleTest)

# --- Strict unit tests: link only strict targets. -----------------------------
add_executable(cna_engine_tests
    Core/VersionTests.cpp
    Input/InputNamesTests.cpp
    Input/ActionMapTests.cpp
    Input/BindingCaptureTests.cpp
    Diagnostics/FrameStatisticsTests.cpp
    Diagnostics/ScopedTimerTests.cpp
    Graphics/CameraTests.cpp
    Graphics/CameraControllersTests.cpp
    Graphics/MeshDataTests.cpp
    Graphics/MeshBuilderTests.cpp
    Graphics/VertexLayoutsTests.cpp
    Graphics/GpuMeshTests.cpp
    Graphics/ScreenCaptureTests.cpp
    Graphics/RenderQueueTests.cpp
    Graphics/VisibilityTests.cpp
    Graphics/ForwardRendererTests.cpp
    Graphics/QualityTests.cpp
    Graphics/InstancingTests.cpp
    Assets/AssetRegistryTests.cpp
)
target_link_libraries(cna_engine_tests PRIVATE cna_engine_core cna_engine_graphics GTest::gtest_main)
gtest_discover_tests(cna_engine_tests
    TEST_PREFIX "cna_engine."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit;strict")

# --- Enhanced tests: the only executable linking cna-extended. -----------------
if(CNA_ENGINE_WITH_EXTENDED)
    add_executable(cna_engine_extended_tests
        Extended/EngineWorldScreenTests.cpp
        Extended/WorldCameraSyncTests.cpp
    )
    target_link_libraries(cna_engine_extended_tests PRIVATE cna_engine_extended GTest::gtest_main)
    gtest_discover_tests(cna_engine_extended_tests
        TEST_PREFIX "cna_engine_extended."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "unit;enhanced")
endif()

# --- Public dependency-boundary checks (ENG-011). ------------------------------
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME cna_engine.strict_boundary
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/check_strict_boundary.py"
                --root "${PROJECT_SOURCE_DIR}")
    add_test(NAME cna_engine.strict_boundary_detects_leaks
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/check_strict_boundary.py"
                --root "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/boundary/violations")
    set_tests_properties(cna_engine.strict_boundary_detects_leaks PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(cna_engine.strict_boundary cna_engine.strict_boundary_detects_leaks
        PROPERTIES LABELS "boundary")
else()
    message(WARNING "cna-engine: Python 3 not found; strict boundary tests are not registered")
endif()

# Every strict public header must compile on its own with only the strict
# targets linked: catches headers that rely on transitive includes or that
# would need cna-extended.
file(GLOB_RECURSE _cna_engine_strict_headers CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Core/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Input/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Diagnostics/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Graphics/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Assets/*.hpp")
set(_cna_engine_header_tus "")
foreach(_header IN LISTS _cna_engine_strict_headers)
    file(RELATIVE_PATH _rel "${PROJECT_SOURCE_DIR}/include" "${_header}")
    string(MAKE_C_IDENTIFIER "${_rel}" _id)
    set(_tu "${CMAKE_CURRENT_BINARY_DIR}/header_checks/${_id}.cpp")
    file(CONFIGURE OUTPUT "${_tu}" CONTENT "#include \"${_rel}\"\n" @ONLY)
    list(APPEND _cna_engine_header_tus "${_tu}")
endforeach()
add_library(cna_engine_strict_header_check OBJECT ${_cna_engine_header_tus})
target_link_libraries(cna_engine_strict_header_check PRIVATE cna_engine_core cna_engine_graphics)
