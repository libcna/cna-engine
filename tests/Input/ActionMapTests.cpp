// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/ActionMap.hpp"

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <stdexcept>

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::GamePadState;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;
    using Microsoft::Xna::Framework::Input::MouseState;

    namespace
    {
        InputSnapshot Keyboard(std::initializer_list<Keys> keys, bool focus = true)
        {
            InputSnapshot snapshot;
            snapshot.keyboard = KeyboardState(keys);
            snapshot.hasFocus = focus;
            return snapshot;
        }

        InputSnapshot Mouse(int x, int y, int wheel, bool left = false)
        {
            InputSnapshot snapshot;
            const auto l = left ? ButtonState::Pressed : ButtonState::Released;
            snapshot.mouse = MouseState(x, y, wheel, l, ButtonState::Released, ButtonState::Released,
                                        ButtonState::Released, ButtonState::Released);
            return snapshot;
        }

        InputSnapshot Pad(Vector2 left, float leftTrigger, std::initializer_list<Buttons> buttons)
        {
            InputSnapshot snapshot;
            snapshot.gamePad = GamePadState(left, Vector2(0.0f, 0.0f), leftTrigger, 0.0f, buttons);
            return snapshot;
        }
    }

    TEST(ActionMapTests, DeclaringActionsIsIdempotent)
    {
        ActionMap map;
        const ActionId jump = map.AddAction("Jump");
        EXPECT_EQ(map.AddAction("Jump"), jump);
        EXPECT_EQ(map.FindAction("Jump"), jump);
        EXPECT_FALSE(map.FindAction("Fire").has_value());
        EXPECT_EQ(map.ActionName(jump), "Jump");
        EXPECT_THROW(map.AddAction(""), std::invalid_argument);
        EXPECT_THROW(map.AddAction("a=b"), std::invalid_argument);
        EXPECT_THROW((void)map.Held(42), std::out_of_range);
    }

    TEST(ActionMapTests, HeldPressedReleasedFollowSnapshots)
    {
        ActionMap map;
        const ActionId jump = map.AddAction("Jump");
        map.Bind(jump, Binding::ForKey(Keys::Space));
        map.Bind(jump, Binding::ForKey(Keys::W));

        map.Update(Keyboard({}));
        EXPECT_FALSE(map.Held(jump));
        EXPECT_FALSE(map.Pressed(jump));

        map.Update(Keyboard({Keys::Space}));
        EXPECT_TRUE(map.Held(jump));
        EXPECT_TRUE(map.Pressed(jump));
        EXPECT_FALSE(map.Released(jump));

        // Switching from one binding to another keeps the action held: no new press.
        map.Update(Keyboard({Keys::W}));
        EXPECT_TRUE(map.Held(jump));
        EXPECT_FALSE(map.Pressed(jump));

        map.Update(Keyboard({}));
        EXPECT_FALSE(map.Held(jump));
        EXPECT_TRUE(map.Released(jump));

        map.Update(Keyboard({}));
        EXPECT_FALSE(map.Released(jump));
    }

    TEST(ActionMapTests, MouseAndGamePadButtonsDriveActions)
    {
        ActionMap map;
        const ActionId fire = map.AddAction("Fire");
        map.Bind(fire, Binding::ForMouse(MouseButton::Left));
        map.Bind(fire, Binding::ForButton(Buttons::A));
        const ActionId brake = map.AddAction("Brake");
        map.Bind(brake, Binding::ForAxis(GamePadAxis::LeftTrigger, 1, 0.5f));

        map.Update(Mouse(0, 0, 0, true));
        EXPECT_TRUE(map.Pressed(fire));
        map.Update(Pad(Vector2(0.0f, 0.0f), 0.4f, {Buttons::A}));
        EXPECT_TRUE(map.Held(fire));
        EXPECT_FALSE(map.Held(brake));
        map.Update(Pad(Vector2(0.0f, 0.0f), 0.6f, {}));
        EXPECT_FALSE(map.Held(fire));
        EXPECT_TRUE(map.Pressed(brake));
    }

    TEST(ActionMapTests, RebindingReplacesInPlaceAndCollapsesDuplicates)
    {
        ActionMap map;
        const ActionId jump = map.AddAction("Jump");
        EXPECT_TRUE(map.Bind(jump, Binding::ForKey(Keys::Space)));
        EXPECT_FALSE(map.Bind(jump, Binding::ForKey(Keys::Space)));
        EXPECT_TRUE(map.Bind(jump, Binding::ForKey(Keys::W)));

        EXPECT_TRUE(map.Rebind(jump, Binding::ForKey(Keys::Space), Binding::ForKey(Keys::J)));
        ASSERT_EQ(map.Bindings(jump).size(), 2u);
        EXPECT_EQ(map.Bindings(jump)[0], Binding::ForKey(Keys::J));

        EXPECT_TRUE(map.Rebind(jump, Binding::ForKey(Keys::J), Binding::ForKey(Keys::W)));
        ASSERT_EQ(map.Bindings(jump).size(), 1u);
        EXPECT_EQ(map.Bindings(jump)[0], Binding::ForKey(Keys::W));

        EXPECT_FALSE(map.Rebind(jump, Binding::ForKey(Keys::Q), Binding::ForKey(Keys::E)));
        EXPECT_TRUE(map.Unbind(jump, Binding::ForKey(Keys::W)));
        EXPECT_FALSE(map.Unbind(jump, Binding::ForKey(Keys::W)));
        EXPECT_TRUE(map.Bindings(jump).empty());
    }

    TEST(ActionMapTests, ConflictsListEveryActionSharingAnInput)
    {
        ActionMap map;
        const ActionId shiftUp = map.AddAction("ShiftUp");
        const ActionId run = map.AddAction("Run");
        const ActionId horn = map.AddAction("Horn");
        map.Bind(shiftUp, Binding::ForKey(Keys::LeftShift));
        map.Bind(run, Binding::ForKey(Keys::LeftShift));
        map.Bind(horn, Binding::ForKey(Keys::B));
        // Axis-as-button bindings conflict regardless of threshold.
        map.Bind(run, Binding::ForAxis(GamePadAxis::RightTrigger, 1, 0.3f));
        map.Bind(horn, Binding::ForAxis(GamePadAxis::RightTrigger, 1, 0.8f));

        const auto conflicts = map.FindConflicts();
        ASSERT_EQ(conflicts.size(), 2u);
        EXPECT_EQ(conflicts[0].binding, Binding::ForKey(Keys::LeftShift));
        EXPECT_EQ(conflicts[0].actions, (std::vector<ActionId>{shiftUp, run}));
        EXPECT_EQ(conflicts[1].actions, (std::vector<ActionId>{run, horn}));
    }

    TEST(ActionMapTests, KeyPairAndGamePadAxesCombineWithDeadZoneAndClamp)
    {
        ActionMap map;
        const AxisId steer = map.AddAxis("Steer");
        map.BindAxis(steer, AxisBinding::ForKeys(Keys::A, Keys::D));
        map.BindAxis(steer, AxisBinding::ForGamePad(GamePadAxis::LeftStickX, 0.2f));

        map.Update(Keyboard({Keys::D}));
        EXPECT_FLOAT_EQ(map.Axis(steer), 1.0f);
        map.Update(Keyboard({Keys::A, Keys::D}));
        EXPECT_FLOAT_EQ(map.Axis(steer), 0.0f);

        map.Update(Pad(Vector2(0.1f, 0.0f), 0.0f, {}));
        EXPECT_FLOAT_EQ(map.Axis(steer), 0.0f) << "inside the dead zone";
        map.Update(Pad(Vector2(0.6f, 0.0f), 0.0f, {}));
        EXPECT_NEAR(map.Axis(steer), 0.5f, 1e-6f) << "rescaled outside the dead zone";
        map.Update(Pad(Vector2(-1.0f, 0.0f), 0.0f, {}));
        EXPECT_FLOAT_EQ(map.Axis(steer), -1.0f);

        InputSnapshot both = Pad(Vector2(0.4f, 0.0f), 0.0f, {});
        both.keyboard = KeyboardState({Keys::A});
        map.Update(both);
        EXPECT_FLOAT_EQ(map.Axis(steer), -1.0f) << "largest magnitude wins, no summing";
        both = Pad(Vector2(-0.9f, 0.0f), 0.0f, {});
        both.keyboard = KeyboardState({});
        map.Update(both);
        EXPECT_NEAR(map.Axis(steer), -0.875f, 1e-6f);
    }

    TEST(ActionMapTests, KeyPairsFormOneComposite)
    {
        ActionMap map;
        const AxisId steer = map.AddAxis("Steer");
        map.BindAxis(steer, AxisBinding::ForKeys(Keys::A, Keys::D));
        map.BindAxis(steer, AxisBinding::ForKeys(Keys::Left, Keys::Right));
        map.Update(Keyboard({Keys::A, Keys::Right}));
        EXPECT_FLOAT_EQ(map.Axis(steer), 0.0f) << "opposite keys cancel across pairs";
        map.Update(Keyboard({Keys::A, Keys::Left}));
        EXPECT_FLOAT_EQ(map.Axis(steer), -1.0f) << "same direction does not add up";
    }

    TEST(ActionMapTests, GamePadResponseCurve)
    {
        ActionMap map;
        const AxisId steer = map.AddAxis("Steer");
        map.BindAxis(steer, AxisBinding::ForGamePad(GamePadAxis::LeftStickX, 0.0f, 1.0f, 2.0f));
        map.Update(Pad(Vector2(-0.5f, 0.0f), 0.0f, {}));
        EXPECT_FLOAT_EQ(map.Axis(steer), -0.25f) << "sign kept, magnitude curved";
        map.Update(Pad(Vector2(1.0f, 0.0f), 0.0f, {}));
        EXPECT_FLOAT_EQ(map.Axis(steer), 1.0f) << "full deflection still reaches 1";
    }

    TEST(ActionMapTests, MouseAxesReportDeltasBetweenSnapshots)
    {
        ActionMap map;
        const AxisId lookX = map.AddAxis("LookX");
        const AxisId zoom = map.AddAxis("Zoom");
        map.BindAxis(lookX, AxisBinding::ForMouse(MouseAxis::DeltaX, 0.5f));
        map.BindAxis(zoom, AxisBinding::ForMouse(MouseAxis::Wheel, 1.0f / 120.0f));

        map.Update(Mouse(100, 50, 0));
        EXPECT_FLOAT_EQ(map.Axis(lookX), 0.0f) << "first snapshot only sets the reference";
        map.Update(Mouse(110, 50, 240));
        EXPECT_FLOAT_EQ(map.Axis(lookX), 5.0f) << "mouse contributions are not clamped";
        EXPECT_FLOAT_EQ(map.Axis(zoom), 2.0f);
        map.Update(Mouse(110, 50, 240));
        EXPECT_FLOAT_EQ(map.Axis(lookX), 0.0f);
    }

    TEST(ActionMapTests, FocusLossReleasesAndSuppressesUntilLetGo)
    {
        ActionMap map;
        const ActionId fire = map.AddAction("Fire");
        map.Bind(fire, Binding::ForKey(Keys::Space));
        const AxisId lookX = map.AddAxis("LookX");
        map.BindAxis(lookX, AxisBinding::ForMouse(MouseAxis::DeltaX));

        map.Update(Keyboard({Keys::Space}));
        ASSERT_TRUE(map.Held(fire));

        map.Update(Keyboard({Keys::Space}, false));
        EXPECT_FALSE(map.Held(fire));
        EXPECT_TRUE(map.Released(fire));
        EXPECT_FLOAT_EQ(map.Axis(lookX), 0.0f);

        // Focus returns with the key still down: no phantom press.
        map.Update(Keyboard({Keys::Space}));
        EXPECT_FALSE(map.Held(fire));
        EXPECT_FALSE(map.Pressed(fire));

        map.Update(Keyboard({}));
        EXPECT_FALSE(map.Held(fire));
        map.Update(Keyboard({Keys::Space}));
        EXPECT_TRUE(map.Pressed(fire));
    }

    TEST(ActionMapTests, FocusLossDropsMouseReference)
    {
        ActionMap map;
        const AxisId lookX = map.AddAxis("LookX");
        map.BindAxis(lookX, AxisBinding::ForMouse(MouseAxis::DeltaX));

        map.Update(Mouse(10, 0, 0));
        InputSnapshot away = Mouse(500, 0, 0);
        away.hasFocus = false;
        map.Update(away);
        map.Update(Mouse(900, 0, 0));
        EXPECT_FLOAT_EQ(map.Axis(lookX), 0.0f) << "the cursor's journey outside the window is not a look delta";
        map.Update(Mouse(905, 0, 0));
        EXPECT_FLOAT_EQ(map.Axis(lookX), 5.0f);
    }

    TEST(ActionMapTests, BindingsSaveAndLoadRoundTrip)
    {
        ActionMap source;
        const ActionId jump = source.AddAction("Jump");
        const ActionId crouch = source.AddAction("Crouch");
        const AxisId move = source.AddAxis("MoveX");
        source.Bind(jump, Binding::ForKey(Keys::Space));
        source.Bind(jump, Binding::ForButton(Buttons::A));
        source.BindAxis(move, AxisBinding::ForKeys(Keys::A, Keys::D));
        source.BindAxis(move, AxisBinding::ForGamePad(GamePadAxis::LeftStickX, 0.25f));
        (void)crouch;

        const std::string text = source.SaveBindings();

        ActionMap target;
        const ActionId tJump = target.AddAction("Jump");
        const ActionId tCrouch = target.AddAction("Crouch");
        const AxisId tMove = target.AddAxis("MoveX");
        target.Bind(tCrouch, Binding::ForKey(Keys::C));
        const auto result = target.LoadBindings(text);
        EXPECT_TRUE(result.warnings.empty());
        EXPECT_EQ(result.applied, 3);
        EXPECT_EQ(target.Bindings(tJump), source.Bindings(jump));
        EXPECT_TRUE(target.Bindings(tCrouch).empty()) << "an empty saved list clears the defaults";
        EXPECT_EQ(target.AxisBindings(tMove), source.AxisBindings(move));
        EXPECT_EQ(target.SaveBindings(), text);
    }

    TEST(ActionMapTests, LoadReportsAndSkipsBadLines)
    {
        ActionMap map;
        const ActionId jump = map.AddAction("Jump");
        map.Bind(jump, Binding::ForKey(Keys::Space));

        const auto result = map.LoadBindings(
            "# comment\n"
            "\n"
            "action.Fly=Key:F\n"
            "action.Jump=Key:J,Key:Nope\n"
            "garbage\n"
            "axis.Turn=Keys:A:D:1\n"
            "other.Jump=Key:J\n");
        EXPECT_EQ(result.applied, 0);
        ASSERT_EQ(result.warnings.size(), 5u);
        EXPECT_NE(result.warnings[0].find("unknown action 'Fly'"), std::string::npos);
        EXPECT_NE(result.warnings[1].find("invalid binding 'Key:Nope'"), std::string::npos);
        ASSERT_EQ(map.Bindings(jump).size(), 1u) << "a line with any invalid binding is ignored whole";
        EXPECT_EQ(map.Bindings(jump)[0], Binding::ForKey(Keys::Space));
    }
}
