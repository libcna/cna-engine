// SPDX-License-Identifier: MIT
#include "CnaEngine/Assets/AssetRegistry.hpp"

#include <exception>
#include <stdexcept>

namespace CnaEngine::Assets
{
    std::string AssetRegistry::NormalizeKey(const std::string_view key)
    {
        std::string normalized(key);
        for (char& c : normalized) {
            if (c == '\\') {
                c = '/';
            }
        }
        while (normalized.starts_with("./")) {
            normalized.erase(0, 2);
        }
        return normalized;
    }

    void AssetRegistry::SetNormalizeKeys(const bool normalize)
    {
        if (!entries_.empty() && normalize != normalizeKeys_) {
            throw std::logic_error("AssetRegistry::SetNormalizeKeys: the registry already holds assets");
        }
        normalizeKeys_ = normalize;
    }

    std::shared_ptr<void> AssetRegistry::GetErased(const std::type_index type, const char* typeName,
                                                   const std::string_view key, const bool throwOnFailure)
    {
        EntryKey entryKey{type, KeyFor(key)};
        if (const auto it = entries_.find(entryKey); it != entries_.end()) {
            ++statistics_.hits;
            return it->second.asset;
        }

        std::string error;
        std::shared_ptr<void> asset;
        if (const auto loader = loaders_.find(type); loader == loaders_.end()) {
            error = "no loader registered for this type";
        } else {
            ++statistics_.loads;
            try {
                asset = loader->second(entryKey.key);
                if (!asset) {
                    error = "loader returned no asset";
                }
            } catch (const std::exception& e) {
                error = e.what();
            } catch (...) {
                error = "unknown exception";
            }
        }

        if (asset) {
            entries_.emplace(std::move(entryKey), Entry{asset, false});
            return asset;
        }

        ++statistics_.failures;
        const auto fallback = fallbacks_.find(type);
        const bool useFallback = fallback != fallbacks_.end() && fallback->second;
        failures_.push_back(AssetFailure{typeName, entryKey.key, error, useFallback});
        if (useFallback) {
            ++statistics_.fallbacks;
            entries_.emplace(std::move(entryKey), Entry{fallback->second, true});
            return fallback->second;
        }
        if (throwOnFailure) {
            throw std::runtime_error("AssetRegistry: cannot load '" + failures_.back().key + "': " + error);
        }
        return nullptr;
    }
}
