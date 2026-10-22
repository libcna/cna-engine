#pragma once

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <memory>
#include <vector>

// @GAME_NAME@: walk around a field of crates in the first person.
class @GAME_CLASS@ final : public CnaEngine::App::EngineGame
{
public:
    @GAME_CLASS@();

protected:
    void LoadGame() override;
    void FixedUpdate(double stepSeconds) override;
    void UpdateFrame(double frameSeconds) override;
    void Submit(CnaEngine::App::Frame& frame) override;

private:
    CnaEngine::Graphics::CameraInputAxes axes_;
    CnaEngine::Input::ActionId captureMouse_{};
    CnaEngine::Graphics::WalkCameraController walk_;

    CnaEngine::Graphics::GpuMesh ground_;
    CnaEngine::Graphics::GpuMesh crate_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> crateTexture_;
    CnaEngine::Graphics::Material groundMaterial_;
    CnaEngine::Graphics::Material crateMaterial_;
    std::vector<Microsoft::Xna::Framework::Vector3> crates_;
};
