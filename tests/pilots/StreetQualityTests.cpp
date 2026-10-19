// SPDX-License-Identifier: MIT
//
// ENG-036 probe: street-simulator's four quality presets
// (RenderSettings::applyPreset, compiled read-only) expressed as an engine
// QualitySchema. Every setting the presets touch is declared; resolution on a
// capable device changes nothing, and on a limited device every change is
// recorded as a decision.
#include "CnaEngine/Graphics/QualitySchema.hpp"

#include "CnaStreet/Render/RenderSettings.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace CnaEngine::Pilot
{
    using Graphics::QualitySchema;
    using Graphics::QualityValue;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;

    namespace
    {
        // The settings applyPreset() writes, with a pointer into Street's own
        // struct so values are read from Street's code, never retyped here.
        template <typename T>
        struct Field
        {
            const char* name;
            T CnaStreet::RenderSettings::*member;
        };

        const std::vector<Field<bool>> Bools = {
            {"shadows", &CnaStreet::RenderSettings::shadows},   {"hdr", &CnaStreet::RenderSettings::hdr},
            {"bloom", &CnaStreet::RenderSettings::bloom},       {"ssao", &CnaStreet::RenderSettings::ssao},
            {"fxaa", &CnaStreet::RenderSettings::fxaa},         {"heightFog", &CnaStreet::RenderSettings::heightFog},
            {"lightShafts", &CnaStreet::RenderSettings::lightShafts}, {"ssr", &CnaStreet::RenderSettings::ssr},
            {"depthOfField", &CnaStreet::RenderSettings::depthOfField}, {"clouds", &CnaStreet::RenderSettings::clouds},
            {"reflectionProbes", &CnaStreet::RenderSettings::reflectionProbes},
        };
        const std::vector<Field<int>> Ints = {
            {"multiSample", &CnaStreet::RenderSettings::multiSample},
            {"shadowCascades", &CnaStreet::RenderSettings::shadowCascades},
            {"shadowQuality", &CnaStreet::RenderSettings::shadowQuality},
            {"probeFaceSize", &CnaStreet::RenderSettings::probeFaceSize},
        };
        const std::vector<Field<float>> Floats = {
            {"renderScale", &CnaStreet::RenderSettings::renderScale},
            {"shadowDistance", &CnaStreet::RenderSettings::shadowDistance},
            {"propShadowDistance", &CnaStreet::RenderSettings::propShadowDistance},
            {"architectureShadowDistance", &CnaStreet::RenderSettings::architectureShadowDistance},
            {"contextShadowDistance", &CnaStreet::RenderSettings::contextShadowDistance},
            {"probeSpacing", &CnaStreet::RenderSettings::probeSpacing},
            {"propCullDistance", &CnaStreet::RenderSettings::propCullDistance},
            {"pedestrianCullDistance", &CnaStreet::RenderSettings::pedestrianCullDistance},
            {"pedestrianDetailDistance", &CnaStreet::RenderSettings::pedestrianDetailDistance},
            {"pedestrianShadowDistance", &CnaStreet::RenderSettings::pedestrianShadowDistance},
            {"ssaoIntensity", &CnaStreet::RenderSettings::ssaoIntensity},
        };

        constexpr CnaStreet::QualityPreset Presets[] = {CnaStreet::QualityPreset::Low, CnaStreet::QualityPreset::Medium,
                                                        CnaStreet::QualityPreset::High, CnaStreet::QualityPreset::Ultra};

        QualitySchema StreetSchema()
        {
            const CnaStreet::RenderSettings defaults;
            QualitySchema schema;
            for (const auto& f : Bools) {
                schema.Declare({f.name, defaults.*f.member, {}, {}, {}});
            }
            for (const auto& f : Ints) {
                Graphics::QualitySetting setting{f.name, defaults.*f.member, 0.0, 4096.0, {}};
                if (std::string(f.name) == "multiSample") {
                    // The one setting the device can veto.
                    setting.limit = [](const QualityValue& requested, const Graphics::DeviceCapabilities& caps,
                                       std::string& reason) {
                        if (std::get<int>(requested) > caps.maxMultiSampleCount) {
                            reason = "multisampling limited by the device";
                            return QualityValue(caps.maxMultiSampleCount);
                        }
                        return requested;
                    };
                }
                schema.Declare(std::move(setting));
            }
            for (const auto& f : Floats) {
                schema.Declare({f.name, defaults.*f.member, 0.0, 10000.0, {}});
            }
            for (const auto preset : Presets) {
                CnaStreet::RenderSettings applied;
                applied.applyPreset(preset);
                std::map<std::string, QualityValue> values;
                for (const auto& f : Bools) {
                    values[f.name] = applied.*f.member;
                }
                for (const auto& f : Ints) {
                    values[f.name] = applied.*f.member;
                }
                for (const auto& f : Floats) {
                    values[f.name] = applied.*f.member;
                }
                schema.DefinePreset(CnaStreet::RenderSettings::presetName(preset), std::move(values));
            }
            return schema;
        }
    }

    TEST(StreetQualityTests, PresetsMapWithoutSilentChanges)
    {
        const QualitySchema schema = StreetSchema();
        EXPECT_EQ(schema.Settings().size(), Bools.size() + Ints.size() + Floats.size());
        const auto hiDef = Graphics::DeviceCapabilities::ForProfile(GraphicsProfile::HiDef);
        for (const auto preset : Presets) {
            CnaStreet::RenderSettings expected;
            expected.applyPreset(preset);
            const auto resolved = schema.Resolve({std::string(CnaStreet::RenderSettings::presetName(preset)), {}}, hiDef);
            EXPECT_TRUE(resolved.decisions.empty()) << CnaStreet::RenderSettings::presetName(preset);
            for (const auto& f : Bools) {
                EXPECT_EQ(resolved.Bool(f.name), expected.*f.member) << f.name;
            }
            for (const auto& f : Ints) {
                EXPECT_EQ(resolved.Int(f.name), expected.*f.member) << f.name;
            }
            for (const auto& f : Floats) {
                EXPECT_EQ(resolved.Float(f.name), expected.*f.member) << f.name;
            }
        }
    }

    TEST(StreetQualityTests, LimitedDevicesChangeOnlyRecordedSettings)
    {
        const QualitySchema schema = StreetSchema();
        const auto reach = Graphics::DeviceCapabilities::ForProfile(GraphicsProfile::Reach);
        const auto high = schema.Resolve({"high", {}}, reach);
        ASSERT_EQ(high.decisions.size(), 1u);
        EXPECT_EQ(high.decisions[0].setting, "multiSample");
        EXPECT_EQ(high.decisions[0].requested, "4");
        EXPECT_EQ(high.decisions[0].resolved, "0");
        EXPECT_EQ(high.Int("multiSample"), 0);
        // A player's override survives persistence and resolution.
        const auto loaded = schema.LoadSelection("preset=medium\nssao=true\npedestrianCullDistance=150\n", "high");
        EXPECT_TRUE(loaded.warnings.empty());
        const auto medium = schema.Resolve(loaded.selection, reach);
        EXPECT_TRUE(medium.Bool("ssao"));
        EXPECT_EQ(medium.Float("pedestrianCullDistance"), 150.0f);
        EXPECT_TRUE(medium.decisions.empty()) << "Medium requests no multisampling";
    }
}
