#include "Game.hpp"

#include "CnaEngine/Enhanced/RelativeMouseCapture.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <cmath>

using namespace CnaEngine;
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
        return options;
    }

    constexpr float MouseRadiansPerPixel = 0.0025f;
}

@GAME_CLASS@::@GAME_CLASS@() : EngineGame(MakeOptions())
{
    // Mouse look that also works in a browser (its pointer lock).
    SetMouseCapture(std::make_unique<Enhanced::RelativeMouseCapture>());
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
    captureMouse_ = actions.AddAction("CaptureMouse");
    actions.Bind(captureMouse_, Input::Binding::ForMouse(Input::MouseButton::Left));

    // The world: a field and a ring of crates, built in code; the crates'
    // texture comes from assets/.
    auto& device = getGraphicsDeviceProperty();
    const auto& layout = Graphics::VertexLayout::PositionNormalTexture();
    Graphics::MeshBuilder builder;
    builder.AddGrid(Vector3::Zero, 80.0f, 80.0f, 16, 16, 20.0f);
    ground_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
    builder.AddBox(Vector3(-0.6f, 0.0f, -0.6f), Vector3(0.6f, 1.2f, 0.6f));
    crate_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
    crateTexture_ = std::make_unique<Microsoft::Xna::Framework::Graphics::Texture2D>(
        getContentProperty().Load<Microsoft::Xna::Framework::Graphics::Texture2D>("crate"));
    crateMaterial_.baseTexture = crateTexture_.get();
    crateMaterial_.sampler = &Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp;
    groundMaterial_.diffuseColor = Vector3(0.36f, 0.52f, 0.3f);
    for (int i = 0; i < 12; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / 12.0f;
        crates_.emplace_back(9.0f * std::cos(angle), 0.0f, 9.0f * std::sin(angle));
    }

    // Light and fog: a low sun, a cool sky fill, fog the colour of the sky.
    auto& lighting = Renderer().Lighting();
    lighting.defaultLighting = false;
    lighting.lightDirection = Vector3(-0.5f, -0.7f, -0.4f);
    lighting.lightColor = Vector3(1.0f, 0.95f, 0.85f);
    lighting.ambientColor = Vector3(0.3f, 0.32f, 0.36f);
    lighting.fogEnabled = true;
    lighting.fogColor = Options().clearColor.ToVector3();
    lighting.fogStart = 20.0f;
    lighting.fogEnd = 60.0f;

    MainCamera().SetPosition(Vector3(0.0f, 1.7f, 0.0f));
    walk_.Settings().speed = 3.5f;
    walk_.Settings().fastMultiplier = 1.8f;
    walk_.SyncFrom(MainCamera());
}

void @GAME_CLASS@::FixedUpdate(const double stepSeconds)
{
    // Walking in fixed steps: the same input moves the same way at any frame rate.
    auto input = Graphics::ReadCameraInput(Actions(), axes_, static_cast<float>(stepSeconds));
    input.yaw = 0.0f;
    input.pitch = 0.0f;
    walk_.Update(MainCamera(), input, static_cast<float>(stepSeconds));
}

void @GAME_CLASS@::UpdateFrame(const double frameSeconds)
{
    if (Actions().Pressed(captureMouse_)) {
        CaptureMouse(true);
    }
    // Looking per frame, so the view follows the mouse without delay.
    Graphics::CameraControlInput look = Graphics::ReadCameraInput(Actions(), axes_, static_cast<float>(frameSeconds));
    look.move = Vector3::Zero;
    look.yaw -= MouseDelta().X * MouseRadiansPerPixel;
    look.pitch -= MouseDelta().Y * MouseRadiansPerPixel;
    walk_.Update(MainCamera(), look, 0.0f);
}

void @GAME_CLASS@::Submit(App::Frame& frame)
{
    frame.Add(ground_, groundMaterial_, Matrix::getIdentityProperty());
    for (const auto& position : crates_) {
        frame.Add(crate_, crateMaterial_, Matrix::CreateTranslation(position));
    }
}
