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
    PATTERN "Extended" EXCLUDE)
if(CNA_ENGINE_WITH_EXTENDED)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/CnaEngine/Extended"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/CnaEngine"
        FILES_MATCHING PATTERN "*.hpp")
endif()

install(EXPORT CnaEngineTargets
    NAMESPACE CnaEngine::
    DESTINATION "${CNA_ENGINE_INSTALL_CMAKEDIR}")

set(CNA_ENGINE_PACKAGE_HAS_EXTENDED ${CNA_ENGINE_WITH_EXTENDED})
get_property(CNA_ENGINE_PACKAGE_CORE_LINKS GLOBAL PROPERTY CNA_ENGINE_EXTERNAL_LINKS_cna_engine_core)
get_property(CNA_ENGINE_PACKAGE_GRAPHICS_LINKS GLOBAL PROPERTY CNA_ENGINE_EXTERNAL_LINKS_cna_engine_graphics)
get_property(CNA_ENGINE_PACKAGE_EXTENDED_LINKS GLOBAL PROPERTY CNA_ENGINE_EXTERNAL_LINKS_cna_engine_extended)
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
    DESTINATION "${CNA_ENGINE_INSTALL_CMAKEDIR}")
