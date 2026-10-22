// SPDX-License-Identifier: MIT
#include "CnaEngine/UI/SettingsMenu.hpp"

#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>

namespace CnaEngine::UI
{
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    namespace
    {
        MenuInput Press(bool MenuInput::*field)
        {
            MenuInput input;
            input.*field = true;
            return input;
        }

        Input::InputSnapshot Holding(std::initializer_list<Keys> keys)
        {
            Input::InputSnapshot snapshot;
            snapshot.keyboard = KeyboardState(keys);
            return snapshot;
        }
    }

    TEST(MenuTest, SelectionSkipsLabelsAndWraps)
    {
        int started = 0;
        Menu menu("Paused");
        menu.Label("Choose").Button("Resume", [&] { ++started; }).Label("--").Button("Quit", {});
        EXPECT_EQ(menu.Selected(), 1u); // the first selectable item
        menu.Update(Press(&MenuInput::down));
        EXPECT_EQ(menu.Selected(), 3u);
        menu.Update(Press(&MenuInput::down));
        EXPECT_EQ(menu.Selected(), 1u); // wrapped past the label
        menu.Update(Press(&MenuInput::up));
        EXPECT_EQ(menu.Selected(), 3u);
        menu.Select(1);
        menu.Update(Press(&MenuInput::accept));
        EXPECT_EQ(started, 1);
        EXPECT_TRUE(menu.Update(Press(&MenuInput::back)));
        EXPECT_THROW(menu.Select(0), std::out_of_range);
    }

    TEST(MenuTest, ChoicesCycleAndSlidersStepWithinTheirRange)
    {
        int chosen = -1;
        float volume = -1.0f;
        Menu menu;
        menu.Choice("Shadows", {"Off", "Low", "High"}, 2, [&](const int c) { chosen = c; })
            .Slider("Volume", 0.0f, 1.0f, 0.25f, 0.5f, [&](const float v) { volume = v; });
        menu.Update(Press(&MenuInput::right));
        EXPECT_EQ(chosen, 0); // High wraps to Off
        EXPECT_EQ(menu.ItemText(0), "Shadows: < Off >");
        menu.Update(Press(&MenuInput::left));
        EXPECT_EQ(menu.ChoiceValue(0), 2);
        menu.Update(Press(&MenuInput::down));
        menu.Update(Press(&MenuInput::right));
        EXPECT_FLOAT_EQ(volume, 0.75f);
        menu.Update(Press(&MenuInput::right));
        menu.Update(Press(&MenuInput::right)); // stays at the maximum
        EXPECT_FLOAT_EQ(menu.SliderValue(1), 1.0f);
        EXPECT_EQ(menu.ItemText(1), "Volume: < 1.00 >");
        EXPECT_THROW(menu.Slider("Bad", 1.0f, 0.0f, 0.1f, 0.5f), std::invalid_argument);
    }

    TEST(MenuTest, ThePointerSelectsAndClicks)
    {
        Microsoft::Xna::Framework::Graphics::GraphicsDevice device;
        const BuiltInFont font(device);
        Painter painter(device);
        int clicked = 0;
        Menu menu("Main");
        menu.Button("Play", [&] { ++clicked; }).Button("Settings", {}).Button("Quit", {});
        painter.Begin();
        menu.Draw(painter, font);
        painter.End();
        // Over the middle of the screen, a little below the first item: the second.
        const Vector2 middle = painter.VirtualSize() * 0.5f;
        MenuInput hover;
        hover.pointer = Vector2(middle.X, middle.Y + 22.0f);
        menu.Update(hover);
        const auto over = menu.Selected();
        EXPECT_NE(over, 0u);
        MenuInput click;
        click.pointer = Vector2(middle.X, middle.Y - 200.0f); // nothing there
        click.click = true;
        menu.Update(click);
        EXPECT_EQ(clicked, 0);
        EXPECT_EQ(menu.Selected(), over);
    }

    TEST(MenuTest, TheStackGivesInputToTheTopAndBackCloses)
    {
        MenuStack stack;
        auto main = std::make_shared<Menu>("Main");
        auto settings = std::make_shared<Menu>("Settings");
        settings->pausesWorld = false;
        main->Button("Settings", [&] { stack.Push(settings); });
        stack.Push(main);
        stack.Update(Press(&MenuInput::accept));
        EXPECT_EQ(stack.Depth(), 2u);
        EXPECT_EQ(stack.Top(), settings.get());
        EXPECT_TRUE(stack.PausesWorld()); // the main menu below still does
        stack.Update(Press(&MenuInput::back));
        EXPECT_EQ(stack.Top(), main.get());
        stack.Update(Press(&MenuInput::back));
        EXPECT_TRUE(stack.Empty());
        EXPECT_FALSE(stack.PausesWorld());
    }

    TEST(SettingsMenuTest, QualityRowsWriteTheSelection)
    {
        Graphics::QualitySchema schema;
        schema.Declare({"shadows", true, std::nullopt, std::nullopt, {}});
        schema.Declare({"viewDistance", 200.0f, 50.0, 1000.0, {}});
        schema.Declare({"msaa", 4, 0.0, 8.0, {}});
        schema.Declare({"unbounded", 1.0f, std::nullopt, std::nullopt, {}});
        schema.DefinePreset("low", {{"shadows", false}, {"viewDistance", 100.0f}, {"msaa", 0}});
        schema.DefinePreset("high", {{"shadows", true}, {"viewDistance", 600.0f}, {"msaa", 4}});
        Graphics::QualitySelection selection{"low", {}};
        int changes = 0;
        Menu menu("Settings");
        AddQualitySettings(menu, schema, selection, [&] { ++changes; });
        ASSERT_EQ(menu.ItemCount(), 4u); // the preset and three bounded settings
        EXPECT_EQ(menu.ItemText(1), "shadows: < Off >"); // the preset's value
        menu.Update(Press(&MenuInput::down));
        menu.Update(Press(&MenuInput::right));
        EXPECT_TRUE(std::get<bool>(selection.overrides.at("shadows")));
        menu.Update(Press(&MenuInput::down));
        menu.Update(Press(&MenuInput::down));
        menu.Update(Press(&MenuInput::right));
        EXPECT_EQ(std::get<int>(selection.overrides.at("msaa")), 1);
        menu.Select(0);
        menu.Update(Press(&MenuInput::right)); // another preset clears the overrides
        EXPECT_TRUE(selection.overrides.empty());
        EXPECT_FALSE(selection.preset == "low");
        EXPECT_EQ(changes, 3);
    }

    TEST(SettingsMenuTest, ARowRebindsAnActionToTheNextKey)
    {
        Input::ActionMap actions;
        const auto jump = actions.AddAction("Jump");
        actions.Bind(jump, Input::Binding::ForKey(Keys::Space));
        Menu menu("Controls");
        RebindRows rows(menu, actions, {jump});
        EXPECT_EQ(menu.ItemText(0), "Jump: Space");

        menu.Update(Press(&MenuInput::accept)); // Enter on the row
        EXPECT_TRUE(rows.Listening());
        EXPECT_EQ(menu.ItemText(0), "Jump: press a key...");
        EXPECT_FALSE(rows.Update(Holding({Keys::Enter}))); // still held from accepting
        EXPECT_FALSE(rows.Update(Holding({})));
        EXPECT_TRUE(rows.Update(Holding({Keys::F})));
        EXPECT_FALSE(rows.Listening());
        ASSERT_EQ(actions.Bindings(jump).size(), 1u);
        EXPECT_EQ(actions.Bindings(jump).front(), Input::Binding::ForKey(Keys::F));
        EXPECT_EQ(menu.ItemText(0), "Jump: F");
        // F, still held, does not press Jump.
        actions.Update(Holding({Keys::F}));
        EXPECT_FALSE(actions.Pressed(jump));

        menu.Update(Press(&MenuInput::accept));
        (void)rows.Update(Holding({}));
        EXPECT_FALSE(rows.Update(Holding({Keys::Escape}))); // cancelled
        EXPECT_EQ(actions.Bindings(jump).front(), Input::Binding::ForKey(Keys::F));
    }
}
