// SPDX-License-Identifier: MIT
//
// PILOT-001 / ENG-024 probe: Street's derived-material cache
// (MaterialLibrary::derive / deriveTinted: one material per name, first
// definition wins) expressed with the engine AssetRegistry::GetOrCreate over
// Street's own base materials (MaterialLibrary compiled read-only; build() is
// not called, so no textures are generated).
#include "CnaEngine/Assets/AssetRegistry.hpp"

#include "CnaStreet/Render/MaterialLibrary.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace CnaEngine::Pilot
{
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;

    TEST(StreetAssetsTests, DerivedMaterialCacheExpressedWithAssetRegistry)
    {
        CnaStreet::MaterialLibrary library(nullptr);
        Assets::AssetRegistry materials;
        materials.SetNormalizeKeys(false); // material names, not paths

        auto derive = [&](const std::string& name, CnaStreet::MaterialId base, Vector2 scale, Vector2 offset) {
            return materials.GetOrCreate<CnaStreet::Material>(name, [&] {
                auto material = std::make_shared<CnaStreet::Material>(library.get(base));
                material->name = name;
                material->uvScale = scale;
                material->uvOffset = offset;
                return material;
            });
        };

        const auto base = CnaStreet::MaterialId::AsphaltMain;
        const auto engineFirst = derive("shop-12", base, Vector2(0.25f, 0.5f), Vector2(0.5f, 0.0f));
        const auto engineAgain = derive("shop-12", base, Vector2(9.0f, 9.0f), Vector2(9.0f, 9.0f));
        const CnaStreet::Material* streetFirst = library.derive("shop-12", base, Vector2(0.25f, 0.5f), Vector2(0.5f, 0.0f));
        const CnaStreet::Material* streetAgain = library.derive("shop-12", base, Vector2(9.0f, 9.0f), Vector2(9.0f, 9.0f));

        // Same identity rule: the first definition of a name wins.
        EXPECT_EQ(streetFirst, streetAgain);
        EXPECT_EQ(engineFirst, engineAgain);
        // Same content.
        EXPECT_EQ(engineFirst->name, streetFirst->name);
        EXPECT_EQ(engineFirst->uvScale, streetFirst->uvScale);
        EXPECT_EQ(engineFirst->uvOffset, streetFirst->uvOffset);
        EXPECT_EQ(engineFirst->roughness, streetFirst->roughness);
        EXPECT_EQ(engineFirst->metallic, streetFirst->metallic);
        EXPECT_EQ(engineFirst->baseColour, streetFirst->baseColour);
        EXPECT_EQ(materials.Statistics().loads, 1u);
        EXPECT_EQ(materials.Statistics().hits, 1u);

        // deriveTinted shares the same name space in Street; so does the registry.
        const CnaStreet::Material* tinted = library.deriveTinted("shop-12", base, Vector3(1.0f, 0.0f, 0.0f));
        EXPECT_EQ(tinted, streetFirst) << "Street returns the existing material";
        EXPECT_TRUE(materials.Contains<CnaStreet::Material>("shop-12"));
    }
}
