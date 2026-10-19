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
