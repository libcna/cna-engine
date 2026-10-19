// SPDX-License-Identifier: MIT
#include "CnaEngine/Assets/AssetRegistry.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace CnaEngine::Assets
{
    namespace
    {
        struct Mesh
        {
            std::string source;
        };
        struct Sound
        {
            int id = 0;
        };
    }

    TEST(AssetRegistryTests, LoadsOnceAndCachesPerTypeAndKey)
    {
        AssetRegistry registry;
        int loads = 0;
        registry.RegisterLoader<Mesh>([&](const std::string& key) {
            ++loads;
            return std::make_shared<Mesh>(Mesh{key});
        });
        registry.RegisterLoader<Sound>([](const std::string&) { return std::make_shared<Sound>(Sound{7}); });

        const auto a = registry.Get<Mesh>("models\\car");
        const auto b = registry.Get<Mesh>("./models/car");
        EXPECT_EQ(a, b) << "keys are normalized";
        EXPECT_EQ(a->source, "models/car");
        EXPECT_EQ(loads, 1);
        EXPECT_EQ(registry.Get<Sound>("models/car")->id, 7) << "same key, different type, different entry";
        EXPECT_EQ(registry.Size(), 2u);
        EXPECT_EQ(registry.Statistics().hits, 1u);
        EXPECT_EQ(registry.Statistics().loads, 2u);
        EXPECT_TRUE(registry.Contains<Mesh>("models/car"));
        EXPECT_FALSE(registry.Contains<Mesh>("Models/Car")) << "keys are case-sensitive";
    }

    TEST(AssetRegistryTests, InvalidateDropsOnlyTheRegistryReference)
    {
        AssetRegistry registry;
        int loads = 0;
        registry.RegisterLoader<Mesh>([&](const std::string& key) {
            ++loads;
            return std::make_shared<Mesh>(Mesh{key + std::to_string(loads)});
        });
        const auto held = registry.Get<Mesh>("tree");
        EXPECT_TRUE(registry.Invalidate<Mesh>("tree"));
        EXPECT_FALSE(registry.Invalidate<Mesh>("tree"));
        EXPECT_EQ(held->source, "tree1") << "existing holders keep a valid object";
        EXPECT_EQ(registry.Get<Mesh>("tree")->source, "tree2") << "the next Get reloads";
        registry.InvalidateAll();
        EXPECT_EQ(registry.Size(), 0u);
    }

    TEST(AssetRegistryTests, FailuresThrowOrReturnNullWithoutFallback)
    {
        AssetRegistry registry;
        registry.RegisterLoader<Mesh>([](const std::string& key) -> std::shared_ptr<Mesh> {
            if (key == "null") {
                return nullptr;
            }
            throw std::runtime_error("file not found");
        });
        EXPECT_THROW((void)registry.Get<Mesh>("missing"), std::runtime_error);
        EXPECT_EQ(registry.TryGet<Mesh>("null"), nullptr);
        EXPECT_EQ(registry.TryGet<Sound>("anything"), nullptr) << "no loader for the type";
        ASSERT_EQ(registry.Failures().size(), 3u);
        EXPECT_EQ(registry.Failures()[0].key, "missing");
        EXPECT_EQ(registry.Failures()[0].message, "file not found");
        EXPECT_EQ(registry.Failures()[1].message, "loader returned no asset");
        EXPECT_EQ(registry.Failures()[2].message, "no loader registered for this type");
        EXPECT_FALSE(registry.Contains<Mesh>("missing")) << "failures are not cached without a fallback";
        EXPECT_EQ(registry.Statistics().failures, 3u);
        registry.ClearFailures();
        EXPECT_TRUE(registry.Failures().empty());
    }

    TEST(AssetRegistryTests, FallbackIsReturnedAndCachedForTheFailedKey)
    {
        AssetRegistry registry;
        int attempts = 0;
        registry.RegisterLoader<Mesh>([&](const std::string&) -> std::shared_ptr<Mesh> {
            ++attempts;
            throw std::runtime_error("corrupt");
        });
        const auto placeholder = std::make_shared<Mesh>(Mesh{"placeholder"});
        registry.SetFallback<Mesh>(placeholder);

        EXPECT_EQ(registry.Get<Mesh>("broken"), placeholder);
        EXPECT_EQ(registry.Get<Mesh>("broken"), placeholder);
        EXPECT_EQ(attempts, 1) << "a failed key is not retried until invalidated";
        ASSERT_EQ(registry.Failures().size(), 1u);
        EXPECT_TRUE(registry.Failures()[0].usedFallback);
        EXPECT_EQ(registry.Statistics().fallbacks, 1u);

        registry.Invalidate<Mesh>("broken");
        (void)registry.Get<Mesh>("broken");
        EXPECT_EQ(attempts, 2);
    }

    TEST(AssetRegistryTests, ContentManagerLoaderRequiresAManager)
    {
        AssetRegistry registry;
        EXPECT_THROW(registry.UseContentManager<Microsoft::Xna::Framework::Graphics::Texture2D>(), std::logic_error);
    }

    TEST(AssetRegistryTests, ExactKeysForGeneratedAssets)
    {
        AssetRegistry registry;
        registry.SetNormalizeKeys(false);
        registry.RegisterLoader<Mesh>([](const std::string& key) { return std::make_shared<Mesh>(Mesh{key}); });
        EXPECT_NE(registry.Get<Mesh>("./A"), registry.Get<Mesh>("A")) << "text keys are taken literally";
        EXPECT_EQ(registry.Get<Mesh>("a\\b")->source, "a\\b");
        EXPECT_THROW(registry.SetNormalizeKeys(true), std::logic_error);
        registry.InvalidateAll();
        EXPECT_NO_THROW(registry.SetNormalizeKeys(true));
    }

    TEST(AssetRegistryTests, GetOrCreateKeepsTheFirstDefinition)
    {
        AssetRegistry registry;
        int created = 0;
        const auto first = registry.GetOrCreate<Mesh>("window", [&] {
            ++created;
            return std::make_shared<Mesh>(Mesh{"first recipe"});
        });
        const auto second = registry.GetOrCreate<Mesh>("window", [&] {
            ++created;
            return std::make_shared<Mesh>(Mesh{"second recipe"});
        });
        EXPECT_EQ(first, second);
        EXPECT_EQ(second->source, "first recipe");
        EXPECT_EQ(created, 1);
        EXPECT_EQ(registry.Get<Mesh>("window"), first) << "cached like any other asset";
        EXPECT_THROW((void)registry.GetOrCreate<Sound>("x", []() -> std::shared_ptr<Sound> { return nullptr; }),
                     std::runtime_error);
    }
}
