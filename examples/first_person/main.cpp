// SPDX-License-Identifier: MIT
//
// first_person: walking with collisions and picking things up, on the
// strict profile (plan.md GAME-005). A player walks up a flight of stairs to
// a platform, looks at a crate and takes it (a ray from the eye, E), while
// footsteps count the strides and the sky follows the clock.
//
// Run as a test it plays a script instead of reading the devices
// (--interactive plays it yourself: WASD, mouse after a click, E, Space).
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Collision/CharacterController.hpp"
#include "CnaEngine/Collision/StaticWorld.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/Sky.hpp"
#include "CnaEngine/UI/Painter.hpp"

#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace CnaEngine;
using Microsoft::Xna::Framework::BoundingBox;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;
using Microsoft::Xna::Framework::Input::Keys;

namespace
{
    constexpr std::uint32_t CrateTag = 7;

    App::GameOptions MakeOptions()
    {
        App::GameOptions options;
        options.title = "First person";
        options.width = 480;
        options.height = 270;
        options.fixedStepSeconds = 1.0 / 120.0;
        options.assetDirectory.clear();
        return options;
    }
}

class FirstPerson final : public App::EngineGame
{
public:
    FirstPerson() : EngineGame(MakeOptions()) {}

    bool scripted = true;
    int taken = 0;
    std::uint64_t strides = 0;
    Vector3 feet = Vector3::Zero;

protected:
    bool ParseArgument(std::span<const std::string_view> arguments, std::size_t& index) override
    {
        if (arguments[index] == "--interactive") {
            scripted = false;
            return true;
        }
        return false;
    }

    // The script: walk to the stairs and up them (W), stop, look down at the
    // crate on the platform, take it (E).
    Input::InputSnapshot ReadInput() override
    {
        if (!scripted) {
            return EngineGame::ReadInput();
        }
        const auto update = UpdateCount();
        Input::InputSnapshot snapshot;
        if (update < 300) {
            snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::W});
        } else if (update == 370) {
            snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::E});
        }
        return snapshot;
    }

    void LoadGame() override
    {
        auto& actions = Actions();
        axes_.moveRight = actions.AddAxis("MoveRight");
        actions.BindAxis(axes_.moveRight, Input::AxisBinding::ForKeys(Keys::A, Keys::D));
        axes_.moveForward = actions.AddAxis("MoveForward");
        actions.BindAxis(axes_.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
        take_ = actions.AddAction("Take");
        actions.Bind(take_, Input::Binding::ForKey(Keys::E));
        jump_ = actions.AddAction("Jump");
        actions.Bind(jump_, Input::Binding::ForKey(Keys::Space));
        capture_ = actions.AddAction("CaptureMouse");
        actions.Bind(capture_, Input::Binding::ForMouse(Input::MouseButton::Left));

        auto& device = getGraphicsDeviceProperty();
        Graphics::MeshBuilder builder;
        builder.AddBox(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f));
        cube_ = Graphics::GpuMesh::Upload(device, builder.Take(), Graphics::VertexLayout::PositionNormalTexture());
        stone_.diffuseColor = Vector3(0.6f, 0.58f, 0.55f);
        grass_.diffuseColor = Vector3(0.35f, 0.5f, 0.3f);
        crate_.diffuseColor = Vector3(0.75f, 0.5f, 0.25f);

        // The world: ground, six steps of 0.17 m along -Z, a platform, a crate on it.
        Block(BoundingBox(Vector3(-20, -1, -20), Vector3(20, 0, 20)), grass_);
        for (int step = 0; step < 6; ++step) {
            const float z = -2.0f - 0.3f * static_cast<float>(step);
            Block(BoundingBox(Vector3(-1, 0, z - 0.3f), Vector3(1, 0.17f * static_cast<float>(step + 1), z)), stone_);
        }
        Block(BoundingBox(Vector3(-3, 0, -8), Vector3(3, 1.02f, -3.8f)), stone_);
        crateBox_ = BoundingBox(Vector3(-0.3f, 1.02f, -5.6f), Vector3(0.3f, 1.62f, -5.0f));
        crateShape_ = world_.AddBox(crateBox_, CrateTag);

        sky_ = std::make_unique<Graphics::SkyDome>(device);
        sky_->AttachTo(Renderer());
        time_.hours = 10.0;
        const auto colors = Graphics::DayNightPalette::Default().At(static_cast<float>(time_.Sun().elevationDegrees));
        sky_->SetColors(colors, time_.DirectionToSun());
        Graphics::ApplySkyLighting(Renderer().Lighting(), colors, time_.DirectionToSun(), 30.0f, 80.0f);

        font_ = std::make_unique<UI::BuiltInFont>(device);
        painter_ = std::make_unique<UI::Painter>(device);
        body_.Teleport(world_, Vector3(0, 0, 2));
    }

    void FixedUpdate(const double step) override
    {
        const auto input = Graphics::ReadCameraInput(Actions(), axes_, static_cast<float>(step));
        body_.Step(world_, Collision::WishVelocity(input.move, yaw_, 3.0f), Actions().Held(jump_),
                   static_cast<float>(step));
        strides = body_.StrideCount();
        feet = body_.Feet();
    }

    void UpdateFrame(double) override
    {
        if (!scripted && Actions().Pressed(capture_)) {
            CaptureMouse(true);
        }
        yaw_ -= MouseDelta().X * 0.0025f;
        // On the platform the script looks down at the crate.
        if (scripted && UpdateCount() > 300) {
            pitch_ = std::max(pitch_ - 0.02f, -1.1f);
        } else {
            pitch_ = std::clamp(pitch_ - MouseDelta().Y * 0.0025f, -1.5f, 1.5f);
        }
        MainCamera().SetYawPitchRoll(yaw_, pitch_);
        MainCamera().SetPosition(body_.Eye());

        // Taking: the nearest thing the view's ray meets within 3 m.
        const Microsoft::Xna::Framework::Ray ray(MainCamera().Position(), MainCamera().Forward());
        const auto hit = world_.Raycast(ray, 3.0f);
        aimed_ = hit && hit->tag == CrateTag;
        if (aimed_ && Actions().Pressed(take_)) {
            world_.Remove(hit->shape);
            crateTaken_ = true;
            ++taken;
        }
    }

    void Submit(App::Frame& frame) override
    {
        for (const auto& block : blocks_) {
            frame.Add(cube_, *block.material, block.world);
        }
        if (!crateTaken_) {
            const Vector3 size = crateBox_.Max - crateBox_.Min;
            frame.Add(cube_, crate_, Matrix::CreateScale(size) * Matrix::CreateTranslation((crateBox_.Min + crateBox_.Max) * 0.5f));
        }
    }

    void DrawOverlay(Microsoft::Xna::Framework::Graphics::GraphicsDevice&) override
    {
        painter_->Begin();
        const auto middle = painter_->VirtualSize() * 0.5f;
        painter_->Fill({middle.X - 1.0f, middle.Y - 1.0f, 2.0f, 2.0f}, Microsoft::Xna::Framework::Color(255, 255, 255, 220));
        if (aimed_) {
            painter_->Text(*font_, "E: take the crate", UI::Anchor::Bottom);
        }
        painter_->Text(*font_, "Crates: " + std::to_string(taken), UI::Anchor::TopLeft);
        painter_->End();
    }

private:
    struct Placed
    {
        Matrix world;
        const Graphics::Material* material;
    };

    void Block(const BoundingBox& box, const Graphics::Material& material)
    {
        world_.AddBox(box);
        const Vector3 size = box.Max - box.Min;
        blocks_.push_back({Matrix::CreateScale(size) * Matrix::CreateTranslation((box.Min + box.Max) * 0.5f), &material});
    }

    Graphics::CameraInputAxes axes_;
    Input::ActionId take_{};
    Input::ActionId jump_{};
    Input::ActionId capture_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    Collision::StaticWorld world_;
    Collision::CharacterController body_;
    BoundingBox crateBox_;
    Collision::ShapeId crateShape_ = 0;
    bool crateTaken_ = false;
    bool aimed_ = false;
    Graphics::GpuMesh cube_;
    Graphics::Material stone_;
    Graphics::Material grass_;
    Graphics::Material crate_;
    std::vector<Placed> blocks_;
    Graphics::TimeOfDay time_;
    std::unique_ptr<Graphics::SkyDome> sky_;
    std::unique_ptr<UI::BuiltInFont> font_;
    std::unique_ptr<UI::Painter> painter_;
};

int main(int argc, char** argv)
{
    // The engine's RunGame, plus the script's own checks.
    const auto arguments = App::Detail::Arguments(argc, argv);
    try {
        FirstPerson game;
        game.ParseCommandLine(arguments);
        game.Run();
        const int code = App::Detail::ExitCodeAfterRun(game);
        if (!game.scripted) {
            return code;
        }
        std::printf("first_person: feet at (%.2f, %.2f, %.2f), %llu strides, %d crate(s) taken\n", game.feet.X,
                    game.feet.Y, game.feet.Z, static_cast<unsigned long long>(game.strides), game.taken);
        // Up the stairs onto the platform (1.02 m), the crate taken.
        if (std::fabs(game.feet.Y - 1.02f) > 0.01f || game.taken != 1 || game.strides < 4) {
            std::fprintf(stderr, "first_person: the script did not end on the platform with the crate\n");
            return 1;
        }
        return code;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "first_person: %s\n", e.what());
        return 1;
    }
}
