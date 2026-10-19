// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/BindingCapture.hpp"

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <gtest/gtest.h>

#include <initializer_list>

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::GamePadState;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;
    using Microsoft::Xna::Framework::Input::MouseState;
    using State = BindingCapture::State;

    namespace
    {
        InputSnapshot Keys_(std::initializer_list<Keys> keys)
        {
            InputSnapshot snapshot;
            snapshot.keyboard = KeyboardState(keys);
            return snapshot;
        }
    }

    TEST(BindingCaptureTests, KeyHeldAtStartIsIgnoredUntilReleased)
    {
        BindingCapture capture;
        capture.Begin(Keys_({Keys::Enter}));
        EXPECT_EQ(capture.Update(Keys_({Keys::Enter})), State::Listening);
        EXPECT_EQ(capture.Update(Keys_({})), State::Listening);
        EXPECT_EQ(capture.Update(Keys_({Keys::Enter})), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForKey(Keys::Enter));
    }

    TEST(BindingCaptureTests, CapturesNewKeyWhileOldOneIsStillHeld)
    {
        BindingCapture capture;
        capture.Begin(Keys_({Keys::Enter}));
        EXPECT_EQ(capture.Update(Keys_({Keys::Enter, Keys::J})), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForKey(Keys::J));
        // Further updates do not change a finished capture.
        EXPECT_EQ(capture.Update(Keys_({Keys::K})), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForKey(Keys::J));
    }

    TEST(BindingCaptureTests, EscapeCancels)
    {
        BindingCapture capture;
        capture.Begin(Keys_({}));
        EXPECT_EQ(capture.Update(Keys_({Keys::Escape})), State::Cancelled);
        EXPECT_FALSE(capture.Result().has_value());

        BindingCapture::Options noCancel;
        noCancel.cancelKey = Keys::None;
        capture.Begin(Keys_({}), noCancel);
        EXPECT_EQ(capture.Update(Keys_({Keys::Escape})), State::Captured);
    }

    TEST(BindingCaptureTests, CapturesMouseButtonsAndPadInputs)
    {
        BindingCapture capture;
        capture.Begin(InputSnapshot{});
        InputSnapshot mouse;
        mouse.mouse = MouseState(0, 0, 0, ButtonState::Released, ButtonState::Released, ButtonState::Pressed,
                                 ButtonState::Released, ButtonState::Released);
        EXPECT_EQ(capture.Update(mouse), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForMouse(MouseButton::Right));

        capture.Begin(InputSnapshot{});
        InputSnapshot pad;
        pad.gamePad = GamePadState(Vector2(0.0f, 0.0f), Vector2(0.0f, 0.0f), 0.0f, 0.0f, {Buttons::Y});
        EXPECT_EQ(capture.Update(pad), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForButton(Buttons::Y));

        capture.Begin(InputSnapshot{});
        InputSnapshot stick;
        stick.gamePad = GamePadState(Vector2(0.0f, -0.9f), Vector2(0.0f, 0.0f), 0.0f, 0.0f, {});
        EXPECT_EQ(capture.Update(stick), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForAxis(GamePadAxis::LeftStickY, -1, 0.5f));
    }

    TEST(BindingCaptureTests, DisabledDevicesAreIgnored)
    {
        BindingCapture capture;
        BindingCapture::Options options;
        options.keyboard = false;
        capture.Begin(InputSnapshot{}, options);
        EXPECT_EQ(capture.Update(Keys_({Keys::A})), State::Listening);
    }

    TEST(BindingCaptureTests, FocusLossRequiresReleaseAfterReturn)
    {
        BindingCapture capture;
        capture.Begin(Keys_({}));
        InputSnapshot away = Keys_({Keys::Tab});
        away.hasFocus = false;
        EXPECT_EQ(capture.Update(away), State::Listening);
        EXPECT_EQ(capture.Update(Keys_({Keys::Tab})), State::Listening) << "Alt+Tab leftovers are not captured";
        EXPECT_EQ(capture.Update(Keys_({Keys::Tab, Keys::Q})), State::Captured);
        EXPECT_EQ(capture.Result(), Binding::ForKey(Keys::Q));
    }
}
