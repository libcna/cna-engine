// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CnaEngine
{
    /// A player's preferences as a text file of `key = value` lines.
    ///
    /// `#` starts a comment anywhere on a line; keys and values are trimmed;
    /// empty lines are skipped. A later line for the same key replaces the
    /// earlier value. The grammar is the one ActionMap::SaveBindings and
    /// QualitySchema::SaveSelection write, so those texts read back here too.
    ///
    /// Typed reads take a fallback and a range: a missing, malformed or
    /// non-finite value gives the fallback, and a value outside the range is
    /// clamped -- a hand-edited file can never put the game in a state its
    /// menus could not. Keys keep the order they were first seen or set in, so
    /// a saved file reads like the last one.
    ///
    /// This is train-simulator's `Config::Settings` file as a reusable store;
    /// the settings probe reads and writes its files byte for byte.
    class SettingsFile
    {
    public:
        /// A line Parse could not read (no `=`, or an empty key).
        struct Problem
        {
            int line = 0;
            std::string message;
        };

        [[nodiscard]] static SettingsFile Parse(std::string_view text);
        /// A missing file gives an empty set; one that cannot be read is a
        /// Problem on line 0.
        [[nodiscard]] static SettingsFile Load(const std::filesystem::path& path);

        [[nodiscard]] const std::vector<Problem>& Problems() const noexcept { return problems_; }
        [[nodiscard]] const std::vector<std::pair<std::string, std::string>>& Entries() const noexcept { return entries_; }
        [[nodiscard]] bool Contains(std::string_view key) const;
        [[nodiscard]] std::optional<std::string> Get(std::string_view key) const;
        /// The line the key's value was read from (the last one, for a key
        /// given twice); 0 for a key set in code or missing. For diagnostics.
        [[nodiscard]] int Line(std::string_view key) const;

        /// A decimal number (std::stof, the whole value), clamped.
        [[nodiscard]] float GetFloat(std::string_view key, float fallback, float minimum, float maximum) const;
        /// The value as GetFloat reads it, unclamped; nothing when the key is
        /// missing or its value is not a finite number -- for a file that
        /// rejects what GetFloat would quietly replace with the fallback.
        [[nodiscard]] std::optional<float> TryGetFloat(std::string_view key) const;
        /// A decimal number rounded to the nearest integer, clamped.
        [[nodiscard]] int GetInt(std::string_view key, int fallback, int minimum, int maximum) const;
        /// true/false, yes/no, on/off, or a number (true from 0.5 up).
        [[nodiscard]] bool GetBool(std::string_view key, bool fallback) const;
        [[nodiscard]] std::string GetString(std::string_view key, std::string fallback) const;

        void Set(std::string_view key, std::string value);
        /// Fixed-point with `decimals` places (std::fixed).
        void SetFloat(std::string_view key, float value, int decimals = 4);
        void SetInt(std::string_view key, int value);
        /// Written as 1 or 0.
        void SetBool(std::string_view key, bool value);
        /// Returns whether the key was there.
        bool Remove(std::string_view key);

        /// Comment written before the entries (each line prefixed with "# "); empty for none.
        void SetHeader(std::string header) { header_ = std::move(header); }
        [[nodiscard]] std::string Serialize() const;
        /// Writes Serialize() to `<path>.tmp` and renames it over `path`, so a
        /// crash never leaves half a file; missing directories are created.
        /// Throws std::runtime_error when the file cannot be written.
        void Save(const std::filesystem::path& path) const;

    private:
        void Set(std::string_view key, std::string value, int line);

        std::vector<std::pair<std::string, std::string>> entries_;
        /// The line each entry was read from (0: set in code), parallel to entries_.
        std::vector<int> lines_;
        std::vector<Problem> problems_;
        std::string header_;
    };

    /// Where a per-user preferences file lives: `$XDG_CONFIG_HOME/<application>/<fileName>`,
    /// else `$HOME/.config/<application>/<fileName>` (on Windows
    /// `%APPDATA%\<application>\<fileName>`); empty when none of those is set.
    [[nodiscard]] std::filesystem::path UserConfigPath(std::string_view application, std::string_view fileName);
}
