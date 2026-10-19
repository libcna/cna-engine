// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/InputNames.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::Keys;

    namespace
    {
        template <typename T>
        struct NamedValue
        {
            T value;
            std::string_view name;
        };

        // XNA 4.0 Keys enumerators, in declaration order.
        constexpr NamedValue<Keys> KeyNames[] = {
            {Keys::None, "None"},
            {Keys::Back, "Back"},
            {Keys::Tab, "Tab"},
            {Keys::Enter, "Enter"},
            {Keys::Pause, "Pause"},
            {Keys::CapsLock, "CapsLock"},
            {Keys::Kana, "Kana"},
            {Keys::Kanji, "Kanji"},
            {Keys::Escape, "Escape"},
            {Keys::ImeConvert, "ImeConvert"},
            {Keys::ImeNoConvert, "ImeNoConvert"},
            {Keys::Space, "Space"},
            {Keys::PageUp, "PageUp"},
            {Keys::PageDown, "PageDown"},
            {Keys::End, "End"},
            {Keys::Home, "Home"},
            {Keys::Left, "Left"},
            {Keys::Up, "Up"},
            {Keys::Right, "Right"},
            {Keys::Down, "Down"},
            {Keys::Select, "Select"},
            {Keys::Print, "Print"},
            {Keys::Execute, "Execute"},
            {Keys::PrintScreen, "PrintScreen"},
            {Keys::Insert, "Insert"},
            {Keys::Delete, "Delete"},
            {Keys::Help, "Help"},
            {Keys::D0, "D0"},
            {Keys::D1, "D1"},
            {Keys::D2, "D2"},
            {Keys::D3, "D3"},
            {Keys::D4, "D4"},
            {Keys::D5, "D5"},
            {Keys::D6, "D6"},
            {Keys::D7, "D7"},
            {Keys::D8, "D8"},
            {Keys::D9, "D9"},
            {Keys::A, "A"},
            {Keys::B, "B"},
            {Keys::C, "C"},
            {Keys::D, "D"},
            {Keys::E, "E"},
            {Keys::F, "F"},
            {Keys::G, "G"},
            {Keys::H, "H"},
            {Keys::I, "I"},
            {Keys::J, "J"},
            {Keys::K, "K"},
            {Keys::L, "L"},
            {Keys::M, "M"},
            {Keys::N, "N"},
            {Keys::O, "O"},
            {Keys::P, "P"},
            {Keys::Q, "Q"},
            {Keys::R, "R"},
            {Keys::S, "S"},
            {Keys::T, "T"},
            {Keys::U, "U"},
            {Keys::V, "V"},
            {Keys::W, "W"},
            {Keys::X, "X"},
            {Keys::Y, "Y"},
            {Keys::Z, "Z"},
            {Keys::LeftWindows, "LeftWindows"},
            {Keys::RightWindows, "RightWindows"},
            {Keys::Apps, "Apps"},
            {Keys::Sleep, "Sleep"},
            {Keys::NumPad0, "NumPad0"},
            {Keys::NumPad1, "NumPad1"},
            {Keys::NumPad2, "NumPad2"},
            {Keys::NumPad3, "NumPad3"},
            {Keys::NumPad4, "NumPad4"},
            {Keys::NumPad5, "NumPad5"},
            {Keys::NumPad6, "NumPad6"},
            {Keys::NumPad7, "NumPad7"},
            {Keys::NumPad8, "NumPad8"},
            {Keys::NumPad9, "NumPad9"},
            {Keys::Multiply, "Multiply"},
            {Keys::Add, "Add"},
            {Keys::Separator, "Separator"},
            {Keys::Subtract, "Subtract"},
            {Keys::Decimal, "Decimal"},
            {Keys::Divide, "Divide"},
            {Keys::F1, "F1"},
            {Keys::F2, "F2"},
            {Keys::F3, "F3"},
            {Keys::F4, "F4"},
            {Keys::F5, "F5"},
            {Keys::F6, "F6"},
            {Keys::F7, "F7"},
            {Keys::F8, "F8"},
            {Keys::F9, "F9"},
            {Keys::F10, "F10"},
            {Keys::F11, "F11"},
            {Keys::F12, "F12"},
            {Keys::F13, "F13"},
            {Keys::F14, "F14"},
            {Keys::F15, "F15"},
            {Keys::F16, "F16"},
            {Keys::F17, "F17"},
            {Keys::F18, "F18"},
            {Keys::F19, "F19"},
            {Keys::F20, "F20"},
            {Keys::F21, "F21"},
            {Keys::F22, "F22"},
            {Keys::F23, "F23"},
            {Keys::F24, "F24"},
            {Keys::NumLock, "NumLock"},
            {Keys::Scroll, "Scroll"},
            {Keys::LeftShift, "LeftShift"},
            {Keys::RightShift, "RightShift"},
            {Keys::LeftControl, "LeftControl"},
            {Keys::RightControl, "RightControl"},
            {Keys::LeftAlt, "LeftAlt"},
            {Keys::RightAlt, "RightAlt"},
            {Keys::BrowserBack, "BrowserBack"},
            {Keys::BrowserForward, "BrowserForward"},
            {Keys::BrowserRefresh, "BrowserRefresh"},
            {Keys::BrowserStop, "BrowserStop"},
            {Keys::BrowserSearch, "BrowserSearch"},
            {Keys::BrowserFavorites, "BrowserFavorites"},
            {Keys::BrowserHome, "BrowserHome"},
            {Keys::VolumeMute, "VolumeMute"},
            {Keys::VolumeDown, "VolumeDown"},
            {Keys::VolumeUp, "VolumeUp"},
            {Keys::MediaNextTrack, "MediaNextTrack"},
            {Keys::MediaPreviousTrack, "MediaPreviousTrack"},
            {Keys::MediaStop, "MediaStop"},
            {Keys::MediaPlayPause, "MediaPlayPause"},
            {Keys::LaunchMail, "LaunchMail"},
            {Keys::SelectMedia, "SelectMedia"},
            {Keys::LaunchApplication1, "LaunchApplication1"},
            {Keys::LaunchApplication2, "LaunchApplication2"},
            {Keys::OemSemicolon, "OemSemicolon"},
            {Keys::OemPlus, "OemPlus"},
            {Keys::OemComma, "OemComma"},
            {Keys::OemMinus, "OemMinus"},
            {Keys::OemPeriod, "OemPeriod"},
            {Keys::OemQuestion, "OemQuestion"},
            {Keys::OemTilde, "OemTilde"},
            {Keys::ChatPadGreen, "ChatPadGreen"},
            {Keys::ChatPadOrange, "ChatPadOrange"},
            {Keys::OemOpenBrackets, "OemOpenBrackets"},
            {Keys::OemPipe, "OemPipe"},
            {Keys::OemCloseBrackets, "OemCloseBrackets"},
            {Keys::OemQuotes, "OemQuotes"},
            {Keys::Oem8, "Oem8"},
            {Keys::OemBackslash, "OemBackslash"},
            {Keys::ProcessKey, "ProcessKey"},
            {Keys::OemCopy, "OemCopy"},
            {Keys::OemAuto, "OemAuto"},
            {Keys::OemEnlW, "OemEnlW"},
            {Keys::Attn, "Attn"},
            {Keys::Crsel, "Crsel"},
            {Keys::Exsel, "Exsel"},
            {Keys::EraseEof, "EraseEof"},
            {Keys::Play, "Play"},
            {Keys::Zoom, "Zoom"},
            {Keys::Pa1, "Pa1"},
            {Keys::OemClear, "OemClear"},
        };

        // XNA 4.0 Buttons enumerators. CNA's EXT-suffixed extension buttons are
        // deliberately absent: the names are part of the strict surface.
        constexpr NamedValue<Buttons> ButtonNames[] = {
            {Buttons::DPadUp, "DPadUp"},
            {Buttons::DPadDown, "DPadDown"},
            {Buttons::DPadLeft, "DPadLeft"},
            {Buttons::DPadRight, "DPadRight"},
            {Buttons::Start, "Start"},
            {Buttons::Back, "Back"},
            {Buttons::LeftStick, "LeftStick"},
            {Buttons::RightStick, "RightStick"},
            {Buttons::LeftShoulder, "LeftShoulder"},
            {Buttons::RightShoulder, "RightShoulder"},
            {Buttons::BigButton, "BigButton"},
            {Buttons::A, "A"},
            {Buttons::B, "B"},
            {Buttons::X, "X"},
            {Buttons::Y, "Y"},
            {Buttons::LeftThumbstickLeft, "LeftThumbstickLeft"},
            {Buttons::RightTrigger, "RightTrigger"},
            {Buttons::LeftTrigger, "LeftTrigger"},
            {Buttons::RightThumbstickUp, "RightThumbstickUp"},
            {Buttons::RightThumbstickDown, "RightThumbstickDown"},
            {Buttons::RightThumbstickRight, "RightThumbstickRight"},
            {Buttons::RightThumbstickLeft, "RightThumbstickLeft"},
            {Buttons::LeftThumbstickUp, "LeftThumbstickUp"},
            {Buttons::LeftThumbstickDown, "LeftThumbstickDown"},
            {Buttons::LeftThumbstickRight, "LeftThumbstickRight"},
        };

        constexpr NamedValue<MouseButton> MouseButtonNames[] = {
            {MouseButton::Left, "Left"},
            {MouseButton::Right, "Right"},
            {MouseButton::Middle, "Middle"},
            {MouseButton::XButton1, "XButton1"},
            {MouseButton::XButton2, "XButton2"},
        };

        constexpr NamedValue<GamePadAxis> GamePadAxisNames[] = {
            {GamePadAxis::LeftStickX, "LeftStickX"},
            {GamePadAxis::LeftStickY, "LeftStickY"},
            {GamePadAxis::RightStickX, "RightStickX"},
            {GamePadAxis::RightStickY, "RightStickY"},
            {GamePadAxis::LeftTrigger, "LeftTrigger"},
            {GamePadAxis::RightTrigger, "RightTrigger"},
        };

        constexpr NamedValue<MouseAxis> MouseAxisNames[] = {
            {MouseAxis::DeltaX, "DeltaX"},
            {MouseAxis::DeltaY, "DeltaY"},
            {MouseAxis::Wheel, "Wheel"},
        };

        bool EqualsIgnoreCase(std::string_view a, std::string_view b)
        {
            return a.size() == b.size()
                   && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                          auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
                          return lower(x) == lower(y);
                      });
        }

        template <typename T, std::size_t N>
        std::string_view NameOf(const NamedValue<T> (&table)[N], T value)
        {
            for (const auto& entry : table) {
                if (entry.value == value) {
                    return entry.name;
                }
            }
            return {};
        }

        template <typename T, std::size_t N>
        std::optional<T> ValueOf(const NamedValue<T> (&table)[N], std::string_view name)
        {
            for (const auto& entry : table) {
                if (EqualsIgnoreCase(entry.name, name)) {
                    return entry.value;
                }
            }
            return std::nullopt;
        }

        std::vector<std::string_view> Split(std::string_view text, char separator)
        {
            std::vector<std::string_view> parts;
            std::size_t start = 0;
            while (true) {
                const std::size_t end = text.find(separator, start);
                parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
                if (end == std::string_view::npos) {
                    return parts;
                }
                start = end + 1;
            }
        }

        std::optional<float> ParseFloat(std::string_view text)
        {
            float value = 0.0f;
            const char* first = text.data();
            const char* last = text.data() + text.size();
            if (first != last && *first == '+') {
                ++first;
            }
            const auto [ptr, ec] = std::from_chars(first, last, value);
            if (ec != std::errc{} || ptr != last || !std::isfinite(value)) {
                return std::nullopt;
            }
            return value;
        }

        std::string FormatFloat(float value)
        {
            std::array<char, 32> buffer{};
            const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            return ec == std::errc{} ? std::string(buffer.data(), ptr) : std::string("0");
        }
    }

    std::string_view KeyName(const Keys key) { return NameOf(KeyNames, key); }
    std::optional<Keys> KeyFromName(const std::string_view name) { return ValueOf(KeyNames, name); }

    std::string_view ButtonName(const Buttons button) { return NameOf(ButtonNames, button); }
    std::optional<Buttons> ButtonFromName(const std::string_view name) { return ValueOf(ButtonNames, name); }

    std::string_view MouseButtonName(const MouseButton button) { return NameOf(MouseButtonNames, button); }
    std::optional<MouseButton> MouseButtonFromName(const std::string_view name)
    {
        return ValueOf(MouseButtonNames, name);
    }

    std::string_view GamePadAxisName(const GamePadAxis axis) { return NameOf(GamePadAxisNames, axis); }
    std::optional<GamePadAxis> GamePadAxisFromName(const std::string_view name)
    {
        return ValueOf(GamePadAxisNames, name);
    }

    std::string_view MouseAxisName(const MouseAxis axis) { return NameOf(MouseAxisNames, axis); }
    std::optional<MouseAxis> MouseAxisFromName(const std::string_view name) { return ValueOf(MouseAxisNames, name); }

    std::string FormatBinding(const Binding& binding)
    {
        switch (binding.kind) {
            case Binding::Kind::Key:
                return "Key:" + std::string(KeyName(static_cast<Keys>(binding.code)));
            case Binding::Kind::MouseButton:
                return "Mouse:" + std::string(MouseButtonName(static_cast<MouseButton>(binding.code)));
            case Binding::Kind::GamePadButton:
                return "Pad:" + std::string(ButtonName(static_cast<Buttons>(binding.code)));
            case Binding::Kind::GamePadAxis:
                return "PadAxis:" + std::string(GamePadAxisName(static_cast<GamePadAxis>(binding.code)))
                       + (binding.direction < 0 ? ":-:" : ":+:") + FormatFloat(binding.threshold);
        }
        return {};
    }

    std::optional<Binding> ParseBinding(const std::string_view text)
    {
        const auto parts = Split(text, ':');
        if (parts.size() == 2 && EqualsIgnoreCase(parts[0], "Key")) {
            if (const auto key = KeyFromName(parts[1]); key && *key != Keys::None) {
                return Binding::ForKey(*key);
            }
        } else if (parts.size() == 2 && EqualsIgnoreCase(parts[0], "Mouse")) {
            if (const auto button = MouseButtonFromName(parts[1])) {
                return Binding::ForMouse(*button);
            }
        } else if (parts.size() == 2 && EqualsIgnoreCase(parts[0], "Pad")) {
            if (const auto button = ButtonFromName(parts[1])) {
                return Binding::ForButton(*button);
            }
        } else if (parts.size() == 4 && EqualsIgnoreCase(parts[0], "PadAxis")) {
            const auto axis = GamePadAxisFromName(parts[1]);
            const auto threshold = ParseFloat(parts[3]);
            if (axis && threshold && *threshold > 0.0f && *threshold <= 1.0f && (parts[2] == "+" || parts[2] == "-")) {
                return Binding::ForAxis(*axis, parts[2] == "-" ? -1 : 1, *threshold);
            }
        }
        return std::nullopt;
    }

    std::string FormatAxisBinding(const AxisBinding& binding)
    {
        switch (binding.kind) {
            case AxisBinding::Kind::KeyPair:
                return "Keys:" + std::string(KeyName(binding.negative)) + ":" + std::string(KeyName(binding.positive))
                       + ":" + FormatFloat(binding.scale);
            case AxisBinding::Kind::GamePad:
                return "PadAxis:" + std::string(GamePadAxisName(binding.padAxis)) + ":" + FormatFloat(binding.deadZone)
                       + ":" + FormatFloat(binding.scale)
                       + (binding.exponent != 1.0f ? ":" + FormatFloat(binding.exponent) : std::string());
            case AxisBinding::Kind::Mouse:
                return "MouseAxis:" + std::string(MouseAxisName(binding.mouseAxis)) + ":" + FormatFloat(binding.scale);
        }
        return {};
    }

    std::optional<AxisBinding> ParseAxisBinding(const std::string_view text)
    {
        const auto parts = Split(text, ':');
        if (parts.size() == 4 && EqualsIgnoreCase(parts[0], "Keys")) {
            const auto negative = KeyFromName(parts[1]);
            const auto positive = KeyFromName(parts[2]);
            const auto scale = ParseFloat(parts[3]);
            if (negative && positive && scale) {
                return AxisBinding::ForKeys(*negative, *positive, *scale);
            }
        } else if ((parts.size() == 4 || parts.size() == 5) && EqualsIgnoreCase(parts[0], "PadAxis")) {
            const auto axis = GamePadAxisFromName(parts[1]);
            const auto deadZone = ParseFloat(parts[2]);
            const auto scale = ParseFloat(parts[3]);
            const auto exponent = parts.size() == 5 ? ParseFloat(parts[4]) : std::optional<float>(1.0f);
            if (axis && deadZone && scale && exponent && *deadZone >= 0.0f && *deadZone < 1.0f && *exponent > 0.0f) {
                return AxisBinding::ForGamePad(*axis, *deadZone, *scale, *exponent);
            }
        } else if (parts.size() == 3 && EqualsIgnoreCase(parts[0], "MouseAxis")) {
            const auto axis = MouseAxisFromName(parts[1]);
            const auto scale = ParseFloat(parts[2]);
            if (axis && scale) {
                return AxisBinding::ForMouse(*axis, *scale);
            }
        }
        return std::nullopt;
    }
}
