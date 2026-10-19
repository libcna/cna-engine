// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/EngineWorldScreen.hpp"

#include "CnaEngine/Extended/WorldCameraSync.hpp"

#include "Microsoft/Xna/Framework/GameTime.hpp"

#include <utility>

namespace CnaEngine::Extended
{
    using Microsoft::Xna::Framework::GameTime;

    EngineWorldScreen::EngineWorldScreen() { SyncToWorldCamera(camera_, GetCamera3DEXT()); }

    void EngineWorldScreen::SetCameraController(std::unique_ptr<Graphics::ICameraController> controller,
                                                const Graphics::CameraInputAxes& axes)
    {
        controller_ = std::move(controller);
        axes_ = axes;
        if (controller_) {
            controller_->SyncFrom(camera_);
        }
    }

    Input::InputSnapshot EngineWorldScreen::CaptureInput() { return Input::InputSnapshot::Capture(getIsActiveProperty()); }

    void EngineWorldScreen::Update(GameTime& gameTime)
    {
        actions_.Update(CaptureInput());
        UpdateScreen(gameTime);
        if (controller_) {
            const auto elapsed = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
            controller_->Update(camera_, Graphics::ReadCameraInput(actions_, axes_), elapsed);
        }
        SyncToWorldCamera(camera_, GetCamera3DEXT());
        World3DScreenEXT::Update(gameTime);
    }

    void EngineWorldScreen::Draw(const GameTime& gameTime)
    {
        World3DScreenEXT::Draw(gameTime);
        frameStats_.AddFrame(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
    }
}
