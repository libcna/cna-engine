// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Quality.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;

    TEST(QualityTests, IntentCapabilityAndResultStaySeparate)
    {
        QualityIntent intent;
        intent.preset = QualityPreset::Ultra;
        intent.drawDistance = 750.0f;

        const auto hiDef = Resolve(intent, DeviceCapabilities::ForProfile(GraphicsProfile::HiDef));
        EXPECT_EQ(hiDef.preset, QualityPreset::Ultra);
        EXPECT_FLOAT_EQ(hiDef.drawDistance, 750.0f) << "override wins over the preset";
        EXPECT_EQ(hiDef.shadowMapSize, 4096);
        EXPECT_EQ(hiDef.multiSampleCount, 4);
        EXPECT_TRUE(hiDef.decisions.empty()) << "nothing was limited";

        const auto reach = Resolve(intent, DeviceCapabilities::ForProfile(GraphicsProfile::Reach));
        EXPECT_EQ(reach.shadowMapSize, 0);
        EXPECT_EQ(reach.multiSampleCount, 0);
        EXPECT_EQ(reach.maxTextureSize, 2048);
        ASSERT_EQ(reach.decisions.size(), 3u);
        EXPECT_EQ(reach.decisions[0].setting, "shadowMapSize");
        EXPECT_EQ(reach.decisions[0].requested, "4096");
        EXPECT_EQ(reach.decisions[0].resolved, "0");
        EXPECT_NE(reach.decisions[0].reason.find("Reach"), std::string::npos);
        EXPECT_EQ(intent.preset, QualityPreset::Ultra) << "the intent is never rewritten";
    }

    TEST(QualityTests, CapabilitiesComeFromTheDeviceProfile)
    {
        GraphicsDevice device;
        EXPECT_EQ(DeviceCapabilities::FromDevice(device).maxTextureSize, 2048);
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        const auto caps = DeviceCapabilities::FromDevice(device);
        EXPECT_EQ(caps.profile, GraphicsProfile::HiDef);
        EXPECT_EQ(caps.maxTextureSize, 4096);
        EXPECT_TRUE(caps.supportsShadowMaps);
    }

    TEST(QualityTests, PresetsAreMonotonic)
    {
        const auto low = PresetDefaults(QualityPreset::Low);
        const auto ultra = PresetDefaults(QualityPreset::Ultra);
        EXPECT_LT(low.drawDistance, ultra.drawDistance);
        EXPECT_LT(low.maxTextureSize, ultra.maxTextureSize);
        EXPECT_EQ(low.shadowMapSize, 0);
    }

    TEST(QualityTests, SaveAndLoadRoundTripOnlyOverrides)
    {
        QualityIntent intent;
        intent.preset = QualityPreset::Medium;
        intent.lodBias = 1.25f;
        intent.shadowMapSize = 0;
        const std::string text = SaveQualityIntent(intent);
        EXPECT_EQ(text, "# cna-engine quality v1\npreset=Medium\nlodBias=1.25\nshadowMapSize=0\n");
        const auto loaded = LoadQualityIntent(text);
        EXPECT_TRUE(loaded.warnings.empty());
        EXPECT_EQ(loaded.intent.preset, QualityPreset::Medium);
        EXPECT_EQ(loaded.intent.lodBias, 1.25f);
        EXPECT_EQ(loaded.intent.shadowMapSize, 0);
        EXPECT_FALSE(loaded.intent.drawDistance.has_value());
    }

    TEST(QualityTests, InvalidValuesAreReportedNotApplied)
    {
        const auto loaded = LoadQualityIntent("preset=Insane\nshadowMapSize=1000\nmultiSampleCount=3\n"
                                              "drawDistance=-5\nmaxTextureSize=4096\nvsync=on\nbroken\n");
        EXPECT_EQ(loaded.warnings.size(), 6u);
        EXPECT_EQ(loaded.intent.preset, QualityPreset::High) << "default kept";
        EXPECT_FALSE(loaded.intent.shadowMapSize.has_value());
        EXPECT_EQ(loaded.intent.maxTextureSize, 4096);
    }
}
