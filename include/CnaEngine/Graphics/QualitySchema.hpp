// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Quality.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace CnaEngine::Graphics
{
    /// A quality setting's value.
    using QualityValue = std::variant<bool, int, float>;

    [[nodiscard]] std::string FormatQualityValue(const QualityValue& value);

    /// Declares one setting a game exposes. The game owns its meaning (SSAO,
    /// mirror distance, crowd density, ...); the schema owns presets,
    /// overrides, validation, capability limits and persistence.
    struct QualitySetting
    {
        std::string name;
        /// Also fixes the setting's type.
        QualityValue defaultValue;
        /// Numeric bounds for int/float settings (inclusive).
        std::optional<double> minimum;
        std::optional<double> maximum;
        /// Optional capability limit: returns the value the device allows and
        /// sets `reason` when it differs from `requested`.
        std::function<QualityValue(const QualityValue& requested, const DeviceCapabilities& capabilities,
                                   std::string& reason)>
            limit;
    };

    /// What the player chose: a preset plus per-setting overrides.
    struct QualitySelection
    {
        std::string preset;
        std::map<std::string, QualityValue> overrides;
    };

    /// Effective values with the record of every capability change.
    struct QualityResult
    {
        std::string preset;
        std::map<std::string, QualityValue> values;
        std::vector<QualityDecision> decisions;

        [[nodiscard]] bool Bool(const std::string& name) const;
        [[nodiscard]] int Int(const std::string& name) const;
        [[nodiscard]] float Float(const std::string& name) const;
    };

    /// A game's quality model: declared settings and named presets.
    ///
    /// Resolution order: setting defaults, then the preset's values, then the
    /// selection's overrides, then each setting's capability limit. Values are
    /// type-checked and range-checked when presets are defined and when
    /// overrides are loaded, so an invalid value is rejected with a message
    /// instead of being silently clamped.
    class QualitySchema
    {
    public:
        /// Throws std::invalid_argument for a duplicate name, an empty name, or
        /// a default outside the bounds.
        void Declare(QualitySetting setting);
        /// Values not listed keep the setting default. Throws
        /// std::invalid_argument for unknown settings, wrong types or values
        /// outside the bounds.
        void DefinePreset(std::string name, std::map<std::string, QualityValue> values);

        [[nodiscard]] bool HasPreset(std::string_view name) const;
        [[nodiscard]] std::vector<std::string> PresetNames() const;
        [[nodiscard]] const std::vector<QualitySetting>& Settings() const noexcept { return settings_; }

        /// Throws std::invalid_argument for an unknown preset or an invalid
        /// override (overrides from files go through LoadSelection first).
        [[nodiscard]] QualityResult Resolve(const QualitySelection& selection,
                                            const DeviceCapabilities& capabilities) const;

        /// `preset=<name>` followed by one `<setting>=<value>` line per
        /// override; booleans as true/false, numbers locale-independent.
        [[nodiscard]] std::string SaveSelection(const QualitySelection& selection) const;
        struct LoadResult
        {
            QualitySelection selection;
            std::vector<std::string> warnings;
        };
        /// Unknown settings, bad values and unknown presets are reported and
        /// skipped (an unknown preset leaves `defaultPreset`).
        [[nodiscard]] LoadResult LoadSelection(std::string_view text, std::string defaultPreset) const;

        /// Validates `value` for setting `name`; returns an error message or
        /// an empty string.
        [[nodiscard]] std::string Check(const std::string& name, const QualityValue& value) const;

    private:
        [[nodiscard]] const QualitySetting* Find(std::string_view name) const;

        std::vector<QualitySetting> settings_;
        std::map<std::string, std::map<std::string, QualityValue>, std::less<>> presets_;
        std::vector<std::string> presetOrder_;
    };
}
