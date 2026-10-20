# SPDX-License-Identifier: MIT

# Strict-profile vertical slice: links only the strict targets.
add_executable(cna_engine_orbit_scene orbit_scene/main.cpp)
target_link_libraries(cna_engine_orbit_scene PRIVATE CnaEngine::Core CnaEngine::Graphics)
if(MSVC)
    target_compile_options(cna_engine_orbit_scene PRIVATE /W4 /WX)
else()
    target_compile_options(cna_engine_orbit_scene PRIVATE -Wall -Wextra -Werror)
endif()

if(CNA_ENGINE_BUILD_TESTS)
    add_test(NAME cna_engine.example.orbit_scene
        COMMAND cna_engine_orbit_scene "${CMAKE_CURRENT_BINARY_DIR}/orbit_scene_output")
    set_tests_properties(cna_engine.example.orbit_scene PROPERTIES LABELS "example")
endif()

# Quick start: a small strict-profile game as an ordinary XNA Game (needs
# CNA's runtime module for Game and GraphicsDeviceManager).
add_executable(cna_engine_quick_start quick_start/main.cpp)
target_link_libraries(cna_engine_quick_start PRIVATE CnaEngine::Core CnaEngine::Graphics CNA::Runtime)
if(MSVC)
    target_compile_options(cna_engine_quick_start PRIVATE /W4 /WX)
else()
    target_compile_options(cna_engine_quick_start PRIVATE -Wall -Wextra -Werror)
endif()
if(CNA_ENGINE_BUILD_TESTS)
    add_test(NAME cna_engine.example.quick_start
        COMMAND cna_engine_quick_start "${CMAKE_CURRENT_BINARY_DIR}/quick_start_output")
    set_tests_properties(cna_engine.example.quick_start PROPERTIES LABELS "example")
endif()

# Enhanced-profile vertical slice: PBR through CnaEngine::EnhancedGraphics.
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    add_executable(cna_engine_street_slice street_slice/main.cpp)
    target_link_libraries(cna_engine_street_slice PRIVATE CnaEngine::Core CnaEngine::Graphics CnaEngine::EnhancedGraphics)
    if(MSVC)
        target_compile_options(cna_engine_street_slice PRIVATE /W4 /WX)
    else()
        target_compile_options(cna_engine_street_slice PRIVATE -Wall -Wextra -Werror)
    endif()
    if(CNA_ENGINE_BUILD_TESTS)
        add_test(NAME cna_engine.example.street_slice
            COMMAND cna_engine_street_slice "${CMAKE_CURRENT_BINARY_DIR}/street_slice_output")
        set_tests_properties(cna_engine.example.street_slice PROPERTIES LABELS "example")
    endif()
endif()
