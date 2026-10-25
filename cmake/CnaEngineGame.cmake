# SPDX-License-Identifier: MIT
#
# Adds CNA and cna-engine to a game's own CMake project with the settings a
# game wants (plan.md GAME-001), so that the game's CMakeLists.txt is short:
#
#     cmake_minimum_required(VERSION 3.21)
#     project(my_game LANGUAGES C CXX)
#     set(CNA_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna-engine" CACHE PATH "cna-engine checkout")
#     include("${CNA_ENGINE_ROOT}/cmake/CnaEngineGame.cmake")
#     cna_engine_add_game(my_game SOURCES src/main.cpp src/MyGame.cpp ASSETS assets TITLE "My game")
#
# CNA, Sharp Runtime, easy-gl and meta-gl are expected beside the engine
# (tools/fetch_dependencies.sh puts them there); otherwise name them with
# -DCNA_ENGINE_CNA_DIR=..., -DCNA_ENGINE_SHARP_RUNTIME_DIR=... and
# -DCNA_EASYGL_ROOT=... (meta-gl beside easy-gl).
#
# The same project builds for the browser when configured with emcmake;
# cna_engine_add_game() then produces <build>/web-package/.
include_guard(GLOBAL)

get_filename_component(_cna_engine_game_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# CNA chooses the renderer, window platform and audio for the host: OpenGL ES
# 3 on Linux, SDL_GPU on Windows and macOS, WebGL 2 under Emscripten, SDL3
# for windows, input and sound. Empty values leave that choice to CNA; name
# one (e.g. -DCNA_ENGINE_CNA_RENDERER=OPENGL33) to override it.
set(CNA_ENGINE_CNA_RENDERER "" CACHE STRING "CNA renderer (empty: CNA's default for the host)")
set(CNA_ENGINE_CNA_PLATFORM "" CACHE STRING "CNA window platform (empty: CNA's default)")
set(CNA_ENGINE_CNA_AUDIO_PLATFORM "" CACHE STRING "CNA audio platform (empty: CNA's default)")

# A game builds only what it links: no tests, examples or install rules of the
# engine, and a newer compiler's warning inside the engine does not stop it.
set(CNA_ENGINE_BUILD_TESTS OFF CACHE BOOL "Build cna-engine's tests")
set(CNA_ENGINE_BUILD_EXAMPLES OFF CACHE BOOL "Build cna-engine's examples")
set(CNA_ENGINE_INSTALL OFF CACHE BOOL "cna-engine install rules")
set(CNA_ENGINE_PILOT_PROBES OFF CACHE BOOL "cna-engine pilot probes")
set(CNA_ENGINE_WARNINGS_AS_ERRORS OFF CACHE BOOL "Warnings in cna-engine's targets are errors")
# The cna-extended integration needs a cna-extended checkout; a game that uses
# it turns this on.
set(CNA_ENGINE_WITH_EXTENDED OFF CACHE BOOL "Build CnaEngine::ExtendedIntegration (needs cna-extended)")

# CNA chooses its supported runtime form for the host (shared on native ELF
# toolchains with recent CMake, static elsewhere). UP-006's mixed umbrella /
# module link is fixed in the pinned CNA and covered by engine-owned runtime
# and symbol tests; a game may still select -DCNA_SHARED_LIBRARY=OFF.
# No video playback (it needs FFmpeg) and none of CNA's developer audits.
set(CNA_ENABLE_VIDEO OFF CACHE STRING "CNA video playback (FFmpeg)")
set(CNA_PLATFORM_RATCHET OFF CACHE BOOL "CNA developer SDL-coupling audit")
set(BUILD_TESTING OFF CACHE BOOL "Dependency tests")
set(EASYGL_BUILD_TESTS OFF CACHE BOOL "easy-gl tests")
set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "easy-gl examples")
set(METAGL_BUILD_TESTS OFF CACHE BOOL "meta-gl tests")

add_subdirectory("${_cna_engine_game_root}" "${CMAKE_BINARY_DIR}/cna-engine" EXCLUDE_FROM_ALL)
