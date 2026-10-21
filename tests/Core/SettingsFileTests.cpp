// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/SettingsFile.hpp"

#include "CnaEngine/Input/ActionMap.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace CnaEngine
{
    namespace
    {
        // Output stays inside the test's working directory (the build tree).
        class ScratchDirectory
        {
        public:
            explicit ScratchDirectory(const std::string& name) : path_(std::filesystem::current_path() / name)
            {
                std::filesystem::remove_all(path_);
            }
            ~ScratchDirectory() { std::filesystem::remove_all(path_); }
            [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

        private:
            std::filesystem::path path_;
        };

#if !defined(_WIN32)
        // Sets an environment variable for one scope (POSIX setenv).
        class ScopedVariable
        {
        public:
            ScopedVariable(const char* name, const char* value) : name_(name)
            {
                if (const char* old = std::getenv(name)) {
                    old_ = old;
                }
                if (value != nullptr) {
                    ::setenv(name, value, 1);
                } else {
                    ::unsetenv(name);
                }
            }
            ~ScopedVariable()
            {
                if (old_) {
                    ::setenv(name_, old_->c_str(), 1);
                } else {
                    ::unsetenv(name_);
                }
            }
            ScopedVariable(const ScopedVariable&) = delete;
            ScopedVariable& operator=(const ScopedVariable&) = delete;

        private:
            const char* name_;
            std::optional<std::string> old_;
        };
#endif
    }

    TEST(SettingsFileTests, ParsesKeysValuesAndComments)
    {
        const SettingsFile settings = SettingsFile::Parse("# preferences\n"
                                                          "  volume =  0.5   # a comment\r\n"
                                                          "\n"
                                                          "name = Northern Valley\n"
                                                          "volume = 0.7\n"
                                                          "broken line\n"
                                                          " = orphan\n");
        ASSERT_EQ(settings.Entries().size(), 2u);
        EXPECT_EQ(settings.Entries()[0].first, "volume") << "first seen, first written";
        EXPECT_EQ(settings.Get("volume"), "0.7") << "a later line replaces";
        EXPECT_EQ(settings.GetString("name", ""), "Northern Valley");
        ASSERT_EQ(settings.Problems().size(), 2u);
        EXPECT_EQ(settings.Problems()[0].line, 6);
        EXPECT_EQ(settings.Problems()[1].line, 7);
        EXPECT_FALSE(settings.Contains("broken line"));
    }

    TEST(SettingsFileTests, TypedReadsFallBackAndClamp)
    {
        const SettingsFile settings = SettingsFile::Parse("high = 4.5\nlow = -3\ntext = fast\ntrailing = 1.5x\n"
                                                          "infinite = inf\nhalf = 2.5\nyes = YES\noff = off\n"
                                                          "one = 1\nzero = 0.2\n");
        EXPECT_FLOAT_EQ(settings.GetFloat("high", 0.0f, 0.0f, 1.0f), 1.0f);
        EXPECT_FLOAT_EQ(settings.GetFloat("low", 0.0f, -1.0f, 1.0f), -1.0f);
        EXPECT_FLOAT_EQ(settings.GetFloat("text", 0.25f, 0.0f, 1.0f), 0.25f) << "malformed";
        EXPECT_FLOAT_EQ(settings.GetFloat("trailing", 0.25f, 0.0f, 9.0f), 0.25f) << "the whole value must be a number";
        EXPECT_FLOAT_EQ(settings.GetFloat("infinite", 0.25f, 0.0f, 9.0f), 0.25f);
        EXPECT_FLOAT_EQ(settings.GetFloat("missing", 0.25f, 0.0f, 9.0f), 0.25f);
        EXPECT_EQ(settings.GetInt("half", 0, 0, 10), 3) << "rounded to nearest";
        EXPECT_EQ(settings.GetInt("high", 0, 0, 3), 3);
        EXPECT_EQ(settings.GetInt("low", 7, 0, 10), 0);
        EXPECT_TRUE(settings.GetBool("yes", false));
        EXPECT_FALSE(settings.GetBool("off", true));
        EXPECT_TRUE(settings.GetBool("one", false));
        EXPECT_FALSE(settings.GetBool("zero", true));
        EXPECT_TRUE(settings.GetBool("text", true)) << "neither a word nor a number";
    }

    TEST(SettingsFileTests, WritesInOrderWithAHeader)
    {
        SettingsFile settings;
        settings.SetHeader("Preferences.\nEdited by hand at your own risk.");
        settings.SetFloat("volume", 0.8f);
        settings.SetInt("quality", 2);
        settings.SetBool("captions", true);
        settings.SetFloat("volume", 0.125f, 2);
        settings.Set("name", "Lakehurst");
        EXPECT_TRUE(settings.Remove("name"));
        EXPECT_FALSE(settings.Remove("name"));
        EXPECT_EQ(settings.Serialize(), "# Preferences.\n# Edited by hand at your own risk.\n"
                                        "volume = 0.12\nquality = 2\ncaptions = 1\n");
        const SettingsFile back = SettingsFile::Parse(settings.Serialize());
        EXPECT_EQ(back.Entries(), settings.Entries());
    }

    TEST(SettingsFileTests, SavesAtomicallyAndLoadsBack)
    {
        ScratchDirectory scratch("cna_engine_settings");
        const auto path = scratch.Path() / "nested" / "game.cfg";
        EXPECT_TRUE(SettingsFile::Load(path).Entries().empty()) << "no file yet";
        EXPECT_TRUE(SettingsFile::Load(path).Problems().empty());
        SettingsFile settings;
        settings.SetInt("weather", 3);
        settings.Save(path);
        EXPECT_FALSE(std::filesystem::exists(scratch.Path() / "nested" / "game.cfg.tmp"));
        EXPECT_EQ(SettingsFile::Load(path).GetInt("weather", 0, 0, 5), 3);
        settings.SetInt("weather", 4);
        settings.Save(path);
        EXPECT_EQ(SettingsFile::Load(path).GetInt("weather", 0, 0, 5), 4) << "replaced";
    }

#if !defined(_WIN32)
    TEST(SettingsFileTests, UserConfigPathFollowsXdg)
    {
        {
            ScopedVariable xdg("XDG_CONFIG_HOME", "/tmp/xdg-config");
            EXPECT_EQ(UserConfigPath("game", "game.cfg"), std::filesystem::path("/tmp/xdg-config/game/game.cfg"));
        }
        {
            ScopedVariable xdg("XDG_CONFIG_HOME", "");
            ScopedVariable home("HOME", "/home/player");
            EXPECT_EQ(UserConfigPath("game", "game.cfg"), std::filesystem::path("/home/player/.config/game/game.cfg"));
        }
        {
            ScopedVariable xdg("XDG_CONFIG_HOME", nullptr);
            ScopedVariable home("HOME", nullptr);
            EXPECT_TRUE(UserConfigPath("game", "game.cfg").empty());
        }
    }
#endif

    TEST(SettingsFileTests, ReadsTheEnginesOtherSettingsTexts)
    {
        Input::ActionMap map;
        const auto jump = map.AddAction("Jump");
        map.Bind(jump, Input::Binding::ForKey(Microsoft::Xna::Framework::Input::Keys::Space));
        const SettingsFile bindings = SettingsFile::Parse(map.SaveBindings());
        EXPECT_TRUE(bindings.Problems().empty());
        EXPECT_TRUE(bindings.Contains("action.Jump"));
    }
}
