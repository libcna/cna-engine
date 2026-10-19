// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/ActionMap.hpp"

#include "CnaEngine/Input/InputNames.hpp"

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadThumbSticks.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadTriggers.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::Keys;

    namespace
    {
        float PadAxisValue(const InputSnapshot& snapshot, const GamePadAxis axis)
        {
            const auto& pad = snapshot.gamePad;
            if (!pad.getIsConnectedProperty()) {
                return 0.0f;
            }
            switch (axis) {
                case GamePadAxis::LeftStickX: return pad.getThumbSticksProperty().getLeftProperty().X;
                case GamePadAxis::LeftStickY: return pad.getThumbSticksProperty().getLeftProperty().Y;
                case GamePadAxis::RightStickX: return pad.getThumbSticksProperty().getRightProperty().X;
                case GamePadAxis::RightStickY: return pad.getThumbSticksProperty().getRightProperty().Y;
                case GamePadAxis::LeftTrigger: return pad.getTriggersProperty().getLeftProperty();
                case GamePadAxis::RightTrigger: return pad.getTriggersProperty().getRightProperty();
            }
            return 0.0f;
        }

        bool MouseButtonDown(const InputSnapshot& snapshot, const MouseButton button)
        {
            const auto& mouse = snapshot.mouse;
            switch (button) {
                case MouseButton::Left: return mouse.getLeftButtonProperty() == ButtonState::Pressed;
                case MouseButton::Right: return mouse.getRightButtonProperty() == ButtonState::Pressed;
                case MouseButton::Middle: return mouse.getMiddleButtonProperty() == ButtonState::Pressed;
                case MouseButton::XButton1: return mouse.getXButton1Property() == ButtonState::Pressed;
                case MouseButton::XButton2: return mouse.getXButton2Property() == ButtonState::Pressed;
            }
            return false;
        }

        float ApplyDeadZone(const float value, const float deadZone)
        {
            const float magnitude = std::fabs(value);
            if (magnitude <= deadZone) {
                return 0.0f;
            }
            const float rescaled = std::min(1.0f, (magnitude - deadZone) / (1.0f - deadZone));
            return value < 0.0f ? -rescaled : rescaled;
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
                text.remove_prefix(1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
                text.remove_suffix(1);
            }
            return text;
        }

        std::vector<std::string_view> SplitList(std::string_view text)
        {
            std::vector<std::string_view> items;
            text = Trim(text);
            if (text.empty()) {
                return items;
            }
            std::size_t start = 0;
            while (true) {
                const std::size_t comma = text.find(',', start);
                items.push_back(Trim(text.substr(start, comma == std::string_view::npos ? std::string_view::npos
                                                                                       : comma - start)));
                if (comma == std::string_view::npos) {
                    return items;
                }
                start = comma + 1;
            }
        }
    }

    ActionId ActionMap::AddAction(const std::string_view name)
    {
        if (name.empty() || name.find_first_of("=,\n") != std::string_view::npos) {
            throw std::invalid_argument("ActionMap: action names must be non-empty and contain no '=', ',' or newline");
        }
        if (const auto existing = FindAction(name)) {
            return *existing;
        }
        actions_.push_back(ActionState{std::string(name), {}, false, false, false});
        return static_cast<ActionId>(actions_.size() - 1);
    }

    AxisId ActionMap::AddAxis(const std::string_view name)
    {
        if (name.empty() || name.find_first_of("=,\n") != std::string_view::npos) {
            throw std::invalid_argument("ActionMap: axis names must be non-empty and contain no '=', ',' or newline");
        }
        if (const auto existing = FindAxis(name)) {
            return *existing;
        }
        axes_.push_back(AxisState{std::string(name), {}, 0.0f});
        return static_cast<AxisId>(axes_.size() - 1);
    }

    std::optional<ActionId> ActionMap::FindAction(const std::string_view name) const
    {
        for (std::size_t i = 0; i < actions_.size(); ++i) {
            if (actions_[i].name == name) {
                return static_cast<ActionId>(i);
            }
        }
        return std::nullopt;
    }

    std::optional<AxisId> ActionMap::FindAxis(const std::string_view name) const
    {
        for (std::size_t i = 0; i < axes_.size(); ++i) {
            if (axes_[i].name == name) {
                return static_cast<AxisId>(i);
            }
        }
        return std::nullopt;
    }

    const std::string& ActionMap::ActionName(const ActionId action) const { return ActionAt(action).name; }
    const std::string& ActionMap::AxisName(const AxisId axis) const { return AxisAt(axis).name; }

    bool ActionMap::Bind(const ActionId action, const Binding& binding)
    {
        auto& bindings = ActionAt(action).bindings;
        if (std::any_of(bindings.begin(), bindings.end(), [&](const Binding& b) { return b.SameInput(binding); })) {
            return false;
        }
        bindings.push_back(binding);
        return true;
    }

    bool ActionMap::Unbind(const ActionId action, const Binding& binding)
    {
        auto& bindings = ActionAt(action).bindings;
        const auto it =
            std::find_if(bindings.begin(), bindings.end(), [&](const Binding& b) { return b.SameInput(binding); });
        if (it == bindings.end()) {
            return false;
        }
        bindings.erase(it);
        return true;
    }

    bool ActionMap::Rebind(const ActionId action, const Binding& from, const Binding& to)
    {
        auto& bindings = ActionAt(action).bindings;
        const auto it = std::find_if(bindings.begin(), bindings.end(), [&](const Binding& b) { return b.SameInput(from); });
        if (it == bindings.end()) {
            return false;
        }
        // Rebinding onto an input the action already has collapses the two.
        const auto duplicate = std::find_if(bindings.begin(), bindings.end(), [&](const Binding& b) {
            return &b != &*it && b.SameInput(to);
        });
        *it = to;
        if (duplicate != bindings.end()) {
            bindings.erase(duplicate);
        }
        return true;
    }

    void ActionMap::ClearBindings(const ActionId action) { ActionAt(action).bindings.clear(); }

    const std::vector<Binding>& ActionMap::Bindings(const ActionId action) const { return ActionAt(action).bindings; }

    std::vector<ActionId> ActionMap::ActionsBoundTo(const Binding& binding) const
    {
        std::vector<ActionId> result;
        for (std::size_t i = 0; i < actions_.size(); ++i) {
            const auto& bindings = actions_[i].bindings;
            if (std::any_of(bindings.begin(), bindings.end(), [&](const Binding& b) { return b.SameInput(binding); })) {
                result.push_back(static_cast<ActionId>(i));
            }
        }
        return result;
    }

    void ActionMap::BindAxis(const AxisId axis, const AxisBinding& binding) { AxisAt(axis).bindings.push_back(binding); }
    void ActionMap::ClearAxisBindings(const AxisId axis) { AxisAt(axis).bindings.clear(); }

    const std::vector<AxisBinding>& ActionMap::AxisBindings(const AxisId axis) const { return AxisAt(axis).bindings; }

    std::vector<BindingConflict> ActionMap::FindConflicts() const
    {
        std::vector<BindingConflict> conflicts;
        for (const auto& action : actions_) {
            for (const auto& binding : action.bindings) {
                const bool reported = std::any_of(conflicts.begin(), conflicts.end(), [&](const BindingConflict& c) {
                    return c.binding.SameInput(binding);
                });
                if (reported) {
                    continue;
                }
                auto users = ActionsBoundTo(binding);
                if (users.size() > 1) {
                    conflicts.push_back(BindingConflict{binding, std::move(users)});
                }
            }
        }
        return conflicts;
    }

    bool ActionMap::BindingDown(const Binding& binding, const InputSnapshot& snapshot)
    {
        switch (binding.kind) {
            case Binding::Kind::Key:
                return snapshot.keyboard.IsKeyDown(static_cast<Keys>(binding.code));
            case Binding::Kind::MouseButton:
                return MouseButtonDown(snapshot, static_cast<MouseButton>(binding.code));
            case Binding::Kind::GamePadButton:
                return snapshot.gamePad.getIsConnectedProperty()
                       && snapshot.gamePad.IsButtonDown(static_cast<Buttons>(binding.code));
            case Binding::Kind::GamePadAxis: {
                const float value = PadAxisValue(snapshot, static_cast<GamePadAxis>(binding.code)) * binding.direction;
                return value >= binding.threshold;
            }
        }
        return false;
    }

    float ActionMap::AxisValue(const AxisState& axis, const InputSnapshot& snapshot, const float mouseDeltaX,
                               const float mouseDeltaY, const float wheelDelta)
    {
        // Keys form one digital composite: the strongest push in each
        // direction, so opposite keys cancel and same-direction keys do not add.
        float keysPositive = 0.0f;
        float keysNegative = 0.0f;
        float analog = 0.0f;
        float unbounded = 0.0f;
        auto push = [&](const float part) {
            keysPositive = std::max(keysPositive, part);
            keysNegative = std::min(keysNegative, part);
        };
        for (const auto& binding : axis.bindings) {
            switch (binding.kind) {
                case AxisBinding::Kind::KeyPair:
                    if (binding.negative != Keys::None && snapshot.keyboard.IsKeyDown(binding.negative)) {
                        push(-binding.scale);
                    }
                    if (binding.positive != Keys::None && snapshot.keyboard.IsKeyDown(binding.positive)) {
                        push(binding.scale);
                    }
                    break;
                case AxisBinding::Kind::GamePad: {
                    float value = ApplyDeadZone(PadAxisValue(snapshot, binding.padAxis), binding.deadZone);
                    if (binding.exponent != 1.0f && value != 0.0f) {
                        value = std::copysign(std::pow(std::fabs(value), binding.exponent), value);
                    }
                    value *= binding.scale;
                    if (std::fabs(value) > std::fabs(analog)) {
                        analog = value;
                    }
                    break;
                }
                case AxisBinding::Kind::Mouse: {
                    const float delta = binding.mouseAxis == MouseAxis::DeltaX   ? mouseDeltaX
                                        : binding.mouseAxis == MouseAxis::DeltaY ? mouseDeltaY
                                                                                 : wheelDelta;
                    unbounded += delta * binding.scale;
                    break;
                }
            }
        }
        const float keys = keysPositive + keysNegative;
        // Keys and sticks compete: the larger magnitude wins; keys win a tie.
        const float bounded = std::fabs(analog) > std::fabs(keys) ? analog : keys;
        return std::clamp(bounded, -1.0f, 1.0f) + unbounded;
    }

    void ActionMap::Update(const InputSnapshot& snapshot)
    {
        const bool focused = snapshot.hasFocus;
        if (!focused && hadFocus_) {
            // Every action must be let go of before it can fire again.
            for (auto& action : actions_) {
                action.suppressed = true;
            }
        }
        hadFocus_ = focused;

        for (auto& action : actions_) {
            action.wasHeld = action.held;
            bool down = false;
            if (focused) {
                down = std::any_of(action.bindings.begin(), action.bindings.end(),
                                   [&](const Binding& b) { return BindingDown(b, snapshot); });
                if (action.suppressed) {
                    if (!down) {
                        action.suppressed = false;
                    }
                    down = false;
                }
            }
            action.held = down;
        }

        float dx = 0.0f;
        float dy = 0.0f;
        float dw = 0.0f;
        const int x = snapshot.mouse.getXProperty();
        const int y = snapshot.mouse.getYProperty();
        const int wheel = snapshot.mouse.getScrollWheelValueProperty();
        if (focused) {
            if (hasMouseReference_) {
                dx = static_cast<float>(x - mouseX_);
                dy = static_cast<float>(y - mouseY_);
                dw = static_cast<float>(wheel - wheel_);
            }
            hasMouseReference_ = true;
        } else {
            // The first focused snapshot re-establishes the reference instead
            // of reporting the cursor's whole journey outside the window.
            hasMouseReference_ = false;
        }
        mouseX_ = x;
        mouseY_ = y;
        wheel_ = wheel;

        for (auto& axis : axes_) {
            axis.value = focused ? AxisValue(axis, snapshot, dx, dy, dw) : 0.0f;
        }
    }

    void ActionMap::ResetState()
    {
        for (auto& action : actions_) {
            action.held = false;
            action.wasHeld = false;
            action.suppressed = false;
        }
        for (auto& axis : axes_) {
            axis.value = 0.0f;
        }
        hasMouseReference_ = false;
        hadFocus_ = true;
    }

    bool ActionMap::Held(const ActionId action) const { return ActionAt(action).held; }

    bool ActionMap::Pressed(const ActionId action) const
    {
        const auto& state = ActionAt(action);
        return state.held && !state.wasHeld;
    }

    bool ActionMap::Released(const ActionId action) const
    {
        const auto& state = ActionAt(action);
        return !state.held && state.wasHeld;
    }

    float ActionMap::Axis(const AxisId axis) const { return AxisAt(axis).value; }

    std::string ActionMap::SaveBindings() const
    {
        std::string text = "# cna-engine bindings v1\n";
        for (const auto& action : actions_) {
            text += "action." + action.name + "=";
            for (std::size_t i = 0; i < action.bindings.size(); ++i) {
                text += (i == 0 ? "" : ",") + FormatBinding(action.bindings[i]);
            }
            text += "\n";
        }
        for (const auto& axis : axes_) {
            text += "axis." + axis.name + "=";
            for (std::size_t i = 0; i < axis.bindings.size(); ++i) {
                text += (i == 0 ? "" : ",") + FormatAxisBinding(axis.bindings[i]);
            }
            text += "\n";
        }
        return text;
    }

    BindingLoadResult ActionMap::LoadBindings(const std::string_view text)
    {
        BindingLoadResult result;
        std::size_t lineStart = 0;
        int lineNumber = 0;
        while (lineStart <= text.size()) {
            const std::size_t lineEnd = text.find('\n', lineStart);
            const std::string_view line =
                Trim(text.substr(lineStart, lineEnd == std::string_view::npos ? std::string_view::npos : lineEnd - lineStart));
            ++lineNumber;
            lineStart = lineEnd == std::string_view::npos ? text.size() + 1 : lineEnd + 1;
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const std::string where = "line " + std::to_string(lineNumber) + ": ";
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos) {
                result.warnings.push_back(where + "missing '='");
                continue;
            }
            const std::string_view key = Trim(line.substr(0, equals));
            const auto items = SplitList(line.substr(equals + 1));

            if (key.starts_with("action.")) {
                const auto action = FindAction(key.substr(7));
                if (!action) {
                    result.warnings.push_back(where + "unknown action '" + std::string(key.substr(7)) + "'");
                    continue;
                }
                std::vector<Binding> parsed;
                bool valid = true;
                for (const auto item : items) {
                    const auto binding = ParseBinding(item);
                    if (!binding) {
                        result.warnings.push_back(where + "invalid binding '" + std::string(item) + "'");
                        valid = false;
                        break;
                    }
                    if (std::none_of(parsed.begin(), parsed.end(), [&](const Binding& b) { return b.SameInput(*binding); })) {
                        parsed.push_back(*binding);
                    }
                }
                if (valid) {
                    ActionAt(*action).bindings = std::move(parsed);
                    ++result.applied;
                }
            } else if (key.starts_with("axis.")) {
                const auto axis = FindAxis(key.substr(5));
                if (!axis) {
                    result.warnings.push_back(where + "unknown axis '" + std::string(key.substr(5)) + "'");
                    continue;
                }
                std::vector<AxisBinding> parsed;
                bool valid = true;
                for (const auto item : items) {
                    const auto binding = ParseAxisBinding(item);
                    if (!binding) {
                        result.warnings.push_back(where + "invalid axis binding '" + std::string(item) + "'");
                        valid = false;
                        break;
                    }
                    parsed.push_back(*binding);
                }
                if (valid) {
                    AxisAt(*axis).bindings = std::move(parsed);
                    ++result.applied;
                }
            } else {
                result.warnings.push_back(where + "expected 'action.<name>' or 'axis.<name>'");
            }
        }
        return result;
    }

    const ActionMap::ActionState& ActionMap::ActionAt(const ActionId action) const
    {
        if (action >= actions_.size()) {
            throw std::out_of_range("ActionMap: unknown action id");
        }
        return actions_[action];
    }

    ActionMap::ActionState& ActionMap::ActionAt(const ActionId action)
    {
        return const_cast<ActionState&>(std::as_const(*this).ActionAt(action));
    }

    const ActionMap::AxisState& ActionMap::AxisAt(const AxisId axis) const
    {
        if (axis >= axes_.size()) {
            throw std::out_of_range("ActionMap: unknown axis id");
        }
        return axes_[axis];
    }

    ActionMap::AxisState& ActionMap::AxisAt(const AxisId axis)
    {
        return const_cast<AxisState&>(std::as_const(*this).AxisAt(axis));
    }
}
