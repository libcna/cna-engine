// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/RelativeMouseCapture.hpp"

#include "Microsoft/Xna/Framework/Input/Mouse.hpp"

namespace CnaEngine::Enhanced
{
    void RelativeMouseCapture::SetCaptured(Microsoft::Xna::Framework::Game& game, const bool captured)
    {
        if (captured == captured_) {
            return;
        }
        captured_ = captured;
        Microsoft::Xna::Framework::Input::Mouse::setIsRelativeMouseModeEXTProperty(captured);
        game.setIsMouseVisibleProperty(!captured);
    }

    Microsoft::Xna::Framework::Vector2 RelativeMouseCapture::Delta(
        Microsoft::Xna::Framework::Game&, const Microsoft::Xna::Framework::Input::MouseState& state)
    {
        if (!captured_) {
            return Microsoft::Xna::Framework::Vector2::Zero;
        }
        return Microsoft::Xna::Framework::Vector2(static_cast<float>(state.getXProperty()),
                                                  static_cast<float>(state.getYProperty()));
    }
}
