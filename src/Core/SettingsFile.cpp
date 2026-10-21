// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/SettingsFile.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace CnaEngine
{
    namespace
    {
        std::string Trim(std::string_view text)
        {
            const auto visible = [](const unsigned char character) { return !std::isspace(character); };
            const auto begin = std::find_if(text.begin(), text.end(), visible);
            const auto end = std::find_if(text.rbegin(), text.rend(), visible).base();
            return begin < end ? std::string(begin, end) : std::string();
        }

        // The whole value as a finite number, as std::stof reads it.
        std::optional<float> Number(const std::string& text)
        {
            try {
                std::size_t used = 0;
                const float value = std::stof(text, &used);
                if (used != text.size() || !std::isfinite(value)) {
                    return std::nullopt;
                }
                return value;
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return text;
        }
    }

    SettingsFile SettingsFile::Parse(const std::string_view text)
    {
        SettingsFile settings;
        std::istringstream input{std::string(text)};
        std::string line;
        int lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (const auto comment = line.find('#'); comment != std::string::npos) {
                line.erase(comment);
            }
            line = Trim(line);
            if (line.empty()) {
                continue;
            }
            const auto separator = line.find('=');
            const std::string key = separator == std::string::npos ? std::string() : Trim(std::string_view(line).substr(0, separator));
            if (key.empty()) {
                settings.problems_.push_back(
                    {lineNumber, separator == std::string::npos ? "expected key = value" : "empty key"});
                continue;
            }
            settings.Set(key, Trim(std::string_view(line).substr(separator + 1)), lineNumber);
        }
        return settings;
    }

    SettingsFile SettingsFile::Load(const std::filesystem::path& path)
    {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            return SettingsFile{};
        }
        std::ifstream input(path);
        if (!input) {
            SettingsFile settings;
            settings.problems_.push_back({0, "cannot read " + path.string()});
            return settings;
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        return Parse(contents.str());
    }

    bool SettingsFile::Contains(const std::string_view key) const
    {
        return std::any_of(entries_.begin(), entries_.end(), [&](const auto& entry) { return entry.first == key; });
    }

    std::optional<std::string> SettingsFile::Get(const std::string_view key) const
    {
        for (const auto& [name, value] : entries_) {
            if (name == key) {
                return value;
            }
        }
        return std::nullopt;
    }

    int SettingsFile::Line(const std::string_view key) const
    {
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            if (entries_[index].first == key) {
                return lines_[index];
            }
        }
        return 0;
    }

    float SettingsFile::GetFloat(const std::string_view key, const float fallback, const float minimum,
                                 const float maximum) const
    {
        const auto value = TryGetFloat(key);
        return value ? std::clamp(*value, minimum, maximum) : fallback;
    }

    std::optional<float> SettingsFile::TryGetFloat(const std::string_view key) const
    {
        const auto text = Get(key);
        return text ? Number(*text) : std::nullopt;
    }

    int SettingsFile::GetInt(const std::string_view key, const int fallback, const int minimum, const int maximum) const
    {
        const auto value = TryGetFloat(key);
        if (!value) {
            return fallback;
        }
        // Clamping to whole bounds first gives what rounding first would, without overflow.
        const double clamped = std::clamp(static_cast<double>(*value), static_cast<double>(minimum),
                                          static_cast<double>(maximum));
        return static_cast<int>(std::lround(clamped));
    }

    bool SettingsFile::GetBool(const std::string_view key, const bool fallback) const
    {
        const auto text = Get(key);
        if (!text) {
            return fallback;
        }
        const std::string word = Lower(*text);
        if (word == "true" || word == "yes" || word == "on") {
            return true;
        }
        if (word == "false" || word == "no" || word == "off") {
            return false;
        }
        const auto value = Number(*text);
        return value ? *value >= 0.5f : fallback;
    }

    std::string SettingsFile::GetString(const std::string_view key, std::string fallback) const
    {
        auto value = Get(key);
        return value ? std::move(*value) : std::move(fallback);
    }

    void SettingsFile::Set(const std::string_view key, std::string value)
    {
        Set(key, std::move(value), 0);
    }

    void SettingsFile::Set(const std::string_view key, std::string value, const int line)
    {
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            if (entries_[index].first == key) {
                entries_[index].second = std::move(value);
                lines_[index] = line;
                return;
            }
        }
        entries_.emplace_back(std::string(key), std::move(value));
        lines_.push_back(line);
    }

    void SettingsFile::SetFloat(const std::string_view key, const float value, const int decimals)
    {
        std::ostringstream text;
        text << std::fixed << std::setprecision(decimals) << value;
        Set(key, text.str());
    }

    void SettingsFile::SetInt(const std::string_view key, const int value)
    {
        Set(key, std::to_string(value));
    }

    void SettingsFile::SetBool(const std::string_view key, const bool value)
    {
        Set(key, value ? "1" : "0");
    }

    bool SettingsFile::Remove(const std::string_view key)
    {
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            if (entries_[index].first == key) {
                entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
                lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(index));
                return true;
            }
        }
        return false;
    }

    std::string SettingsFile::Serialize() const
    {
        std::string text;
        std::istringstream header(header_);
        std::string line;
        while (std::getline(header, line)) {
            text += "# " + line + "\n";
        }
        for (const auto& [key, value] : entries_) {
            text += key + " = " + value + "\n";
        }
        return text;
    }

    void SettingsFile::Save(const std::filesystem::path& path) const
    {
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::filesystem::path temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) {
                throw std::runtime_error("SettingsFile: cannot open " + temporary.string() + " for writing");
            }
            output << Serialize();
            if (!output) {
                output.close();
                std::filesystem::remove(temporary);
                throw std::runtime_error("SettingsFile: cannot write " + temporary.string());
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("SettingsFile: cannot replace " + path.string() + ": " + error.message());
        }
    }

    std::filesystem::path UserConfigPath(const std::string_view application, const std::string_view fileName)
    {
        const auto variable = [](const char* name) -> std::string {
            const char* value = std::getenv(name);
            return value != nullptr ? std::string(value) : std::string();
        };
#if defined(_WIN32)
        const std::string base = variable("APPDATA");
        if (base.empty()) {
            return {};
        }
        return std::filesystem::path(base) / application / fileName;
#else
        if (const std::string xdg = variable("XDG_CONFIG_HOME"); !xdg.empty()) {
            return std::filesystem::path(xdg) / application / fileName;
        }
        if (const std::string home = variable("HOME"); !home.empty()) {
            return std::filesystem::path(home) / ".config" / application / fileName;
        }
        return {};
#endif
    }
}
