// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/InputNames.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::Keys;

    TEST(InputNamesTests, KeyNamesRoundTripCaseInsensitively)
    {
        EXPECT_EQ(KeyName(Keys::LeftShift), "LeftShift");
        EXPECT_EQ(KeyFromName("leftshift"), Keys::LeftShift);
        EXPECT_EQ(KeyFromName("D1"), Keys::D1);
        EXPECT_EQ(KeyFromName("OemComma"), Keys::OemComma);
        EXPECT_FALSE(KeyFromName("NoSuchKey").has_value());
        EXPECT_FALSE(KeyFromName("").has_value());
    }

    TEST(InputNamesTests, EveryNamedKeyRoundTrips)
    {
        for (int code = 0; code < 256; ++code) {
            const auto key = static_cast<Keys>(code);
            const auto name = KeyName(key);
            if (!name.empty()) {
                EXPECT_EQ(KeyFromName(name), key) << name;
            }
        }
    }

    TEST(InputNamesTests, ButtonNamesExcludeExtensions)
    {
        EXPECT_EQ(ButtonName(Buttons::A), "A");
        EXPECT_EQ(ButtonFromName("dpadup"), Buttons::DPadUp);
        EXPECT_EQ(ButtonName(Buttons::Paddle1EXT), "");
        EXPECT_FALSE(ButtonFromName("Paddle1EXT").has_value());
    }

    TEST(InputNamesTests, BindingsFormatAndParse)
    {
        const Binding bindings[] = {
            Binding::ForKey(Keys::Space),
            Binding::ForMouse(MouseButton::XButton2),
            Binding::ForButton(Buttons::RightShoulder),
            Binding::ForAxis(GamePadAxis::LeftStickY, -1, 0.25f),
        };
        for (const auto& binding : bindings) {
            const auto text = FormatBinding(binding);
            const auto parsed = ParseBinding(text);
            ASSERT_TRUE(parsed.has_value()) << text;
            EXPECT_EQ(*parsed, binding) << text;
        }
        EXPECT_EQ(FormatBinding(Binding::ForAxis(GamePadAxis::LeftTrigger)), "PadAxis:LeftTrigger:+:0.5");
    }

    TEST(InputNamesTests, InvalidBindingsAreRejected)
    {
        EXPECT_FALSE(ParseBinding("Key:None").has_value());
        EXPECT_FALSE(ParseBinding("Key").has_value());
        EXPECT_FALSE(ParseBinding("Pad:Paddle1EXT").has_value());
        EXPECT_FALSE(ParseBinding("PadAxis:LeftTrigger:*:0.5").has_value());
        EXPECT_FALSE(ParseBinding("PadAxis:LeftTrigger:+:0").has_value());
        EXPECT_FALSE(ParseBinding("PadAxis:LeftTrigger:+:nan").has_value());
        EXPECT_FALSE(ParseBinding("Joystick:Fire").has_value());
    }

    TEST(InputNamesTests, AxisBindingsFormatAndParse)
    {
        const AxisBinding bindings[] = {
            AxisBinding::ForKeys(Keys::A, Keys::D),
            AxisBinding::ForGamePad(GamePadAxis::RightStickX, 0.15f, -2.0f),
            AxisBinding::ForMouse(MouseAxis::Wheel, 1.0f / 120.0f),
        };
        for (const auto& binding : bindings) {
            const auto text = FormatAxisBinding(binding);
            const auto parsed = ParseAxisBinding(text);
            ASSERT_TRUE(parsed.has_value()) << text;
            EXPECT_EQ(*parsed, binding) << text;
        }
        EXPECT_FALSE(ParseAxisBinding("PadAxis:LeftStickX:1:1").has_value());
    }
}
