// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Input/Bindings.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace CnaEngine::Input
{
    /// Stable, locale-independent names used by binding files and settings
    /// screens. Key and button names are the XNA enumerator names ("Space",
    /// "LeftShift", "D1", "DPadUp"). Parsing is case-insensitive. Only XNA 4.0
    /// enumerators have names; CNA-specific extension values do not.

    [[nodiscard]] std::string_view KeyName(Microsoft::Xna::Framework::Input::Keys key);
    [[nodiscard]] std::optional<Microsoft::Xna::Framework::Input::Keys> KeyFromName(std::string_view name);

    [[nodiscard]] std::string_view ButtonName(Microsoft::Xna::Framework::Input::Buttons button);
    [[nodiscard]] std::optional<Microsoft::Xna::Framework::Input::Buttons> ButtonFromName(std::string_view name);

    [[nodiscard]] std::string_view MouseButtonName(MouseButton button);
    [[nodiscard]] std::optional<MouseButton> MouseButtonFromName(std::string_view name);

    [[nodiscard]] std::string_view GamePadAxisName(GamePadAxis axis);
    [[nodiscard]] std::optional<GamePadAxis> GamePadAxisFromName(std::string_view name);

    [[nodiscard]] std::string_view MouseAxisName(MouseAxis axis);
    [[nodiscard]] std::optional<MouseAxis> MouseAxisFromName(std::string_view name);

    /// Text form of a binding as written in binding files:
    /// "Key:Space", "Mouse:Left", "Pad:A", "PadAxis:LeftTrigger:+:0.5".
    [[nodiscard]] std::string FormatBinding(const Binding& binding);
    [[nodiscard]] std::optional<Binding> ParseBinding(std::string_view text);

    /// Text form of an axis binding:
    /// "Keys:A:D:1", "PadAxis:LeftStickX:0.2:1" (dead zone, scale, and an
    /// optional fifth field for a non-linear response exponent),
    /// "MouseAxis:DeltaX:0.01".
    [[nodiscard]] std::string FormatAxisBinding(const AxisBinding& binding);
    [[nodiscard]] std::optional<AxisBinding> ParseAxisBinding(std::string_view text);
}
