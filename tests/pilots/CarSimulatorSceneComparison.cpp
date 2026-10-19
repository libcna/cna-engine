// SPDX-License-Identifier: MIT
//
// Strict-profile counterpart of the Street comparison: car-simulator's real map
// (content/maps/lipova) drawn by car-simulator's own WorldRenderer and by the
// engine (ForwardRenderer + stock material binder + engine culling) over the
// same GPU buffers and textures.
//
// car-simulator's sources are compiled read-only; its renderer's private batch
// lists are read with the explicit-instantiation access idiom (the batch types
// are private too, so the exposing templates take `auto` member pointers).
//
// Compared: terrain chunks, road and marking batches, static object batches
// and tree batches — the world geometry. Not compared: sky, signs, lamp
// glows, vehicles and pedestrians, which WorldRenderer draws with its own
// special-purpose passes. The program fails when the engine's visibility
// decisions differ from car-simulator's.
//
// Usage: cna_engine_carsim_scene_comparison <output-directory>
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"

#include "CarSim/Map/MapDocument.hpp"
#include "CarSim/Map/MapWorld.hpp"
#include "CarSim/Render/BitmapFont.hpp"
#include "CarSim/Render/Camera.hpp"
#include "CarSim/Render/GpuMesh.hpp"
#include "CarSim/Render/LightingRig.hpp"
#include "CarSim/Render/WorldRenderer.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#ifndef CNA_ENGINE_CARSIM_CONTENT_DIR
#error "CNA_ENGINE_CARSIM_CONTENT_DIR must name car-simulator's content directory"
#endif

// --- Read-only access to WorldRenderer's private state ---------------------------
namespace
{
    template <typename Tag, auto Member>
    struct Expose
    {
        friend constexpr auto Get(Tag) { return Member; }
    };
#define CNA_ENGINE_EXPOSE(TagName, member)                                                                             \
    struct TagName                                                                                                     \
    {                                                                                                                  \
        friend constexpr auto Get(TagName);                                                                            \
    };                                                                                                                 \
    template struct Expose<TagName, &CarSim::Render::WorldRenderer::member>
    CNA_ENGINE_EXPOSE(RoadBatches, roadBatches_);
    CNA_ENGINE_EXPOSE(ObjectBatches, objectBatches_);
    CNA_ENGINE_EXPOSE(TreeBatches, treeBatches_);
    CNA_ENGINE_EXPOSE(TerrainChunks, terrainChunks_);
    CNA_ENGINE_EXPOSE(TerrainSkirt, terrainSkirt_);
    CNA_ENGINE_EXPOSE(TextureFor, TextureFor);
    CNA_ENGINE_EXPOSE(BakedScale, bakedScale_);
    CNA_ENGINE_EXPOSE(LampFactor, lampFactor_);
    CNA_ENGINE_EXPOSE(Grass, grass_);
    CNA_ENGINE_EXPOSE(Macro, macro_);
    CNA_ENGINE_EXPOSE(Lod1Distance, lod1DistanceM);
    CNA_ENGINE_EXPOSE(Lod2Distance, lod2DistanceM);
    CNA_ENGINE_EXPOSE(TerrainCull, terrainCullDistanceM);
#undef CNA_ENGINE_EXPOSE
}

namespace
{
    using namespace CnaEngine;
    using Clock = std::chrono::steady_clock;
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SamplerState;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;

    constexpr int Width = 480;
    constexpr int Height = 270;

    double Milliseconds(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

    Graphics::AttributeMask AttributesOf(CarSim::Render::VertexLayout layout)
    {
        using A = Graphics::VertexAttribute;
        switch (layout) {
            case CarSim::Render::VertexLayout::PositionColor: return {A::Position, A::Color};
            case CarSim::Render::VertexLayout::PositionTexture: return {A::Position, A::TexCoord0};
            case CarSim::Render::VertexLayout::PositionColorTexture: return {A::Position, A::Color, A::TexCoord0};
            case CarSim::Render::VertexLayout::PositionNormalTexture: return {A::Position, A::Normal, A::TexCoord0};
            case CarSim::Render::VertexLayout::PositionNormalDualTexture:
                return {A::Position, A::Normal, A::TexCoord0, A::TexCoord1};
        }
        return {A::Position};
    }

    std::vector<Color> Pixels(RenderTarget2D& target)
    {
        std::vector<Color> pixels(static_cast<std::size_t>(Width * Height));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        return pixels;
    }
}

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path output = argc > 1 ? argv[1] : std::filesystem::current_path();
        std::filesystem::create_directories(output);

        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);

        // --- car-simulator's world, built by car-simulator. ------------------
        const auto buildStart = Clock::now();
        std::vector<std::string> errors;
        const auto world = CarSim::Map::MapWorld::Load(
            CarSim::Map::MapDirectory(CNA_ENGINE_CARSIM_CONTENT_DIR, "lipova"), errors, nullptr);
        if (!world) {
            for (const auto& e : errors) {
                std::fprintf(stderr, "map: %s\n", e.c_str());
            }
            return 1;
        }
        CarSim::Render::LightingRig rig;
        const auto font = CarSim::Render::BitmapFont::CreateBuiltin(device);
        CarSim::Render::WorldRenderer renderer(device, rig, *world, font.get());
        renderer.ApplyLighting();
        const double buildMs = Milliseconds(Clock::now() - buildStart);

        // A driver's-eye view from the player spawn.
        const auto spawn = world->PlayerSpawn();
        const Vector3 ground = world->SpawnPosition(spawn);
        const float heading = spawn.headingDeg * 3.14159265f / 180.0f;
        CarSim::Render::CameraPose pose;
        pose.position = ground + Vector3(0.0f, 1.3f, 0.0f);
        pose.target = pose.position + Vector3(std::sin(heading), -0.05f, -std::cos(heading)) * 10.0f;
        const float aspect = static_cast<float>(Width) / Height;

        // --- car-simulator frame (world geometry only). ----------------------
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        const auto carStart = Clock::now();
        renderer.Draw(device, pose.View(), pose.Projection(aspect), pose.Frustum(aspect));
        const double carMs = Milliseconds(Clock::now() - carStart);
        device.SetRenderTarget(nullptr);
        const auto carPixels = Pixels(target);
        const auto& carStats = renderer.Stats();

        // --- The same world through the engine. ------------------------------
        Graphics::Camera camera;
        camera.SetPosition(pose.position);
        camera.LookAt(pose.target, pose.up);
        camera.SetPerspective(Microsoft::Xna::Framework::MathHelper::ToRadians(pose.fieldOfViewDeg), aspect,
                              pose.nearPlane, pose.farPlane);

        const auto& roads = renderer.*Get(RoadBatches{});
        const auto& objects = renderer.*Get(ObjectBatches{});
        const auto& trees = renderer.*Get(TreeBatches{});
        const auto& chunks = renderer.*Get(TerrainChunks{});
        const auto& skirt = renderer.*Get(TerrainSkirt{});
        const Vector3 baked = renderer.*Get(BakedScale{});
        const float lampFactor = renderer.*Get(LampFactor{});
        const float lod1 = renderer.*Get(Lod1Distance{});
        const float lod2 = renderer.*Get(Lod2Distance{});
        const float terrainCull = renderer.*Get(TerrainCull{});
        auto textureFor = [&](auto surface) { return (renderer.*Get(TextureFor{}))(surface); };

        std::map<const CarSim::Render::GpuMesh*, Graphics::GpuMesh> meshes;
        auto meshFor = [&](const CarSim::Render::GpuMesh& mesh) -> const Graphics::GpuMesh& {
            auto it = meshes.find(&mesh);
            if (it == meshes.end()) {
                it = meshes.emplace(&mesh, Graphics::GpuMesh::Borrow(*mesh.VertexBufferPtr(), *mesh.IndexBufferPtr(),
                                                                     mesh.VertexCount(), mesh.PrimitiveCount(),
                                                                     mesh.Bounds(), AttributesOf(mesh.Layout())))
                         .first;
            }
            return it->second;
        };
        std::vector<std::unique_ptr<Graphics::Material>> materials;
        auto material = [&](Graphics::Material m) -> const Graphics::Material* {
            materials.push_back(std::make_unique<Graphics::Material>(std::move(m)));
            return materials.back().get();
        };

        const auto engineStart = Clock::now();
        Graphics::RenderQueue queue;
        auto submit = [&](const CarSim::Render::GpuMesh& mesh, const Graphics::Material* m, std::int32_t order,
                          const BoundingSphere& bounds) {
            Graphics::RenderItem item;
            item.mesh = &meshFor(mesh);
            item.material = m;
            item.bounds = bounds;
            item.order = order;
            queue.Submit(item);
        };
        // Two frustum tests. The engine's is the plane test
        // (BoundingFrustum::Contains(sphere) != Disjoint); car-simulator calls
        // BoundingFrustum::Intersects(sphere), which XNA implements with GJK
        // and a scale-relative tolerance, so it also accepts some spheres just
        // outside a plane. The `xna*` counts apply the engine's rules with
        // car-simulator's test and must equal car-simulator's own counts.
        const auto xnaFrustum = [&](const BoundingSphere& sphere) { return camera.Frustum().Intersects(sphere) ? 1 : 0; };
        const auto withinRange = [&](const BoundingSphere& sphere, const float maxDistance) {
            Graphics::VisibilitySettings rangeOnly;
            rangeOnly.frustum = false;
            rangeOnly.maxDistance = maxDistance;
            return Graphics::Classify(sphere, 1, camera, rangeOnly) == Graphics::Visibility::Visible;
        };
        int xnaTerrain = 0;
        int xnaRoads = 0;
        int xnaObjects = 0;
        int xnaTrees = 0;
        const auto inFrustum = [&](const BoundingSphere& sphere) {
            Graphics::VisibilitySettings frustumOnly;
            return Graphics::Classify(sphere, 1, camera, frustumOnly) == Graphics::Visibility::Visible;
        };

        // Terrain: macro-tinted grass (two textures), LOD by distance.
        Graphics::Material terrainMaterial;
        terrainMaterial.name = "terrain";
        terrainMaterial.shading = Graphics::ShadingModel::Unlit;
        terrainMaterial.diffuseColor = baked;
        terrainMaterial.baseTexture = (renderer.*Get(Grass{})).get();
        terrainMaterial.secondaryTexture = (renderer.*Get(Macro{})).get();
        terrainMaterial.sampler = &SamplerState::AnisotropicWrap;
        terrainMaterial.secondarySampler = &SamplerState::LinearClamp;
        const Graphics::Material* terrain = material(terrainMaterial);
        int terrainDrawn = 0;
        if (skirt) {
            submit(*skirt, terrain, 0, skirt->Sphere());
        }
        const Graphics::LodSelector terrainLod({lod1, lod2}, 0.0f);
        for (const auto& chunk : chunks) {
            if (!chunk.lod0) {
                continue;
            }
            const float distance = Vector3::Distance(camera.Position(), chunk.centre) - chunk.radius;
            if (distance > terrainCull) {
                continue;
            }
            xnaTerrain += xnaFrustum(chunk.lod0->Sphere());
            if (!inFrustum(chunk.lod0->Sphere())) {
                continue;
            }
            const int level = terrainLod.Select(distance);
            const CarSim::Render::GpuMesh* mesh = level == 0 ? chunk.lod0.get() : level == 1 ? chunk.lod1.get() : chunk.lod2.get();
            submit(mesh ? *mesh : *chunk.lod0, terrain, 0, chunk.lod0->Sphere());
            ++terrainDrawn;
        }

        // Roads and verges, then markings (biased) — baked vertex colours.
        int roadsDrawn = 0;
        std::map<const void*, const Graphics::Material*> roadMaterials;
        for (const auto& b : roads) {
            if (!b.mesh) {
                continue;
            }
            xnaRoads += xnaFrustum(b.mesh->Sphere());
            if (!inFrustum(b.mesh->Sphere())) {
                continue;
            }
            auto* texture = textureFor(b.surface);
            const bool marking = b.mesh && texture == textureFor(decltype(b.surface)::Marking);
            const Graphics::Material*& m = roadMaterials[texture];
            if (m == nullptr) {
                Graphics::Material road;
                road.name = marking ? "marking" : "road";
                road.shading = Graphics::ShadingModel::Unlit;
                road.vertexColors = true;
                road.baseTexture = texture;
                road.sampler = &SamplerState::AnisotropicWrap;
                road.diffuseColor = marking ? Vector3(0.92f * baked.X, 0.92f * baked.Y, 0.90f * baked.Z) : baked;
                if (marking) {
                    road.depthBias = -0.00002f;
                    road.slopeScaleDepthBias = -1.0f;
                }
                m = material(road);
            }
            submit(*b.mesh, m, marking ? 2 : 1, b.mesh->Sphere());
            ++roadsDrawn;
        }

        // Static objects: lit, textured; their own range capped by the fog horizon.
        const float horizon = rig.fogEnd;
        int objectsDrawn = 0;
        for (const auto& b : objects) {
            if (!b.mesh) {
                continue;
            }
            Graphics::VisibilitySettings visibility;
            visibility.maxDistance = b.cullDistance > 0.0f ? std::min(b.cullDistance, horizon) : horizon;
            if (withinRange(b.mesh->Sphere(), visibility.maxDistance)) {
                xnaObjects += xnaFrustum(b.mesh->Sphere());
            }
            if (Graphics::Classify(b.mesh->Sphere(), 1, camera, visibility) != Graphics::Visibility::Visible) {
                continue;
            }
            Graphics::Material object;
            object.name = "object";
            object.baseTexture = b.texture;
            object.sampler = &SamplerState::AnisotropicWrap;
            object.diffuseColor = b.diffuse;
            object.emissiveColor = b.emissive + b.nightEmissive * lampFactor;
            object.specularColor = b.specular;
            object.specularPower = b.specularPower;
            submit(*b.mesh, material(object), 3, b.mesh->Sphere());
            ++objectsDrawn;
        }

        // Trees: alpha-tested cards (Greater 110 == GreaterEqual 111).
        const float treeRange = std::min(horizon, 1100.0f);
        Graphics::Material treeMaterial;
        treeMaterial.name = "tree";
        treeMaterial.shading = Graphics::ShadingModel::Unlit;
        treeMaterial.blend = Graphics::BlendMode::AlphaTest;
        treeMaterial.alphaCutoff = 111.0f / 255.0f;
        treeMaterial.vertexColors = true;
        treeMaterial.sampler = &SamplerState::LinearClamp;
        treeMaterial.diffuseColor = baked;
        int treesDrawn = 0;
        for (const auto& b : trees) {
            if (!b.mesh) {
                continue;
            }
            Graphics::VisibilitySettings visibility;
            visibility.maxDistance = treeRange;
            if (withinRange(b.mesh->Sphere(), treeRange)) {
                xnaTrees += xnaFrustum(b.mesh->Sphere());
            }
            if (Graphics::Classify(b.mesh->Sphere(), 1, camera, visibility) != Graphics::Visibility::Visible) {
                continue;
            }
            Graphics::Material tree = treeMaterial;
            tree.baseTexture = b.texture;
            submit(*b.mesh, material(tree), 4, b.mesh->Sphere());
            ++treesDrawn;
        }
        queue.Sort(camera.Position());

        Graphics::ForwardRenderer engine(device);
        auto& lighting = engine.Lighting();
        lighting.defaultLighting = false;
        lighting.lightDirection = rig.sunDirection;
        lighting.lightColor = rig.sunColor;
        lighting.fillLights = {{Vector3(0.0f, -1.0f, 0.0f), rig.skyFillColor, Vector3::Zero},
                               {Vector3(0.3f, 1.0f, 0.2f), rig.groundBounceColor, Vector3::Zero}};
        lighting.ambientColor = rig.skyAmbient;
        lighting.perPixelLighting = true;
        lighting.fogEnabled = true;
        lighting.fogColor = rig.fogColor;
        lighting.fogStart = rig.fogStart;
        lighting.fogEnd = rig.fogEnd;
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        // The engine sets every sampler it needs from the materials: leave
        // the device in a state no material asks for.
        for (int slot = 0; slot < 2; ++slot) {
            device.getSamplerStatesProperty()[slot] = SamplerState::PointClamp;
        }
        Graphics::RenderStatistics engineStats;
        engine.Render(queue, camera, &engineStats);
        const double engineMs = Milliseconds(Clock::now() - engineStart);
        device.SetRenderTarget(nullptr);
        const auto enginePixels = Pixels(target);

        double difference = 0.0;
        int largest = 0;
        std::size_t differing = 0;
        for (std::size_t i = 0; i < carPixels.size(); ++i) {
            const int r = std::abs(carPixels[i].getRProperty() - enginePixels[i].getRProperty());
            const int g = std::abs(carPixels[i].getGProperty() - enginePixels[i].getGProperty());
            const int b = std::abs(carPixels[i].getBProperty() - enginePixels[i].getBProperty());
            difference += r + g + b;
            largest = std::max({largest, r, g, b});
            differing += r + g + b > 0 ? 1u : 0u;
        }
        difference /= static_cast<double>(carPixels.size() * 3);
        const auto carPng = Graphics::WritePng(device, carPixels, Width, Height, (output / "car-simulator.png").string());
        const auto enginePng = Graphics::WritePng(device, enginePixels, Width, Height, (output / "engine.png").string());

        std::printf("world: %zu terrain chunks, %zu road batches, %zu object batches, %zu tree batches; built in %.0f ms\n",
                    chunks.size(), roads.size(), objects.size(), trees.size(), buildMs);
        std::printf("drawn (car-simulator / engine rules with Intersects / engine):\n");
        std::printf("  terrain chunks %d / %d / %d\n", carStats.terrainChunksDrawn, xnaTerrain, terrainDrawn);
        std::printf("  road batches   %d / %d / %d\n", carStats.roadBatchesDrawn, xnaRoads, roadsDrawn);
        std::printf("  object batches %d / %d / %d\n", carStats.objectBatchesDrawn, xnaObjects, objectsDrawn);
        std::printf("  tree batches   %d / %d / %d\n", carStats.treeBatchesDrawn, xnaTrees, treesDrawn);
        std::printf("draw calls: car-simulator %d (all world passes), engine %llu (compared geometry)\n",
                    carStats.drawCalls, static_cast<unsigned long long>(engineStats.drawCalls));
        std::printf("cpu frame: car-simulator %.1f ms, engine %.1f ms (software renderer, Debug)\n", carMs, engineMs);
        std::printf("image difference: %.2f / 255 mean per channel, %d / 255 largest, %zu of %zu pixels differ\n",
                    difference, largest, differing, carPixels.size());
        for (const auto& fallback : engine.LastFallbacks()) {
            std::printf("fallback: %s\n", fallback.c_str());
        }
        std::printf("images: %s %s\n", carPng.path.c_str(), enginePng.path.c_str());
        std::fflush(stdout);

        // The engine's rules reproduce car-simulator's exactly when given
        // its frustum test; the engine's own plane test may differ only on
        // spheres outside the view (the image difference shows the rest).
        const bool same = carStats.terrainChunksDrawn == xnaTerrain && carStats.roadBatchesDrawn == xnaRoads
                          && carStats.objectBatchesDrawn == xnaObjects && carStats.treeBatchesDrawn == xnaTrees;
        if (!same) {
            std::fprintf(stderr, "visibility differs from car-simulator's\n");
            return 1;
        }
        return carPng.success && enginePng.success ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "car-simulator scene comparison: %s\n", e.what());
        return 1;
    }
}
