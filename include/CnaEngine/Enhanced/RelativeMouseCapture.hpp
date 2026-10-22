// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001 addendum): this header uses a CNA input extension
// (relative mouse mode) and must never be included from the strict modules.
// It does not depend on cna-extended.
#pragma once

#include "CnaEngine/App/MouseCapture.hpp"

namespace CnaEngine::Enhanced
{
    /// Mouse look through CNA's relative mouse mode (an FNA extension): the
    /// platform reports raw movement and, in a browser, takes the pointer
    /// lock -- the one way mouse look works there, since a page cannot move
    /// the cursor (App::RecenteringMouseCapture). Backrooms-simulator's mouse
    /// look, as an EngineGame capture:
    ///
    ///     game.SetMouseCapture(std::make_unique<CnaEngine::Enhanced::RelativeMouseCapture>());
    class RelativeMouseCapture final : public App::IMouseCapture
    {
    public:
        void SetCaptured(Microsoft::Xna::Framework::Game& game, bool captured) override;
        [[nodiscard]] bool Captured() const noexcept override { return captured_; }
        /// In relative mode the mouse state's position is the movement since
        /// the previous state.
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Delta(
            Microsoft::Xna::Framework::Game& game, const Microsoft::Xna::Framework::Input::MouseState& state) override;

    private:
        bool captured_ = false;
    };
}
