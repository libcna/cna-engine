// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace CnaEngine::App
{
    /// Where a game's asset directory is looked for, in order: the
    /// executable's directory (where cna_engine_add_game() copies the assets),
    /// then the working directory (a browser build preloads them under "/",
    /// its working directory).
    [[nodiscard]] std::vector<std::filesystem::path> DefaultAssetSearchRoots();

    /// The first `<root>/<name>` that is a directory, as an absolute path;
    /// nothing when none is. Backrooms-, school- and train-simulator each look
    /// their assets up this way.
    [[nodiscard]] std::optional<std::filesystem::path> FindAssetDirectory(
        std::string_view name, std::span<const std::filesystem::path> roots);

    /// FindAssetDirectory over DefaultAssetSearchRoots().
    [[nodiscard]] std::optional<std::filesystem::path> FindAssetDirectory(std::string_view name);
}
