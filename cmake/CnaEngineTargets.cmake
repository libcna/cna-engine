# SPDX-License-Identifier: MIT
#
# Helper defining one cna-engine library: STATIC target `<target>` with alias
# CnaEngine::<alias>, the shared public include root, and warning flags that
# stay on cna-engine's own targets (dependency flags are never changed).

# Instruments one of cna-engine's own targets (libraries, tests, examples)
# with CNA_ENGINE_SANITIZE. CNA and cna-extended are compiled as before, so a
# sanitizer tree shares their objects with the ordinary tree through ccache.
function(cna_engine_sanitize target)
    if(CNA_ENGINE_SANITIZE AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fsanitize=${CNA_ENGINE_SANITIZE} -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC "$<BUILD_INTERFACE:-fsanitize=${CNA_ENGINE_SANITIZE}>")
    endif()
endfunction()

function(cna_engine_add_library target alias)
    add_library(${target} STATIC ${ARGN})
    add_library(CnaEngine::${alias} ALIAS ${target})
    set_target_properties(${target} PROPERTIES
        EXPORT_NAME ${alias}
        OUTPUT_NAME ${target})
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${CNA_ENGINE_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        # No -Wpedantic: Sharp Runtime's public headers use __int128.
        target_compile_options(${target} PRIVATE -Wall -Wextra
            $<$<BOOL:${CNA_ENGINE_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
    cna_engine_sanitize(${target})
endfunction()

# Links CNA / cna-extended targets. They are not part of any export set
# (upstream-issues.md UP-001), so they are linked for the build tree only and
# recorded in CNA_ENGINE_EXTERNAL_LINKS_<target>; CnaEngineConfig.cmake re-adds
# them to the imported targets after checking that the consumer defined them.
#
# Their headers are system headers for the engine's own compilation: a warning
# inside CNA or cna-extended (cna-extended's SizeF under Clang, UP-015) must
# not fail the engine's -Werror build.
function(cna_engine_link_external target)
    foreach(_dependency IN LISTS ARGN)
        target_link_libraries(${target} PUBLIC "$<BUILD_INTERFACE:${_dependency}>")
        target_include_directories(${target} SYSTEM PRIVATE
            "$<TARGET_PROPERTY:${_dependency},INTERFACE_INCLUDE_DIRECTORIES>")
    endforeach()
    set_property(GLOBAL APPEND PROPERTY CNA_ENGINE_EXTERNAL_LINKS_${target} ${ARGN})
endfunction()
