# SPDX-License-Identifier: MIT
#
# cna_engine_add_game(): one call that builds a game on the engine for the
# desktop and, under Emscripten, for the browser (plan.md GAME-001). The
# engine's CMakeLists includes this file, so the function exists wherever the
# engine was added -- through cmake/CnaEngineGame.cmake or by hand.
#
#     cna_engine_add_game(<target>
#         SOURCES <file>...              the game's sources
#         [ASSETS <dir>]                 copied beside the executable; preloaded in the browser
#         [ASSETS_NAME <name>]           the directory's name at run time (default: its own name)
#         [TITLE <text>]                 the browser page's title (default: <target>)
#         [HINT <text>]                  a line of controls under the browser picture
#         [WEB_SHELL <file>]             an own page instead of the engine's cmake/web/shell.html
#         [WEB_PACKAGE <dir>]            where the browser files are gathered
#                                        (default: <build>/web-package)
#         [PROFILE strict|enhanced]      enhanced adds CnaEngine::EnhancedGraphics (default: strict)
#         [LIBRARIES <target>...])       further libraries to link
#
# The recipe is the one backrooms-, school- and house-simulator each wrote for
# themselves: assets beside the executable on the desktop; in the browser one
# page with the wasm, the assets as a single upfront preload, the blocking
# Game::Run loop through Asyncify and CNA's JS-lowered exception ABI.

# A global property, not a variable: the function is called from the game's
# own directory, where variables of the engine's directory are not visible.
set_property(GLOBAL PROPERTY CNA_ENGINE_GAME_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(cna_engine_add_game target)
    cmake_parse_arguments(PARSE_ARGV 1 GAME ""
        "ASSETS;ASSETS_NAME;TITLE;HINT;WEB_SHELL;WEB_PACKAGE;PROFILE" "SOURCES;LIBRARIES")
    get_property(cmake_dir GLOBAL PROPERTY CNA_ENGINE_GAME_CMAKE_DIR)
    if(GAME_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "cna_engine_add_game(${target}): unknown arguments '${GAME_UNPARSED_ARGUMENTS}'")
    endif()
    if(NOT GAME_SOURCES)
        message(FATAL_ERROR "cna_engine_add_game(${target}): SOURCES is required")
    endif()
    if(NOT GAME_PROFILE)
        set(GAME_PROFILE strict)
    endif()
    if(NOT GAME_PROFILE MATCHES "^(strict|enhanced)$")
        message(FATAL_ERROR "cna_engine_add_game(${target}): PROFILE must be strict or enhanced, not '${GAME_PROFILE}'")
    endif()
    if(NOT GAME_TITLE)
        set(GAME_TITLE "${target}")
    endif()
    if(NOT GAME_WEB_PACKAGE)
        set(GAME_WEB_PACKAGE "${CMAKE_BINARY_DIR}/web-package")
    endif()
    set(assets "")
    if(GAME_ASSETS)
        get_filename_component(assets "${GAME_ASSETS}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT IS_DIRECTORY "${assets}")
            message(FATAL_ERROR "cna_engine_add_game(${target}): ASSETS '${assets}' is not a directory")
        endif()
        if(NOT GAME_ASSETS_NAME)
            get_filename_component(GAME_ASSETS_NAME "${assets}" NAME)
        endif()
    endif()

    add_executable(${target} ${GAME_SOURCES})
    # The `CNA` umbrella is the runtime plus the selected renderer. With a
    # shared CNA it is libcna.so, a second CNA beside the engine's static
    # modules (upstream-issues.md UP-006; CnaEngineGame.cmake builds CNA
    # statically); the runtime module alone then brings what the process needs.
    if(CNA_SHARED_LIBRARY)
        set(cna CNA::Runtime)
    else()
        set(cna CNA)
    endif()
    target_link_libraries(${target} PRIVATE CnaEngine::App CnaEngine::Audio CnaEngine::UI ${cna} ${GAME_LIBRARIES})
    if(GAME_PROFILE STREQUAL "enhanced")
        if(NOT TARGET CnaEngine::EnhancedGraphics)
            message(FATAL_ERROR
                "cna_engine_add_game(${target}): PROFILE enhanced needs CNA_ENGINE_WITH_ENHANCED_GRAPHICS=ON")
        endif()
        target_link_libraries(${target} PRIVATE CnaEngine::EnhancedGraphics)
    endif()
    if(TARGET SDL3::SDL3main)
        target_link_libraries(${target} PRIVATE SDL3::SDL3main)
    endif()

    if(EMSCRIPTEN)
        set(shell "${GAME_WEB_SHELL}")
        if(shell)
            get_filename_component(shell "${shell}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        else()
            set(CNA_ENGINE_GAME_TITLE "${GAME_TITLE}")
            set(CNA_ENGINE_GAME_HINT "${GAME_HINT}")
            set(shell "${CMAKE_CURRENT_BINARY_DIR}/${target}.shell.html")
            configure_file("${cmake_dir}/web/shell.html" "${shell}" @ONLY)
        endif()
        target_link_libraries(${target} PRIVATE CNA::EmscriptenExceptionAbi CNA::EmscriptenAsyncify)
        target_link_options(${target} PRIVATE
            -sALLOW_MEMORY_GROWTH=1
            -sSTACK_SIZE=1048576
            -sINITIAL_MEMORY=268435456
            -sFORCE_FILESYSTEM=1
            -sEXIT_RUNTIME=1
            "SHELL:--shell-file ${shell}")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${shell}")
        if(assets)
            # The working directory in the browser is "/", so the assets are
            # found the same way as beside a desktop executable.
            target_link_options(${target} PRIVATE "SHELL:--preload-file ${assets}@/${GAME_ASSETS_NAME}")
        endif()
        set_target_properties(${target} PROPERTIES SUFFIX ".html")
        cna_apply_emscripten_renderer_link_contract(${target})
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -DGAME=${target} "-DFROM=$<TARGET_FILE_DIR:${target}>"
                    "-DTO=${GAME_WEB_PACKAGE}" -P "${cmake_dir}/CnaEngineWebPackage.cmake"
            COMMENT "Gathering ${target}'s browser files in ${GAME_WEB_PACKAGE}"
            VERBATIM)
    else()
        if(NOT WIN32 AND NOT APPLE)
            set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
        endif()
        if(assets)
            # Only changed files are written: a rebuild should not rewrite every asset.
            if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
                set(copy copy_directory_if_different)
            else()
                set(copy copy_directory)
            endif()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E ${copy} "${assets}" "$<TARGET_FILE_DIR:${target}>/${GAME_ASSETS_NAME}"
                COMMENT "Copying ${target}'s assets beside it"
                VERBATIM)
        endif()
        if(COMMAND cna_copy_renderer_runtime)
            cna_copy_renderer_runtime(${target})
        endif()
        if(WIN32 AND COMMAND cna_copy_sdl_runtime)
            cna_copy_sdl_runtime(${target})
        endif()
    endif()
endfunction()
