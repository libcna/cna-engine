#include "Game.hpp"

#include "CnaEngine/Enhanced/RelativeMouseCapture.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <algorithm>
#include <cmath>

using namespace CnaEngine;
using Microsoft::Xna::Framework::BoundingBox;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;
using Microsoft::Xna::Framework::Input::Keys;

namespace
{
    App::GameOptions MakeOptions()
    {
        App::GameOptions options;
        options.title = "@GAME_NAME@";
        // The preferences (window size, key bindings) live in
        // ~/.config/@GAME_TARGET@/ on Linux.
        options.application = "@GAME_TARGET@";
        // The simulation's step: walking and collisions run at 120 Hz.
        options.fixedStepSeconds = 1.0 / 120.0;
        return options;
    }

    constexpr float WalkSpeed = 3.5f;   // m/s
    constexpr float RunMultiplier = 1.8f;
    constexpr float MouseRadiansPerPixel = 0.0025f;
}

@GAME_CLASS@::@GAME_CLASS@() : EngineGame(MakeOptions())
{
    // Mouse look that also works in a browser (its pointer lock).
    SetMouseCapture(std::make_unique<Enhanced::RelativeMouseCapture>());
}

void @GAME_CLASS@::AddBlock(const BoundingBox& box, const Graphics::Material& material)
{
    // One shape for the body to collide with, one cube to draw.
    world_.AddBox(box);
    const Vector3 size = box.Max - box.Min;
    blocks_.push_back({Matrix::CreateScale(size) * Matrix::CreateTranslation((box.Min + box.Max) * 0.5f), &material});
}

void @GAME_CLASS@::LoadGame()
{
    // Input as named actions and axes; a player can rebind them in
    // bindings.ini beside the preferences.
    auto& actions = Actions();
    axes_.moveRight = actions.AddAxis("MoveRight");
    actions.BindAxis(axes_.moveRight, Input::AxisBinding::ForKeys(Keys::A, Keys::D));
    actions.BindAxis(axes_.moveRight, Input::AxisBinding::ForGamePad(Input::GamePadAxis::LeftStickX));
    axes_.moveForward = actions.AddAxis("MoveForward");
    actions.BindAxis(axes_.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
    actions.BindAxis(axes_.moveForward, Input::AxisBinding::ForGamePad(Input::GamePadAxis::LeftStickY));
    axes_.lookYawRate = actions.AddAxis("LookYaw");
    actions.BindAxis(*axes_.lookYawRate, Input::AxisBinding::ForGamePad(Input::GamePadAxis::RightStickX, 0.2f, -1.0f));
    axes_.lookPitchRate = actions.AddAxis("LookPitch");
    actions.BindAxis(*axes_.lookPitchRate, Input::AxisBinding::ForGamePad(Input::GamePadAxis::RightStickY));
    axes_.fast = actions.AddAction("Run");
    actions.Bind(*axes_.fast, Input::Binding::ForKey(Keys::LeftShift));
    jump_ = actions.AddAction("Jump");
    actions.Bind(jump_, Input::Binding::ForKey(Keys::Space));
    actions.Bind(jump_, Input::Binding::ForButton(Microsoft::Xna::Framework::Input::Buttons::A));
    captureMouse_ = actions.AddAction("CaptureMouse");
    actions.Bind(captureMouse_, Input::Binding::ForMouse(Input::MouseButton::Left));

    // Meshes and materials; the crates' texture comes from assets/.
    auto& device = getGraphicsDeviceProperty();
    const auto& layout = Graphics::VertexLayout::PositionNormalTexture();
    Graphics::MeshBuilder builder;
    builder.AddGrid(Vector3::Zero, 80.0f, 80.0f, 16, 16, 20.0f);
    ground_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
    builder.AddBox(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f));
    cube_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
    crateTexture_ = std::make_unique<Microsoft::Xna::Framework::Graphics::Texture2D>(
        getContentProperty().Load<Microsoft::Xna::Framework::Graphics::Texture2D>("crate"));
    crateMaterial_.baseTexture = crateTexture_.get();
    crateMaterial_.sampler = &Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp;
    groundMaterial_.diffuseColor = Vector3(0.36f, 0.52f, 0.3f);
    stoneMaterial_.diffuseColor = Vector3(0.62f, 0.6f, 0.56f);

    // The world: the ground, a ring of crates, and stairs up to a platform.
    world_.AddBox(BoundingBox(Vector3(-40.0f, -1.0f, -40.0f), Vector3(40.0f, 0.0f, 40.0f)));
    for (int i = 0; i < 12; ++i) {
        const float angle = Microsoft::Xna::Framework::MathHelper::TwoPi * static_cast<float>(i) / 12.0f;
        const Vector3 at(9.0f * std::cos(angle), 0.0f, 9.0f * std::sin(angle));
        AddBlock(BoundingBox(at - Vector3(0.6f, 0.0f, 0.6f), at + Vector3(0.6f, 1.2f, 0.6f)), crateMaterial_);
    }
    for (int step = 0; step < 5; ++step) {
        const float z = -3.0f - 0.3f * static_cast<float>(step);
        AddBlock(BoundingBox(Vector3(-1.0f, 0.0f, z - 0.3f), Vector3(1.0f, 0.17f * static_cast<float>(step + 1), z)),
                 stoneMaterial_);
    }
    AddBlock(BoundingBox(Vector3(-2.5f, 0.0f, -8.5f), Vector3(2.5f, 0.85f, -4.5f)), stoneMaterial_);

    // The sky: a dome drawn before everything else, and the light and fog
    // set from the same colours, an afternoon to start with.
    time_.hours = 16.0;
    sky_ = std::make_unique<Graphics::SkyDome>(device);
    sky_->AttachTo(Renderer());
    UpdateSky();

    body_.Teleport(world_, Vector3(0.0f, 0.0f, 2.0f));
}

void @GAME_CLASS@::UpdateSky()
{
    const Vector3 sun = time_.DirectionToSun();
    const auto colors = palette_.At(static_cast<float>(time_.Sun().elevationDegrees));
    sky_->SetColors(colors, sun);
    Graphics::ApplySkyLighting(Renderer().Lighting(), colors, sun, 25.0f, 70.0f);
}

void @GAME_CLASS@::FixedUpdate(const double stepSeconds)
{
    // Walking in fixed steps: the same input moves the same way at any frame rate.
    const auto input = Graphics::ReadCameraInput(Actions(), axes_, static_cast<float>(stepSeconds));
    const float speed = WalkSpeed * (input.fast ? RunMultiplier : 1.0f);
    body_.Step(world_, Collision::WishVelocity(input.move, yaw_, speed), jumpQueued_, static_cast<float>(stepSeconds));
    jumpQueued_ = false;
}

void @GAME_CLASS@::UpdateFrame(const double frameSeconds)
{
    if (Actions().Pressed(captureMouse_)) {
        CaptureMouse(true);
    }
    if (Actions().Pressed(jump_)) {
        jumpQueued_ = true; // the next fixed step jumps
    }
    // Looking per frame, so the view follows the mouse without delay.
    const auto look = Graphics::ReadCameraInput(Actions(), axes_, static_cast<float>(frameSeconds));
    yaw_ += look.yaw - MouseDelta().X * MouseRadiansPerPixel;
    pitch_ = std::clamp(pitch_ + look.pitch - MouseDelta().Y * MouseRadiansPerPixel, -1.5f, 1.5f);
    MainCamera().SetYawPitchRoll(yaw_, pitch_);
    MainCamera().SetPosition(body_.Eye() + Vector3(0.0f, body_.HeadBob(0.02f), 0.0f));

    // A day passes in 48 minutes.
    time_.Advance(frameSeconds, 30.0);
    UpdateSky();
}

void @GAME_CLASS@::Submit(App::Frame& frame)
{
    frame.Add(ground_, groundMaterial_, Matrix::getIdentityProperty());
    for (const auto& block : blocks_) {
        frame.Add(cube_, *block.material, block.world);
    }
}
