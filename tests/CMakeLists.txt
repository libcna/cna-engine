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
    Core/FixedStepClockTests.cpp
    Core/NoiseTests.cpp
    Core/SunPositionTests.cpp
    Core/SettingsFileTests.cpp
    Collision/StaticWorldTests.cpp
    Collision/CharacterControllerTests.cpp
    Animation/AnimatorTests.cpp
    Input/InputNamesTests.cpp
    Input/ActionMapTests.cpp
    Input/BindingCaptureTests.cpp
    Diagnostics/FrameStatisticsTests.cpp
    Diagnostics/ScopedTimerTests.cpp
    Graphics/CameraTests.cpp
    Graphics/CameraControllersTests.cpp
    Graphics/MeshDataTests.cpp
    Graphics/MeshBuilderTests.cpp
    Graphics/TextureAtlasTests.cpp
    Graphics/ImageTests.cpp
    Graphics/DebugDrawTests.cpp
    Graphics/VertexLayoutsTests.cpp
    Graphics/GpuMeshTests.cpp
    Graphics/ScreenCaptureTests.cpp
    Graphics/AutomatedRunTests.cpp
    Graphics/RenderQueueTests.cpp
    Graphics/VisibilityTests.cpp
    Graphics/ForwardRendererTests.cpp
    Graphics/QualityTests.cpp
    Graphics/QualitySchemaTests.cpp
    Graphics/InstancingTests.cpp
    Graphics/DiagnosticsReportTests.cpp
    Graphics/GpuMemoryTests.cpp
    Graphics/SkyTests.cpp
    Graphics/ShadowMapTests.cpp
    Assets/AssetRegistryTests.cpp
)
target_link_libraries(cna_engine_tests PRIVATE cna_engine_core cna_engine_graphics GTest::gtest_main)
cna_engine_sanitize(cna_engine_tests)
gtest_discover_tests(cna_engine_tests
    TEST_PREFIX "cna_engine."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS unit)

# --- The game loop (GAME-003): strict, runs XNA Games on the headless platform. -
add_executable(cna_engine_app_tests App/AssetDirectoryTests.cpp App/EngineGameTests.cpp App/PostProcessTests.cpp)
target_link_libraries(cna_engine_app_tests PRIVATE cna_engine_app GTest::gtest_main)
cna_engine_sanitize(cna_engine_app_tests)
gtest_discover_tests(cna_engine_app_tests
    TEST_PREFIX "cna_engine.app."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS unit)

# --- Sound (GAME-040): strict, on CNA's null audio platform. ------------------
add_executable(cna_engine_audio_tests Audio/SoundSystemTests.cpp)
target_link_libraries(cna_engine_audio_tests PRIVATE cna_engine_audio GTest::gtest_main)
cna_engine_sanitize(cna_engine_audio_tests)
gtest_discover_tests(cna_engine_audio_tests
    TEST_PREFIX "cna_engine.audio."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS unit)

# --- Text, HUD and menus (GAME-050). ------------------------------------------
add_executable(cna_engine_ui_tests UI/FontAndPainterTests.cpp UI/MenuTests.cpp)
target_link_libraries(cna_engine_ui_tests PRIVATE cna_engine_ui GTest::gtest_main)
cna_engine_sanitize(cna_engine_ui_tests)
gtest_discover_tests(cna_engine_ui_tests
    TEST_PREFIX "cna_engine.ui."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS unit)

# --- Performance scaling checks (REL-003): their own label, no budgets. -------
add_executable(cna_engine_performance_tests Performance/ScalingTests.cpp)
target_link_libraries(cna_engine_performance_tests PRIVATE cna_engine_core cna_engine_graphics GTest::gtest_main)
cna_engine_sanitize(cna_engine_performance_tests)
gtest_discover_tests(cna_engine_performance_tests
    TEST_PREFIX "cna_engine.performance."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS performance)

# --- Enhanced tests: the only executable linking cna-extended. -----------------
if(CNA_ENGINE_WITH_EXTENDED)
    add_executable(cna_engine_extended_tests
        Extended/DebugOverlayTests.cpp
        Extended/EngineWorldScreenTests.cpp
        Extended/WorldCameraSyncTests.cpp
    )
    target_link_libraries(cna_engine_extended_tests PRIVATE cna_engine_extended GTest::gtest_main)
    cna_engine_sanitize(cna_engine_extended_tests)
    gtest_discover_tests(cna_engine_extended_tests
        TEST_PREFIX "cna_engine_extended."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS unit)
endif()

# --- Enhanced graphics tests (CNA extensions, no cna-extended). ----------------
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    add_executable(cna_engine_enhanced_tests
        Enhanced/PbrMaterialBinderTests.cpp
        Enhanced/ModelImportTests.cpp
        Enhanced/ShadowReceptionTests.cpp
    )
    target_link_libraries(cna_engine_enhanced_tests PRIVATE cna_engine_enhanced GTest::gtest_main)
    target_compile_definitions(cna_engine_enhanced_tests PRIVATE
        CNA_ENGINE_TEST_MODELS="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/models")
    cna_engine_sanitize(cna_engine_enhanced_tests)
    gtest_discover_tests(cna_engine_enhanced_tests
        TEST_PREFIX "cna_engine_enhanced."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS unit)
endif()

# --- Pilot probes: compare engine facilities with pilot implementations. ------
option(CNA_ENGINE_PILOT_PROBES "Build probes that compile pilot-project sources read-only" ON)
if(CNA_ENGINE_PILOT_PROBES)
    add_subdirectory(pilots)
endif()

# --- Baseline range report (ADR-002, REL-001). ---------------------------------
foreach(_mode descendant diverged)
    add_test(NAME cna_engine.baseline_range_${_mode}
        COMMAND "${CMAKE_COMMAND}" -DENGINE_ROOT=${PROJECT_SOURCE_DIR}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/baseline_range_${_mode} -DMODE=${_mode}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/baseline/BaselineCheckTest.cmake")
endforeach()
set_tests_properties(cna_engine.baseline_range_descendant PROPERTIES LABELS boundary
    PASS_REGULAR_EXPRESSION "1 commits after the[ \r\n]+baseline")
# A CMake error from the check, whatever the exit code (messages wrap).
set_tests_properties(cna_engine.baseline_range_diverged PROPERTIES LABELS boundary
    PASS_REGULAR_EXPRESSION "CMake Error at[^\n]*\n[^(]*does not contain the[^(]*\\(older or diverged\\)")

# --- The dependency fetcher (GAME-002), dry: clones nothing. -------------------
if(NOT WIN32)
    add_test(NAME cna_engine.fetch_dependencies_dry_run
        COMMAND bash "${PROJECT_SOURCE_DIR}/tools/fetch_dependencies.sh" --dry-run --with-extended
                "${CMAKE_CURRENT_BINARY_DIR}/fetch_dependencies_dry_run")
    # Every dependency at its pin (cmake/CnaEngineBaseline.cmake), and nothing created.
    set(_fetch_expected "")
    foreach(_pair cna:CNA sharp-runtime:SHARP_RUNTIME easy-gl:EASYGL meta-gl:METAGL cna-extended:EXTENDED)
        string(REPLACE ":" ";" _pair "${_pair}")
        list(GET _pair 0 _name)
        list(GET _pair 1 _variable)
        string(SUBSTRING "${CNA_ENGINE_BASELINE_${_variable}}" 0 10 _short)
        string(APPEND _fetch_expected "${_name}: cloning[^\n]*${_short}.*")
    endforeach()
    set_tests_properties(cna_engine.fetch_dependencies_dry_run PROPERTIES LABELS boundary
        PASS_REGULAR_EXPRESSION "${_fetch_expected}done")
endif()

# --- A new game from the template (GAME-004); the template's game itself is
# --- built and run by examples/ (cna_engine.example.template_game). -----------
if(NOT WIN32)
    add_test(NAME cna_engine.new_game
        COMMAND "${CMAKE_COMMAND}" -DENGINE=${PROJECT_SOURCE_DIR}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/new_game_project
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/NewGameCheck.cmake")
    set_tests_properties(cna_engine.new_game PROPERTIES LABELS boundary)
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
    # Pilot-validated API changes must be deliberate (PILOT-003).
    add_test(NAME cna_engine.api_snapshot
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/api_snapshot.py"
                --root "${PROJECT_SOURCE_DIR}" --check)
    set_tests_properties(cna_engine.strict_boundary cna_engine.strict_boundary_detects_leaks
        cna_engine.api_snapshot PROPERTIES LABELS "boundary")
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
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Assets/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/App/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Collision/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Animation/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/Audio/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/CnaEngine/UI/*.hpp")
set(_cna_engine_header_tus "")
foreach(_header IN LISTS _cna_engine_strict_headers)
    file(RELATIVE_PATH _rel "${PROJECT_SOURCE_DIR}/include" "${_header}")
    string(MAKE_C_IDENTIFIER "${_rel}" _id)
    set(_tu "${CMAKE_CURRENT_BINARY_DIR}/header_checks/${_id}.cpp")
    file(CONFIGURE OUTPUT "${_tu}" CONTENT "#include \"${_rel}\"\n" @ONLY)
    list(APPEND _cna_engine_header_tus "${_tu}")
endforeach()
add_library(cna_engine_strict_header_check OBJECT ${_cna_engine_header_tus})
target_link_libraries(cna_engine_strict_header_check PRIVATE cna_engine_core cna_engine_graphics cna_engine_app
    cna_engine_audio cna_engine_ui)
