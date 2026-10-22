# SPDX-License-Identifier: MIT
#
# Script mode (cmake -DENGINE=<engine> -DWORK=<dir> -P ...): runs
# tools/new_game.sh into WORK and checks the project it writes (GAME-004):
# every file is there, no template token is left, and the engine path in
# its CMakeLists.txt leads back to this engine.
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND bash "${ENGINE}/tools/new_game.sh" "${WORK}" "Test Walk 3D"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "new_game.sh failed (${result}): ${output}${error}")
endif()
foreach(file CMakeLists.txt README.md .gitignore src/Game.hpp src/Game.cpp src/main.cpp
             assets/crate.png assets/step1.wav assets/step2.wav assets/LICENSE.md tools/build_web.sh)
    if(NOT EXISTS "${WORK}/${file}")
        message(FATAL_ERROR "new_game.sh did not write ${file}")
    endif()
endforeach()
file(GLOB_RECURSE texts "${WORK}/*.txt" "${WORK}/*.md" "${WORK}/*.cpp" "${WORK}/*.hpp" "${WORK}/*.sh")
foreach(text IN LISTS texts)
    file(READ "${text}" content)
    if(content MATCHES "@(GAME_[A-Z]+|ENGINE_ROOT)@")
        message(FATAL_ERROR "template token '${CMAKE_MATCH_0}' left in ${text}")
    endif()
endforeach()
file(READ "${WORK}/src/Game.hpp" header)
if(NOT header MATCHES "class TestWalk3DGame final")
    message(FATAL_ERROR "unexpected class name in src/Game.hpp")
endif()
file(STRINGS "${WORK}/CMakeLists.txt" root REGEX "^set\\(CNA_ENGINE_ROOT ")
string(REGEX REPLACE "^set\\(CNA_ENGINE_ROOT \"([^\"]*)\".*" "\\1" root "${root}")
string(REPLACE "\${CMAKE_CURRENT_SOURCE_DIR}" "${WORK}" root "${root}")
get_filename_component(root "${root}" REALPATH)
get_filename_component(engine "${ENGINE}" REALPATH)
if(NOT root STREQUAL engine)
    message(FATAL_ERROR "the new project's CNA_ENGINE_ROOT is '${root}', not the engine '${engine}'")
endif()
file(STRINGS "${WORK}/CMakeLists.txt" game REGEX "^cna_engine_add_game\\(test-walk-3d$")
if(NOT game)
    message(FATAL_ERROR "the new project does not build the target test-walk-3d")
endif()
execute_process(COMMAND bash "${ENGINE}/tools/new_game.sh" "${WORK}" "Again" RESULT_VARIABLE again
    OUTPUT_QUIET ERROR_QUIET)
if(again EQUAL 0)
    message(FATAL_ERROR "new_game.sh wrote into a directory that was not empty")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "new_game.sh: project written and checked")
