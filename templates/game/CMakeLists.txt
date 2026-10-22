cmake_minimum_required(VERSION 3.21)
project(@GAME_TARGET@ LANGUAGES C CXX)

# cna-engine adds CNA, Sharp Runtime and easy-gl for this game; they are
# expected beside the engine (cna-engine's tools/fetch_dependencies.sh puts
# them there). Another engine checkout: -DCNA_ENGINE_ROOT=/path/to/cna-engine.
set(CNA_ENGINE_ROOT "@ENGINE_ROOT@" CACHE PATH "cna-engine checkout")
include("${CNA_ENGINE_ROOT}/cmake/CnaEngineGame.cmake")

# One call for the desktop and, configured with emcmake, the browser
# (tools/build_web.sh).
cna_engine_add_game(@GAME_TARGET@
    SOURCES src/main.cpp src/Game.cpp
    ASSETS assets
    TITLE "@GAME_NAME@"
    PROFILE enhanced)  # no HINT: the game draws its own
