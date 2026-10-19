// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Input/GamePadState.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/MouseState.hpp"
#include "Microsoft/Xna/Framework/PlayerIndex.hpp"

namespace CnaEngine::Input
{
    /// One frame of raw device state for a single player.
    ///
    /// Engine input code never polls devices itself: a snapshot is captured once
    /// per update (or built by a test or a replay) and handed to ActionMap::Update.
    /// That makes held/pressed/released results a pure function of the snapshot
    /// sequence, which is what keeps input deterministic and testable.
    struct InputSnapshot
    {
        Microsoft::Xna::Framework::Input::KeyboardState keyboard;
        Microsoft::Xna::Framework::Input::MouseState mouse;
        Microsoft::Xna::Framework::Input::GamePadState gamePad;
        /// False while the game window is not active. ActionMap releases every
        /// action and ignores input until each binding has been let go again.
        bool hasFocus = true;

        /// Polls the keyboard, the mouse and the given player's game pad.
        ///
        /// The pad is read without CNA's dead-zone processing
        /// (GamePadDeadZone::None) so the dead zones configured on axis bindings
        /// are the only ones applied.
        [[nodiscard]] static InputSnapshot Capture(
            bool hasFocus = true,
            Microsoft::Xna::Framework::PlayerIndex player = Microsoft::Xna::Framework::PlayerIndex::One);
    };
}
