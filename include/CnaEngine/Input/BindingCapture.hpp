// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Input/Bindings.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace CnaEngine::Input
{
    /// Listens for the next physical input, for "press a key for Jump" screens.
    ///
    /// Inputs already held when listening starts are ignored until released,
    /// so the key that opened the prompt is never captured. Analog pad controls
    /// count once they cross `axisThreshold` and are captured as axis bindings
    /// (XNA's derived thumbstick/trigger buttons are then not offered). A
    /// configured cancel key ends listening without a result.
    class BindingCapture
    {
    public:
        struct Options
        {
            bool keyboard = true;
            bool mouse = true;
            bool gamePadButtons = true;
            bool gamePadAxes = true;
            float axisThreshold = 0.5f;
            /// Ends listening as Cancelled; Keys::None disables cancelling.
            Microsoft::Xna::Framework::Input::Keys cancelKey = Microsoft::Xna::Framework::Input::Keys::Escape;
        };

        enum class State : std::uint8_t
        {
            Idle,
            Listening,
            Captured,
            Cancelled,
        };

        /// Starts listening; `current` is the snapshot of this frame.
        void Begin(const InputSnapshot& current, const Options& options);
        void Begin(const InputSnapshot& current) { Begin(current, Options{}); }
        /// Feeds the next snapshot; returns the resulting state.
        State Update(const InputSnapshot& snapshot);
        void Stop() noexcept { state_ = State::Idle; }

        [[nodiscard]] State Current() const noexcept { return state_; }
        /// The captured binding once Current() == Captured.
        [[nodiscard]] const std::optional<Binding>& Result() const noexcept { return result_; }

    private:
        [[nodiscard]] std::vector<Binding> Active(const InputSnapshot& snapshot) const;

        Options options_;
        State state_ = State::Idle;
        std::vector<Binding> ignored_;
        std::optional<Binding> result_;
    };
}
