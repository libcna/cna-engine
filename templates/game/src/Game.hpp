#pragma once

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Audio/SoundSystem.hpp"
#include "CnaEngine/Collision/CharacterController.hpp"
#include "CnaEngine/Collision/StaticWorld.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/Sky.hpp"

#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <cstdint>
#include <memory>
#include <vector>

// @GAME_NAME@: walk around a field of crates in the first person, climb the
// stairs to the platform, jump.
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
    void AddBlock(const Microsoft::Xna::Framework::BoundingBox& box, const CnaEngine::Graphics::Material& material);
    void UpdateSky();

    CnaEngine::Graphics::CameraInputAxes axes_;
    CnaEngine::Input::ActionId captureMouse_{};
    CnaEngine::Input::ActionId jump_{};
    bool jumpQueued_ = false;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    // The sky and the light follow the time of day.
    CnaEngine::Graphics::TimeOfDay time_;
    CnaEngine::Graphics::DayNightPalette palette_ = CnaEngine::Graphics::DayNightPalette::Default();
    std::unique_ptr<CnaEngine::Graphics::SkyDome> sky_;

    // Footsteps: a stride of the body plays one of the step sounds.
    CnaEngine::Audio::SoundSystem sound_;
    CnaEngine::Audio::SoundBank steps_;
    std::vector<std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect>> stepSounds_;
    std::uint32_t stepSeed_ = 1;

    // What the body collides with, and the body.
    CnaEngine::Collision::StaticWorld world_;
    CnaEngine::Collision::CharacterController body_;

    CnaEngine::Graphics::GpuMesh ground_;
    CnaEngine::Graphics::GpuMesh cube_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> crateTexture_;
    CnaEngine::Graphics::Material groundMaterial_;
    CnaEngine::Graphics::Material crateMaterial_;
    CnaEngine::Graphics::Material stoneMaterial_;
    struct Block
    {
        Microsoft::Xna::Framework::Matrix world;
        const CnaEngine::Graphics::Material* material;
    };
    std::vector<Block> blocks_;
};
