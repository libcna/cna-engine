// SPDX-License-Identifier: MIT
//
// quick_start: a small complete game on the strict profile, written as an
// ordinary XNA Game. A player block gathers glowing pickups on a checkered
// field. Everything a game needs from the engine appears once:
//
//  - input: an ActionMap with keyboard and gamepad bindings (named, so a
//    settings file could rebind them);
//  - simulation: a FixedStepClock stepping movement at 120 Hz whatever the
//    frame rate;
//  - content: MeshBuilder shapes uploaded as GpuMesh, Material descriptions
//    (lit, textured with its own sampler, emissive, blended);
//  - rendering: render items culled with CullInto, the pickups as one
//    instanced item, a RenderQueue and the ForwardRenderer with a lighting rig;
//  - a rear-view mirror: the engine renders a mirrored camera into a render
//    target, and XNA's SpriteBatch puts it on screen after the engine's frame
//    (the renderer leaves the device states as it found them);
//  - diagnostics and a screenshot through the ScreenshotQueue.
//
// It runs headless and deterministic: input comes from a script, and the
// game logic counts updates, not wall-clock time. Without the script (pass
// --interactive) it reads the real keyboard and gamepad.
//
// Usage: cna_engine_quick_start [output-directory] [--interactive]
#include "CnaEngine/Core/FixedStepClock.hpp"
#include "CnaEngine/Diagnostics/FrameStatistics.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/DiagnosticsReport.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"
#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace CnaEngine;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::GameTime;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::SamplerState;
    using Microsoft::Xna::Framework::Graphics::Texture2D;
    using Microsoft::Xna::Framework::Input::Keys;

    constexpr int Width = 320;
    constexpr int Height = 180;
    // The rear-view mirror inset, centred at the top of the screen.
    constexpr int MirrorWidth = 96;
    constexpr int MirrorHeight = 54;
    const Color Sky(135, 175, 215, 255);
    constexpr float ArenaHalf = 9.0f;
    constexpr float Speed = 6.0f; // m/s at full stick
    constexpr float PickupRadius = 0.9f;
    // The scripted run: this many updates, then a screenshot, then exit.
    constexpr int ScriptedUpdates = 150;

    // A route through the field: right, then forward, then left and forward.
    Input::InputSnapshot ScriptedInput(const int update)
    {
        Input::InputSnapshot snapshot;
        if (update < 40) {
            snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::D});
        } else if (update < 90) {
            snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::W});
        } else if (update < 130) {
            snapshot.keyboard = Microsoft::Xna::Framework::Input::KeyboardState({Keys::A, Keys::W});
        }
        return snapshot;
    }

    class QuickStartGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        QuickStartGame(std::filesystem::path output, const bool scripted)
            : graphics_(this), scripted_(scripted), screenshots_(output.string(), "quick_start")
        {
            graphics_.setPreferredBackBufferWidthProperty(Width);
            graphics_.setPreferredBackBufferHeightProperty(Height);
            // HiDef: hardware instancing and back-buffer capture.
            graphics_.setGraphicsProfileProperty(Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef);
        }

        [[nodiscard]] bool Finished() const noexcept { return finished_; }
        [[nodiscard]] int Collected() const noexcept { return collected_; }
        [[nodiscard]] const Graphics::CaptureResult& Screenshot() const noexcept { return screenshot_; }

    protected:
        void Initialize() override
        {
            // Input as data: names and bindings, no key codes in the game logic.
            moveX_ = actions_.AddAxis("MoveRight");
            moveZ_ = actions_.AddAxis("MoveForward");
            actions_.BindAxis(moveX_, Input::AxisBinding::ForKeys(Keys::A, Keys::D));
            actions_.BindAxis(moveX_, Input::AxisBinding::ForGamePad(Input::GamePadAxis::LeftStickX));
            actions_.BindAxis(moveZ_, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
            actions_.BindAxis(moveZ_, Input::AxisBinding::ForGamePad(Input::GamePadAxis::LeftStickY));
            screenshotAction_ = actions_.AddAction("Screenshot");
            actions_.Bind(screenshotAction_, Input::Binding::ForKey(Keys::F12));
            Game::Initialize(); // creates the device, then calls LoadContent
        }

        void LoadContent() override
        {
            GraphicsDevice& device = getGraphicsDeviceProperty();
            const auto& layout = Graphics::VertexLayout::PositionNormalTexture();

            Graphics::MeshBuilder builder;
            builder.AddGrid(Vector3::Zero, 2.0f * ArenaHalf, 2.0f * ArenaHalf, 8, 8, 8.0f);
            ground_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
            builder.AddBox(Vector3(-0.4f, 0.0f, -0.4f), Vector3(0.4f, 0.9f, 0.4f));
            player_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
            builder.AddSphere(Vector3::Zero, 0.35f, 16, 10);
            pickup_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
            builder.AddCylinder(Vector3::Zero, 0.6f, 0.01f, 24);
            shadow_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);

            // A 2x2 checker repeated by the grid's UVs, sampled without filtering.
            checker_ = std::make_unique<Texture2D>(device, 2, 2);
            const Color texels[4] = {Color(90, 140, 80, 255), Color(110, 165, 95, 255), Color(110, 165, 95, 255),
                                     Color(90, 140, 80, 255)};
            checker_->SetData(texels, 4);

            groundMaterial_.name = "ground";
            groundMaterial_.baseTexture = checker_.get();
            groundMaterial_.sampler = &SamplerState::PointWrap;
            playerMaterial_.name = "player";
            playerMaterial_.diffuseColor = Vector3(0.85f, 0.2f, 0.15f);
            playerMaterial_.specularColor = Vector3(0.4f, 0.4f, 0.4f);
            pickupMaterial_.name = "pickup";
            pickupMaterial_.diffuseColor = Vector3(1.0f, 0.8f, 0.2f);
            pickupMaterial_.emissiveColor = Vector3(0.5f, 0.35f, 0.0f);
            shadowMaterial_.name = "shadow";
            shadowMaterial_.shading = Graphics::ShadingModel::Unlit;
            shadowMaterial_.diffuseColor = Vector3::Zero;
            shadowMaterial_.alpha = 0.45f;
            shadowMaterial_.blend = Graphics::BlendMode::AlphaBlend;

            // Pickups on a ring and a trail: one instanced render item.
            pickups_.SetMesh(&pickup_);
            for (int i = 0; i < 12; ++i) {
                const float angle = Microsoft::Xna::Framework::MathHelper::TwoPi * static_cast<float>(i) / 12.0f;
                pickupPositions_.emplace_back(6.0f * std::cos(angle), 0.5f, 6.0f * std::sin(angle));
            }
            for (const auto& [x, z] : {std::pair{2.0f, 0.0f}, std::pair{4.0f, 0.0f}, std::pair{4.0f, -2.0f},
                                       std::pair{4.0f, -4.0f}, std::pair{-2.0f, 2.0f}, std::pair{-4.0f, 2.0f}}) {
                pickupPositions_.emplace_back(x, 0.5f, z);
            }
            RebuildPickups();

            renderer_ = std::make_unique<Graphics::ForwardRenderer>(device);
            auto& lighting = renderer_->Lighting();
            lighting.defaultLighting = false;
            lighting.lightDirection = Vector3(-0.4f, -1.0f, -0.3f);
            lighting.lightColor = Vector3(0.9f, 0.85f, 0.75f);
            lighting.fillLights = {{Vector3(0.3f, -0.2f, 1.0f), Vector3(0.2f, 0.25f, 0.35f), Vector3::Zero}};
            lighting.ambientColor = Vector3(0.25f, 0.27f, 0.3f);

            camera_.SetPerspective(Microsoft::Xna::Framework::MathHelper::ToRadians(60.0f),
                                   static_cast<float>(Width) / Height, 0.1f, 100.0f);

            mirrorTarget_ = std::make_unique<Microsoft::Xna::Framework::Graphics::RenderTarget2D>(
                device, MirrorWidth, MirrorHeight, false, Microsoft::Xna::Framework::Graphics::SurfaceFormat::Color,
                Microsoft::Xna::Framework::Graphics::DepthFormat::Depth24);
            mirrorCamera_.SetPerspective(Microsoft::Xna::Framework::MathHelper::ToRadians(50.0f),
                                         static_cast<float>(MirrorWidth) / MirrorHeight, 0.1f, 60.0f);
            mirrorCamera_.SetMirrored(true); // a mirror shows the world flipped
            sprites_ = std::make_unique<Microsoft::Xna::Framework::Graphics::SpriteBatch>(device);
        }

        void Update(GameTime& gameTime) override
        {
            const auto snapshot = scripted_ ? ScriptedInput(updates_) : Input::InputSnapshot::Capture();
            actions_.Update(snapshot);
            ++updates_;

            // The simulation advances in fixed 1/120 s steps.
            clock_.Run(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty(), [&](const double dt) {
                Vector3 wish(actions_.Axis(moveX_), 0.0f, -actions_.Axis(moveZ_));
                if (wish.LengthSquared() > 1.0f) {
                    wish.Normalize(); // diagonals are not faster
                }
                position_ += wish * (Speed * static_cast<float>(dt));
                position_.X = std::clamp(position_.X, -ArenaHalf, ArenaHalf);
                position_.Z = std::clamp(position_.Z, -ArenaHalf, ArenaHalf);
                Collect();
            });

            // A follow camera above and behind the player, looking at it.
            const Vector3 offset(0.0f, 7.0f, 9.0f);
            camera_.SetPosition(position_ + offset);
            camera_.SetYawPitchRoll(0.0f, -std::atan2(offset.Y - 0.5f, offset.Z));
            // The mirror looks back over the player's shoulder.
            mirrorCamera_.SetPosition(position_ + Vector3(0.0f, 1.6f, 0.0f));
            mirrorCamera_.SetYawPitchRoll(Microsoft::Xna::Framework::MathHelper::Pi, -0.25f);

            if (actions_.Pressed(screenshotAction_) || (scripted_ && updates_ == ScriptedUpdates)) {
                screenshots_.RequestNext(true);
            }
            Game::Update(gameTime);
        }

        void Draw(const GameTime& gameTime) override
        {
            GraphicsDevice& device = getGraphicsDeviceProperty();

            // Rigid items go through the culling policy; the pickups cull
            // themselves per instance and form one instanced item.
            std::vector<Graphics::RenderItem> items;
            auto add = [&](const Graphics::GpuMesh& mesh, const Graphics::Material& material, const Matrix& world) {
                Graphics::RenderItem item;
                item.mesh = &mesh;
                item.material = &material;
                item.world = world;
                item.bounds = Graphics::WorldBounds(mesh, world);
                items.push_back(item);
            };
            add(ground_, groundMaterial_, Matrix::getIdentityProperty());
            add(player_, playerMaterial_, Matrix::CreateTranslation(position_));
            add(shadow_, shadowMaterial_, Matrix::CreateTranslation(position_ + Vector3(0.0f, 0.02f, 0.0f)));

            Graphics::VisibilitySettings visibility;
            visibility.maxDistance = 60.0f;
            // One view: cull for its camera, queue, render.
            auto view = [&](const Graphics::Camera& camera, Graphics::CullStatistics& culling,
                            Graphics::RenderStatistics* statistics) {
                Graphics::RenderQueue queue;
                Graphics::CullInto(items, camera, visibility, queue, culling);
                pickups_.Cull(camera, visibility, culling);
                if (pickups_.VisibleCount() > 0) {
                    Graphics::RenderItem item;
                    item.mesh = &pickup_;
                    item.material = &pickupMaterial_;
                    item.bounds = Microsoft::Xna::Framework::BoundingSphere(Vector3::Zero, 2.0f * ArenaHalf);
                    item.instances = &pickups_;
                    queue.Submit(item);
                }
                queue.Sort(camera.Position());
                renderer_->Render(queue, camera, statistics);
            };

            // The mirror first, into its own target.
            device.SetRenderTarget(mirrorTarget_.get());
            device.Clear(Sky);
            Graphics::CullStatistics mirrorCulling;
            view(mirrorCamera_, mirrorCulling, nullptr);
            device.SetRenderTarget(nullptr);

            device.Clear(Sky);
            culling_ = {};
            render_ = {};
            view(camera_, culling_, &render_);

            // Plain XNA drawing after the engine's frame: the mirror inset.
            sprites_->Begin();
            sprites_->Draw(*mirrorTarget_,
                           Microsoft::Xna::Framework::Rectangle((Width - MirrorWidth) / 2, 4, MirrorWidth, MirrorHeight),
                           Color(255, 255, 255, 255));
            sprites_->End();
            frames_.AddFrame(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());

            for (auto& result : screenshots_.Process(device)) {
                screenshot_ = std::move(result);
                finished_ = scripted_;
            }
            Game::Draw(gameTime);
        }

    public:
        void Report() const
        {
            Graphics::DiagnosticsSources sources;
            sources.frames = &frames_;
            sources.render = &render_;
            sources.culling = &culling_;
            for (const auto& line : Graphics::FormatDiagnostics(sources)) {
                std::printf("  %s\n", line.c_str());
            }
            for (const auto& fallback : renderer_->LastFallbacks()) {
                std::printf("  fallback: %s\n", fallback.c_str());
            }
        }

    private:
        void Collect()
        {
            const auto before = pickupPositions_.size();
            std::erase_if(pickupPositions_, [&](const Vector3& p) {
                return Vector3::Distance(Vector3(position_.X, 0.5f, position_.Z), p) < PickupRadius;
            });
            if (pickupPositions_.size() != before) {
                collected_ += static_cast<int>(before - pickupPositions_.size());
                RebuildPickups();
            }
        }

        void RebuildPickups()
        {
            pickups_.Clear();
            for (const auto& p : pickupPositions_) {
                pickups_.Add(Matrix::CreateTranslation(p));
            }
        }

        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        bool scripted_;
        Input::ActionMap actions_;
        Input::AxisId moveX_{};
        Input::AxisId moveZ_{};
        Input::ActionId screenshotAction_{};
        FixedStepClock clock_{1.0 / 120.0};
        int updates_ = 0;

        Graphics::GpuMesh ground_;
        Graphics::GpuMesh player_;
        Graphics::GpuMesh pickup_;
        Graphics::GpuMesh shadow_;
        std::unique_ptr<Texture2D> checker_;
        Graphics::Material groundMaterial_;
        Graphics::Material playerMaterial_;
        Graphics::Material pickupMaterial_;
        Graphics::Material shadowMaterial_;
        Graphics::InstanceSet pickups_;
        std::vector<Vector3> pickupPositions_;

        Vector3 position_ = Vector3::Zero;
        int collected_ = 0;
        Graphics::Camera camera_;
        Graphics::Camera mirrorCamera_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> mirrorTarget_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> sprites_;
        std::unique_ptr<Graphics::ForwardRenderer> renderer_;

        Diagnostics::FrameStatistics frames_{120};
        Graphics::RenderStatistics render_;
        Graphics::CullStatistics culling_;
        Graphics::ScreenshotQueue screenshots_;
        Graphics::CaptureResult screenshot_;
        bool finished_ = false;
    };
}

int main(int argc, char** argv)
{
    try {
        std::filesystem::path output = std::filesystem::current_path();
        bool scripted = true;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--interactive") {
                scripted = false;
            } else {
                output = argument;
            }
        }
        std::filesystem::create_directories(output);

        QuickStartGame game(output, scripted);
        if (!scripted) {
            game.Run();
            return 0;
        }
        // Headless and deterministic: one frame at a time until the scripted
        // run has taken its screenshot (with a generous cap).
        for (int frame = 0; frame < 10 * ScriptedUpdates && !game.Finished(); ++frame) {
            game.RunOneFrame();
        }

        std::printf("quick_start: gathered %d pickups\n", game.Collected());
        game.Report();
        const auto& shot = game.Screenshot();
        if (!shot.success) {
            std::fprintf(stderr, "quick_start: no screenshot (%s)\n", shot.error.c_str());
            return 1;
        }
        std::printf("  screenshot %s (%dx%d)\n", shot.path.c_str(), shot.width, shot.height);
        // The scripted route runs along the trail (four pickups); the
        // screenshot must show the field, not the clear color.
        auto pixel = [&](const int x, const int y) { return shot.pixels[static_cast<std::size_t>(y * shot.width + x)]; };
        const bool drawn = pixel(shot.width / 2, shot.height / 2) != Sky;
        // The bottom of the mirror inset shows the field behind the player.
        const Color mirror = pixel(shot.width / 2, 4 + MirrorHeight - 6);
        const bool mirrored = mirror.getGProperty() > mirror.getRProperty() && mirror.getGProperty() > mirror.getBProperty();
        if (game.Collected() < 4 || !drawn || !mirrored) {
            std::fprintf(stderr, "quick_start: unexpected result (gathered %d, drawn %d, mirror %d)\n",
                         game.Collected(), drawn ? 1 : 0, mirrored ? 1 : 0);
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "quick_start: %s\n", e.what());
        return 1;
    }
}
