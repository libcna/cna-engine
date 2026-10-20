// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/QualitySchema.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;

    namespace
    {
        QualitySchema MakeSchema()
        {
            QualitySchema schema;
            schema.Declare({"bloom", true, {}, {}, {}});
            schema.Declare({"crowdDensity", 1.0f, 0.0, 2.0, {}});
            schema.Declare({"msaa", 0, 0.0, 8.0,
                            [](const QualityValue& requested, const DeviceCapabilities& caps, std::string& reason) {
                                const int value = std::get<int>(requested);
                                if (value > caps.maxMultiSampleCount) {
                                    reason = "device supports at most " + std::to_string(caps.maxMultiSampleCount);
                                    return QualityValue(caps.maxMultiSampleCount);
                                }
                                return requested;
                            }});
            schema.DefinePreset("low", {{"bloom", false}, {"crowdDensity", 0.25f}});
            schema.DefinePreset("high", {{"msaa", 4}});
            return schema;
        }
    }

    TEST(QualitySchemaTests, DefaultsPresetsOverridesThenLimits)
    {
        const QualitySchema schema = MakeSchema();
        EXPECT_EQ(schema.PresetNames(), (std::vector<std::string>{"low", "high"}));
        const auto hiDef = DeviceCapabilities::ForProfile(GraphicsProfile::HiDef);

        const auto low = schema.Resolve({"low", {}}, hiDef);
        EXPECT_FALSE(low.Bool("bloom"));
        EXPECT_FLOAT_EQ(low.Float("crowdDensity"), 0.25f);
        EXPECT_EQ(low.Int("msaa"), 0) << "unlisted settings keep their default";

        const auto high = schema.Resolve({"high", {{"crowdDensity", 1.5f}}}, hiDef);
        EXPECT_TRUE(high.Bool("bloom"));
        EXPECT_FLOAT_EQ(high.Float("crowdDensity"), 1.5f);
        EXPECT_EQ(high.Int("msaa"), 4);
        EXPECT_TRUE(high.decisions.empty());

        const auto reach = schema.Resolve({"high", {}}, DeviceCapabilities::ForProfile(GraphicsProfile::Reach));
        EXPECT_EQ(reach.Int("msaa"), 0);
        ASSERT_EQ(reach.decisions.size(), 1u);
        EXPECT_EQ(reach.decisions[0].setting, "msaa");
        EXPECT_EQ(reach.decisions[0].requested, "4");
        EXPECT_EQ(reach.decisions[0].resolved, "0");
        EXPECT_EQ(reach.decisions[0].reason, "device supports at most 0");
    }

    TEST(QualitySchemaTests, PresetValuesAreOnlyWhatThePresetLists)
    {
        const QualitySchema schema = MakeSchema();
        const auto* low = schema.PresetValues("low");
        ASSERT_NE(low, nullptr);
        EXPECT_EQ(low->size(), 2u);
        EXPECT_EQ(std::get<bool>(low->at("bloom")), false);
        EXPECT_EQ(std::get<float>(low->at("crowdDensity")), 0.25f);
        EXPECT_FALSE(low->contains("msaa")) << "left out: the overlay keeps the current value";
        EXPECT_EQ(schema.PresetValues("ultra"), nullptr);
    }

    TEST(QualitySchemaTests, InvalidDefinitionsAreRejected)
    {
        QualitySchema schema = MakeSchema();
        EXPECT_THROW(schema.Declare({"bloom", false, {}, {}, {}}), std::invalid_argument) << "duplicate";
        EXPECT_THROW(schema.Declare({"gamma", 5.0f, 0.0, 3.0, {}}), std::invalid_argument) << "default out of range";
        EXPECT_THROW(schema.DefinePreset("bad", {{"bloom", 1}}), std::invalid_argument) << "wrong type";
        EXPECT_THROW(schema.DefinePreset("bad", {{"crowdDensity", 3.0f}}), std::invalid_argument) << "range";
        EXPECT_THROW(schema.DefinePreset("bad", {{"nope", true}}), std::invalid_argument) << "unknown";
        EXPECT_THROW((void)schema.Resolve({"ultra", {}}, DeviceCapabilities{}), std::invalid_argument);
        EXPECT_THROW((void)schema.Resolve({"low", {{"msaa", 2.0f}}}, DeviceCapabilities{}), std::invalid_argument);
    }

    TEST(QualitySchemaTests, SelectionsPersistAndBadLinesAreReported)
    {
        const QualitySchema schema = MakeSchema();
        const QualitySelection selection{"low", {{"crowdDensity", 0.5f}, {"msaa", 2}}};
        const std::string text = schema.SaveSelection(selection);
        EXPECT_EQ(text, "# cna-engine quality selection v1\npreset=low\ncrowdDensity=0.5\nmsaa=2\n");
        const auto loaded = schema.LoadSelection(text, "high");
        EXPECT_TRUE(loaded.warnings.empty());
        EXPECT_EQ(loaded.selection.preset, "low");
        EXPECT_EQ(loaded.selection.overrides, selection.overrides);

        const auto bad = schema.LoadSelection("preset=ultra\nbloom=maybe\ncrowdDensity=9\nfoo=1\nmsaa=4\n", "high");
        EXPECT_EQ(bad.warnings.size(), 4u);
        EXPECT_EQ(bad.selection.preset, "high");
        EXPECT_EQ(bad.selection.overrides.size(), 1u);
        EXPECT_EQ(std::get<int>(bad.selection.overrides.at("msaa")), 4);
    }
}
