// SPDX-License-Identifier: MIT
//
// ENG-036 probe: car-simulator's quality tiers (QualityTier.cpp, compiled
// read-only) expressed as an engine QualitySchema with no value changed.
#include "CnaEngine/Graphics/QualitySchema.hpp"

#include "CarSim/Render/QualityTier.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Pilot
{
    using Graphics::QualitySchema;
    using Graphics::QualityValue;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;

    TEST(CarSimulatorQualityTests, TiersMapWithoutSilentChanges)
    {
        using CarSim::Render::QualityTier;
        const CarSim::Render::QualitySettings defaults;
        QualitySchema schema;
        schema.Declare({"drawDistanceScale", defaults.drawDistanceScale, 0.1, 4.0, {}});
        schema.Declare({"mirrorDistanceM", defaults.mirrorDistanceM, 0.0, 5000.0, {}});
        schema.Declare({"mirrorUpdateEvery", defaults.mirrorUpdateEvery, 1.0, 16.0, {}});
        schema.Declare({"vegetationScale", defaults.vegetationScale, 0.0, 4.0, {}});
        for (const QualityTier tier : {QualityTier::Low, QualityTier::Medium, QualityTier::High}) {
            const auto s = CarSim::Render::SettingsFor(tier);
            schema.DefinePreset(CarSim::Render::ToString(tier),
                                {{"drawDistanceScale", s.drawDistanceScale},
                                 {"mirrorDistanceM", s.mirrorDistanceM},
                                 {"mirrorUpdateEvery", s.mirrorUpdateEvery},
                                 {"vegetationScale", s.vegetationScale}});
        }
        for (const auto profile : {GraphicsProfile::Reach, GraphicsProfile::HiDef}) {
            for (const QualityTier tier : {QualityTier::Low, QualityTier::Medium, QualityTier::High}) {
                const auto expected = CarSim::Render::SettingsFor(tier);
                const auto resolved = schema.Resolve({CarSim::Render::ToString(tier), {}},
                                                     Graphics::DeviceCapabilities::ForProfile(profile));
                EXPECT_EQ(resolved.Float("drawDistanceScale"), expected.drawDistanceScale);
                EXPECT_EQ(resolved.Float("mirrorDistanceM"), expected.mirrorDistanceM);
                EXPECT_EQ(resolved.Int("mirrorUpdateEvery"), expected.mirrorUpdateEvery);
                EXPECT_EQ(resolved.Float("vegetationScale"), expected.vegetationScale);
                EXPECT_TRUE(resolved.decisions.empty()) << "car-simulator's tiers have no device-limited settings";
            }
        }
        // The pilot's tier names are the schema's preset names.
        CarSim::Render::QualityTier parsed{};
        for (const auto& name : schema.PresetNames()) {
            EXPECT_TRUE(CarSim::Render::QualityFromName(name, parsed)) << name;
        }
    }
}
