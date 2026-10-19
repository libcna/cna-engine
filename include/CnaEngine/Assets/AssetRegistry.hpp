// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace CnaEngine::Assets
{
    /// A load that failed, kept for diagnostics.
    struct AssetFailure
    {
        std::string type;
        std::string key;
        std::string message;
        /// True when a registered fallback was returned instead.
        bool usedFallback = false;
    };

    struct AssetRegistryStatistics
    {
        std::uint64_t hits = 0;
        std::uint64_t loads = 0;
        std::uint64_t failures = 0;
        std::uint64_t fallbacks = 0;
    };

    /// Typed cache of shared assets keyed by (type, normalized key).
    ///
    /// Contract:
    ///  - Keys are normalized: backslashes become '/', and leading "./" is
    ///    removed. Keys are case-sensitive (file systems are).
    ///  - A type is loaded by the loader registered for it. A registry built
    ///    with a ContentManager can register ContentManager-backed loaders
    ///    with UseContentManager<T>(); the ContentManager stays the owner of
    ///    what it loads (the registry only holds a shared handle to a copy of
    ///    the XNA value, which shares the underlying resource).
    ///  - Get() returns the cached asset, loading it on first use. When the
    ///    loader throws or returns null, the failure is recorded; the type's
    ///    fallback is returned (and cached for that key) when one is set,
    ///    otherwise std::runtime_error is thrown. TryGet() returns null instead
    ///    of throwing.
    ///  - Invalidate() drops the registry's reference only; holders of the
    ///    shared_ptr keep a valid object. The next Get() reloads.
    ///  - Not thread-safe; use from the game thread.
    ///
    /// The registry never parses file formats itself: format readers stay in
    /// CNA's content pipeline and cna-extended.
    class AssetRegistry
    {
    public:
        template <typename T>
        using Loader = std::function<std::shared_ptr<T>(const std::string& key)>;

        AssetRegistry() = default;
        explicit AssetRegistry(Microsoft::Xna::Framework::Content::ContentManager& content) : content_(&content) {}

        template <typename T>
        void RegisterLoader(Loader<T> loader)
        {
            loaders_[std::type_index(typeid(T))] = [loader = std::move(loader)](const std::string& key) -> std::shared_ptr<void> {
                return loader(key);
            };
        }

        /// Loads T through the ContentManager given at construction.
        template <typename T>
        void UseContentManager()
        {
            if (content_ == nullptr) {
                throw std::logic_error("AssetRegistry::UseContentManager: no ContentManager was given");
            }
            auto* content = content_;
            RegisterLoader<T>([content](const std::string& key) { return std::make_shared<T>(content->Load<T>(key)); });
        }

        /// Asset returned (and cached under the failed key) when loading T fails.
        template <typename T>
        void SetFallback(std::shared_ptr<T> fallback)
        {
            fallbacks_[std::type_index(typeid(T))] = std::move(fallback);
        }

        template <typename T>
        [[nodiscard]] std::shared_ptr<T> Get(std::string_view key)
        {
            return std::static_pointer_cast<T>(GetErased(std::type_index(typeid(T)), typeid(T).name(), key, true));
        }

        template <typename T>
        [[nodiscard]] std::shared_ptr<T> TryGet(std::string_view key)
        {
            return std::static_pointer_cast<T>(GetErased(std::type_index(typeid(T)), typeid(T).name(), key, false));
        }

        template <typename T>
        [[nodiscard]] bool Contains(std::string_view key) const
        {
            return entries_.contains(EntryKey{std::type_index(typeid(T)), NormalizeKey(key)});
        }

        template <typename T>
        bool Invalidate(std::string_view key)
        {
            return entries_.erase(EntryKey{std::type_index(typeid(T)), NormalizeKey(key)}) > 0;
        }

        void InvalidateAll() noexcept { entries_.clear(); }
        [[nodiscard]] std::size_t Size() const noexcept { return entries_.size(); }

        [[nodiscard]] const std::vector<AssetFailure>& Failures() const noexcept { return failures_; }
        void ClearFailures() noexcept { failures_.clear(); }
        [[nodiscard]] const AssetRegistryStatistics& Statistics() const noexcept { return statistics_; }

        [[nodiscard]] static std::string NormalizeKey(std::string_view key);

    private:
        struct EntryKey
        {
            std::type_index type;
            std::string key;
            bool operator==(const EntryKey&) const = default;
        };
        struct EntryKeyHash
        {
            std::size_t operator()(const EntryKey& k) const noexcept
            {
                return std::hash<std::type_index>{}(k.type) * 31u ^ std::hash<std::string>{}(k.key);
            }
        };
        struct Entry
        {
            std::shared_ptr<void> asset;
            bool isFallback = false;
        };

        std::shared_ptr<void> GetErased(std::type_index type, const char* typeName, std::string_view key, bool throwOnFailure);

        Microsoft::Xna::Framework::Content::ContentManager* content_ = nullptr;
        std::unordered_map<std::type_index, std::function<std::shared_ptr<void>(const std::string&)>> loaders_;
        std::unordered_map<std::type_index, std::shared_ptr<void>> fallbacks_;
        std::unordered_map<EntryKey, Entry, EntryKeyHash> entries_;
        std::vector<AssetFailure> failures_;
        AssetRegistryStatistics statistics_;
    };
}
