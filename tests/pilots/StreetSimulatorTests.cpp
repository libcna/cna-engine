// SPDX-License-Identifier: MIT
//
// PILOT-001 probe: street-simulator's camera and tangent-frame geometry
// through the engine's Camera, MeshData and GpuMesh, compared with Street's own
// Camera and GpuMesh (compiled read-only from the pilot checkout).
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/MeshData.hpp"

#include "CnaStreet/Geometry/MeshBuilder.hpp"
#include "CnaStreet/Render/Camera.hpp"
#include "CnaStreet/Render/CameraController.hpp"
#include "CnaStreet/Render/GpuMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <vector>

namespace CnaEngine::Pilot
{
    using Microsoft::Xna::Framework::Color;
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
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;
    using Microsoft::Xna::Framework::Input::MouseState;

    namespace
    {
        // Street measures yaw clockwise seen from above; the engine (like
        // XNA's CreateFromYawPitchRoll) counter-clockwise. Pitch agrees.
        void ApplyStreetCamera(const CnaStreet::Camera& street, Graphics::Camera& camera)
        {
            camera.SetPosition(street.position());
            camera.SetYawPitchRoll(-street.yaw(), street.pitch());
            camera.SetPerspective(street.verticalFov(), street.aspect(), street.nearPlane(), street.farPlane());
        }

        Graphics::MeshData ToEngine(const CnaStreet::Geometry::MeshData& source)
        {
            Graphics::MeshData mesh;
            for (const auto& v : source.vertices) {
                mesh.positions.push_back(v.Position);
                mesh.normals.push_back(v.Normal);
                mesh.tangents.push_back(v.Tangent);
                mesh.texCoords0.push_back(v.TextureCoordinate);
            }
            mesh.indices = source.indices;
            return mesh;
        }

        CnaStreet::Geometry::MeshData StreetProps()
        {
            CnaStreet::Geometry::MeshBuilder builder;
            builder.addBox(Vector3(-1.6f, 0.0f, -0.8f), Vector3(-0.4f, 1.4f, 0.8f));
            builder.addCylinder(Vector3(1.0f, 0.0f, 0.0f), 0.5f, 0.5f, 1.8f, 20);
            builder.addSphere(Vector3(0.0f, 2.2f, 0.0f), 0.6f, 20, 12);
            return builder.take();
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

        MouseState Mouse(int x, int y)
        {
            return MouseState(x, y, 0, ButtonState::Released, ButtonState::Released, ButtonState::Released,
                              ButtonState::Released, ButtonState::Released);
        }
    }

    TEST(StreetSimulatorTests, FlyingCameraDrivesTheEngineCamera)
    {
        CnaStreet::Camera street;
        street.setPerspective(1.1f, 16.0f / 9.0f, 0.12f, 620.0f);
        CnaStreet::CameraController controller;
        controller.setCamera(&street);
        controller.setMode(CnaStreet::CameraMode::Fly);
        controller.setMouseCaptured(true);

        Graphics::Camera camera;
        KeyboardState previousKeys;
        MouseState previousMouse = Mouse(0, 0);
        float worst = 0.0f;
        for (int frame = 0; frame < 300; ++frame) {
            // Fly forward, strafe, climb, and look around with the mouse.
            std::initializer_list<Keys> keys = frame % 90 < 45 ? std::initializer_list<Keys>{Keys::W, Keys::D}
                                                                : std::initializer_list<Keys>{Keys::W, Keys::E};
            const KeyboardState keyboard(keys);
            const MouseState mouse = Mouse(frame * 3, (frame % 60) - 30);
            controller.update(1.0f / 60.0f, keyboard, previousKeys, mouse, previousMouse);
            previousKeys = keyboard;
            previousMouse = mouse;

            ApplyStreetCamera(street, camera);
            worst = std::fmax(worst, MaxDifference(camera.View(), street.view()));
            ASSERT_LT(MaxDifference(camera.Projection(), street.projection()), 1e-6f) << "frame " << frame;
            ASSERT_LT(Vector3::Distance(camera.Forward(), street.forward()), 1e-4f) << "frame " << frame;
            ASSERT_LT(Vector3::Distance(camera.Right(), street.right()), 1e-4f) << "frame " << frame;
        }
        EXPECT_LT(worst, 1e-3f);
        EXPECT_GT(street.position().Length(), 5.0f) << "the camera actually flew";
    }

    TEST(StreetSimulatorTests, TangentFrameGeometryConvertsWithoutLoss)
    {
        const CnaStreet::Geometry::MeshData street = StreetProps();
        const Graphics::MeshData engine = ToEngine(street);
        EXPECT_TRUE(engine.Validate().empty());
        EXPECT_EQ(engine.Attributes(),
                  (Graphics::AttributeMask{Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Normal,
                                           Graphics::VertexAttribute::Tangent, Graphics::VertexAttribute::TexCoord0}));
        EXPECT_EQ(engine.Bounds().Min, street.bounds().Min);
        EXPECT_EQ(engine.Bounds().Max, street.bounds().Max);
        EXPECT_GT(engine.SignedVolume(), 0.0f) << "Street winds front faces the XNA way too";

        // The tangent frames Street authors agree with the engine's
        // generator wherever the UV mapping is well-defined.
        Graphics::MeshData regenerated = engine;
        regenerated.ComputeTangents();
        int compared = 0;
        int agreeing = 0;
        for (std::size_t i = 0; i < engine.VertexCount(); ++i) {
            const auto& a = engine.tangents[i];
            const auto& b = regenerated.tangents[i];
            ++compared;
            if (Vector3::Dot(Vector3(a.X, a.Y, a.Z), Vector3(b.X, b.Y, b.Z)) > 0.9f && a.W == b.W) {
                ++agreeing;
            }
        }
        EXPECT_GT(agreeing, compared * 9 / 10) << agreeing << " of " << compared;
    }

    TEST(StreetSimulatorTests, EngineUploadRendersIdenticallyToStreetUpload)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        constexpr int Width = 96;
        constexpr int Height = 64;
        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);

        const CnaStreet::Geometry::MeshData props = StreetProps();
        const CnaStreet::GpuMesh streetGpu(device, props, "props");
        const Graphics::GpuMesh engineGpu =
            Graphics::GpuMesh::Upload(device, ToEngine(props), Graphics::VertexLayout::PositionNormalTangentTexture());
        EXPECT_TRUE(engineGpu.ConversionReport().dropped.Empty());
        EXPECT_TRUE(engineGpu.ConversionReport().narrowed.Empty());

        CnaStreet::Camera street;
        street.setPosition(Vector3(0.5f, 2.0f, 4.5f));
        street.setOrientation(-0.1f, -0.25f);
        street.setPerspective(1.1f, static_cast<float>(Width) / Height, 0.12f, 100.0f);
        Graphics::Camera camera;
        ApplyStreetCamera(street, camera);

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
        const auto streetPixels = render([&] { streetGpu.draw(device); });
        const auto enginePixels = render([&] { engineGpu.Draw(device); });

        int covered = 0;
        for (const auto& p : enginePixels) {
            covered += p == Color(20, 30, 40, 255) ? 0 : 1;
        }
        EXPECT_GT(covered, Width * Height / 10) << "the props are on screen";
        EXPECT_EQ(enginePixels, streetPixels);
    }
}
