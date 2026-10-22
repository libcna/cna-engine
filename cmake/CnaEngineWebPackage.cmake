# SPDX-License-Identifier: MIT
#
# Script mode (cmake -DGAME=<name> -DFROM=<dir> -DTO=<dir> -P ...): gathers a
# browser build's page, script, wasm and preloaded data in one directory any
# static web server can serve, with the page also as index.html. Run after
# each link by cna_engine_add_game().

foreach(variable GAME FROM TO)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "CnaEngineWebPackage: ${variable} is not set")
    endif()
endforeach()

file(MAKE_DIRECTORY "${TO}")
foreach(extension html js wasm data)
    set(file "${FROM}/${GAME}.${extension}")
    if(EXISTS "${file}")
        file(COPY_FILE "${file}" "${TO}/${GAME}.${extension}" ONLY_IF_DIFFERENT)
    elseif(NOT extension STREQUAL "data")
        message(FATAL_ERROR "CnaEngineWebPackage: ${file} is missing")
    endif()
endforeach()
file(COPY_FILE "${FROM}/${GAME}.html" "${TO}/index.html" ONLY_IF_DIFFERENT)
