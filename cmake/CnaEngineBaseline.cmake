# SPDX-License-Identifier: MIT
#
# Pinned compatibility baseline (ADR-002, docs/adr/0002-compatibility-baseline.md).
#
# The engine is verified against exactly these revisions. Configuration
# compares them with the checked-out dependency trees; a mismatch is a warning
# by default and an error with -DCNA_ENGINE_ENFORCE_BASELINE=ON (CI).

set(CNA_ENGINE_BASELINE_CNA "705ea232439135654ce93279dfe87c1ab047ef5c")
set(CNA_ENGINE_BASELINE_CNA_BRANCH "next")
set(CNA_ENGINE_BASELINE_EXTENDED "0061e3e17c30b2a3a302bce617e64765883079aa")
set(CNA_ENGINE_BASELINE_EXTENDED_BRANCH "develop")
set(CNA_ENGINE_BASELINE_SHARP_RUNTIME "f2aecfefeac171b331787e5cdc4849507363ef0e")
set(CNA_ENGINE_BASELINE_SHARP_RUNTIME_BRANCH "next")

function(_cna_engine_check_one name dir expected)
    if(NOT EXISTS "${dir}/.git")
        message(STATUS "cna-engine baseline: ${name} at '${dir}' is not a git checkout; not verified")
        return()
    endif()
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(STATUS "cna-engine baseline: git not found; ${name} not verified")
        return()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${dir}" rev-parse HEAD
        OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE rc ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${dir}" status --porcelain --untracked-files=no
        OUTPUT_VARIABLE dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT rc EQUAL 0)
        message(STATUS "cna-engine baseline: cannot read ${name} revision; not verified")
        return()
    endif()
    set(problem "")
    if(NOT head STREQUAL expected)
        set(problem "${name} is at ${head}, baseline is ${expected}")
    elseif(NOT dirty STREQUAL "")
        set(problem "${name} has modified tracked files at the baseline revision")
    endif()
    if(problem STREQUAL "")
        message(STATUS "cna-engine baseline: ${name} matches ${expected}")
    elseif(CNA_ENGINE_ENFORCE_BASELINE)
        message(FATAL_ERROR "cna-engine baseline: ${problem}")
    else()
        message(WARNING "cna-engine baseline: ${problem} (results are not baseline evidence)")
    endif()
endfunction()

function(cna_engine_check_baseline)
    if(DEFINED CNA_ENGINE_CNA_DIR)
        _cna_engine_check_one("CNA" "${CNA_ENGINE_CNA_DIR}" "${CNA_ENGINE_BASELINE_CNA}")
    endif()
    if(DEFINED CNA_ENGINE_SHARP_RUNTIME_DIR)
        _cna_engine_check_one("Sharp Runtime" "${CNA_ENGINE_SHARP_RUNTIME_DIR}" "${CNA_ENGINE_BASELINE_SHARP_RUNTIME}")
    endif()
    if(CNA_ENGINE_WITH_EXTENDED AND DEFINED CNA_ENGINE_EXTENDED_DIR)
        _cna_engine_check_one("cna-extended" "${CNA_ENGINE_EXTENDED_DIR}" "${CNA_ENGINE_BASELINE_EXTENDED}")
    endif()
endfunction()
