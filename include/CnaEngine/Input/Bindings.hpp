// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Input/Buttons.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

#include <compare>
#include <cstdint>

namespace CnaEngine::Input
{
    /// Mouse buttons addressable by a binding (the five buttons of XNA's MouseState).
    enum class MouseButton : std::uint8_t
    {
        Left,
        Right,
        Middle,
        XButton1,
        XButton2,
    };

    /// Analog game-pad controls. Sticks report -1..1 (Y up is positive, as in
    /// XNA); triggers report 0..1.
    enum class GamePadAxis : std::uint8_t
    {
        LeftStickX,
        LeftStickY,
        RightStickX,
        RightStickY,
        LeftTrigger,
        RightTrigger,
    };

    /// Relative mouse controls. Deltas are measured between consecutive
    /// snapshots in pixels; the wheel in XNA wheel units (120 per notch).
    enum class MouseAxis : std::uint8_t
    {
        DeltaX,
        DeltaY,
        Wheel,
    };

    /// A physical input that drives a digital action.
    struct Binding
    {
        enum class Kind : std::uint8_t
        {
            Key,
            MouseButton,
            GamePadButton,
            /// An analog pad control treated as a button: held while the
            /// signed value crosses `threshold` in the bound direction.
            GamePadAxis,
        };

        Kind kind = Kind::Key;
        /// Keys, MouseButton, Buttons or GamePadAxis value, by `kind`.
        std::uint32_t code = 0;
        /// GamePadAxis only: +1 or -1.
        std::int8_t direction = 1;
        /// GamePadAxis only: magnitude at which the control counts as held.
        float threshold = 0.5f;

        [[nodiscard]] static Binding ForKey(Microsoft::Xna::Framework::Input::Keys key) noexcept
        {
            return {Kind::Key, static_cast<std::uint32_t>(key)};
        }
        [[nodiscard]] static Binding ForMouse(MouseButton button) noexcept
        {
            return {Kind::MouseButton, static_cast<std::uint32_t>(button)};
        }
        [[nodiscard]] static Binding ForButton(Microsoft::Xna::Framework::Input::Buttons button) noexcept
        {
            return {Kind::GamePadButton, static_cast<std::uint32_t>(button)};
        }
        [[nodiscard]] static Binding ForAxis(GamePadAxis axis, int direction = 1, float threshold = 0.5f) noexcept
        {
            return {Kind::GamePadAxis, static_cast<std::uint32_t>(axis),
                    static_cast<std::int8_t>(direction < 0 ? -1 : 1), threshold};
        }

        /// Same physical input (threshold excluded: two thresholds on the same
        /// direction of the same control are the same binding).
        [[nodiscard]] bool SameInput(const Binding& other) const noexcept
        {
            return kind == other.kind && code == other.code
                   && (kind != Kind::GamePadAxis || direction == other.direction);
        }

        friend bool operator==(const Binding&, const Binding&) = default;
    };

    /// A physical input that contributes to an analog axis.
    struct AxisBinding
    {
        enum class Kind : std::uint8_t
        {
            /// Two keys: `negative` adds -1, `positive` adds +1.
            KeyPair,
            GamePad,
            Mouse,
        };

        Kind kind = Kind::KeyPair;
        Microsoft::Xna::Framework::Input::Keys negative = Microsoft::Xna::Framework::Input::Keys::None;
        Microsoft::Xna::Framework::Input::Keys positive = Microsoft::Xna::Framework::Input::Keys::None;
        GamePadAxis padAxis = GamePadAxis::LeftStickX;
        MouseAxis mouseAxis = MouseAxis::DeltaX;
        /// GamePad only: values with a smaller magnitude read as 0; the rest is
        /// rescaled so the output still spans the full range.
        float deadZone = 0.0f;
        /// Multiplier applied after the dead zone. Negative inverts the axis.
        float scale = 1.0f;
        /// GamePad only: response curve exponent applied to the magnitude
        /// after the dead zone (1 = linear; > 1 gives finer control near the
        /// centre while still reaching full deflection).
        float exponent = 1.0f;

        [[nodiscard]] static AxisBinding ForKeys(Microsoft::Xna::Framework::Input::Keys negativeKey,
                                                 Microsoft::Xna::Framework::Input::Keys positiveKey,
                                                 float scale = 1.0f) noexcept
        {
            AxisBinding b;
            b.kind = Kind::KeyPair;
            b.negative = negativeKey;
            b.positive = positiveKey;
            b.scale = scale;
            return b;
        }
        [[nodiscard]] static AxisBinding ForGamePad(GamePadAxis axis, float deadZone = 0.2f, float scale = 1.0f,
                                                    float exponent = 1.0f) noexcept
        {
            AxisBinding b;
            b.kind = Kind::GamePad;
            b.padAxis = axis;
            b.deadZone = deadZone;
            b.scale = scale;
            b.exponent = exponent;
            return b;
        }
        [[nodiscard]] static AxisBinding ForMouse(MouseAxis axis, float scale = 1.0f) noexcept
        {
            AxisBinding b;
            b.kind = Kind::Mouse;
            b.mouseAxis = axis;
            b.scale = scale;
            return b;
        }

        friend bool operator==(const AxisBinding&, const AxisBinding&) = default;
    };
}
