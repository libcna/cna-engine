// SPDX-License-Identifier: MIT
#include "CnaEngine/App/MouseCapture.hpp"

#include "Microsoft/Xna/Framework/GameWindow.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

namespace CnaEngine::App
{
    namespace
    {
        void Recenter(Microsoft::Xna::Framework::Game& game)
        {
            const auto bounds = game.getWindowProperty().getClientBoundsProperty();
            Microsoft::Xna::Framework::Input::Mouse::SetPosition(bounds.Width / 2, bounds.Height / 2);
        }
    }

    void RecenteringMouseCapture::SetCaptured(Microsoft::Xna::Framework::Game& game, const bool captured)
    {
        if (captured == captured_) {
            return;
        }
        captured_ = captured;
        game.setIsMouseVisibleProperty(!captured);
        if (captured) {
            Recenter(game);
            skipNext_ = true;
        }
    }

    Microsoft::Xna::Framework::Vector2 RecenteringMouseCapture::Delta(
        Microsoft::Xna::Framework::Game& game, const Microsoft::Xna::Framework::Input::MouseState& state)
    {
        if (!captured_) {
            return Microsoft::Xna::Framework::Vector2::Zero;
        }
        const auto bounds = game.getWindowProperty().getClientBoundsProperty();
        Microsoft::Xna::Framework::Vector2 delta(static_cast<float>(state.getXProperty() - bounds.Width / 2),
                                                 static_cast<float>(state.getYProperty() - bounds.Height / 2));
        if (skipNext_) {
            skipNext_ = false;
            delta = Microsoft::Xna::Framework::Vector2::Zero;
        }
        Recenter(game);
        return delta;
    }
}
