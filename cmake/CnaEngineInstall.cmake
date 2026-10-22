# SPDX-License-Identifier: MIT
#
# Install rules and the exported CMake package (ADR-001 versioning).

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CNA_ENGINE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/CnaEngine"
    CACHE STRING "Install directory of CnaEngineConfig.cmake, relative to the prefix")

install(TARGETS ${CNA_ENGINE_EXPORTED_TARGETS}
    EXPORT CnaEngineTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Strict headers always; enhanced headers only with the enhanced target, so a
# strict-only install cannot even offer the cna-extended adapters.
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/CnaEngine"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp"
    PATTERN "Extended" EXCLUDE
    PATTERN "Enhanced" EXCLUDE)
if(CNA_ENGINE_WITH_ENHANCED_GRAPHICS)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/CnaEngine/Enhanced"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/CnaEngine"
        FILES_MATCHING PATTERN "*.hpp")
endif()
if(CNA_ENGINE_WITH_EXTENDED)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/CnaEngine/Extended"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/CnaEngine"
        FILES_MATCHING PATTERN "*.hpp")
endif()

install(EXPORT CnaEngineTargets
    NAMESPACE CnaEngine::
    DESTINATION "${CNA_ENGINE_INSTALL_CMAKEDIR}")

set(CNA_ENGINE_PACKAGE_HAS_EXTENDED ${CNA_ENGINE_WITH_EXTENDED})
# CNA and cna-extended targets cannot be exported (UP-001): the package
# configuration re-attaches each target's links once the consumer defined them.
set(CNA_ENGINE_PACKAGE_REQUIRED "")
set(CNA_ENGINE_PACKAGE_LINK_LINES "")
foreach(_target IN LISTS CNA_ENGINE_EXPORTED_TARGETS)
    get_property(_links GLOBAL PROPERTY CNA_ENGINE_EXTERNAL_LINKS_${_target})
    if(NOT _links)
        continue()
    endif()
    get_target_property(_alias ${_target} EXPORT_NAME)
    set(_condition "TARGET CnaEngine::${_alias}")
    foreach(_link IN LISTS _links)
        string(APPEND _condition " AND TARGET ${_link}")
        if(NOT _link STREQUAL "CNA::Extended")
            list(APPEND CNA_ENGINE_PACKAGE_REQUIRED ${_link})
        endif()
    endforeach()
    list(JOIN _links " " _joined)
    string(APPEND CNA_ENGINE_PACKAGE_LINK_LINES
        "if(${_condition})\n"
        "    set_property(TARGET CnaEngine::${_alias} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${_joined})\n"
        "endif()\n")
endforeach()
list(REMOVE_DUPLICATES CNA_ENGINE_PACKAGE_REQUIRED)
list(JOIN CNA_ENGINE_PACKAGE_REQUIRED " " CNA_ENGINE_PACKAGE_REQUIRED)
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/CnaEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CnaEngineConfig.cmake"
    INSTALL_DESTINATION "${CNA_ENGINE_INSTALL_CMAKEDIR}")
# While the major version is 0, minor versions may break the API.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CnaEngineConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CnaEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CnaEngineConfigVersion.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/CnaEngineGameFunctions.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/CnaEngineWebPackage.cmake"
    DESTINATION "${CNA_ENGINE_INSTALL_CMAKEDIR}")
install(FILES "${PROJECT_SOURCE_DIR}/cmake/web/shell.html" DESTINATION "${CNA_ENGINE_INSTALL_CMAKEDIR}/web")

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/NOTICE.md" "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/cna-engine")
