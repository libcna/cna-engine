# SPDX-License-Identifier: MIT
#
# UP-006: the executable must use libcna.so and must not define a second copy
# of CNA's strong Graphics/Input/Vector3 implementation symbols.
execute_process(COMMAND "${CNA_ENGINE_NM}" -C --defined-only "${CNA_ENGINE_PROBE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR symbols STREQUAL "")
    message(FATAL_ERROR "Cannot inspect mixed-runtime consumer: ${result}: ${error}")
endif()

string(REGEX MATCHALL
    "[0-9a-fA-F]+ [Tt] Microsoft::Xna::Framework::(Graphics::|Input::|Vector3::)[^\n]+"
    duplicates "${symbols}")
if(duplicates)
    message(FATAL_ERROR "Mixed-runtime consumer contains duplicate CNA definitions: ${duplicates}")
endif()

execute_process(COMMAND "${CNA_ENGINE_READELF}" -d "${CNA_ENGINE_PROBE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE dynamic ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect mixed-runtime dependencies: ${result}: ${error}")
endif()
string(REGEX MATCHALL "\\(NEEDED\\)[^\n]*\\[libcna\\.so\\]" cna_needed "${dynamic}")
list(LENGTH cna_needed cna_count)
if(NOT cna_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one libcna.so dependency, found ${cna_count}: ${dynamic}")
endif()

message(STATUS "Mixed-runtime consumer uses one libcna.so and defines no CNA runtime symbols")
