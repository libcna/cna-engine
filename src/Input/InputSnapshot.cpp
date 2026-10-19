// SPDX-License-Identifier: MIT
#include "CnaEngine/Input/InputSnapshot.hpp"

#include "Microsoft/Xna/Framework/Input/GamePad.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadDeadZone.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"

namespace CnaEngine::Input
{
    using Microsoft::Xna::Framework::PlayerIndex;
    using Microsoft::Xna::Framework::Input::GamePad;
    using Microsoft::Xna::Framework::Input::GamePadDeadZone;
    using Microsoft::Xna::Framework::Input::Keyboard;
    using Microsoft::Xna::Framework::Input::Mouse;

    InputSnapshot InputSnapshot::Capture(const bool hasFocus, const PlayerIndex player)
    {
        InputSnapshot snapshot;
        snapshot.keyboard = Keyboard::GetState();
        snapshot.mouse = Mouse::GetState();
        snapshot.gamePad = GamePad::GetState(player, GamePadDeadZone::None);
        snapshot.hasFocus = hasFocus;
        return snapshot;
    }
}
