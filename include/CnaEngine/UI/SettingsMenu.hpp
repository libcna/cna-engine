// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/QualitySchema.hpp"
#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/Input/BindingCapture.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"
#include "CnaEngine/UI/Menu.hpp"

#include <functional>
#include <vector>

namespace CnaEngine::UI
{
    /// A settings page's rows for a quality schema: the preset as a choice,
    /// then every setting -- on/off for a bool, a slider for an int or float
    /// with bounds (settings without bounds are left out). A change writes
    /// `selection` (a preset clears the overrides; a setting becomes an
    /// override) and calls `changed`; QualitySchema::SaveSelection stores it.
    /// `selection` must outlive the menu. House-simulator's settings page,
    /// car-simulator's quality tiers.
    void AddQualitySettings(Menu& menu, const Graphics::QualitySchema& schema, Graphics::QualitySelection& selection,
                            std::function<void()> changed = {});

    /// Rows that rebind actions: each shows "<action>: <its first binding>";
    /// accepting a row listens for the next key, mouse button or pad input
    /// (Input::BindingCapture) and puts it in place of that binding; the
    /// capture's cancel key (Escape) leaves it as it was. No game had a
    /// rebinding screen yet (car keeps overrides in its save).
    ///
    /// While Listening() the game gives input to Update instead of the menu.
    class RebindRows
    {
    public:
        /// Adds a row per action to `menu`; the map and the menu must outlive this.
        RebindRows(Menu& menu, Input::ActionMap& actions, std::vector<Input::ActionId> rows);
        // The menu's rows call back into this object.
        RebindRows(const RebindRows&) = delete;
        RebindRows& operator=(const RebindRows&) = delete;

        [[nodiscard]] bool Listening() const noexcept { return listening_ >= 0; }
        /// Feeds the capture while listening; returns true when a binding changed.
        bool Update(const Input::InputSnapshot& snapshot);
        /// Refreshes the rows' text from the map (after loading bindings).
        void Refresh();

    private:
        void Begin(int row);

        Menu& menu_;
        Input::ActionMap& actions_;
        std::vector<Input::ActionId> rows_;
        std::vector<std::size_t> items_;
        Input::BindingCapture capture_;
        Input::InputSnapshot last_;
        int listening_ = -1;
        bool begin_ = false;
    };
}
