// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/SaveSlots.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace CnaEngine
{
    namespace
    {
        class SaveSlotsTest : public ::testing::Test
        {
        protected:
            SaveSlotsTest() : directory(std::filesystem::current_path() / "save_slots_test")
            {
                std::filesystem::remove_all(directory);
            }
            ~SaveSlotsTest() override { std::filesystem::remove_all(directory); }

            std::filesystem::path directory;
        };
    }

    TEST_F(SaveSlotsTest, ASaveComesBackWithItsVersionAndSummary)
    {
        SaveSlots slots(directory);
        slots.Save("slot-1", "level = 2\ncrates = 15\n", 3, "Level 2, 15 crates");
        const auto loaded = slots.Load("slot-1");
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->content, "level = 2\ncrates = 15\n");
        EXPECT_EQ(loaded->version, 3);
        EXPECT_EQ(loaded->summary, "Level 2, 15 crates");
        EXPECT_FALSE(loaded->fromBackup);
        EXPECT_FALSE(slots.Load("slot-2").has_value());
        // Empty content and text that looks like a header both survive.
        slots.Save("empty", "", 1);
        EXPECT_EQ(slots.Load("empty")->content, "");
        slots.Save("tricky", "version: 99\n\nchecksum: 0\n", 1);
        EXPECT_EQ(slots.Load("tricky")->content, "version: 99\n\nchecksum: 0\n");
    }

    TEST_F(SaveSlotsTest, ADamagedSaveFallsBackToTheOneBefore)
    {
        SaveSlots slots(directory);
        slots.Save("main", "first", 1);
        slots.Save("main", "second", 1);
        EXPECT_TRUE(std::filesystem::exists(directory / "main.bak"));
        EXPECT_EQ(slots.Load("main")->content, "second");

        // Cut short, as by a crash while writing in place.
        std::filesystem::resize_file(directory / "main.save", std::filesystem::file_size(directory / "main.save") - 3);
        auto loaded = slots.Load("main");
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->content, "first");
        EXPECT_TRUE(loaded->fromBackup);

        // A changed byte breaks the checksum.
        slots.Save("other", "abcdef", 1);
        slots.Save("other", "ghijkl", 1);
        {
            std::fstream file(directory / "other.save", std::ios::in | std::ios::out | std::ios::binary);
            file.seekp(-2, std::ios::end);
            file.put('X');
        }
        EXPECT_EQ(slots.Load("other")->content, "abcdef");

        // Between the two renames of a save there is only the backup.
        std::filesystem::remove(directory / "other.save");
        EXPECT_EQ(slots.Load("other")->content, "abcdef");
    }

    TEST_F(SaveSlotsTest, SlotsAreListedAndRemoved)
    {
        SaveSlots slots(directory);
        slots.Save("b", "2", 2, "second");
        slots.Save("a", "1", 1, "first");
        std::ofstream(directory / "junk.save") << "not a save";
        const auto list = slots.List();
        ASSERT_EQ(list.size(), 2u);
        EXPECT_EQ(list[0].slot, "a");
        EXPECT_EQ(list[0].summary, "first");
        EXPECT_EQ(list[1].version, 2);
        EXPECT_TRUE(slots.Remove("a"));
        EXPECT_FALSE(slots.Remove("a"));
        EXPECT_EQ(slots.List().size(), 1u);
        EXPECT_THROW(slots.Save("../escape", "x", 1), std::invalid_argument);
        EXPECT_THROW(slots.Save("", "x", 1), std::invalid_argument);
        EXPECT_THROW(SaveSlots(""), std::invalid_argument);
    }

    TEST(UserDataPathTest, FollowsXdgThenHome)
    {
#ifndef _WIN32
        const char* data = std::getenv("XDG_DATA_HOME");
        const std::string saved = data != nullptr ? data : "";
        ::setenv("XDG_DATA_HOME", "/data/here", 1);
        EXPECT_EQ(UserDataPath("game"), std::filesystem::path("/data/here/game"));
        ::unsetenv("XDG_DATA_HOME");
        if (const char* home = std::getenv("HOME")) {
            EXPECT_EQ(UserDataPath("game"), std::filesystem::path(home) / ".local" / "share" / "game");
        }
        if (data != nullptr) {
            ::setenv("XDG_DATA_HOME", saved.c_str(), 1);
        }
#endif
    }
}
