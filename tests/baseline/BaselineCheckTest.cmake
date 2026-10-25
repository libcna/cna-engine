# SPDX-License-Identifier: MIT
#
# Checks cmake/CnaEngineBaseline.cmake's range report on throwaway git
# repositories: a checkout after the pin is a descendant, a checkout on another
# line does not contain the pin (an error when
# CNA_ENGINE_REQUIRE_BASELINE_DESCENDANT is ON).
#
#   cmake -DENGINE_ROOT=<repo> -DWORK=<empty dir> -DMODE=descendant|diverged -P BaselineCheckTest.cmake
include("${ENGINE_ROOT}/cmake/CnaEngineBaseline.cmake")
find_package(Git REQUIRED)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
function(run)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK}" ${ARGN} RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} failed")
    endif()
endfunction()
function(commit message)
    file(WRITE "${WORK}/file.txt" "${message}\n")
    run(add file.txt)
    # The fixture must not inherit a developer's global commit.gpgsign=true:
    # its throwaway repository has no need for signatures and CI/sandboxes may
    # not have a writable GnuPG home.
    run(-c user.name=test -c user.email=test@example.invalid -c commit.gpgsign=false
        commit -q -m "${message}")
endfunction()

run(init -q -b main)
commit("base")
commit("pin")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK}" rev-parse HEAD OUTPUT_VARIABLE pin
    OUTPUT_STRIP_TRAILING_WHITESPACE)

if(MODE STREQUAL "descendant")
    commit("after the pin")
    _cna_engine_check_one("Dependency" "${WORK}" "${pin}")
elseif(MODE STREQUAL "diverged")
    run(checkout -q -b other HEAD~1)
    commit("another line")
    set(CNA_ENGINE_REQUIRE_BASELINE_DESCENDANT ON)
    _cna_engine_check_one("Dependency" "${WORK}" "${pin}")
else()
    message(FATAL_ERROR "MODE must be descendant or diverged")
endif()
