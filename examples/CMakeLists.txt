# SPDX-License-Identifier: MIT

# Strict-profile vertical slice: links only the strict targets.
add_executable(cna_engine_orbit_scene orbit_scene/main.cpp)
target_link_libraries(cna_engine_orbit_scene PRIVATE CnaEngine::Core CnaEngine::Graphics)
cna_engine_sanitize(cna_engine_orbit_scene)
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
cna_engine_sanitize(cna_engine_quick_start)
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

# The smallest game on EngineGame, built the way a game builds itself
# (cna_engine_add_game: assets beside the executable, a browser page under
# Emscripten).
cna_engine_add_game(cna_engine_hello
    SOURCES hello_engine/main.cpp
    ASSETS hello_engine/assets
    TITLE "Hello, engine"
    HINT "F3 statistics &middot; F12 screenshot"
    WEB_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}/hello_engine_web")
cna_engine_sanitize(cna_engine_hello)
if(MSVC)
    target_compile_options(cna_engine_hello PRIVATE /W4 /WX)
else()
    target_compile_options(cna_engine_hello PRIVATE -Wall -Wextra -Werror)
endif()
if(CNA_ENGINE_BUILD_TESTS AND NOT EMSCRIPTEN)
    add_test(NAME cna_engine.example.hello_engine
        COMMAND cna_engine_hello --frames 120 --lockstep
                --screenshot "${CMAKE_CURRENT_BINARY_DIR}/hello_engine.png")
    set_tests_properties(cna_engine.example.hello_engine PROPERTIES LABELS "example")
endif()

# The game template (templates/game, tools/new_game.sh) built and run as
# tools/new_game.sh would write it, so the template cannot rot (GAME-004).
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    set(GAME_NAME "Template Game")
    set(GAME_TARGET "cna_engine_template_game")
    set(GAME_CLASS "TemplateGame")
    set(_template_sources "")
    foreach(_file Game.hpp Game.cpp main.cpp)
        configure_file("${PROJECT_SOURCE_DIR}/templates/game/src/${_file}"
                       "${CMAKE_CURRENT_BINARY_DIR}/template_game/src/${_file}" @ONLY)
        list(APPEND _template_sources "${CMAKE_CURRENT_BINARY_DIR}/template_game/src/${_file}")
    endforeach()
    cna_engine_add_game(cna_engine_template_game
        SOURCES ${_template_sources}
        ASSETS "${PROJECT_SOURCE_DIR}/templates/game/assets"
        TITLE "${GAME_NAME}"
        PROFILE enhanced
        WEB_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}/template_game_web")
    cna_engine_sanitize(cna_engine_template_game)
    if(NOT MSVC)
        target_compile_options(cna_engine_template_game PRIVATE -Wall -Wextra -Werror)
    endif()
    if(CNA_ENGINE_BUILD_TESTS AND NOT EMSCRIPTEN)
        add_test(NAME cna_engine.example.template_game
            COMMAND cna_engine_template_game --frames 40 --lockstep --width 480 --height 270
                    --screenshot "${CMAKE_CURRENT_BINARY_DIR}/template_game.png")
        set_tests_properties(cna_engine.example.template_game PROPERTIES LABELS "example")
    endif()
endif()

# Enhanced-profile vertical slice: PBR through CnaEngine::EnhancedGraphics.
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    add_executable(cna_engine_street_slice street_slice/main.cpp)
    target_link_libraries(cna_engine_street_slice PRIVATE CnaEngine::Core CnaEngine::Graphics CnaEngine::EnhancedGraphics)
    cna_engine_sanitize(cna_engine_street_slice)
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
