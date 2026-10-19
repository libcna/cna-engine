// SPDX-License-Identifier: MIT
//
// PILOT-002 probe: car-simulator's camera poses and procedural meshes through
// the engine's Camera, MeshData and GpuMesh, compared with car-simulator's own
// CameraPose and GpuMesh (compiled read-only from the pilot checkout).
//
// The chase camera itself is vehicle-specific policy and stays in the game; the
// probe shows that its output drives an engine Camera without loss.
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/MeshData.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"

#include "CarSim/Render/Camera.hpp"
#include "CarSim/Render/GpuMesh.hpp"
#include "CarSim/Render/MeshData.hpp"
#include "CarSim/Render/Screenshot.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace CnaEngine::Pilot
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;

    namespace
    {
        // The adapter a game would write once: a look-at pose drives the engine camera.
        void ApplyPose(const CarSim::Render::CameraPose& pose, float aspect, Graphics::Camera& camera)
        {
            camera.SetPosition(pose.position);
            camera.LookAt(pose.target, pose.up);
            camera.SetPerspective(MathHelper::ToRadians(pose.fieldOfViewDeg), aspect, pose.nearPlane, pose.farPlane);
        }

        // The game's array-of-structs vertices to the engine's attribute streams.
        Graphics::MeshData ToEngine(const CarSim::Render::MeshData& source)
        {
            Graphics::MeshData mesh;
            for (const auto& v : source.vertices) {
                mesh.positions.push_back(v.position);
                mesh.normals.push_back(v.normal);
                mesh.texCoords0.push_back(v.uv);
                mesh.texCoords1.push_back(v.uv2);
                mesh.colors.push_back(v.color);
            }
            mesh.indices = source.indices;
            return mesh;
        }

        CarSim::Render::MeshData PilotScenery()
        {
            CarSim::Render::MeshData mesh;
            mesh.AddBox(Vector3(-1.5f, 0.0f, -1.0f), Vector3(-0.5f, 1.2f, 1.0f));
            mesh.AddCylinder(Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), 0.6f, 1.5f, 24, true);
            mesh.AddTorus(Vector3(0.0f, 2.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), 0.8f, 0.2f, 24, 12);
            return mesh;
        }

        float MaxDifference(const Matrix& a, const Matrix& b)
        {
            const float x[] = {a.M11 - b.M11, a.M12 - b.M12, a.M13 - b.M13, a.M14 - b.M14, a.M21 - b.M21, a.M22 - b.M22,
                               a.M23 - b.M23, a.M24 - b.M24, a.M31 - b.M31, a.M32 - b.M32, a.M33 - b.M33, a.M34 - b.M34,
                               a.M41 - b.M41, a.M42 - b.M42, a.M43 - b.M43, a.M44 - b.M44};
            float m = 0.0f;
            for (const float d : x) {
                m = std::fmax(m, std::fabs(d));
            }
            return m;
        }
    }

    TEST(CarSimulatorRenderTests, ChaseCameraPosesDriveTheEngineCamera)
    {
        CarSim::Render::ChaseCamera chase;
        CarSim::Sim::VehicleState state;
        const float aspect = 16.0f / 9.0f;
        Graphics::Camera camera;
        float worst = 0.0f;
        // A vehicle driving a curve, accelerating, over gentle hills.
        for (int frame = 0; frame < 600; ++frame) {
            const float t = static_cast<float>(frame) / 60.0f;
            const float yaw = 0.4f * t;
            state.worldMatrix = Matrix::CreateRotationY(yaw)
                                * Matrix::CreateTranslation(20.0f * std::sin(yaw), 0.3f * std::sin(t), -20.0f * t);
            state.originPosition = state.worldMatrix.getTranslationProperty();
            state.speedKmh = std::fmin(120.0f, 12.0f * t);
            state.velocity = state.worldMatrix.getForwardProperty() * (state.speedKmh / 3.6f);
            if (frame == 0) {
                chase.Snap(state);
            } else {
                chase.Update(state, 1.0f / 60.0f);
            }
            const auto& pose = chase.Pose();
            ApplyPose(pose, aspect, camera);
            worst = std::fmax(worst, MaxDifference(camera.View(), pose.View()));
            ASSERT_LT(MaxDifference(camera.Projection(), pose.Projection(aspect)), 1e-6f) << "frame " << frame;
            const Vector3 forward = pose.Forward();
            ASSERT_LT(Vector3::Distance(camera.Forward(), forward), 1e-4f) << "frame " << frame;
        }
        // View matrices agree to float precision at world coordinates up to ~200 m.
        EXPECT_LT(worst, 2e-3f);
    }

    TEST(CarSimulatorRenderTests, PilotMeshesConvertWithoutLoss)
    {
        const CarSim::Render::MeshData pilot = PilotScenery();
        const Graphics::MeshData engine = ToEngine(pilot);
        EXPECT_TRUE(engine.Validate().empty());
        EXPECT_EQ(engine.TriangleCount(), pilot.TriangleCount());
        const auto pb = pilot.Bounds();
        const auto eb = engine.Bounds();
        EXPECT_EQ(eb.Min, pb.Min);
        EXPECT_EQ(eb.Max, pb.Max);
        // Same winding convention (XNA clockwise front faces); the pilot
        // reports the signed volume with the opposite sign.
        EXPECT_NEAR(engine.SignedVolume(), -pilot.SignedVolume(), 1e-3f);
        EXPECT_GT(engine.SignedVolume(), 0.0f);
        for (std::size_t t = 0; t < engine.TriangleCount(); t += 37) {
            const Vector3 e = engine.FaceNormal(t);
            const Vector3 p = pilot.EmittedTriangleNormal(t);
            // The pilot's right-hand-rule normal points inward for front faces.
            EXPECT_LE(Vector3::Dot(e, p), 0.0f) << "triangle " << t;
        }
    }

    TEST(CarSimulatorRenderTests, EngineUploadRendersIdenticallyToThePilotUpload)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef); // the pilot always uses 32-bit indices
        constexpr int Width = 96;
        constexpr int Height = 64;
        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);

        const CarSim::Render::MeshData pilotMesh = PilotScenery();
        const auto pilotGpu =
            CarSim::Render::GpuMesh::Create(device, pilotMesh, CarSim::Render::VertexLayout::PositionNormalTexture);
        const Graphics::GpuMesh engineGpu = Graphics::GpuMesh::Upload(
            device, ToEngine(pilotMesh), Graphics::VertexLayout::PositionNormalTexture(),
            Graphics::GpuMeshOptions{.preferSixteenBitIndices = true});
        EXPECT_EQ(engineGpu.ConversionReport().dropped,
                  (Graphics::AttributeMask{Graphics::VertexAttribute::Color, Graphics::VertexAttribute::TexCoord1}))
            << "the layout drops what it cannot carry, and says so";

        CarSim::Render::CameraPose pose;
        pose.position = Vector3(1.5f, 2.0f, 3.5f);
        pose.target = Vector3(0.0f, 1.0f, 0.0f);
        Graphics::Camera camera;
        ApplyPose(pose, static_cast<float>(Width) / Height, camera);

        BasicEffect effect(device);
        effect.EnableDefaultLighting();
        auto render = [&](auto&& draw) {
            device.SetRenderTarget(&target);
            device.Clear(Color(20, 30, 40, 255));
            device.setDepthStencilStateProperty(DepthStencilState::Default);
            device.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
            effect.setViewProperty(camera.View());
            effect.setProjectionProperty(camera.Projection());
            effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
            draw();
            device.SetRenderTarget(nullptr);
            std::vector<Color> pixels(static_cast<std::size_t>(Width * Height));
            target.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return pixels;
        };
        const auto pilotPixels = render([&] { pilotGpu->Draw(device); });
        const auto enginePixels = render([&] { engineGpu.Draw(device); });

        int covered = 0;
        for (const auto& p : enginePixels) {
            covered += p == Color(20, 30, 40, 255) ? 0 : 1;
        }
        EXPECT_GT(covered, Width * Height / 10) << "the scenery is actually on screen";
        EXPECT_EQ(enginePixels, pilotPixels) << "16-bit engine indices and 32-bit pilot indices draw the same image";
    }

    TEST(CarSimulatorRenderTests, ScreenshotsMatchThePilotCapture)
    {
        using Microsoft::Xna::Framework::Graphics::Texture2D;
        const auto directory = std::filesystem::current_path() / "pilot_screenshots";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const std::string pilotPath = (directory / "pilot.png").string();
        const std::string enginePath = (directory / "engine.png").string();

        GraphicsDevice reach;
        EXPECT_FALSE(CarSim::Render::SaveBackBufferPng(reach, pilotPath)) << "Reach cannot read the back buffer";
        const auto refused = Graphics::CaptureBackBuffer(reach, Graphics::CaptureOptions{enginePath, false});
        EXPECT_FALSE(refused.success);
        EXPECT_FALSE(refused.error.empty()) << "the engine says why instead of printing to stderr";

        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        const auto pilotMesh = PilotScenery();
        const auto gpu = Graphics::GpuMesh::Upload(device, ToEngine(pilotMesh),
                                                   Graphics::VertexLayout::PositionNormalTexture());
        device.Clear(Color(90, 120, 150, 255));
        device.setDepthStencilStateProperty(DepthStencilState::Default);
        BasicEffect effect(device);
        effect.EnableDefaultLighting();
        CarSim::Render::CameraPose pose;
        Graphics::Camera camera;
        ApplyPose(pose, 4.0f / 3.0f, camera);
        effect.setViewProperty(camera.View());
        effect.setProjectionProperty(camera.Projection());
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        gpu.Draw(device);

        ASSERT_TRUE(CarSim::Render::SaveBackBufferPng(device, pilotPath));
        const auto captured = Graphics::CaptureBackBuffer(device, Graphics::CaptureOptions{enginePath, true});
        ASSERT_TRUE(captured.success) << captured.error;

        auto decode = [&](const std::string& path) {
            System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
            Texture2D texture = Texture2D::FromStream(device, stream);
            std::vector<Color> pixels(static_cast<std::size_t>(texture.getWidthProperty() * texture.getHeightProperty()));
            texture.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return pixels;
        };
        const auto pilotPixels = decode(pilotPath);
        EXPECT_EQ(decode(enginePath), pilotPixels) << "both PNG files hold the same image";
        EXPECT_EQ(captured.pixels, pilotPixels) << "and the in-memory capture matches the file";
        std::filesystem::remove_all(directory);
    }
}
