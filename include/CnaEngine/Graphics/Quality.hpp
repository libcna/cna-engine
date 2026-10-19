// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::Graphics
{
    enum class QualityPreset : std::uint8_t
    {
        Low,
        Medium,
        High,
        Ultra,
    };

    [[nodiscard]] std::string_view ToString(QualityPreset preset) noexcept;
    [[nodiscard]] std::optional<QualityPreset> QualityPresetFromName(std::string_view name);

    /// What the player asked for: a preset plus optional per-setting
    /// overrides. Never modified by capability resolution.
    struct QualityIntent
    {
        QualityPreset preset = QualityPreset::High;
        std::optional<float> drawDistance;      ///< world units, > 0
        std::optional<float> lodBias;           ///< multiplies LOD distances, > 0
        std::optional<int> shadowMapSize;       ///< 0 disables shadows; otherwise a power of two
        std::optional<int> multiSampleCount;    ///< 0 or 1 disables MSAA; otherwise 2, 4 or 8
        std::optional<int> maxTextureSize;      ///< largest texture edge the game should load
    };

    /// What the device can do. FromDevice() derives it from the XNA graphics
    /// profile (XNA 4.0 limits: Reach 2048 px textures, no MSAA guarantees;
    /// HiDef 4096 px); games may override fields after their own probing.
    struct DeviceCapabilities
    {
        Microsoft::Xna::Framework::Graphics::GraphicsProfile profile =
            Microsoft::Xna::Framework::Graphics::GraphicsProfile::Reach;
        int maxTextureSize = 2048;
        int maxMultiSampleCount = 0;
        /// Largest shadow map the game may allocate (a render target edge).
        int maxShadowMapSize = 2048;
        bool supportsShadowMaps = false;

        [[nodiscard]] static DeviceCapabilities FromDevice(
            const Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
        [[nodiscard]] static DeviceCapabilities ForProfile(Microsoft::Xna::Framework::Graphics::GraphicsProfile profile);
    };

    /// One resolution step, kept so settings screens and logs can explain why
    /// a value differs from what was asked for.
    struct QualityDecision
    {
        std::string setting;
        std::string requested;
        std::string resolved;
        std::string reason;
    };

    /// Effective runtime settings.
    struct ResolvedQuality
    {
        QualityPreset preset = QualityPreset::High;
        float drawDistance = 0.0f;
        float lodBias = 1.0f;
        int shadowMapSize = 0;
        int multiSampleCount = 0;
        int maxTextureSize = 0;
        /// Only the settings whose value was changed by a capability limit.
        std::vector<QualityDecision> decisions;
    };

    /// Preset values before overrides and capability limits.
    [[nodiscard]] ResolvedQuality PresetDefaults(QualityPreset preset);

    /// Applies preset, overrides and capability limits, in that order.
    [[nodiscard]] ResolvedQuality Resolve(const QualityIntent& intent, const DeviceCapabilities& capabilities);

    /// `key=value` lines (`preset`, `drawDistance`, `lodBias`, `shadowMapSize`,
    /// `multiSampleCount`, `maxTextureSize`); only overrides that are set are
    /// written. Locale-independent.
    [[nodiscard]] std::string SaveQualityIntent(const QualityIntent& intent);

    struct QualityLoadResult
    {
        QualityIntent intent;
        std::vector<std::string> warnings;
    };
    /// Parses SaveQualityIntent() text; invalid lines or values are reported
    /// and leave the corresponding setting at its default.
    [[nodiscard]] QualityLoadResult LoadQualityIntent(std::string_view text);
}
