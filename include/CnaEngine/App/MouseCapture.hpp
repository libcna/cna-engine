// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Input/MouseState.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

namespace CnaEngine::App
{
    /// How a game holds the mouse for mouse look: hide the cursor while
    /// captured and report how far the mouse moved.
    ///
    /// XNA 4.0 has one way, RecenteringMouseCapture: put the cursor back in
    /// the middle of the window after every reading. A browser cannot move
    /// the cursor, so a browser build needs the pointer lock of CNA's relative
    /// mouse mode, an extension (Enhanced/RelativeMouseCapture.hpp).
    class IMouseCapture
    {
    public:
        virtual ~IMouseCapture() = default;

        virtual void SetCaptured(Microsoft::Xna::Framework::Game& game, bool captured) = 0;
        [[nodiscard]] virtual bool Captured() const noexcept = 0;
        /// The mouse's movement in pixels since the previous call, from this
        /// update's state; zero while not captured.
        [[nodiscard]] virtual Microsoft::Xna::Framework::Vector2 Delta(
            Microsoft::Xna::Framework::Game& game, const Microsoft::Xna::Framework::Input::MouseState& state) = 0;
    };

    /// XNA's mouse look: the cursor is hidden and moved back to the middle of
    /// the window after each reading. The first reading after capturing is
    /// skipped, since the cursor may not have reached the middle yet.
    class RecenteringMouseCapture final : public IMouseCapture
    {
    public:
        void SetCaptured(Microsoft::Xna::Framework::Game& game, bool captured) override;
        [[nodiscard]] bool Captured() const noexcept override { return captured_; }
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Delta(
            Microsoft::Xna::Framework::Game& game, const Microsoft::Xna::Framework::Input::MouseState& state) override;

    private:
        bool captured_ = false;
        bool skipNext_ = false;
    };
}
