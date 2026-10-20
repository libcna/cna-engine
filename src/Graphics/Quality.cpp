// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Quality.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <type_traits>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;

    namespace
    {
        constexpr std::array<std::string_view, 4> PresetNames = {"Low", "Medium", "High", "Ultra"};

        bool IsPowerOfTwo(const int value) { return value > 0 && (value & (value - 1)) == 0; }

        std::string Format(const float value)
        {
            std::array<char, 32> buffer{};
            const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            return ec == std::errc{} ? std::string(buffer.data(), ptr) : std::string("0");
        }

        std::string Format(const int value) { return std::to_string(value); }

        template <typename T>
        std::optional<T> Parse(std::string_view text)
        {
            T value{};
            const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (ec != std::errc{} || ptr != text.data() + text.size()) {
                return std::nullopt;
            }
            if constexpr (std::is_floating_point_v<T>) {
                if (!std::isfinite(value)) {
                    return std::nullopt;
                }
            }
            return value;
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
                text.remove_prefix(1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
                text.remove_suffix(1);
            }
            return text;
        }

        template <typename T>
        void Limit(ResolvedQuality& result, const char* setting, T& value, const T limit, const std::string& reason)
        {
            if (value > limit) {
                result.decisions.push_back(QualityDecision{setting, Format(value), Format(limit), reason});
                value = limit;
            }
        }
    }

    std::string_view ToString(const QualityPreset preset) noexcept
    {
        return PresetNames[static_cast<std::size_t>(preset)];
    }

    std::optional<QualityPreset> QualityPresetFromName(const std::string_view name)
    {
        for (std::size_t i = 0; i < PresetNames.size(); ++i) {
            if (PresetNames[i] == name) {
                return static_cast<QualityPreset>(i);
            }
        }
        return std::nullopt;
    }

    DeviceCapabilities DeviceCapabilities::ForProfile(const GraphicsProfile profile)
    {
        DeviceCapabilities caps;
        caps.profile = profile;
        if (profile == GraphicsProfile::HiDef) {
            // XNA 4.0 HiDef guarantees 4096 px textures, floating-point render
            // targets (shadow maps) and multisampling.
            caps.maxTextureSize = 4096;
            caps.maxMultiSampleCount = 4;
            caps.maxShadowMapSize = 4096;
            caps.supportsShadowMaps = true;
        } else {
            // Reach: 2048 px textures, no floating-point render targets, and
            // multisampling is a request the platform may ignore.
            caps.maxTextureSize = 2048;
            caps.maxMultiSampleCount = 0;
            caps.maxShadowMapSize = 0;
            caps.supportsShadowMaps = false;
        }
        return caps;
    }

    DeviceCapabilities DeviceCapabilities::FromDevice(const GraphicsDevice& device)
    {
        return ForProfile(device.getGraphicsProfileProperty());
    }

    ResolvedQuality PresetDefaults(const QualityPreset preset)
    {
        ResolvedQuality q;
        q.preset = preset;
        switch (preset) {
            case QualityPreset::Low:
                q.drawDistance = 250.0f;
                q.lodBias = 0.5f;
                q.shadowMapSize = 0;
                q.multiSampleCount = 0;
                q.maxTextureSize = 512;
                break;
            case QualityPreset::Medium:
                q.drawDistance = 500.0f;
                q.lodBias = 0.75f;
                q.shadowMapSize = 1024;
                q.multiSampleCount = 0;
                q.maxTextureSize = 1024;
                break;
            case QualityPreset::High:
                q.drawDistance = 1000.0f;
                q.lodBias = 1.0f;
                q.shadowMapSize = 2048;
                q.multiSampleCount = 2;
                q.maxTextureSize = 2048;
                break;
            case QualityPreset::Ultra:
                q.drawDistance = 2000.0f;
                q.lodBias = 1.5f;
                q.shadowMapSize = 4096;
                q.multiSampleCount = 4;
                q.maxTextureSize = 4096;
                break;
        }
        return q;
    }

    ResolvedQuality Resolve(const QualityIntent& intent, const DeviceCapabilities& capabilities)
    {
        ResolvedQuality q = PresetDefaults(intent.preset);
        if (intent.drawDistance) {
            q.drawDistance = *intent.drawDistance;
        }
        if (intent.lodBias) {
            q.lodBias = *intent.lodBias;
        }
        if (intent.shadowMapSize) {
            q.shadowMapSize = *intent.shadowMapSize;
        }
        if (intent.multiSampleCount) {
            q.multiSampleCount = *intent.multiSampleCount <= 1 ? 0 : *intent.multiSampleCount;
        }
        if (intent.maxTextureSize) {
            q.maxTextureSize = *intent.maxTextureSize;
        }

        const std::string profile = capabilities.profile == GraphicsProfile::HiDef ? "HiDef" : "Reach";
        if (!capabilities.supportsShadowMaps && q.shadowMapSize > 0) {
            q.decisions.push_back(QualityDecision{"shadowMapSize", Format(q.shadowMapSize), "0",
                                                  profile + " device has no shadow-map support"});
            q.shadowMapSize = 0;
        }
        Limit(q, "shadowMapSize", q.shadowMapSize, capabilities.maxShadowMapSize,
              "largest shadow map supported by the " + profile + " device");
        Limit(q, "multiSampleCount", q.multiSampleCount, capabilities.maxMultiSampleCount,
              "largest multisample count supported by the " + profile + " device");
        Limit(q, "maxTextureSize", q.maxTextureSize, capabilities.maxTextureSize,
              "largest texture supported by the " + profile + " device");
        return q;
    }

    std::string SaveQualityIntent(const QualityIntent& intent)
    {
        std::string text = "# cna-engine quality v1\npreset=" + std::string(ToString(intent.preset)) + "\n";
        if (intent.drawDistance) {
            text += "drawDistance=" + Format(*intent.drawDistance) + "\n";
        }
        if (intent.lodBias) {
            text += "lodBias=" + Format(*intent.lodBias) + "\n";
        }
        if (intent.shadowMapSize) {
            text += "shadowMapSize=" + Format(*intent.shadowMapSize) + "\n";
        }
        if (intent.multiSampleCount) {
            text += "multiSampleCount=" + Format(*intent.multiSampleCount) + "\n";
        }
        if (intent.maxTextureSize) {
            text += "maxTextureSize=" + Format(*intent.maxTextureSize) + "\n";
        }
        return text;
    }

    QualityLoadResult LoadQualityIntent(const std::string_view text)
    {
        QualityLoadResult result;
        std::size_t start = 0;
        int lineNumber = 0;
        while (start <= text.size()) {
            const std::size_t end = text.find('\n', start);
            const std::string_view line =
                Trim(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            start = end == std::string_view::npos ? text.size() + 1 : end + 1;
            ++lineNumber;
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const std::string where = "line " + std::to_string(lineNumber) + ": ";
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos) {
                result.warnings.push_back(where + "missing '='");
                continue;
            }
            const std::string_view key = Trim(line.substr(0, equals));
            const std::string_view value = Trim(line.substr(equals + 1));
            auto invalid = [&] { result.warnings.push_back(where + "invalid value for " + std::string(key)); };

            if (key == "preset") {
                if (const auto preset = QualityPresetFromName(value)) {
                    result.intent.preset = *preset;
                } else {
                    invalid();
                }
            } else if (key == "drawDistance" || key == "lodBias") {
                const auto number = Parse<float>(value);
                if (!number || *number <= 0.0f) {
                    invalid();
                } else if (key == "drawDistance") {
                    result.intent.drawDistance = number;
                } else {
                    result.intent.lodBias = number;
                }
            } else if (key == "shadowMapSize") {
                const auto number = Parse<int>(value);
                if (!number || (*number != 0 && !IsPowerOfTwo(*number))) {
                    invalid();
                } else {
                    result.intent.shadowMapSize = number;
                }
            } else if (key == "multiSampleCount") {
                const auto number = Parse<int>(value);
                if (!number || (*number > 1 && *number != 2 && *number != 4 && *number != 8) || *number < 0) {
                    invalid();
                } else {
                    result.intent.multiSampleCount = number;
                }
            } else if (key == "maxTextureSize") {
                const auto number = Parse<int>(value);
                if (!number || !IsPowerOfTwo(*number)) {
                    invalid();
                } else {
                    result.intent.maxTextureSize = number;
                }
            } else {
                result.warnings.push_back(where + "unknown setting '" + std::string(key) + "'");
            }
        }
        return result;
    }
}
