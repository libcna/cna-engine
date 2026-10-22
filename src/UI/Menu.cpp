// SPDX-License-Identifier: MIT
#include "CnaEngine/UI/Menu.hpp"

#include "Microsoft/Xna/Framework/Input/Buttons.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace CnaEngine::UI
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::Keys;

    MenuActions MenuActions::Declare(Input::ActionMap& actions)
    {
        MenuActions menu;
        auto declare = [&actions](const char* name, std::initializer_list<Keys> keys, std::initializer_list<Buttons> buttons) {
            const Input::ActionId action = actions.AddAction(name);
            for (const Keys key : keys) {
                actions.Bind(action, Input::Binding::ForKey(key));
            }
            for (const Buttons button : buttons) {
                actions.Bind(action, Input::Binding::ForButton(button));
            }
            return action;
        };
        menu.up = declare("MenuUp", {Keys::Up}, {Buttons::DPadUp});
        menu.down = declare("MenuDown", {Keys::Down}, {Buttons::DPadDown});
        menu.left = declare("MenuLeft", {Keys::Left}, {Buttons::DPadLeft});
        menu.right = declare("MenuRight", {Keys::Right}, {Buttons::DPadRight});
        menu.accept = declare("MenuAccept", {Keys::Enter, Keys::Space}, {Buttons::A});
        menu.back = declare("MenuBack", {Keys::Escape, Keys::Back}, {Buttons::B});
        return menu;
    }

    MenuInput ReadMenuInput(const Input::ActionMap& actions, const MenuActions& menu,
                            const std::optional<Vector2> pointer, const bool click)
    {
        MenuInput input;
        input.up = actions.Pressed(menu.up);
        input.down = actions.Pressed(menu.down);
        input.left = actions.Pressed(menu.left);
        input.right = actions.Pressed(menu.right);
        input.accept = actions.Pressed(menu.accept);
        input.back = actions.Pressed(menu.back);
        input.pointer = pointer;
        input.click = click;
        return input;
    }

    Menu::Menu(std::string title) : title_(std::move(title)) {}

    Menu& Menu::Add(Item item)
    {
        items_.push_back(std::move(item));
        if (!Selectable(selected_)) {
            for (std::size_t i = 0; i < items_.size(); ++i) {
                if (Selectable(i)) {
                    selected_ = i;
                    break;
                }
            }
        }
        return *this;
    }

    Menu& Menu::Button(std::string label, std::function<void()> activate)
    {
        Item item;
        item.kind = Kind::Button;
        item.label = std::move(label);
        item.activate = std::move(activate);
        return Add(std::move(item));
    }

    Menu& Menu::Choice(std::string label, std::vector<std::string> choices, const int selected,
                       std::function<void(int)> changed)
    {
        if (choices.empty()) {
            throw std::invalid_argument("Menu::Choice: needs at least one choice");
        }
        Item item;
        item.kind = Kind::Choice;
        item.label = std::move(label);
        item.choices = std::move(choices);
        item.choice = std::clamp(selected, 0, static_cast<int>(item.choices.size()) - 1);
        item.choiceChanged = std::move(changed);
        return Add(std::move(item));
    }

    Menu& Menu::Slider(std::string label, const float minimum, const float maximum, const float step, const float value,
                       std::function<void(float)> changed)
    {
        if (!(minimum < maximum) || !(step > 0.0f)) {
            throw std::invalid_argument("Menu::Slider: needs minimum < maximum and a positive step");
        }
        Item item;
        item.kind = Kind::Slider;
        item.label = std::move(label);
        item.minimum = minimum;
        item.maximum = maximum;
        item.step = step;
        item.value = std::clamp(value, minimum, maximum);
        item.sliderChanged = std::move(changed);
        return Add(std::move(item));
    }

    Menu& Menu::Label(std::string text)
    {
        Item item;
        item.kind = Kind::Label;
        item.label = std::move(text);
        return Add(std::move(item));
    }

    bool Menu::Selectable(const std::size_t item) const noexcept
    {
        return item < items_.size() && items_[item].kind != Kind::Label;
    }

    void Menu::Select(const std::size_t item)
    {
        if (!Selectable(item)) {
            throw std::out_of_range("Menu::Select: not a selectable item");
        }
        selected_ = item;
    }

    void Menu::Move(const int direction)
    {
        if (items_.empty()) {
            return;
        }
        const auto count = static_cast<long>(items_.size());
        long index = static_cast<long>(selected_);
        for (long tried = 0; tried < count; ++tried) {
            index = ((index + direction) % count + count) % count;
            if (Selectable(static_cast<std::size_t>(index))) {
                selected_ = static_cast<std::size_t>(index);
                return;
            }
        }
    }

    void Menu::Change(const int direction)
    {
        if (!Selectable(selected_)) {
            return;
        }
        Item& item = items_[selected_];
        if (item.kind == Kind::Choice) {
            const int count = static_cast<int>(item.choices.size());
            item.choice = ((item.choice + direction) % count + count) % count;
            if (item.choiceChanged) {
                item.choiceChanged(item.choice);
            }
        } else if (item.kind == Kind::Slider) {
            // Snapped to whole steps from the minimum, so repeated presses do not drift.
            const float steps = std::round((item.value - item.minimum) / item.step) + static_cast<float>(direction);
            const float value = std::clamp(item.minimum + steps * item.step, item.minimum, item.maximum);
            if (value != item.value) {
                item.value = value;
                if (item.sliderChanged) {
                    item.sliderChanged(item.value);
                }
            }
        }
    }

    bool Menu::Update(const MenuInput& input)
    {
        // Only a pointer that moved or clicks selects: one resting over an
        // item would otherwise take every key press back.
        const bool moved = input.pointer && lastPointer_ && *input.pointer != *lastPointer_;
        lastPointer_ = input.pointer;
        if (input.pointer && (moved || input.click)) {
            for (std::size_t i = 0; i < boxes_.size() && i < items_.size(); ++i) {
                if (Selectable(i) && boxes_[i].Contains(*input.pointer)) {
                    selected_ = i;
                    break;
                }
            }
        }
        if (input.up) {
            Move(-1);
        }
        if (input.down) {
            Move(1);
        }
        if (input.left) {
            Change(-1);
        }
        if (input.right) {
            Change(1);
        }
        const bool pointed = input.click && input.pointer && selected_ < boxes_.size() && boxes_[selected_].Contains(*input.pointer);
        if ((input.accept || pointed) && Selectable(selected_)) {
            Item& item = items_[selected_];
            if (item.kind == Kind::Button && item.activate) {
                item.activate();
            } else if (item.kind == Kind::Choice) {
                Change(1);
            }
        }
        return input.back;
    }

    std::string Menu::ItemText(const std::size_t item) const
    {
        if (item >= items_.size()) {
            throw std::out_of_range("Menu::ItemText: no such item");
        }
        const Item& it = items_[item];
        switch (it.kind) {
        case Kind::Choice:
            return it.label + ": < " + it.choices[static_cast<std::size_t>(it.choice)] + " >";
        case Kind::Slider: {
            char value[32];
            const bool whole = std::fabs(it.step - std::round(it.step)) < 1e-6f;
            std::snprintf(value, sizeof value, whole ? "%.0f" : "%.2f", static_cast<double>(it.value));
            return it.label + ": < " + value + " >";
        }
        default:
            return it.label;
        }
    }

    void Menu::SetLabel(const std::size_t item, std::string label)
    {
        if (item >= items_.size()) {
            throw std::out_of_range("Menu::SetLabel: no such item");
        }
        items_[item].label = std::move(label);
    }

    int Menu::ChoiceValue(const std::size_t item) const
    {
        return item < items_.size() ? items_[item].choice : 0;
    }

    float Menu::SliderValue(const std::size_t item) const
    {
        return item < items_.size() ? items_[item].value : 0.0f;
    }

    void Menu::Draw(Painter& painter, const IFont& font, const Anchor anchor, const float scale)
    {
        TextStyle title;
        title.scale = scale * 1.5f;
        TextStyle normal;
        normal.scale = scale;
        TextStyle chosen = normal;
        chosen.color = Color(255, 220, 90, 255);
        TextStyle quiet = normal;
        quiet.color = Color(170, 175, 185, 255);

        // The whole column first, to place it.
        const float gap = 6.0f * scale * 0.5f;
        float width = title_.empty() ? 0.0f : Painter::Measure(font, title_, title).X;
        float height = title_.empty() ? 0.0f : Painter::Measure(font, title_, title).Y + 3.0f * gap;
        std::vector<Vector2> sizes;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const Vector2 size = Painter::Measure(font, ItemText(i), normal);
            sizes.push_back(size);
            width = std::max(width, size.X);
            height += size.Y + gap;
        }
        const Box column = painter.Place(anchor, Vector2(width, height));
        painter.Fill(Box{column.x - 16.0f, column.y - 12.0f, column.width + 32.0f, column.height + 24.0f},
                     Color(10, 12, 18, 190));

        float y = column.y;
        if (!title_.empty()) {
            const Vector2 size = Painter::Measure(font, title_, title);
            painter.Text(font, title_, Vector2(column.x + (width - size.X) * 0.5f, y), title);
            y += size.Y + 3.0f * gap;
        }
        boxes_.assign(items_.size(), Box{});
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const Vector2& size = sizes[i];
            const Box box{column.x + (width - size.X) * 0.5f, y, size.X, size.Y};
            boxes_[i] = Box{column.x - 8.0f, y - gap * 0.5f, width + 16.0f, size.Y + gap};
            const TextStyle& style = !Selectable(i) ? quiet : i == selected_ ? chosen : normal;
            painter.Text(font, ItemText(i), Vector2(box.x, box.y), style);
            y += size.Y + gap;
        }
    }

    void MenuStack::Push(std::shared_ptr<Menu> menu)
    {
        if (!menu) {
            throw std::invalid_argument("MenuStack::Push: no menu");
        }
        menu->ForgetPointer();
        menus_.push_back(std::move(menu));
    }

    void MenuStack::Pop()
    {
        if (!menus_.empty()) {
            menus_.pop_back();
        }
        if (!menus_.empty()) {
            menus_.back()->ForgetPointer();
        }
    }

    bool MenuStack::PausesWorld() const noexcept
    {
        return std::any_of(menus_.begin(), menus_.end(), [](const auto& menu) { return menu->pausesWorld; });
    }

    void MenuStack::Update(const MenuInput& input)
    {
        if (menus_.empty()) {
            return;
        }
        // Hold the menu: an item may push or pop menus while it runs.
        const std::shared_ptr<Menu> top = menus_.back();
        if (top->Update(input) && !menus_.empty() && menus_.back() == top) {
            Pop();
        }
    }

    void MenuStack::Draw(Painter& painter, const IFont& font, const Anchor anchor, const float scale)
    {
        if (menus_.empty()) {
            return;
        }
        const Vector2 size = painter.VirtualSize();
        painter.Fill(Box{0.0f, 0.0f, size.X, size.Y}, Color(0, 0, 0, 110));
        menus_.back()->Draw(painter, font, anchor, scale);
    }
}
