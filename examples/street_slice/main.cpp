// SPDX-License-Identifier: MIT
//
// street_slice: the enhanced-profile vertical slice in one small program.
//
// A procedural street block rendered the way a Street-class game would use
// the engine, headless and deterministic:
//
//  - generic meshes (MeshBuilder) for road, pavements, buildings in two LODs
//    and street lamps, uploaded once (GpuMesh, tangent layout for PBR);
//  - glTF-style materials drawn through the forward renderer with the PBR
//    material binder (CnaEngine::EnhancedGraphics, CNA's PbrEffect);
//  - per-frame visibility (frustum + distance) and LOD selection with
//    hysteresis, feeding a sorted render queue;
//  - street lamps as one InstanceSet drawn by the per-instance path inside a
//    custom pass (the stock path for effects without an instance stream);
//  - a scripted fly-through via ActionMap + FlyCameraController;
//  - quality resolution, diagnostics text, and a final PNG.
//
// Usage: cna_engine_street_slice [output-directory]
#include "CnaEngine/Diagnostics/FrameStatistics.hpp"
#include "CnaEngine/Diagnostics/ScopedTimer.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/DiagnosticsReport.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/Quality.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"
#include "CnaEngine/Input/ActionMap.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
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
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    constexpr int Width = 400;
    constexpr int Height = 225;
    constexpr int Frames = 45;
    constexpr float Dt = 1.0f / 30.0f;

    Graphics::MeshData Tangents(Graphics::MeshData mesh)
    {
        mesh.ComputeTangents();
        return mesh;
    }

    Graphics::Material Pbr(const char* name, Vector3 color, float metallic, float roughness)
    {
        Graphics::Material m;
        m.name = name;
        m.shading = Graphics::ShadingModel::PhysicallyBased;
        m.diffuseColor = color;
        m.metallic = metallic;
        m.roughness = roughness;
        return m;
    }

    struct Building
    {
        Matrix world;
        const Graphics::Material* material;
        int lod = -1;
    };
}

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path output = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path();

        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        Graphics::QualityIntent intent;
        intent.preset = Graphics::QualityPreset::Medium;
        const auto quality = Graphics::Resolve(intent, Graphics::DeviceCapabilities::FromDevice(device));

        // --- Geometry: built once, uploaded in the PBR tangent layout. -----
        const auto& layout = Graphics::VertexLayout::PositionNormalTangentTexture();
        Graphics::MeshBuilder builder;
        builder.AddGrid(Vector3(0.0f, 0.0f, -60.0f), 8.0f, 160.0f, 1, 20, 1.0f);
        const auto road = Graphics::GpuMesh::Upload(device, Tangents(builder.Take()), layout);
        builder.AddBox(Vector3(-1.5f, 0.0f, -80.0f), Vector3(1.5f, 0.15f, 80.0f));
        const auto pavement = Graphics::GpuMesh::Upload(device, Tangents(builder.Take()), layout);
        builder.AddBox(Vector3(-4.0f, 0.0f, -4.0f), Vector3(4.0f, 1.0f, 4.0f));
        const auto buildingNear = Graphics::GpuMesh::Upload(device, Tangents(builder.Take()), layout);
        // The far LOD is the same block without its separate roof cap,
        // standing in for a cheaper mesh.
        builder.AddCylinder(Vector3::Zero, 5.6f, 1.0f, 4, false);
        const auto buildingFar = Graphics::GpuMesh::Upload(device, Tangents(builder.Take()), layout);
        Graphics::MeshBuilder lampBuilder({Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Color});
        lampBuilder.SetColor(Color(255, 220, 140, 255));
        lampBuilder.AddCylinder(Vector3::Zero, 0.08f, 4.0f, 6);
        lampBuilder.AddSphere(Vector3(0.0f, 4.1f, 0.0f), 0.25f, 8, 6);
        const auto lamp = Graphics::GpuMesh::Upload(device, lampBuilder.Mesh(), Graphics::VertexLayout::PositionColor());

        // --- Materials. ----------------------------------------------------
        const Graphics::Material asphalt = Pbr("asphalt", Vector3(0.12f, 0.12f, 0.13f), 0.0f, 0.9f);
        const Graphics::Material stone = Pbr("stone", Vector3(0.55f, 0.53f, 0.5f), 0.0f, 0.8f);
        const Graphics::Material brick = Pbr("brick", Vector3(0.55f, 0.25f, 0.18f), 0.0f, 0.75f);
        const Graphics::Material glassFront = Pbr("glass-front", Vector3(0.3f, 0.4f, 0.5f), 0.6f, 0.2f);
        Graphics::Material awning = Pbr("awning", Vector3(0.1f, 0.5f, 0.3f), 0.0f, 0.6f);
        awning.blend = Graphics::BlendMode::AlphaBlend;
        awning.alpha = 0.7f;

        std::vector<Building> buildings;
        // From behind the camera to beyond the draw distance, so frustum and
        // distance culling both have work.
        for (int i = -3; i < 16; ++i) {
            const float z = -10.0f * static_cast<float>(i);
            const float height = 6.0f + static_cast<float>((i * 7) % 5) * 3.0f;
            for (const float side : {-1.0f, 1.0f}) {
                buildings.push_back({Matrix::CreateScale(0.6f, height, 0.55f) * Matrix::CreateTranslation(side * 9.0f, 0.0f, z),
                                     (i + 3 + (side > 0.0f ? 1 : 0)) % 3 == 0 ? &glassFront : &brick});
            }
        }
        Graphics::InstanceSet lamps(&lamp);
        for (int i = 0; i < 16; ++i) {
            lamps.Add(Matrix::CreateTranslation(i % 2 == 0 ? -4.4f : 4.4f, 0.0f, -10.0f * static_cast<float>(i) - 5.0f));
        }

        // --- Renderer with the PBR binder and a custom lamp pass. ----------
        Graphics::ForwardRenderer renderer(device);
        renderer.SetMaterialBinder(std::make_shared<Enhanced::PbrMaterialBinder>(device, renderer.StockBinder()));
        renderer.Lighting().defaultLighting = false;
        renderer.Lighting().lightDirection = Vector3(0.3f, -0.8f, -0.5f);
        renderer.Lighting().lightColor = Vector3(1.0f, 0.95f, 0.85f);
        renderer.Lighting().ambientColor = Vector3(0.25f, 0.28f, 0.33f);
        BasicEffect lampEffect(device);
        lampEffect.setVertexColorEnabledProperty(true);
        Graphics::CullStatistics lampCulling;
        renderer.AddPass(Graphics::PassPoint::AfterOpaque, "lamps",
                         [&](GraphicsDevice& d, const Graphics::Camera& camera, Graphics::RenderStatistics& stats) {
                             lampEffect.setViewProperty(camera.View());
                             lampEffect.setProjectionProperty(camera.Projection());
                             lamps.DrawPerInstance(
                                 d, lampEffect,
                                 [&] { lampEffect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply(); },
                                 &stats);
                         });

        // --- Input, camera, controller. -----------------------------------
        Input::ActionMap actions;
        Graphics::CameraInputAxes axes;
        axes.moveForward = actions.AddAxis("MoveForward");
        axes.moveRight = actions.AddAxis("MoveRight");
        axes.lookYaw = actions.AddAxis("LookYaw");
        axes.lookPitch = actions.AddAxis("LookPitch");
        axes.lookYawRate = actions.AddAxis("TurnKeys");
        actions.BindAxis(axes.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
        actions.BindAxis(axes.moveRight, Input::AxisBinding::ForKeys(Keys::A, Keys::D));
        actions.BindAxis(*axes.lookYawRate, Input::AxisBinding::ForKeys(Keys::Right, Keys::Left));
        axes.lookRate = 0.4f;

        Graphics::Camera camera;
        camera.SetPerspective(MathHelper::ToRadians(60.0f), static_cast<float>(Width) / Height, 0.1f,
                              quality.drawDistance * 0.2f);
        camera.SetPosition(Vector3(1.0f, 1.7f, 14.0f));
        camera.SetYawPitchRoll(0.0f, -0.05f);
        Graphics::FirstPersonSettings settings;
        settings.speed = 12.0f;
        Graphics::FlyCameraController fly(settings);
        fly.SyncFrom(camera);

        const Graphics::LodSelector lod({35.0f * quality.lodBias});
        Graphics::VisibilitySettings visibility;
        visibility.maxDistance = quality.drawDistance * 0.2f;

        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);
        Graphics::ScreenshotQueue screenshots(output.string(), "street_slice");
        Diagnostics::FrameStatistics frameStats(Frames);
        Diagnostics::CpuProfiler profiler;
        profiler.SetEnabled(true);
        Graphics::RenderStatistics renderStats;
        Graphics::CullStatistics culling;
        int lodSwitches = 0;

        for (int frame = 0; frame < Frames; ++frame) {
            {
                Diagnostics::ScopedTimer timer(&profiler, "Update");
                Input::InputSnapshot snapshot;
                snapshot.keyboard = frame < 6 ? KeyboardState({Keys::W, Keys::Left}) : KeyboardState({Keys::W});
                actions.Update(snapshot);
                fly.Update(camera, Graphics::ReadCameraInput(actions, axes, Dt), Dt);
            }
            Graphics::RenderQueue queue;
            {
                Diagnostics::ScopedTimer timer(&profiler, "Cull");
                culling = Graphics::CullStatistics{};
                renderStats.Reset();
                std::vector<Graphics::RenderItem> candidates;
                auto add = [&](const Graphics::GpuMesh& mesh, const Graphics::Material& material, const Matrix& world) {
                    Graphics::RenderItem item;
                    item.mesh = &mesh;
                    item.material = &material;
                    item.world = world;
                    item.bounds = Graphics::WorldBounds(mesh, world);
                    candidates.push_back(item);
                };
                add(road, asphalt, Matrix::getIdentityProperty());
                add(pavement, stone, Matrix::CreateTranslation(-5.5f, 0.0f, 0.0f));
                add(pavement, stone, Matrix::CreateTranslation(5.5f, 0.0f, 0.0f));
                for (auto& building : buildings) {
                    const float distance = Vector3::Distance(building.world.getTranslationProperty(), camera.Position());
                    const int level = lod.Select(distance, building.lod);
                    lodSwitches += building.lod >= 0 && level != building.lod ? 1 : 0;
                    building.lod = level;
                    add(level == 0 ? buildingNear : buildingFar, *building.material, building.world);
                    if (level == 0 && building.material == &brick) {
                        add(buildingNear, awning,
                            Matrix::CreateScale(0.1f, 0.05f, 0.4f)
                                * Matrix::CreateTranslation(building.world.getTranslationProperty().X * 0.85f, 3.0f,
                                                            building.world.getTranslationProperty().Z));
                    }
                }
                Graphics::CullInto(candidates, camera, visibility, queue, culling, &renderStats);
                lamps.Cull(camera, visibility, lampCulling);
                queue.Sort(camera.Position());
            }
            {
                Diagnostics::ScopedTimer timer(&profiler, "Draw");
                device.SetRenderTarget(&target);
                device.Clear(Color(150, 175, 205, 255));
                renderer.Render(queue, camera, &renderStats);
                device.SetRenderTarget(nullptr);
            }
            profiler.EndFrame();
            frameStats.AddFrame(Dt);
        }

        screenshots.RequestNext(true);
        const auto captures = screenshots.Process(device, target);
        if (captures.size() != 1 || !captures.front().success) {
            std::fprintf(stderr, "capture failed: %s\n", captures.empty() ? "no request" : captures.front().error.c_str());
            return 1;
        }

        Graphics::DiagnosticsSources sources;
        sources.frames = &frameStats;
        sources.render = &renderStats;
        sources.culling = &culling;
        sources.profiler = &profiler;
        for (const auto& line : Graphics::FormatDiagnostics(sources)) {
            std::printf("%s\n", line.c_str());
        }
        std::printf("quality %s: draw distance %.0f, lod bias %.2f; lod switches %d; lamps drawn %zu\n",
                    std::string(Graphics::ToString(quality.preset)).c_str(), quality.drawDistance, quality.lodBias,
                    lodSwitches, lamps.VisibleCount());
        for (const auto& fallback : renderer.LastFallbacks()) {
            std::printf("fallback: %s\n", fallback.c_str());
        }
        std::printf("wrote %s\n", captures.front().path.c_str());

        // The slice must actually exercise what it claims to.
        const bool ok = renderStats.drawCalls > 10 && culling.byFrustum + culling.byDistance > 0 && lodSwitches > 0
                        && lamps.VisibleCount() > 0 && renderer.LastFallbacks().empty();
        if (!ok) {
            std::fprintf(stderr, "street_slice: the scene did not exercise culling, LOD, lamps or PBR as expected\n");
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "street_slice: %s\n", e.what());
        return 1;
    }
}
