# SPDX-License-Identifier: MIT
#
# Provides CNA's modular targets (and, when CNA_ENGINE_WITH_EXTENDED is ON,
# CNA::Extended) to cna-engine.
#
# CNA and cna-extended export no installable CMake package (upstream-issues.md
# UP-001), so they are consumed as source trees, exactly as cna-extended itself
# consumes CNA. A parent project that already defined CNA::Core (and
# CNA::Extended) is respected and nothing is added again.

set(CNA_ENGINE_CNA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna"
    CACHE PATH "Path to the modular cna repository")
set(CNA_ENGINE_SHARP_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtime"
    CACHE PATH "Path to the modular sharp-runtime repository")
set(CNA_ENGINE_EXTENDED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna-extended"
    CACHE PATH "Path to the cna-extended repository")

# Host-independent defaults: the CPU software renderer lets tests read back
# pixels, the headless platform needs no window, and null audio needs no device.
# These are the same axes cna-extended's standalone build selects, which keeps
# the shared ccache warm across both projects.
set(CNA_ENGINE_CNA_RENDERER "SOFTWARE"
    CACHE STRING "cna renderer used by a standalone cna-engine build")
set_property(CACHE CNA_ENGINE_CNA_RENDERER PROPERTY STRINGS
    SOFTWARE HEADLESS SDL_RENDERER OPENGLES2 OPENGLES3 OPENGL33 BGFX VULKAN)
set(CNA_ENGINE_CNA_PLATFORM "HEADLESS"
    CACHE STRING "cna platform used by a standalone cna-engine build")
set_property(CACHE CNA_ENGINE_CNA_PLATFORM PROPERTY STRINGS HEADLESS SDL3 SDL2 TERMINAL)
# CNA's own built-in extension layer (CNA_CNAEXT: shader packages, post
# effects, ...). No engine target needs it; it is ON only for builds that
# compile code which does (the Street scene comparison probe).
option(CNA_ENGINE_CNA_CNAEXT "Build CNA with its built-in extension layer (CNA_CNAEXT)" OFF)

set(CNA_ENGINE_CNA_AUDIO_PLATFORM "NULL"
    CACHE STRING "cna audio platform used by a standalone cna-engine build")
set_property(CACHE CNA_ENGINE_CNA_AUDIO_PLATFORM PROPERTY STRINGS NULL SDL3 SDL2)

# The union of CNA's own module closure and cna-extended's direct components.
# Setting it before Sharp Runtime is added prevents its standalone `All`
# default from enabling unrelated networking, HTTP, timers, ...
set(CNA_ENGINE_SHARP_RUNTIME_COMPONENTS
    Core.Base
    IO
    Collections.Core
    Collections.ObjectModel
    Runtime
    Threading
    Text
    Globalization
    Storage
    Security.Cryptography
    IO.Compression
    Text.Json
    Threading.Tasks
    Xml
)

set(CNA_ENGINE_REQUIRED_CNA_TARGETS
    CNA::Core
    CNA::Math
    CNA::Input
    CNA::GraphicsCore
    CNA::Content
)

# The checkouts the baseline check (CnaEngineBaseline.cmake) inspects: this
# project's own directories, or the parent's when it provides the targets.
set(CNA_ENGINE_CNA_CHECKOUT "${CNA_ENGINE_CNA_DIR}")
set(CNA_ENGINE_SHARP_RUNTIME_CHECKOUT "${CNA_ENGINE_SHARP_RUNTIME_DIR}")
set(CNA_ENGINE_EXTENDED_CHECKOUT "${CNA_ENGINE_EXTENDED_DIR}")

if(TARGET CNA::Core)
    message(STATUS "cna-engine: using CNA targets provided by the parent project")
    # project() caches every project's source directory.
    if(DEFINED CNA_SOURCE_DIR)
        set(CNA_ENGINE_CNA_CHECKOUT "${CNA_SOURCE_DIR}")
    endif()
    if(DEFINED SHARP_RUNTIME_SOURCE_DIR)
        set(CNA_ENGINE_SHARP_RUNTIME_CHECKOUT "${SHARP_RUNTIME_SOURCE_DIR}")
    endif()
    if(COMMAND sharp_runtime_enable_components)
        sharp_runtime_enable_components(${CNA_ENGINE_SHARP_RUNTIME_COMPONENTS})
    endif()
else()
    if(NOT EXISTS "${CNA_ENGINE_CNA_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "cna-engine: no CNA CMake project at '${CNA_ENGINE_CNA_DIR}'. "
            "Set -DCNA_ENGINE_CNA_DIR=/path/to/cna.")
    endif()
    if(NOT EXISTS "${CNA_ENGINE_SHARP_RUNTIME_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "cna-engine: no Sharp Runtime CMake project at '${CNA_ENGINE_SHARP_RUNTIME_DIR}'. "
            "Set -DCNA_ENGINE_SHARP_RUNTIME_DIR=/path/to/sharp-runtime.")
    endif()

    # Keep optional CNA surfaces out. CNA_CNAEXT is CNA's built-in extension
    # layer; neither strict nor enhanced engine targets need it (see
    # CNA_ENGINE_CNA_CNAEXT above).
    set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CNA_BUILD_C_API OFF CACHE BOOL "" FORCE)
    set(CNA_CNAEXT ${CNA_ENGINE_CNA_CNAEXT} CACHE BOOL "" FORCE)
    set(CNA_DEVICES OFF CACHE BOOL "" FORCE)
    set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)
    set(CNA_ENABLE_DRACO OFF CACHE BOOL "" FORCE)
    set(CNA_GRAPHICS_RENDERER "${CNA_ENGINE_CNA_RENDERER}" CACHE STRING "" FORCE)
    set(CNA_PLATFORM "${CNA_ENGINE_CNA_PLATFORM}" CACHE STRING "" FORCE)
    set(CNA_AUDIO_PLATFORM "${CNA_ENGINE_CNA_AUDIO_PLATFORM}" CACHE STRING "" FORCE)
    set(CNA_SHARP_RUNTIME_ROOT "${CNA_ENGINE_SHARP_RUNTIME_DIR}" CACHE PATH "" FORCE)
    set(SHARP_RUNTIME_COMPONENTS "${CNA_ENGINE_SHARP_RUNTIME_COMPONENTS}"
        CACHE STRING "Sharp Runtime components required by cna-engine and its CNA module closure" FORCE)
    set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    message(STATUS
        "cna-engine: adding modular CNA from '${CNA_ENGINE_CNA_DIR}' "
        "(renderer: ${CNA_ENGINE_CNA_RENDERER}, platform: ${CNA_ENGINE_CNA_PLATFORM}, "
        "audio: ${CNA_ENGINE_CNA_AUDIO_PLATFORM})")
    add_subdirectory("${CNA_ENGINE_CNA_DIR}" "${CMAKE_BINARY_DIR}/_cna" EXCLUDE_FROM_ALL)
endif()

foreach(_target IN LISTS CNA_ENGINE_REQUIRED_CNA_TARGETS)
    if(NOT TARGET ${_target})
        message(FATAL_ERROR "cna-engine: required CNA target '${_target}' is missing")
    endif()
endforeach()

if(CNA_ENGINE_WITH_EXTENDED AND TARGET CNA::Extended AND DEFINED cna_extended_SOURCE_DIR)
    set(CNA_ENGINE_EXTENDED_CHECKOUT "${cna_extended_SOURCE_DIR}")
endif()
if(CNA_ENGINE_WITH_EXTENDED AND NOT TARGET CNA::Extended)
    if(NOT EXISTS "${CNA_ENGINE_EXTENDED_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "cna-engine: CNA_ENGINE_WITH_EXTENDED=ON but no cna-extended project at "
            "'${CNA_ENGINE_EXTENDED_DIR}'. Set -DCNA_ENGINE_EXTENDED_DIR=... or "
            "-DCNA_ENGINE_WITH_EXTENDED=OFF.")
    endif()
    # cna-extended detects the CNA targets defined above and links them
    # instead of adding CNA a second time.
    set(CNA_EXTENDED_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CNA_EXTENDED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CNA_ENGINE_EXTENDED_DIR}" "${CMAKE_BINARY_DIR}/_cna_extended" EXCLUDE_FROM_ALL)
endif()
