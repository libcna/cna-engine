// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Input/Bindings.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::Input
{
    /// Index of a digital action within its ActionMap.
    using ActionId = std::uint32_t;
    /// Index of an analog axis within its ActionMap.
    using AxisId = std::uint32_t;

    /// One physical input bound to more than one action.
    struct BindingConflict
    {
        Binding binding;
        std::vector<ActionId> actions;
    };

    /// Outcome of ActionMap::LoadBindings.
    struct BindingLoadResult
    {
        /// Number of actions and axes whose bindings were replaced.
        int applied = 0;
        /// Human-readable descriptions of ignored lines (unknown action, bad binding, ...).
        std::vector<std::string> warnings;
    };

    /// Named digital actions and analog axes over keyboard, mouse and game pad.
    ///
    /// Actions and axes are declared by game code (their names are the stable
    /// identifiers used by binding files); bindings are data that a settings
    /// file or a rebinding screen may replace at any time.
    ///
    /// State is derived only from the snapshots passed to Update(), once per
    /// game update:
    ///  - Held: some binding is down in the current snapshot;
    ///  - Pressed: held now and not held at the previous Update();
    ///  - Released: not held now and held at the previous Update().
    ///
    /// Focus loss (snapshot.hasFocus == false) releases every action and zeroes
    /// every axis. After focus returns, an action stays released until all of
    /// its bindings have been let go once, so a key still down from another
    /// window never produces a phantom press.
    class ActionMap
    {
    public:
        /// Declares an action, or returns the existing one with that name.
        ActionId AddAction(std::string_view name);
        /// Declares an axis, or returns the existing one with that name.
        AxisId AddAxis(std::string_view name);

        [[nodiscard]] std::optional<ActionId> FindAction(std::string_view name) const;
        [[nodiscard]] std::optional<AxisId> FindAxis(std::string_view name) const;
        [[nodiscard]] const std::string& ActionName(ActionId action) const;
        [[nodiscard]] const std::string& AxisName(AxisId axis) const;
        [[nodiscard]] std::size_t ActionCount() const noexcept { return actions_.size(); }
        [[nodiscard]] std::size_t AxisCount() const noexcept { return axes_.size(); }

        // --- Bindings -------------------------------------------------------

        /// Adds a binding; returns false when the action already has that input.
        bool Bind(ActionId action, const Binding& binding);
        /// Removes a binding; returns false when it was not bound.
        bool Unbind(ActionId action, const Binding& binding);
        /// Replaces `from` with `to`, keeping its position; false when `from` is not bound.
        bool Rebind(ActionId action, const Binding& from, const Binding& to);
        void ClearBindings(ActionId action);
        [[nodiscard]] const std::vector<Binding>& Bindings(ActionId action) const;
        /// Every action that has `binding` among its bindings.
        [[nodiscard]] std::vector<ActionId> ActionsBoundTo(const Binding& binding) const;

        void BindAxis(AxisId axis, const AxisBinding& binding);
        void ClearAxisBindings(AxisId axis);
        [[nodiscard]] const std::vector<AxisBinding>& AxisBindings(AxisId axis) const;

        /// Physical inputs shared by two or more actions. Shared inputs are
        /// legal (a game may use the same key in different modes); this lets a
        /// settings screen warn about them.
        [[nodiscard]] std::vector<BindingConflict> FindConflicts() const;

        // --- Per-update state ----------------------------------------------

        void Update(const InputSnapshot& snapshot);
        /// Forgets all held state, as if every input had been released with no
        /// Released edge, and drops the mouse-delta reference.
        void ResetState();

        [[nodiscard]] bool Held(ActionId action) const;
        [[nodiscard]] bool Pressed(ActionId action) const;
        [[nodiscard]] bool Released(ActionId action) const;
        /// Key-pair and game-pad contributions are summed and clamped to
        /// [-1, 1]; mouse contributions are added unclamped afterwards.
        [[nodiscard]] float Axis(AxisId axis) const;

        // --- Persistence ----------------------------------------------------

        /// Line-based text: `action.<name>=<binding>,<binding>` and
        /// `axis.<name>=<axis binding>,...`. Locale-independent, sorted by
        /// declaration order. An action with no bindings is written with an
        /// empty list so loading it clears the defaults.
        [[nodiscard]] std::string SaveBindings() const;
        /// Replaces the bindings of every action/axis named in `text`. Lines for
        /// undeclared names or with unparsable bindings are reported and skipped;
        /// a line with any invalid binding is ignored as a whole.
        BindingLoadResult LoadBindings(std::string_view text);

    private:
        struct ActionState
        {
            std::string name;
            std::vector<Binding> bindings;
            bool held = false;
            bool wasHeld = false;
            bool suppressed = false;
        };

        struct AxisState
        {
            std::string name;
            std::vector<AxisBinding> bindings;
            float value = 0.0f;
        };

        [[nodiscard]] static bool BindingDown(const Binding& binding, const InputSnapshot& snapshot);
        [[nodiscard]] static float AxisValue(const AxisState& axis, const InputSnapshot& snapshot, float mouseDeltaX,
                                            float mouseDeltaY, float wheelDelta);
        [[nodiscard]] const ActionState& ActionAt(ActionId action) const;
        [[nodiscard]] ActionState& ActionAt(ActionId action);
        [[nodiscard]] const AxisState& AxisAt(AxisId axis) const;
        [[nodiscard]] AxisState& AxisAt(AxisId axis);

        std::vector<ActionState> actions_;
        std::vector<AxisState> axes_;
        bool hasMouseReference_ = false;
        int mouseX_ = 0;
        int mouseY_ = 0;
        int wheel_ = 0;
        bool hadFocus_ = true;
    };
}
