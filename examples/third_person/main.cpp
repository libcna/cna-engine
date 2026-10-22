// SPDX-License-Identifier: MIT
//
// third_person: an animated glTF character walking with a camera behind it,
// on the enhanced profile (plan.md GAME-005). CNA imports assets/figure.gltf
// at run time; ModelView draws its skinned part through the PBR binder; the
// engine's Animator plays "sway" standing and "bend" walking, cross-faded;
// a CharacterController walks it and a FollowCameraController follows.
//
// Run as a test it walks a script: stand, walk forward, stop.
#include "CnaEngine/Animation/Animator.hpp"
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Collision/CharacterController.hpp"
#include "CnaEngine/Collision/StaticWorld.hpp"
#include "CnaEngine/Enhanced/ModelImport.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace CnaEngine;
using Microsoft::Xna::Framework::BoundingBox;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;
using Microsoft::Xna::Framework::Input::Keys;
namespace Xna = Microsoft::Xna::Framework::Graphics;

class ThirdPerson final : public App::EngineGame
{
public:
    ThirdPerson() : EngineGame(Options()) {}

    const Animation::AnimationClip* clipAtEnd = nullptr;
    const Animation::AnimationClip* clipWhileWalking = nullptr;
    float walked = 0.0f;
    Graphics::CaptureResult shot;

protected:
    static App::GameOptions Options()
    {
        App::GameOptions options;
        options.title = "Third person";
        options.width = 480;
        options.height = 270;
        options.fixedStepSeconds = 1.0 / 120.0;
        return options;
    }

    // Stand for half a second, walk forward for two, stand again.
    Input::InputSnapshot ReadInput() override
    {
        Input::InputSnapshot snapshot;
        const auto update = UpdateCount();
        if (update >= 60 && update < 300) {
            snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::W});
        }
        return snapshot;
    }

    void LoadGame() override
    {
        auto& actions = Actions();
        forward_ = actions.AddAxis("MoveForward");
        actions.BindAxis(forward_, Input::AxisBinding::ForKeys(Keys::S, Keys::W));

        auto& device = getGraphicsDeviceProperty();
        Renderer().SetMaterialBinder(std::make_shared<Enhanced::PbrMaterialBinder>(device, Renderer().StockBinder()));
        auto& lighting = Renderer().Lighting();
        lighting.defaultLighting = false;
        lighting.lightDirection = Vector3(-0.4f, -0.8f, -0.45f);
        lighting.lightColor = Vector3(2.5f, 2.4f, 2.2f);
        lighting.ambientColor = Vector3(0.35f, 0.38f, 0.45f);

        // The character: CNA's run-time glTF import, the engine's view and animator.
        model_ = std::make_unique<Xna::Model>(getContentProperty().Load<Xna::Model>("figure"));
        view_ = std::make_unique<Graphics::ModelView>(*model_, Enhanced::PbrMaterialFromEffect);
        const auto* skin = Enhanced::FindSkinningData(*model_);
        if (skin == nullptr) {
            throw std::runtime_error("figure.gltf has no skin");
        }
        skeleton_ = Enhanced::SkeletonFromSkinningData(*skin);
        clips_ = Enhanced::ClipsFromSkinningData(*skin);
        animator_ = std::make_unique<Animation::Animator>(skeleton_);
        animator_->Play(Clip("sway"), {.fadeSeconds = 0.0f});

        Graphics::MeshBuilder builder;
        builder.AddGrid(Vector3::Zero, 40.0f, 40.0f, 20, 20, 20.0f);
        ground_ = Graphics::GpuMesh::Upload(device, builder.Take(), Graphics::VertexLayout::PositionNormalTexture());
        groundMaterial_.shading = Graphics::ShadingModel::PhysicallyBased;
        groundMaterial_.diffuseColor = Vector3(0.3f, 0.45f, 0.28f);
        groundMaterial_.roughness = 0.9f;
        world_.AddBox(BoundingBox(Vector3(-20, -1, -20), Vector3(20, 0, 20)));

        CharacterSettings();
        follow_.SetOffset(Vector3(1.5f, 2.2f, 4.5f));
        follow_.SetTargetPosition(body_.Feet() + Vector3(0, 1, 0));
        follow_.Snap();
    }

    void FixedUpdate(const double step) override
    {
        const float push = Actions().Axis(forward_);
        body_.Step(world_, Vector3(0.0f, 0.0f, -2.0f * push), false, static_cast<float>(step));
    }

    void UpdateFrame(const double seconds) override
    {
        // Walking or standing chooses the clip; Play cross-fades.
        const bool walking = std::fabs(body_.Velocity().Z) > 0.3f;
        animator_->Play(Clip(walking ? "bend" : "sway"), {.speed = walking ? 2.0f : 1.0f, .fadeSeconds = 0.25f});
        animator_->Update(static_cast<float>(seconds));
        if (walking) {
            clipWhileWalking = animator_->Clip();
        }
        clipAtEnd = animator_->Clip();
        walked = static_cast<float>(body_.DistanceWalked());

        follow_.SetTargetPosition(body_.Feet() + Vector3(0, 1, 0));
        follow_.Update(MainCamera(), {}, static_cast<float>(seconds));
        if (UpdateCount() == 200) {
            (void)Screenshots().Request({.path = "", .keepPixels = true, .opaque = true});
        }
    }

    void Submit(App::Frame& frame) override
    {
        frame.Add(ground_, groundMaterial_, Matrix::getIdentityProperty());
        frame.Add(*view_, Matrix::CreateTranslation(body_.Feet()), &animator_->SkinPalette());
    }

    void OnScreenshot(const Graphics::CaptureResult& result) override
    {
        if (!result.pixels.empty()) {
            shot = result;
        } else {
            EngineGame::OnScreenshot(result);
        }
    }

private:
    void CharacterSettings() { body_.Teleport(world_, Vector3(0, 0, 0)); }

    const Animation::AnimationClip& Clip(const char* name) const
    {
        for (const auto& clip : clips_) {
            if (clip.name == name) {
                return clip;
            }
        }
        throw std::runtime_error(std::string("no clip ") + name);
    }

    Input::AxisId forward_{};
    std::unique_ptr<Xna::Model> model_;
    std::unique_ptr<Graphics::ModelView> view_;
    Animation::Skeleton skeleton_;
    std::vector<Animation::AnimationClip> clips_;
    std::unique_ptr<Animation::Animator> animator_;
    Collision::StaticWorld world_;
    Collision::CharacterController body_;
    Graphics::FollowCameraController follow_;
    Graphics::GpuMesh ground_;
    Graphics::Material groundMaterial_;
};

int main(int argc, char** argv)
{
    try {
        ThirdPerson game;
        game.ParseCommandLine(App::Detail::Arguments(argc, argv));
        game.Run();
        const int code = App::Detail::ExitCodeAfterRun(game);
        // Orange pixels: the figure is drawn.
        int orange = 0;
        for (const auto& c : game.shot.pixels) {
            orange += c.getRProperty() > 120 && c.getRProperty() > c.getBProperty() + 60 ? 1 : 0;
        }
        std::printf("third_person: walked %.2f m, %d orange pixels mid-walk, clip while walking %s, at the end %s\n",
                    game.walked, orange, game.clipWhileWalking ? game.clipWhileWalking->name.c_str() : "-",
                    game.clipAtEnd ? game.clipAtEnd->name.c_str() : "-");
        if (game.walked < 3.0f || orange < 50 || game.clipWhileWalking == nullptr ||
            game.clipWhileWalking->name != "bend" || game.clipAtEnd == nullptr || game.clipAtEnd->name != "sway") {
            std::fprintf(stderr, "third_person: the walk did not go as scripted\n");
            return 1;
        }
        return code;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "third_person: %s\n", e.what());
        return 1;
    }
}
