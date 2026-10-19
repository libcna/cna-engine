// SPDX-License-Identifier: MIT
//
// PILOT-002 probe: car-simulator's core controls expressed with the engine's
// ActionMap, checked against car-simulator's own InputMapper (compiled
// read-only from the pilot checkout) on identical input sequences.
//
// Findings recorded by this probe (see docs/pilots/car-simulator.md):
//  - Held is identical for every action, digital and analog-derived.
//  - Pressed differs by design in one case: car-simulator reports a press for
//    every binding that goes down, even while the action is already held by
//    another binding; the engine reports action-level edges. The test checks
//    the exact relation between the two.
//  - Analog steering/pedals map to engine axes with largest-magnitude
//    combination and a 1.6 response exponent and match DriverControls exactly.
#include "CnaEngine/Input/ActionMap.hpp"

#include "CarSim/Input/InputMapper.hpp"

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadButtons.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadDPad.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadThumbSticks.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadTriggers.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

namespace CnaEngine::Pilot
{
    using CarSim::Input::GameAction;
    using CarSim::Input::InputMapper;
    using Input::ActionId;
    using Input::ActionMap;
    using Input::AxisBinding;
    using Input::AxisId;
    using Input::Binding;
    using Input::GamePadAxis;
    using Input::InputSnapshot;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::GamePadButtons;
    using Microsoft::Xna::Framework::Input::GamePadDPad;
    using Microsoft::Xna::Framework::Input::GamePadState;
    using Microsoft::Xna::Framework::Input::GamePadThumbSticks;
    using Microsoft::Xna::Framework::Input::GamePadTriggers;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    namespace
    {
        constexpr auto ActionCount = static_cast<std::size_t>(GameAction::Count);
        // car-simulator curves the stick as |x|^1.6 and treats > 0.5 as held.
        constexpr float SteerExponent = 1.6f;
        const float SteerThreshold = std::pow(0.5f, 1.0f / SteerExponent);

        struct EngineControls
        {
            ActionMap map;
            std::array<ActionId, ActionCount> actions{};
            AxisId throttle = 0;
            AxisId brake = 0;
            AxisId steering = 0;
        };

        // Built from car-simulator's own default tables, so the probe follows
        // the pilot when its defaults change.
        EngineControls BuildEngineControls()
        {
            EngineControls c;
            for (std::size_t a = 0; a < ActionCount; ++a) {
                c.actions[a] = c.map.AddAction(CarSim::Input::ToString(static_cast<GameAction>(a)));
            }
            for (const auto& b : InputMapper::DefaultBindings()) {
                c.map.Bind(c.actions[static_cast<std::size_t>(b.action)], Binding::ForKey(b.key));
            }
            for (const auto& b : InputMapper::DefaultPadBindings()) {
                c.map.Bind(c.actions[static_cast<std::size_t>(b.action)], Binding::ForButton(b.button));
            }
            const auto id = [&](GameAction a) { return c.actions[static_cast<std::size_t>(a)]; };
            c.map.Bind(id(GameAction::Throttle), Binding::ForAxis(GamePadAxis::RightTrigger, 1, 0.5f));
            c.map.Bind(id(GameAction::Brake), Binding::ForAxis(GamePadAxis::LeftTrigger, 1, 0.5f));
            c.map.Bind(id(GameAction::SteerLeft), Binding::ForAxis(GamePadAxis::LeftStickX, -1, SteerThreshold));
            c.map.Bind(id(GameAction::SteerRight), Binding::ForAxis(GamePadAxis::LeftStickX, 1, SteerThreshold));

            // Pedals and wheel as analog axes (DriverControls).
            c.throttle = c.map.AddAxis("ThrottlePedal");
            c.map.BindAxis(c.throttle, AxisBinding::ForKeys(Keys::None, Keys::W));
            c.map.BindAxis(c.throttle, AxisBinding::ForKeys(Keys::None, Keys::Up));
            c.map.BindAxis(c.throttle, AxisBinding::ForGamePad(GamePadAxis::RightTrigger, 0.0f));
            c.brake = c.map.AddAxis("BrakePedal");
            c.map.BindAxis(c.brake, AxisBinding::ForKeys(Keys::None, Keys::S));
            c.map.BindAxis(c.brake, AxisBinding::ForKeys(Keys::None, Keys::Down));
            c.map.BindAxis(c.brake, AxisBinding::ForGamePad(GamePadAxis::LeftTrigger, 0.0f));
            c.steering = c.map.AddAxis("Steering");
            c.map.BindAxis(c.steering, AxisBinding::ForKeys(Keys::A, Keys::D));
            c.map.BindAxis(c.steering, AxisBinding::ForKeys(Keys::Left, Keys::Right));
            c.map.BindAxis(c.steering, AxisBinding::ForGamePad(GamePadAxis::LeftStickX, 0.0f, 1.0f, SteerExponent));
            return c;
        }

        constexpr Buttons PadButtons[] = {
            Buttons::DPadUp, Buttons::DPadDown, Buttons::DPadLeft,     Buttons::DPadRight,     Buttons::Start,
            Buttons::Back,   Buttons::LeftStick, Buttons::RightStick, Buttons::LeftShoulder, Buttons::RightShoulder,
            Buttons::A,      Buttons::B,         Buttons::X,           Buttons::Y,
        };

        // A deterministic input history: keys and buttons toggle with a small
        // probability per frame (so holds, presses and overlapping bindings
        // all occur), analog values jump between levels that avoid the exact
        // thresholds, and the pad occasionally disconnects.
        class InputScript
        {
        public:
            explicit InputScript(std::uint32_t seed) : random_(seed)
            {
                for (const auto& b : InputMapper::DefaultBindings()) {
                    keyPool_.push_back(b.key);
                }
                keyPool_.push_back(Keys::Q);
                keyPool_.push_back(Keys::F9); // unbound
            }

            InputSnapshot Next()
            {
                std::bernoulli_distribution toggleKey(0.08);
                for (const Keys key : keyPool_) {
                    if (toggleKey(random_)) {
                        if (!held_.erase(key)) {
                            held_.insert(key);
                        }
                    }
                }
                std::bernoulli_distribution toggleButton(0.1);
                for (const Buttons b : PadButtons) {
                    if (toggleButton(random_)) {
                        buttons_ ^= static_cast<std::uint32_t>(b);
                    }
                }
                std::uniform_int_distribution<int> level(-20, 20);
                std::bernoulli_distribution move(0.3);
                if (move(random_)) {
                    stickX_ = static_cast<float>(level(random_)) * 0.05f + 0.013f;
                    stickX_ = std::fmax(-1.0f, std::fmin(1.0f, stickX_));
                }
                if (move(random_)) {
                    leftTrigger_ = static_cast<float>(std::abs(level(random_))) * 0.05f + 0.007f;
                }
                if (move(random_)) {
                    rightTrigger_ = static_cast<float>(std::abs(level(random_))) * 0.05f + 0.007f;
                }
                std::bernoulli_distribution disconnect(0.03);
                if (disconnect(random_)) {
                    connected_ = !connected_;
                }

                InputSnapshot snapshot;
                snapshot.keyboard = KeyboardState(held_);
                if (connected_) {
                    const auto down = [&](Buttons b) {
                        return (buttons_ & static_cast<std::uint32_t>(b)) != 0 ? ButtonState::Pressed
                                                                               : ButtonState::Released;
                    };
                    snapshot.gamePad = GamePadState(
                        GamePadThumbSticks(Vector2(stickX_, 0.0f), Vector2(0.0f, 0.0f)),
                        GamePadTriggers(std::fmin(1.0f, leftTrigger_), std::fmin(1.0f, rightTrigger_)),
                        GamePadButtons(static_cast<Buttons>(buttons_ & ~0xFu)),
                        GamePadDPad(down(Buttons::DPadUp), down(Buttons::DPadDown), down(Buttons::DPadLeft),
                                    down(Buttons::DPadRight)));
                }
                return snapshot;
            }

        private:
            std::mt19937 random_;
            std::vector<Keys> keyPool_;
            std::unordered_set<Keys> held_;
            std::uint32_t buttons_ = 0;
            float stickX_ = 0.0f;
            float leftTrigger_ = 0.0f;
            float rightTrigger_ = 0.0f;
            bool connected_ = true;
        };
    }

    TEST(CarSimulatorControlsTests, EveryDefaultBindingIsExpressible)
    {
        const EngineControls engine = BuildEngineControls();
        EXPECT_EQ(engine.map.ActionCount(), ActionCount);
        // car-simulator deliberately shares keys between modes (Shift: ShiftUp
        // in the car, ToggleRun on foot); the engine reports them as conflicts
        // for a settings screen instead of rejecting them.
        const auto conflicts = engine.map.FindConflicts();
        const auto leftShift = std::find_if(conflicts.begin(), conflicts.end(), [](const Input::BindingConflict& c) {
            return c.binding == Binding::ForKey(Keys::LeftShift);
        });
        ASSERT_NE(leftShift, conflicts.end());
        EXPECT_EQ(leftShift->actions.size(), 2u);

        // The full binding set survives the engine's text persistence.
        EngineControls reloaded = BuildEngineControls();
        for (std::size_t a = 0; a < ActionCount; ++a) {
            reloaded.map.ClearBindings(reloaded.actions[a]);
        }
        const auto result = reloaded.map.LoadBindings(engine.map.SaveBindings());
        EXPECT_TRUE(result.warnings.empty());
        EXPECT_EQ(reloaded.map.SaveBindings(), engine.map.SaveBindings());
    }

    TEST(CarSimulatorControlsTests, HeldAndPressedMatchTheGameOverLongRandomHistories)
    {
        for (const std::uint32_t seed : {1u, 2u, 3u, 20261019u}) {
            EngineControls engine = BuildEngineControls();
            InputMapper game;
            InputScript script(seed);
            std::array<bool, ActionCount> heldBefore{};
            int pressesCompared = 0;
            int multiBindingPresses = 0;

            for (int frame = 0; frame < 1500; ++frame) {
                const InputSnapshot snapshot = script.Next();
                engine.map.Update(snapshot);
                game.Update(snapshot.keyboard);
                game.UpdatePad(snapshot.gamePad, false);

                for (std::size_t a = 0; a < ActionCount; ++a) {
                    const auto action = static_cast<GameAction>(a);
                    const ActionId id = engine.actions[a];
                    ASSERT_EQ(engine.map.Held(id), game.Held(action))
                        << "seed " << seed << " frame " << frame << " action " << CarSim::Input::ToString(action);

                    // Engine presses are action-level edges; the game's are
                    // binding-level edges of digital inputs. Exact relation:
                    const bool analogOnly = game.Held(action) && !game.Pressed(action) && !heldBefore[a];
                    if (!analogOnly) {
                        const bool expected = game.Pressed(action) && !heldBefore[a];
                        ASSERT_EQ(engine.map.Pressed(id), expected)
                            << "seed " << seed << " frame " << frame << " action " << CarSim::Input::ToString(action);
                        ++pressesCompared;
                    }
                    if (game.Pressed(action) && heldBefore[a]) {
                        ++multiBindingPresses;
                    }
                    heldBefore[a] = game.Held(action);
                }
            }
            EXPECT_GT(pressesCompared, 1000);
            EXPECT_GT(multiBindingPresses, 0) << "the documented difference actually occurred";
        }
    }

    TEST(CarSimulatorControlsTests, AnalogAxesMatchDriverControls)
    {
        EngineControls engine = BuildEngineControls();
        InputMapper game;
        InputScript script(77);
        for (int frame = 0; frame < 1500; ++frame) {
            const InputSnapshot snapshot = script.Next();
            engine.map.Update(snapshot);
            game.Update(snapshot.keyboard);
            game.UpdatePad(snapshot.gamePad, false);
            const auto controls = game.BuildDriverControls(CarSim::Sim::TransmissionMode::Manual);
            ASSERT_FLOAT_EQ(engine.map.Axis(engine.throttle), controls.throttle) << "frame " << frame;
            ASSERT_FLOAT_EQ(engine.map.Axis(engine.brake), controls.brake) << "frame " << frame;
            ASSERT_FLOAT_EQ(engine.map.Axis(engine.steering), controls.steering) << "frame " << frame;
        }
    }
}
