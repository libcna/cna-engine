// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine
{
    /// Where a player's game data lives: `$XDG_DATA_HOME/<application>`, else
    /// `$HOME/.local/share/<application>` (on Windows `%APPDATA%\<application>`);
    /// empty when none of those is set. car-simulator's save directory.
    [[nodiscard]] std::filesystem::path UserDataPath(std::string_view application);

    /// What a slot holds, without its content.
    struct SaveInfo
    {
        std::string slot;
        /// The game's save format version written with it.
        int version = 0;
        /// A line for a load menu ("Level 2, 15 crates").
        std::string summary;
        std::filesystem::file_time_type written{};
    };

    /// A loaded save.
    struct LoadedSave
    {
        std::string content;
        int version = 0;
        std::string summary;
        /// The slot's file was damaged or missing and its backup was read.
        bool fromBackup = false;
    };

    /// Named save slots in a directory: a game's state as text (its own
    /// format: key = value, JSON, ...), with a small header -- the game's
    /// format version, a summary, the content's length and checksum. Writing
    /// never leaves half a file: the text goes to `<slot>.tmp`, the previous
    /// save becomes `<slot>.bak`, the new one takes its name. A save whose
    /// header, length or checksum does not hold is damaged; Load then reads
    /// the backup. House- and island-simulator's write-tmp-keep-backup
    /// sequence, car's versioned save, house's planned checksum
    /// (docs/game-facilities-survey.md).
    ///
    /// Versions are the game's: a save from a newer version than it knows is
    /// reported with its version, and the game decides (car opens one
    /// read-only).
    class SaveSlots
    {
    public:
        /// Throws std::invalid_argument for an empty directory.
        explicit SaveSlots(std::filesystem::path directory);

        [[nodiscard]] const std::filesystem::path& Directory() const noexcept { return directory_; }

        /// Slot names are letters, digits, '-' and '_' (1 to 64); others
        /// throw std::invalid_argument. Throws std::runtime_error when the
        /// files cannot be written.
        void Save(std::string_view slot, std::string_view content, int version, std::string_view summary = {});
        /// Nothing when neither the slot nor its backup can be read.
        [[nodiscard]] std::optional<LoadedSave> Load(std::string_view slot) const;
        /// Every slot with a readable save, by name.
        [[nodiscard]] std::vector<SaveInfo> List() const;
        /// Deletes a slot and its backup; returns whether there was one.
        bool Remove(std::string_view slot);

    private:
        [[nodiscard]] std::filesystem::path PathOf(std::string_view slot, std::string_view extension) const;

        std::filesystem::path directory_;
    };
}
