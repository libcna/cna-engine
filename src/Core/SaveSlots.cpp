// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/SaveSlots.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace CnaEngine
{
    namespace
    {
        constexpr std::string_view Magic = "cna-engine save 1";

        std::uint64_t Fnv1a(const std::string_view text) noexcept
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const char c : text) {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        bool ValidSlot(const std::string_view slot) noexcept
        {
            return !slot.empty() && slot.size() <= 64 && std::all_of(slot.begin(), slot.end(), [](const char c) {
                       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
                   });
        }

        std::optional<std::string> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) {
                return std::nullopt;
            }
            std::ostringstream text;
            text << stream.rdbuf();
            return std::move(text).str();
        }

        // A save's header and content; nothing when the file is damaged.
        std::optional<LoadedSave> Parse(const std::string& file)
        {
            const auto end = file.find("\n\n");
            if (end == std::string::npos || file.rfind(Magic, 0) != 0) {
                return std::nullopt;
            }
            LoadedSave save;
            std::optional<std::uint64_t> checksum;
            std::optional<std::size_t> length;
            std::istringstream header(file.substr(0, end));
            std::string line;
            std::getline(header, line); // the magic line
            while (std::getline(header, line)) {
                const auto colon = line.find(": ");
                if (colon == std::string::npos) {
                    continue;
                }
                const std::string_view key(line.data(), colon);
                const std::string value = line.substr(colon + 2);
                if (key == "version") {
                    save.version = std::atoi(value.c_str());
                } else if (key == "summary") {
                    save.summary = value;
                } else if (key == "checksum") {
                    std::uint64_t parsed = 0;
                    if (std::from_chars(value.data(), value.data() + value.size(), parsed, 16).ec == std::errc()) {
                        checksum = parsed;
                    }
                } else if (key == "length") {
                    std::size_t parsed = 0;
                    if (std::from_chars(value.data(), value.data() + value.size(), parsed).ec == std::errc()) {
                        length = parsed;
                    }
                }
            }
            save.content = file.substr(end + 2);
            if (!checksum || !length || *length != save.content.size() || *checksum != Fnv1a(save.content)) {
                return std::nullopt;
            }
            return save;
        }
    }

    std::filesystem::path UserDataPath(const std::string_view application)
    {
        const std::filesystem::path name{std::string(application)};
#ifdef _WIN32
        if (const char* appData = std::getenv("APPDATA"); appData != nullptr && *appData != '\0') {
            return std::filesystem::path(appData) / name;
        }
#endif
        if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && *data != '\0') {
            return std::filesystem::path(data) / name;
        }
        if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
            return std::filesystem::path(home) / ".local" / "share" / name;
        }
        return {};
    }

    SaveSlots::SaveSlots(std::filesystem::path directory) : directory_(std::move(directory))
    {
        if (directory_.empty()) {
            throw std::invalid_argument("SaveSlots: needs a directory");
        }
    }

    std::filesystem::path SaveSlots::PathOf(const std::string_view slot, const std::string_view extension) const
    {
        if (!ValidSlot(slot)) {
            throw std::invalid_argument("SaveSlots: a slot is named by letters, digits, '-' and '_', not '" +
                                        std::string(slot) + "'");
        }
        return directory_ / (std::string(slot) + std::string(extension));
    }

    void SaveSlots::Save(const std::string_view slot, const std::string_view content, const int version,
                         const std::string_view summary)
    {
        const auto target = PathOf(slot, ".save");
        const auto temporary = PathOf(slot, ".tmp");
        const auto backup = PathOf(slot, ".bak");
        std::string flat(summary);
        std::replace(flat.begin(), flat.end(), '\n', ' ');
        char checksum[17];
        std::snprintf(checksum, sizeof checksum, "%016llx", static_cast<unsigned long long>(Fnv1a(content)));
        std::error_code error;
        std::filesystem::create_directories(directory_, error);
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream << Magic << "\nversion: " << version << "\nsummary: " << flat << "\nlength: " << content.size()
                   << "\nchecksum: " << checksum << "\n\n" << content;
            stream.flush();
            if (!stream) {
                throw std::runtime_error("SaveSlots: cannot write " + temporary.string());
            }
        }
        // The old save becomes the backup; between the two renames the backup
        // is the save, and Load reads it.
        if (std::filesystem::exists(target, error)) {
            std::filesystem::rename(target, backup, error);
            if (error) {
                throw std::runtime_error("SaveSlots: cannot keep " + target.string() + " as a backup: " + error.message());
            }
        }
        std::filesystem::rename(temporary, target, error);
        if (error) {
            throw std::runtime_error("SaveSlots: cannot write " + target.string() + ": " + error.message());
        }
    }

    std::optional<LoadedSave> SaveSlots::Load(const std::string_view slot) const
    {
        if (const auto file = ReadFile(PathOf(slot, ".save"))) {
            if (auto save = Parse(*file)) {
                return save;
            }
        }
        if (const auto file = ReadFile(PathOf(slot, ".bak"))) {
            if (auto save = Parse(*file)) {
                save->fromBackup = true;
                return save;
            }
        }
        return std::nullopt;
    }

    std::vector<SaveInfo> SaveSlots::List() const
    {
        std::vector<SaveInfo> slots;
        std::error_code error;
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(directory_, error)) {
            const auto path = entry.path();
            const auto extension = path.extension().string();
            if (extension == ".save" || extension == ".bak") {
                const auto stem = path.stem().string();
                if (ValidSlot(stem) && std::find(names.begin(), names.end(), stem) == names.end()) {
                    names.push_back(stem);
                }
            }
        }
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            if (const auto save = Load(name)) {
                SaveInfo info;
                info.slot = name;
                info.version = save->version;
                info.summary = save->summary;
                const auto file = PathOf(name, save->fromBackup ? ".bak" : ".save");
                info.written = std::filesystem::last_write_time(file, error);
                slots.push_back(std::move(info));
            }
        }
        return slots;
    }

    bool SaveSlots::Remove(const std::string_view slot)
    {
        std::error_code error;
        const bool save = std::filesystem::remove(PathOf(slot, ".save"), error);
        const bool backup = std::filesystem::remove(PathOf(slot, ".bak"), error);
        std::filesystem::remove(PathOf(slot, ".tmp"), error);
        return save || backup;
    }
}
