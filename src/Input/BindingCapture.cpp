// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/BindingCapture.hpp"

#include "CnaEngine/Input/InputNames.hpp"

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadThumbSticks.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadTriggers.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <algorithm>
#include <iterator>

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::Keys;

    void BindingCapture::Begin(const InputSnapshot& current, const Options& options)
    {
        options_ = options;
        state_ = State::Listening;
        result_.reset();
        ignored_ = Active(current);
    }

    std::vector<Binding> BindingCapture::Active(const InputSnapshot& snapshot) const
    {
        std::vector<Binding> active;
        if (options_.keyboard) {
            for (const Keys key : snapshot.keyboard.GetPressedKeys()) {
                active.push_back(Binding::ForKey(key));
            }
        }
        if (options_.mouse) {
            const auto& m = snapshot.mouse;
            const std::pair<MouseButton, ButtonState> buttons[] = {
                {MouseButton::Left, m.getLeftButtonProperty()},
                {MouseButton::Right, m.getRightButtonProperty()},
                {MouseButton::Middle, m.getMiddleButtonProperty()},
                {MouseButton::XButton1, m.getXButton1Property()},
                {MouseButton::XButton2, m.getXButton2Property()},
            };
            for (const auto& [button, state] : buttons) {
                if (state == ButtonState::Pressed) {
                    active.push_back(Binding::ForMouse(button));
                }
            }
        }
        const auto& pad = snapshot.gamePad;
        if (pad.getIsConnectedProperty()) {
            if (options_.gamePadButtons) {
                // Only XNA-named buttons are offered (the strict surface has no
                // names for CNA extension buttons, so they could not be saved).
                // With axes enabled, the stick and trigger "buttons" XNA derives
                // from analog values are reported as axis bindings instead.
                constexpr Buttons analogButtons[] = {
                    Buttons::LeftThumbstickUp,    Buttons::LeftThumbstickDown,  Buttons::LeftThumbstickLeft,
                    Buttons::LeftThumbstickRight, Buttons::RightThumbstickUp,   Buttons::RightThumbstickDown,
                    Buttons::RightThumbstickLeft, Buttons::RightThumbstickRight, Buttons::LeftTrigger,
                    Buttons::RightTrigger,
                };
                for (int bit = 0; bit < 32; ++bit) {
                    const auto button = static_cast<Buttons>(1u << bit);
                    const bool analog = std::find(std::begin(analogButtons), std::end(analogButtons), button)
                                        != std::end(analogButtons);
                    if (analog && options_.gamePadAxes) {
                        continue;
                    }
                    if (!ButtonName(button).empty() && pad.IsButtonDown(button)) {
                        active.push_back(Binding::ForButton(button));
                    }
                }
            }
            if (options_.gamePadAxes) {
                const auto& sticks = pad.getThumbSticksProperty();
                const auto& triggers = pad.getTriggersProperty();
                const std::pair<GamePadAxis, float> axes[] = {
                    {GamePadAxis::LeftStickX, sticks.getLeftProperty().X},
                    {GamePadAxis::LeftStickY, sticks.getLeftProperty().Y},
                    {GamePadAxis::RightStickX, sticks.getRightProperty().X},
                    {GamePadAxis::RightStickY, sticks.getRightProperty().Y},
                    {GamePadAxis::LeftTrigger, triggers.getLeftProperty()},
                    {GamePadAxis::RightTrigger, triggers.getRightProperty()},
                };
                for (const auto& [axis, value] : axes) {
                    if (value >= options_.axisThreshold) {
                        active.push_back(Binding::ForAxis(axis, 1, options_.axisThreshold));
                    } else if (value <= -options_.axisThreshold) {
                        active.push_back(Binding::ForAxis(axis, -1, options_.axisThreshold));
                    }
                }
            }
        }
        return active;
    }

    BindingCapture::State BindingCapture::Update(const InputSnapshot& snapshot)
    {
        if (state_ != State::Listening) {
            return state_;
        }
        if (!snapshot.hasFocus) {
            // Whatever is held when focus returns must be released first.
            ignored_ = Active(snapshot);
            return state_;
        }
        const auto active = Active(snapshot);
        // Forget ignored inputs that have been released.
        std::erase_if(ignored_, [&](const Binding& held) {
            return std::none_of(active.begin(), active.end(), [&](const Binding& b) { return b.SameInput(held); });
        });
        for (const auto& binding : active) {
            const bool ignored = std::any_of(ignored_.begin(), ignored_.end(),
                                             [&](const Binding& held) { return held.SameInput(binding); });
            if (ignored) {
                continue;
            }
            if (options_.cancelKey != Keys::None && binding == Binding::ForKey(options_.cancelKey)) {
                state_ = State::Cancelled;
                return state_;
            }
            result_ = binding;
            state_ = State::Captured;
            return state_;
        }
        return state_;
    }
}
