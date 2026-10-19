// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/QualitySchema.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Graphics
{
    namespace
    {
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

        const char* TypeName(const QualityValue& value)
        {
            return std::holds_alternative<bool>(value) ? "bool" : std::holds_alternative<int>(value) ? "int" : "float";
        }

        std::optional<QualityValue> ParseAs(const QualityValue& type, std::string_view text)
        {
            if (std::holds_alternative<bool>(type)) {
                if (text == "true") {
                    return QualityValue(true);
                }
                if (text == "false") {
                    return QualityValue(false);
                }
                return std::nullopt;
            }
            if (std::holds_alternative<int>(type)) {
                int value = 0;
                const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
                if (ec != std::errc{} || ptr != text.data() + text.size()) {
                    return std::nullopt;
                }
                return QualityValue(value);
            }
            float value = 0.0f;
            const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (ec != std::errc{} || ptr != text.data() + text.size() || !std::isfinite(value)) {
                return std::nullopt;
            }
            return QualityValue(value);
        }
    }

    std::string FormatQualityValue(const QualityValue& value)
    {
        if (const auto* b = std::get_if<bool>(&value)) {
            return *b ? "true" : "false";
        }
        if (const auto* i = std::get_if<int>(&value)) {
            return std::to_string(*i);
        }
        std::array<char, 32> buffer{};
        const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), std::get<float>(value));
        return ec == std::errc{} ? std::string(buffer.data(), ptr) : std::string("0");
    }

    bool QualityResult::Bool(const std::string& name) const { return std::get<bool>(values.at(name)); }
    int QualityResult::Int(const std::string& name) const { return std::get<int>(values.at(name)); }
    float QualityResult::Float(const std::string& name) const { return std::get<float>(values.at(name)); }

    const QualitySetting* QualitySchema::Find(const std::string_view name) const
    {
        for (const auto& setting : settings_) {
            if (setting.name == name) {
                return &setting;
            }
        }
        return nullptr;
    }

    std::string QualitySchema::Check(const std::string& name, const QualityValue& value) const
    {
        const QualitySetting* setting = Find(name);
        if (setting == nullptr) {
            return "unknown setting '" + name + "'";
        }
        if (value.index() != setting->defaultValue.index()) {
            return "setting '" + name + "' is " + TypeName(setting->defaultValue) + ", not " + TypeName(value);
        }
        double number = 0.0;
        if (const auto* i = std::get_if<int>(&value)) {
            number = *i;
        } else if (const auto* f = std::get_if<float>(&value)) {
            number = *f;
        } else {
            return {};
        }
        if ((setting->minimum && number < *setting->minimum) || (setting->maximum && number > *setting->maximum)) {
            return "setting '" + name + "' value " + FormatQualityValue(value) + " is out of range";
        }
        return {};
    }

    void QualitySchema::Declare(QualitySetting setting)
    {
        if (setting.name.empty() || setting.name.find_first_of("=\n") != std::string::npos) {
            throw std::invalid_argument("QualitySchema: invalid setting name");
        }
        if (Find(setting.name) != nullptr) {
            throw std::invalid_argument("QualitySchema: duplicate setting '" + setting.name + "'");
        }
        settings_.push_back(std::move(setting));
        if (const std::string error = Check(settings_.back().name, settings_.back().defaultValue); !error.empty()) {
            settings_.pop_back();
            throw std::invalid_argument("QualitySchema: default of " + error);
        }
    }

    void QualitySchema::DefinePreset(std::string name, std::map<std::string, QualityValue> values)
    {
        if (name.empty()) {
            throw std::invalid_argument("QualitySchema: preset names must not be empty");
        }
        for (const auto& [setting, value] : values) {
            if (const std::string error = Check(setting, value); !error.empty()) {
                throw std::invalid_argument("QualitySchema: preset '" + name + "': " + error);
            }
        }
        if (!presets_.contains(name)) {
            presetOrder_.push_back(name);
        }
        presets_[std::move(name)] = std::move(values);
    }

    bool QualitySchema::HasPreset(const std::string_view name) const { return presets_.find(name) != presets_.end(); }

    std::vector<std::string> QualitySchema::PresetNames() const { return presetOrder_; }

    QualityResult QualitySchema::Resolve(const QualitySelection& selection, const DeviceCapabilities& capabilities) const
    {
        const auto preset = presets_.find(selection.preset);
        if (preset == presets_.end()) {
            throw std::invalid_argument("QualitySchema: unknown preset '" + selection.preset + "'");
        }
        QualityResult result;
        result.preset = selection.preset;
        for (const auto& setting : settings_) {
            result.values[setting.name] = setting.defaultValue;
        }
        for (const auto& [name, value] : preset->second) {
            result.values[name] = value;
        }
        for (const auto& [name, value] : selection.overrides) {
            if (const std::string error = Check(name, value); !error.empty()) {
                throw std::invalid_argument("QualitySchema: override: " + error);
            }
            result.values[name] = value;
        }
        for (const auto& setting : settings_) {
            if (!setting.limit) {
                continue;
            }
            QualityValue& value = result.values[setting.name];
            std::string reason;
            QualityValue limited = setting.limit(value, capabilities, reason);
            if (limited.index() != value.index()) {
                throw std::logic_error("QualitySchema: limit of '" + setting.name + "' changed the value type");
            }
            if (limited != value) {
                result.decisions.push_back(QualityDecision{setting.name, FormatQualityValue(value),
                                                           FormatQualityValue(limited),
                                                           reason.empty() ? "device capability" : reason});
                value = limited;
            }
        }
        return result;
    }

    std::string QualitySchema::SaveSelection(const QualitySelection& selection) const
    {
        std::string text = "# cna-engine quality selection v1\npreset=" + selection.preset + "\n";
        for (const auto& [name, value] : selection.overrides) {
            text += name + "=" + FormatQualityValue(value) + "\n";
        }
        return text;
    }

    QualitySchema::LoadResult QualitySchema::LoadSelection(const std::string_view text, std::string defaultPreset) const
    {
        LoadResult result;
        result.selection.preset = std::move(defaultPreset);
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
            const std::string key(Trim(line.substr(0, equals)));
            const std::string_view value = Trim(line.substr(equals + 1));
            if (key == "preset") {
                if (HasPreset(value)) {
                    result.selection.preset = std::string(value);
                } else {
                    result.warnings.push_back(where + "unknown preset '" + std::string(value) + "'");
                }
                continue;
            }
            const QualitySetting* setting = Find(key);
            if (setting == nullptr) {
                result.warnings.push_back(where + "unknown setting '" + key + "'");
                continue;
            }
            const auto parsed = ParseAs(setting->defaultValue, value);
            if (!parsed) {
                result.warnings.push_back(where + "invalid value for " + key);
                continue;
            }
            if (const std::string error = Check(key, *parsed); !error.empty()) {
                result.warnings.push_back(where + error);
                continue;
            }
            result.selection.overrides[key] = *parsed;
        }
        return result;
    }
}
