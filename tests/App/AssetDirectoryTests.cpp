// SPDX-License-Identifier: MIT
#include "CnaEngine/App/AssetDirectory.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace CnaEngine::App
{
    namespace
    {
        // A scratch tree in the test's working directory (the build tree), one
        // per test: CTest runs this fixture's tests as parallel processes.
        class AssetDirectoryTest : public ::testing::Test
        {
        protected:
            AssetDirectoryTest()
                : base(std::filesystem::current_path()
                       / (std::string("asset_directory_test_")
                          + ::testing::UnitTest::GetInstance()->current_test_info()->name()))
            {
                std::filesystem::remove_all(base);
                std::filesystem::create_directories(base / "first");
                std::filesystem::create_directories(base / "second" / "assets");
                std::filesystem::create_directories(base / "third" / "assets");
                std::ofstream(base / "first" / "assets") << "a file, not a directory";
            }
            ~AssetDirectoryTest() override { std::filesystem::remove_all(base); }

            std::filesystem::path base;
        };
    }

    TEST_F(AssetDirectoryTest, TheFirstRootWithTheDirectoryWins)
    {
        const std::vector<std::filesystem::path> roots{base / "missing", base / "first", base / "second",
                                                       base / "third"};
        const auto found = FindAssetDirectory("assets", roots);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*found, (base / "second" / "assets").lexically_normal());
        EXPECT_TRUE(found->is_absolute());
    }

    TEST_F(AssetDirectoryTest, NothingWhenNoRootHasIt)
    {
        const std::vector<std::filesystem::path> roots{base / "first", base / "missing"};
        EXPECT_FALSE(FindAssetDirectory("assets", roots).has_value());
        EXPECT_FALSE(FindAssetDirectory("other", std::vector<std::filesystem::path>{}).has_value());
    }

    TEST_F(AssetDirectoryTest, TheWorkingDirectoryIsSearched)
    {
        const auto roots = DefaultAssetSearchRoots();
        ASSERT_FALSE(roots.empty());
        const auto found = FindAssetDirectory(base.filename().string());
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(std::filesystem::weakly_canonical(*found), std::filesystem::weakly_canonical(base));
    }
}
