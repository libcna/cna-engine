# SPDX-License-Identifier: MIT
#
# Installed-package consumer probes (plan item ENG-012).
#
# This is a separate CMake project that uses cna-engine exactly like a game
# would: through `find_package(CnaEngine)` on an installed prefix, never through
# the engine's source tree. CNA installs no package (upstream-issues.md
# UP-001), so - like any CNA game today - the probe adds it as a source tree
# first. cna-extended comes from its installed package when it has one
# (0.2.0 on: PROBE_EXTENDED_FROM_PACKAGE), else from its source tree.
#
# Run it with tools/run_consumer_probes.sh.
cmake_minimum_required(VERSION 3.20)
project(cna_engine_consumer_probes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROBE_CNA_DIR "" CACHE PATH "CNA source tree")
set(PROBE_SHARP_RUNTIME_DIR "" CACHE PATH "Sharp Runtime source tree")
set(PROBE_EXTENDED_DIR "" CACHE PATH "cna-extended source tree")
option(PROBE_EXTENDED_FROM_PACKAGE "cna-extended from its installed package (found by the engine's package)" OFF)

# --- Dependencies as a CNA game adds them (same axes as the engine build). ---
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_C_API OFF CACHE BOOL "" FORCE)
set(CNA_CNAEXT OFF CACHE BOOL "" FORCE)
set(CNA_DEVICES OFF CACHE BOOL "" FORCE)
set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)
set(CNA_ENABLE_DRACO OFF CACHE BOOL "" FORCE)
set(CNA_GRAPHICS_RENDERER SOFTWARE CACHE STRING "" FORCE)
set(CNA_PLATFORM HEADLESS CACHE STRING "" FORCE)
set(CNA_AUDIO_PLATFORM NULL CACHE STRING "" FORCE)
# Headless consumers need neither CNA's vendored SDL nor video playback (a CNA
# checkout without its SDL submodule, as in CI, must still configure).
set(CNA_ENABLE_SDL OFF CACHE STRING "" FORCE)
set(CNA_ENABLE_VIDEO OFF CACHE STRING "" FORCE)
set(CNA_SHARP_RUNTIME_ROOT "${PROBE_SHARP_RUNTIME_DIR}" CACHE PATH "" FORCE)
set(SHARP_RUNTIME_COMPONENTS
    "Core.Base;IO;Collections.Core;Collections.ObjectModel;Runtime;Threading;Text;Globalization;Storage;Security.Cryptography;IO.Compression;Text.Json;Threading.Tasks;Xml"
    CACHE STRING "" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${PROBE_CNA_DIR}" "${CMAKE_BINARY_DIR}/_cna" EXCLUDE_FROM_ALL)

if(NOT PROBE_EXTENDED_FROM_PACKAGE)
    set(CNA_EXTENDED_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CNA_EXTENDED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PROBE_EXTENDED_DIR}" "${CMAKE_BINARY_DIR}/_cna_extended" EXCLUDE_FROM_ALL)
endif()

# --- The engine, from its installed package only. ----------------------------
find_package(CnaEngine 0.1 REQUIRED COMPONENTS Core Graphics App Audio UI EnhancedGraphics ExtendedIntegration)
if(PROBE_EXTENDED_FROM_PACKAGE)
    get_target_property(_probe_extended_imported CNA::Extended IMPORTED)
    if(NOT _probe_extended_imported)
        message(FATAL_ERROR "CNA::Extended should come from the installed CnaExtended package")
    endif()
    message(STATUS "consumer probes: CNA::Extended from the installed package ${CnaExtended_VERSION}")
endif()

add_executable(strict_consumer strict_consumer.cpp)
target_link_libraries(strict_consumer PRIVATE CnaEngine::Core CnaEngine::Graphics)

add_executable(enhanced_consumer enhanced_consumer.cpp)
target_link_libraries(enhanced_consumer PRIVATE CnaEngine::Engine)

# The strict consumer's link closure must not reach cna-extended.
function(_probe_collect_links target out)
    set(seen "")
    set(queue ${target})
    while(queue)
        list(POP_FRONT queue current)
        if(current IN_LIST seen OR NOT TARGET ${current})
            continue()
        endif()
        list(APPEND seen ${current})
        get_target_property(aliased ${current} ALIASED_TARGET)
        if(aliased)
            list(APPEND queue ${aliased})
        endif()
        get_target_property(links ${current} INTERFACE_LINK_LIBRARIES)
        if(links)
            foreach(link IN LISTS links)
                string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" link "${link}")
                if(NOT link MATCHES "^\\$<")
                    list(APPEND queue ${link})
                endif()
            endforeach()
        endif()
    endwhile()
    set(${out} ${seen} PARENT_SCOPE)
endfunction()
_probe_collect_links(CnaEngine::Graphics _strict_closure)
foreach(_forbidden IN ITEMS CNA::Extended CNA_EXTENDED CnaEngine::ExtendedIntegration CnaEngine::EnhancedGraphics)
    if(_forbidden IN_LIST _strict_closure)
        message(FATAL_ERROR "strict consumer probe: CnaEngine::Graphics reaches ${_forbidden}")
    endif()
endforeach()
message(STATUS "strict consumer probe: link closure of CnaEngine::Graphics has no enhanced or cna-extended target")

# A game the way a game builds itself, from the installed package's
# cna_engine_add_game() (plan.md GAME-001): the engine's hello example.
cna_engine_add_game(installed_game
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../examples/hello_engine/main.cpp"
    ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/../../examples/hello_engine/assets")

enable_testing()
add_test(NAME consumer.strict COMMAND strict_consumer)
add_test(NAME consumer.enhanced COMMAND enhanced_consumer)
add_test(NAME consumer.game COMMAND installed_game --frames 20 --lockstep
         --screenshot "${CMAKE_CURRENT_BINARY_DIR}/installed_game.png")
