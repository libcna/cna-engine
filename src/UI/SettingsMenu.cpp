// SPDX-License-Identifier: MIT
#include "CnaEngine/UI/SettingsMenu.hpp"

#include "CnaEngine/Input/InputNames.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <variant>

namespace CnaEngine::UI
{
    namespace
    {
        // The value a setting shows: the override, else the preset's, else the default.
        Graphics::QualityValue Current(const Graphics::QualitySchema& schema, const Graphics::QualitySelection& selection,
                                       const Graphics::QualitySetting& setting)
        {
            if (const auto found = selection.overrides.find(setting.name); found != selection.overrides.end()) {
                return found->second;
            }
            if (const auto* preset = schema.PresetValues(selection.preset)) {
                if (const auto found = preset->find(setting.name); found != preset->end()) {
                    return found->second;
                }
            }
            return setting.defaultValue;
        }
    }

    void AddQualitySettings(Menu& menu, const Graphics::QualitySchema& schema, Graphics::QualitySelection& selection,
                            std::function<void()> changed)
    {
        auto notify = std::make_shared<std::function<void()>>(std::move(changed));
        const auto presets = schema.PresetNames();
        if (!presets.empty()) {
            const auto at = std::find(presets.begin(), presets.end(), selection.preset);
            menu.Choice("Quality", presets, at == presets.end() ? 0 : static_cast<int>(at - presets.begin()),
                        [&selection, presets, notify](const int chosen) {
                            selection.preset = presets[static_cast<std::size_t>(chosen)];
                            selection.overrides.clear();
                            if (*notify) {
                                (*notify)();
                            }
                        });
        }
        for (const auto& setting : schema.Settings()) {
            const Graphics::QualityValue value = Current(schema, selection, setting);
            const std::string name = setting.name;
            if (std::holds_alternative<bool>(setting.defaultValue)) {
                menu.Choice(name, {"Off", "On"}, std::get<bool>(value) ? 1 : 0, [&selection, name, notify](const int chosen) {
                    selection.overrides[name] = chosen == 1;
                    if (*notify) {
                        (*notify)();
                    }
                });
            } else if (setting.minimum && setting.maximum && *setting.minimum < *setting.maximum) {
                const bool whole = std::holds_alternative<int>(setting.defaultValue);
                const auto minimum = static_cast<float>(*setting.minimum);
                const auto maximum = static_cast<float>(*setting.maximum);
                const float current = whole ? static_cast<float>(std::get<int>(value)) : std::get<float>(value);
                const float step = whole ? 1.0f : (maximum - minimum) / 20.0f;
                menu.Slider(name, minimum, maximum, step, current, [&selection, name, whole, notify](const float chosen) {
                    if (whole) {
                        selection.overrides[name] = static_cast<int>(std::lround(chosen));
                    } else {
                        selection.overrides[name] = chosen;
                    }
                    if (*notify) {
                        (*notify)();
                    }
                });
            }
        }
    }

    RebindRows::RebindRows(Menu& menu, Input::ActionMap& actions, std::vector<Input::ActionId> rows)
        : menu_(menu), actions_(actions), rows_(std::move(rows))
    {
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            items_.push_back(menu_.ItemCount());
            menu_.Button({}, [this, i] { Begin(static_cast<int>(i)); });
        }
        Refresh();
    }

    void RebindRows::Refresh()
    {
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const auto& bindings = actions_.Bindings(rows_[i]);
            std::string text = actions_.ActionName(rows_[i]) + ": ";
            if (listening_ == static_cast<int>(i)) {
                text += "press a key...";
            } else {
                text += bindings.empty() ? std::string("-") : Input::DisplayName(bindings.front());
            }
            menu_.SetLabel(items_[i], text);
        }
    }

    void RebindRows::Begin(const int row)
    {
        listening_ = row;
        // The capture starts with the next snapshot, so the key that accepted
        // the row (still held) is ignored rather than taken.
        begin_ = true;
        Refresh();
    }

    bool RebindRows::Update(const Input::InputSnapshot& snapshot)
    {
        if (listening_ < 0) {
            return false;
        }
        if (begin_) {
            capture_.Begin(snapshot);
            begin_ = false;
            return false;
        }
        const auto state = capture_.Update(snapshot);
        if (state == Input::BindingCapture::State::Listening) {
            return false;
        }
        const Input::ActionId action = rows_[static_cast<std::size_t>(listening_)];
        bool changed = false;
        if (state == Input::BindingCapture::State::Captured && capture_.Result()) {
            const auto bindings = actions_.Bindings(action);
            if (bindings.empty()) {
                changed = actions_.Bind(action, *capture_.Result());
            } else {
                changed = actions_.Rebind(action, bindings.front(), *capture_.Result());
            }
        }
        listening_ = -1;
        // The key that was captured, still held, must not also act in the
        // menu: as after a focus loss, every action stays released until its
        // inputs have been let go.
        Input::InputSnapshot unfocused;
        unfocused.hasFocus = false;
        actions_.Update(unfocused);
        Refresh();
        return changed;
    }
}
