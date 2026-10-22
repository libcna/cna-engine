// SPDX-License-Identifier: MIT
#include "CnaEngine/App/AssetDirectory.hpp"

#include "System/AppContext.hpp"

#include <system_error>

namespace CnaEngine::App
{
    std::vector<std::filesystem::path> DefaultAssetSearchRoots()
    {
        std::vector<std::filesystem::path> roots;
        const std::string base = System::AppContext::getBaseDirectoryProperty();
        if (!base.empty()) {
            roots.emplace_back(base);
        }
        std::error_code error;
        const auto current = std::filesystem::current_path(error);
        if (!error) {
            roots.push_back(current);
        }
        return roots;
    }

    std::optional<std::filesystem::path> FindAssetDirectory(const std::string_view name,
                                                            const std::span<const std::filesystem::path> roots)
    {
        for (const auto& root : roots) {
            const auto candidate = root / std::filesystem::path(name);
            std::error_code error;
            if (std::filesystem::is_directory(candidate, error)) {
                const auto absolute = std::filesystem::absolute(candidate, error);
                return (error ? candidate : absolute).lexically_normal();
            }
        }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> FindAssetDirectory(const std::string_view name)
    {
        const auto roots = DefaultAssetSearchRoots();
        return FindAssetDirectory(name, roots);
    }
}
