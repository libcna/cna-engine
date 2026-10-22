// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/UI/Painter.hpp"

#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CnaEngine::UI
{
    /// One update's menu input -- from actions and the mouse, or a script.
    struct MenuInput
    {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool accept = false;
        bool back = false;
        /// The pointer in the painter's virtual units, when there is one.
        std::optional<Microsoft::Xna::Framework::Vector2> pointer;
        bool click = false;
    };

    /// The actions menus read, declared once in a game's ActionMap ("MenuUp",
    /// ... "MenuBack") with the usual keys and pad buttons, rebindable like
    /// any other action.
    struct MenuActions
    {
        Input::ActionId up{};
        Input::ActionId down{};
        Input::ActionId left{};
        Input::ActionId right{};
        Input::ActionId accept{};
        Input::ActionId back{};

        /// Arrows, Enter or Space, Escape or Backspace; the pad's d-pad, A and B.
        [[nodiscard]] static MenuActions Declare(Input::ActionMap& actions);
    };

    /// The pressed edges of the menu actions this update, with the pointer.
    [[nodiscard]] MenuInput ReadMenuInput(const Input::ActionMap& actions, const MenuActions& menu,
                                          std::optional<Microsoft::Xna::Framework::Vector2> pointer = std::nullopt,
                                          bool click = false);

    /// A list of buttons, choices and sliders navigated by MenuInput, drawn
    /// by a Painter: house-simulator's menu screens, train's start menu with
    /// mouse hits, galaxy-eggbert's pause menu. Selection skips labels and
    /// wraps; left and right change choices and sliders; the pointer selects
    /// what it is over and a click accepts it.
    class Menu
    {
    public:
        explicit Menu(std::string title = {});

        Menu& Button(std::string label, std::function<void()> activate);
        Menu& Choice(std::string label, std::vector<std::string> choices, int selected,
                     std::function<void(int)> changed = {});
        /// Throws std::invalid_argument unless minimum < maximum and step > 0.
        Menu& Slider(std::string label, float minimum, float maximum, float step, float value,
                     std::function<void(float)> changed = {});
        /// A line that cannot be selected (a heading, a hint).
        Menu& Label(std::string text);

        /// Applies one update's input; true when `back` was pressed (the
        /// menu's owner closes it).
        bool Update(const MenuInput& input);
        /// Draws the title and the items in a column placed at `anchor`, and
        /// remembers where each item went for the pointer.
        void Draw(Painter& painter, const IFont& font, Anchor anchor = Anchor::Center, float scale = 2.0f);

        [[nodiscard]] const std::string& Title() const noexcept { return title_; }
        [[nodiscard]] std::size_t ItemCount() const noexcept { return items_.size(); }
        [[nodiscard]] std::size_t Selected() const noexcept { return selected_; }
        void Select(std::size_t item);
        /// What an item shows: its label, and a choice's or slider's value.
        [[nodiscard]] std::string ItemText(std::size_t item) const;
        void SetLabel(std::size_t item, std::string label);
        [[nodiscard]] int ChoiceValue(std::size_t item) const;
        [[nodiscard]] float SliderValue(std::size_t item) const;

        /// Whether the world stands still while this menu is open.
        bool pausesWorld = true;

    private:
        enum class Kind : std::uint8_t { Button, Choice, Slider, Label };
        struct Item
        {
            Kind kind = Kind::Button;
            std::string label;
            std::function<void()> activate;
            std::vector<std::string> choices;
            int choice = 0;
            std::function<void(int)> choiceChanged;
            float minimum = 0.0f;
            float maximum = 1.0f;
            float step = 0.1f;
            float value = 0.0f;
            std::function<void(float)> sliderChanged;
        };
        Menu& Add(Item item);
        void Move(int direction);
        void Change(int direction);
        [[nodiscard]] bool Selectable(std::size_t item) const noexcept;

        std::string title_;
        std::vector<Item> items_;
        std::size_t selected_ = 0;
        std::vector<Box> boxes_;
    };

    /// Menus on top of one another (main, then settings, then a
    /// confirmation): only the top one takes input; Back closes it.
    class MenuStack
    {
    public:
        void Push(std::shared_ptr<Menu> menu);
        void Pop();
        void Clear() noexcept { menus_.clear(); }
        [[nodiscard]] Menu* Top() noexcept { return menus_.empty() ? nullptr : menus_.back().get(); }
        [[nodiscard]] bool Empty() const noexcept { return menus_.empty(); }
        [[nodiscard]] std::size_t Depth() const noexcept { return menus_.size(); }
        /// Whether any open menu stops the world.
        [[nodiscard]] bool PausesWorld() const noexcept;
        /// Gives `input` to the top menu; Back pops it.
        void Update(const MenuInput& input);
        /// Dims the screen and draws the top menu.
        void Draw(Painter& painter, const IFont& font, Anchor anchor = Anchor::Center, float scale = 2.0f);

    private:
        std::vector<std::shared_ptr<Menu>> menus_;
    };
}
