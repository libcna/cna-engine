# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20)

project(cna_engine
    VERSION 0.1.0
    DESCRIPTION "Game-engine layer above CNA (strict-XNA core plus optional cna-extended integration)"
    LANGUAGES CXX)

# Same language level as CNA, Sharp Runtime and cna-extended.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CNA_ENGINE_IS_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CNA_ENGINE_IS_TOP_LEVEL ON)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CNA_ENGINE_BUILD_TESTS "Build cna-engine unit and boundary tests" ${CNA_ENGINE_IS_TOP_LEVEL})
option(CNA_ENGINE_BUILD_EXAMPLES "Build cna-engine examples" ${CNA_ENGINE_IS_TOP_LEVEL})
option(CNA_ENGINE_WITH_EXTENDED
    "Build CnaEngine::ExtendedIntegration (the only target that depends on cna-extended)" ON)
option(CNA_ENGINE_WITH_ENHANCED_GRAPHICS
    "Build CnaEngine::EnhancedGraphics (CNA graphics extensions such as PbrEffect; no cna-extended)" ON)
option(CNA_ENGINE_INSTALL "Generate install and package-export rules" ${CNA_ENGINE_IS_TOP_LEVEL})
option(CNA_ENGINE_WARNINGS_AS_ERRORS "Treat warnings in cna-engine's own targets as errors" ON)
set(CNA_ENGINE_SANITIZE "" CACHE STRING
    "Sanitizers for cna-engine's own targets (GCC/Clang -fsanitize= list, e.g. address,undefined); dependencies stay uninstrumented")
option(CNA_ENGINE_ENFORCE_BASELINE
    "Fail configuration when CNA/cna-extended/Sharp Runtime differ from the pinned baseline (ADR-002)" OFF)
option(CNA_ENGINE_REQUIRE_BASELINE_DESCENDANT
    "Fail configuration when a dependency checkout does not contain its pinned baseline revision" OFF)

# ---------------------------------------------------------------------------
# Dependencies (ADR-001: source-tree consumption, see cmake/CnaEngineDependencies.cmake)
# ---------------------------------------------------------------------------
include(CnaEngineDependencies)
include(CnaEngineBaseline)
cna_engine_check_baseline()

include(CnaEngineTargets)

# ---------------------------------------------------------------------------
# Strict targets: XNA-compatible CNA surface, Sharp Runtime and std only.
# ---------------------------------------------------------------------------
cna_engine_add_library(cna_engine_core Core
    src/Core/Version.cpp
    src/Core/FixedStepClock.cpp
    src/Core/SunPosition.cpp
    src/Core/SettingsFile.cpp
    src/Input/InputSnapshot.cpp
    src/Input/InputNames.cpp
    src/Input/ActionMap.cpp
    src/Input/BindingCapture.cpp
    src/Diagnostics/FrameStatistics.cpp
    src/Diagnostics/ScopedTimer.cpp
)
cna_engine_link_external(cna_engine_core CNA::Math CNA::Input)

cna_engine_add_library(cna_engine_graphics Graphics
    src/Graphics/Camera.cpp
    src/Graphics/CameraControllers.cpp
    src/Graphics/MeshData.cpp
    src/Graphics/MeshBuilder.cpp
    src/Graphics/TextureAtlas.cpp
    src/Graphics/Image.cpp
    src/Graphics/DebugDraw.cpp
    src/Graphics/VertexLayouts.cpp
    src/Graphics/GpuMesh.cpp
    src/Graphics/RenderStatistics.cpp
    src/Graphics/ScreenCapture.cpp
    src/Graphics/AutomatedRun.cpp
    src/Graphics/RenderQueue.cpp
    src/Graphics/Visibility.cpp
    src/Graphics/ForwardRenderer.cpp
    src/Graphics/Quality.cpp
    src/Graphics/QualitySchema.cpp
    src/Graphics/Instancing.cpp
    src/Graphics/DiagnosticsReport.cpp
    src/Graphics/GpuMemory.cpp
    src/Graphics/MaterialBinder.cpp
    src/Assets/AssetRegistry.cpp
)
target_link_libraries(cna_engine_graphics PUBLIC cna_engine_core)
cna_engine_link_external(cna_engine_graphics CNA::GraphicsCore CNA::Content)

set(CNA_ENGINE_EXPORTED_TARGETS cna_engine_core cna_engine_graphics)

# ---------------------------------------------------------------------------
# Enhanced graphics: CNA extension APIs (PbrEffect, ...), never cna-extended.
# ---------------------------------------------------------------------------
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    cna_engine_add_library(cna_engine_enhanced EnhancedGraphics
        src/Enhanced/PbrMaterialBinder.cpp
    )
    target_link_libraries(cna_engine_enhanced PUBLIC cna_engine_graphics)
    cna_engine_link_external(cna_engine_enhanced CNA::GraphicsCore)
    list(APPEND CNA_ENGINE_EXPORTED_TARGETS cna_engine_enhanced)
endif()

# ---------------------------------------------------------------------------
# Enhanced target: the sole dependency on cna-extended.
# ---------------------------------------------------------------------------
if(CNA_ENGINE_WITH_EXTENDED)
    cna_engine_add_library(cna_engine_extended ExtendedIntegration
        src/Extended/DebugOverlay.cpp
        src/Extended/EngineWorldScreen.cpp
        src/Extended/WorldCameraSync.cpp
    )
    target_link_libraries(cna_engine_extended PUBLIC cna_engine_graphics)
    cna_engine_link_external(cna_engine_extended CNA::Extended)
    list(APPEND CNA_ENGINE_EXPORTED_TARGETS cna_engine_extended)
endif()

# Convenience aggregate for enhanced consumers.
add_library(cna_engine INTERFACE)
add_library(CnaEngine::Engine ALIAS cna_engine)
set_target_properties(cna_engine PROPERTIES EXPORT_NAME Engine)
target_link_libraries(cna_engine INTERFACE cna_engine_core cna_engine_graphics)
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    target_link_libraries(cna_engine INTERFACE cna_engine_enhanced)
endif()
if(CNA_ENGINE_WITH_EXTENDED)
    target_link_libraries(cna_engine INTERFACE cna_engine_extended)
endif()
list(APPEND CNA_ENGINE_EXPORTED_TARGETS cna_engine)

# ---------------------------------------------------------------------------
# Tests, examples, install
# ---------------------------------------------------------------------------
if(CNA_ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CNA_ENGINE_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

if(CNA_ENGINE_INSTALL)
    include(CnaEngineInstall)
endif()
