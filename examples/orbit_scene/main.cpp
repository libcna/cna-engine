// SPDX-License-Identifier: MIT
//
// orbit_scene: the strict-profile vertical slice in one small program.
//
// It uses only CnaEngine::Core and CnaEngine::Graphics (no cna-extended), so it
// is also the reference for what a strict-XNA game can do with the engine:
//
//  - an ActionMap with keyboard axes drives a FlyCameraController; the input
//    comes from scripted InputSnapshots, so every run is identical;
//  - MeshBuilder procedurally builds a ground grid, boxes and a sphere, which
//    GpuMesh uploads in two different vertex layouts;
//  - each frame is drawn with XNA's BasicEffect into an off-screen render
//    target, with draw statistics and frame timings collected;
//  - the last frame is written as a PNG through the ScreenshotQueue path and
//    its centre pixel is checked so the program fails if nothing was drawn.
//
// Usage: cna_engine_orbit_scene [output-directory]
#include "CnaEngine/Core/Version.hpp"
#include "CnaEngine/Diagnostics/FrameStatistics.hpp"
#include "CnaEngine/Diagnostics/ScopedTimer.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Input/ActionMap.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    using namespace CnaEngine;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    constexpr int Width = 320;
    constexpr int Height = 180;
    constexpr int Frames = 30;
    constexpr float FrameSeconds = 1.0f / 30.0f;

    struct SceneObject
    {
        Graphics::GpuMesh mesh;
        Matrix world;
        bool vertexColors;
    };

    // Scripted input: walk forward, briefly turning right, then hold still.
    Input::InputSnapshot ScriptedInput(int frame)
    {
        Input::InputSnapshot snapshot;
        if (frame < 5) {
            snapshot.keyboard = KeyboardState({Keys::W, Keys::Right});
        } else if (frame < 20) {
            snapshot.keyboard = KeyboardState({Keys::W});
        }
        return snapshot;
    }
}

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path output = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path();
        std::printf("cna-engine %s orbit_scene\n", std::string(LibraryVersion()).c_str());

        // --- Input: names are data, bindings could come from a settings file.
        Input::ActionMap actions;
        Graphics::CameraInputAxes axes;
        axes.moveRight = actions.AddAxis("MoveRight");
        axes.moveForward = actions.AddAxis("MoveForward");
        axes.lookYaw = actions.AddAxis("LookYaw");
        axes.lookPitch = actions.AddAxis("LookPitch");
        axes.lookScale = 1.2f * FrameSeconds; // radians per second at full deflection
        actions.BindAxis(axes.moveRight, Input::AxisBinding::ForKeys(Keys::A, Keys::D));
        actions.BindAxis(axes.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
        actions.BindAxis(axes.lookYaw, Input::AxisBinding::ForKeys(Keys::Right, Keys::Left));
        actions.BindAxis(axes.lookPitch, Input::AxisBinding::ForKeys(Keys::Down, Keys::Up));
        const Input::ActionId screenshot = actions.AddAction("Screenshot");
        actions.Bind(screenshot, Input::Binding::ForKey(Keys::F12));

        // --- Camera and controller.
        Graphics::Camera camera;
        camera.SetPerspective(MathHelper::PiOver4, static_cast<float>(Width) / Height, 0.1f, 500.0f);
        camera.SetPosition(Vector3(0.0f, 2.0f, 12.0f));
        camera.LookAt(Vector3(0.0f, 1.0f, 0.0f));
        Graphics::FlyCameraController controller(Graphics::FirstPersonSettings{3.0f, 4.0f, 1.4f});
        controller.SyncFrom(camera);

        // --- Geometry: generic CPU meshes, uploaded in two layouts.
        GraphicsDevice device;
        std::vector<SceneObject> scene;
        {
            Graphics::MeshBuilder ground({Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Color});
            ground.SetColor(Color(70, 110, 60, 255));
            ground.AddGrid(Vector3::Zero, 300.0f, 300.0f, 16, 16);
            scene.push_back({Graphics::GpuMesh::Upload(device, ground.Mesh(), Graphics::VertexLayout::PositionColor()),
                             Matrix::getIdentityProperty(), true});

            Graphics::MeshBuilder boxes({Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Color});
            const Color palette[] = {Color(200, 60, 50, 255), Color(220, 180, 40, 255), Color(60, 90, 200, 255)};
            for (int i = 0; i < 3; ++i) {
                boxes.SetColor(palette[i]);
                const float x = -4.0f + 4.0f * static_cast<float>(i);
                boxes.AddBox(Vector3(x - 1.0f, 0.0f, -1.0f), Vector3(x + 1.0f, 1.0f + static_cast<float>(i), 1.0f));
            }
            scene.push_back({Graphics::GpuMesh::Upload(device, boxes.Mesh(), Graphics::VertexLayout::PositionColor()),
                             Matrix::getIdentityProperty(), true});

            Graphics::MeshBuilder sphere; // position, normal, uv: lit by BasicEffect
            sphere.AddSphere(Vector3::Zero, 1.2f, 24, 16);
            scene.push_back({Graphics::GpuMesh::Upload(device, sphere.Mesh(),
                                                       Graphics::VertexLayout::PositionNormalTexture()),
                             Matrix::CreateTranslation(0.0f, 4.5f, 0.0f), false});
        }

        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);
        BasicEffect colored(device);
        colored.setVertexColorEnabledProperty(true);
        BasicEffect lit(device);
        lit.EnableDefaultLighting();
        lit.setDiffuseColorProperty(Vector3(0.9f, 0.9f, 0.95f));

        Diagnostics::FrameStatistics frameStats(Frames);
        Diagnostics::CpuProfiler profiler;
        profiler.SetEnabled(true);
        Graphics::RenderStatistics renderStats;
        Graphics::ScreenshotQueue screenshots(output.string(), "orbit_scene");

        for (int frame = 0; frame < Frames; ++frame) {
            {
                Diagnostics::ScopedTimer timer(&profiler, "Update");
                Input::InputSnapshot snapshot = ScriptedInput(frame);
                if (frame == Frames - 1) {
                    snapshot.keyboard = KeyboardState({Keys::F12});
                }
                actions.Update(snapshot);
                controller.Update(camera, Graphics::ReadCameraInput(actions, axes), FrameSeconds);
                if (actions.Pressed(screenshot)) {
                    screenshots.RequestNext(true); // fulfilled after the frame is drawn
                }
            }
            {
                Diagnostics::ScopedTimer timer(&profiler, "Draw");
                renderStats.Reset();
                device.SetRenderTarget(&target);
                device.Clear(Color(135, 170, 210, 255));
                device.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
                device.setDepthStencilStateProperty(DepthStencilState::Default);
                for (const auto& object : scene) {
                    BasicEffect& effect = object.vertexColors ? colored : lit;
                    effect.setWorldProperty(object.world);
                    effect.setViewProperty(camera.View());
                    effect.setProjectionProperty(camera.Projection());
                    effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
                    object.mesh.Draw(device, &renderStats);
                }
                device.SetRenderTarget(nullptr);
            }
            profiler.EndFrame();
            frameStats.AddFrame(FrameSeconds);
        }

        // A windowed game calls Process(device) to read the back buffer; this
        // headless program rendered off-screen, so it captures the target.
        auto captures = screenshots.Process(device, target);
        if (captures.size() != 1 || !captures.front().success) {
            std::fprintf(stderr, "capture failed: %s\n", captures.empty() ? "no request" : captures.front().error.c_str());
            return 1;
        }
        const Graphics::CaptureResult& result = captures.front();
        const Color centre = result.pixels[static_cast<std::size_t>(Height / 2 * Width + Width / 2)];
        const Color sky(135, 170, 210, 255);

        std::printf("frames: %llu, average %.2f ms (%.1f fps)\n",
                    static_cast<unsigned long long>(frameStats.FrameCount()), frameStats.AverageSeconds() * 1000.0,
                    frameStats.FramesPerSecond());
        std::printf("last frame: %llu draws, %llu triangles, %llu vertices\n",
                    static_cast<unsigned long long>(renderStats.drawCalls),
                    static_cast<unsigned long long>(renderStats.primitives),
                    static_cast<unsigned long long>(renderStats.vertices));
        for (const auto& section : profiler.LastFrame()) {
            std::printf("cpu %-6s %8.3f ms\n", section.name.c_str(), static_cast<double>(section.nanoseconds) / 1e6);
        }
        const auto memory = Graphics::CurrentGpuMemory();
        std::printf("gpu meshes: %lld, %lld vertex bytes, %lld index bytes\n", static_cast<long long>(memory.meshes),
                    static_cast<long long>(memory.vertexBytes), static_cast<long long>(memory.indexBytes));
        std::printf("camera at (%.2f, %.2f, %.2f); wrote %s\n", camera.Position().X, camera.Position().Y,
                    camera.Position().Z, result.path.c_str());

        if (renderStats.drawCalls != scene.size() || centre == sky) {
            std::fprintf(stderr, "scene was not drawn as expected\n");
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "orbit_scene: %s\n", e.what());
        return 1;
    }
}
