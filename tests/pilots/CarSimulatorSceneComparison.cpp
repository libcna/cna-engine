// SPDX-License-Identifier: MIT
//
// Strict-profile counterpart of the Street comparison: car-simulator's real map
// (content/maps/lipova) drawn by car-simulator's own WorldRenderer and by the
// engine (ForwardRenderer + stock material binder + engine culling) over the
// same GPU buffers and textures.
//
// Since 2026-10-20 car-simulator's WorldRenderer draws through the engine
// itself (its adoption); the comparison now checks that car-simulator's use
// of the engine and this independent engine setup over the same batches agree.
//
// car-simulator's sources are compiled read-only; its renderer's private batch
// lists are read with the explicit-instantiation access idiom (the batch types
// are private too, so the exposing templates take `auto` member pointers).
//
// Compared, by day, at night, at night with the headlights on and in a
// rear-view mirror (mirrored, distances capped): terrain
// chunks, road and marking batches, static object batches, tree and sign
// batches, the street lamps' light and the headlight fill (a second lighting
// pass, as nested renderers in a custom pass) — every pass WorldRenderer draws
// at these views. Not compared: vehicles, pedestrians, rain and snow, which
// are other renderers or other conditions. The program fails when the engine's visibility
// decisions differ from car-simulator's or an image leaves the proposed
// screenshot tolerance (ImageTolerance.hpp).
//
// Usage: cna_engine_carsim_scene_comparison <output-directory>
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"

#include "ImageTolerance.hpp"

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
#include <optional>
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
    CNA_ENGINE_EXPOSE(SignBatches, signBatches_);
    CNA_ENGINE_EXPOSE(LampLights, lampLights_);
    CNA_ENGINE_EXPOSE(GlowTexture, glowTexture_);
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

        // Headlights, for the last view (car-simulator's SetHeadlights).
        struct Headlights
        {
            Vector3 position;
            Vector3 forward;
            float intensity = 1.0f;
            bool highBeam = false;
        };
        std::optional<Headlights> headlights;

        // One view drawn by car-simulator and by the engine at the rig's
        // current time of day; false when the visibility rules disagree or an
        // image cannot be written.
        // `mirrored` and `cap` are car-simulator's mirror pass: the image
        // flipped horizontally and every distance capped (0: no cap).
        auto compare = [&](const std::string& name, const CarSim::Render::CameraPose& view, const bool mirrored,
                           const float cap) -> bool {
            // --- car-simulator frame (world geometry only). ----------------------
            device.SetRenderTarget(&target);
            device.Clear(Color(0, 0, 0, 255));
            const auto carStart = Clock::now();
            const Matrix carProjection = mirrored ? view.Projection(aspect) * Matrix::CreateScale(-1.0f, 1.0f, 1.0f)
                                                  : view.Projection(aspect);
            renderer.Draw(device, view.View(), carProjection, view.Frustum(aspect), mirrored, cap);
            const double carMs = Milliseconds(Clock::now() - carStart);
            device.SetRenderTarget(nullptr);
            const auto carPixels = Pixels(target);
            const auto& carStats = renderer.Stats();

            // --- The same world through the engine. ------------------------------
            Graphics::Camera camera;
            camera.SetPosition(view.position);
            camera.LookAt(view.target, view.up);
            camera.SetPerspective(Microsoft::Xna::Framework::MathHelper::ToRadians(view.fieldOfViewDeg), aspect,
                                  view.nearPlane, view.farPlane);
            camera.SetMirrored(mirrored);

            const auto& roads = renderer.*Get(RoadBatches{});
            const auto& objects = renderer.*Get(ObjectBatches{});
            const auto& trees = renderer.*Get(TreeBatches{});
            const auto& signs = renderer.*Get(SignBatches{});
            const auto& lampLights = renderer.*Get(LampLights{});
            auto* glowTexture = (renderer.*Get(GlowTexture{})).get();
            const auto& chunks = renderer.*Get(TerrainChunks{});
            const auto& skirt = renderer.*Get(TerrainSkirt{});
            const Vector3 baked = renderer.*Get(BakedScale{});
            const float lampFactor = renderer.*Get(LampFactor{});
            const float lod1 = renderer.*Get(Lod1Distance{});
            const float lod2 = renderer.*Get(Lod2Distance{});
            const float terrainCull = cap > 0.0f ? std::min(cap, renderer.*Get(TerrainCull{}))
                                                 : renderer.*Get(TerrainCull{});
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
            const float horizon = cap > 0.0f ? std::min(cap, rig.fogEnd) : rig.fogEnd;
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
            const float treeRange = std::min(horizon, cap > 0.0f ? std::min(cap, 1100.0f) : 1100.0f);
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

            // Sign faces: the tree material, both sides, frustum culling only.
            int signsDrawn = 0;
            int xnaSigns = 0;
            for (const auto& b : signs) {
                if (!b.mesh) {
                    continue;
                }
                xnaSigns += xnaFrustum(b.mesh->Sphere());
                if (!inFrustum(b.mesh->Sphere())) {
                    continue;
                }
                Graphics::Material sign = treeMaterial;
                sign.name = "sign";
                sign.baseTexture = b.texture;
                sign.doubleSided = true;
                submit(*b.mesh, material(sign), 5, b.mesh->Sphere());
                ++signsDrawn;
            }

            // Street lamps after dark: glow quads in the lamp colour, added on
            // top of the scene without fog, frustum culling only.
            int lampsDrawn = 0;
            int xnaLamps = 0;
            if (lampFactor > 0.01f && glowTexture != nullptr) {
                Graphics::Material lamp;
                lamp.name = "lamp";
                lamp.shading = Graphics::ShadingModel::Unlit;
                lamp.blend = Graphics::BlendMode::Additive;
                lamp.vertexColors = true;
                lamp.doubleSided = true;
                lamp.fog = false;
                lamp.baseTexture = glowTexture;
                lamp.sampler = &SamplerState::LinearClamp;
                lamp.diffuseColor = Vector3(0.95f, 0.84f, 0.60f) * lampFactor;
                const Graphics::Material* lampMaterial = material(lamp);
                for (const auto& mesh : lampLights) {
                    if (!mesh) {
                        continue;
                    }
                    xnaLamps += xnaFrustum(mesh->Sphere());
                    if (!inFrustum(mesh->Sphere())) {
                        continue;
                    }
                    submit(*mesh, lampMaterial, 6, mesh->Sphere());
                    ++lampsDrawn;
                }
            }
            // Headlight fill (car-simulator's DrawHeadlightFill): objects,
            // trees and signs inside the beam lit again by the headlights and
            // added on top. A second lighting setup, so in engine terms two
            // nested renderers in a custom pass after the opaque buckets.
            Graphics::RenderQueue beamObjects;
            Graphics::RenderQueue beamCards;
            int beamDrawn = 0;
            float beamRange = 0.0f;
            Vector3 beamForward = Vector3::Forward;
            // car-simulator's mirror pass draws no headlight fill.
            if (headlights && !mirrored) {
                const Headlights& h = *headlights;
                beamForward = Vector3(h.forward.X, 0.0f, h.forward.Z);
                beamForward.Normalize();
                beamRange = h.highBeam ? 275.0f : 165.0f;
                const auto inBeam = [&](const BoundingSphere& sphere) {
                    const Vector3 delta = sphere.Center - h.position;
                    const float along = Vector3::Dot(delta, beamForward);
                    if (along + sphere.Radius < 0.0f || along - sphere.Radius > beamRange) {
                        return false;
                    }
                    const float sideways =
                        std::sqrt(std::max(0.0f, delta.X * delta.X + delta.Z * delta.Z - along * along));
                    return sideways < sphere.Radius + 5.0f + std::max(0.0f, along) * (h.highBeam ? 0.15f : 0.22f);
                };
                auto submitTo = [&](Graphics::RenderQueue& target, const CarSim::Render::GpuMesh& mesh,
                                    const Graphics::Material* m) {
                    Graphics::RenderItem item;
                    item.mesh = &meshFor(mesh);
                    item.material = m;
                    item.bounds = mesh.Sphere();
                    target.Submit(item);
                    ++beamDrawn;
                };
                for (const auto& b : objects) {
                    if (!b.mesh || !inFrustum(b.mesh->Sphere()) || !inBeam(b.mesh->Sphere())) {
                        continue;
                    }
                    if (b.treeTrunk && rig.fogEnd < 400.0f
                        && Vector3::Distance(b.mesh->Sphere().Center, h.position) > rig.fogEnd + 30.0f) {
                        continue;
                    }
                    Graphics::Material lit;
                    lit.name = "headlit object";
                    lit.blend = Graphics::BlendMode::Additive;
                    lit.baseTexture = b.texture;
                    lit.sampler = &SamplerState::AnisotropicWrap;
                    lit.diffuseColor = b.diffuse;
                    submitTo(beamObjects, *b.mesh, material(lit));
                }
                auto card = [&](Microsoft::Xna::Framework::Graphics::Texture2D* texture, const Vector3& color) {
                    Graphics::Material m;
                    m.name = "headlit card";
                    m.shading = Graphics::ShadingModel::Unlit;
                    m.blend = Graphics::BlendMode::Additive;
                    m.discardBelowCutoff = true;
                    m.alphaCutoff = 111.0f / 255.0f;
                    m.vertexColors = true;
                    m.baseTexture = texture;
                    m.sampler = &SamplerState::LinearClamp;
                    m.diffuseColor = color;
                    return material(m);
                };
                const Vector3 treeLight =
                    (h.highBeam ? Vector3(0.40f, 0.39f, 0.34f) : Vector3(0.32f, 0.30f, 0.26f)) * h.intensity;
                const Vector3 signLight =
                    (h.highBeam ? Vector3(0.59f, 0.56f, 0.48f) : Vector3(0.50f, 0.46f, 0.39f)) * h.intensity;
                for (const auto& b : trees) {
                    if (b.mesh && inFrustum(b.mesh->Sphere()) && inBeam(b.mesh->Sphere())) {
                        submitTo(beamCards, *b.mesh, card(b.texture, treeLight));
                    }
                }
                for (const auto& b : signs) {
                    if (b.mesh && inFrustum(b.mesh->Sphere()) && inBeam(b.mesh->Sphere())) {
                        submitTo(beamCards, *b.mesh, card(b.texture, signLight));
                    }
                }
                beamObjects.Sort(camera.Position());
                beamCards.Sort(camera.Position());
            }
            queue.Sort(camera.Position());

            Graphics::ForwardRenderer engine(device);
            Graphics::ForwardRenderer beamObjectRenderer(device);
            Graphics::ForwardRenderer beamCardRenderer(device);
            if (headlights && !mirrored) {
                const Headlights& h = *headlights;
                auto& objectLight = beamObjectRenderer.Lighting();
                objectLight.defaultLighting = false;
                objectLight.lightDirection = beamForward;
                objectLight.lightColor =
                    (h.highBeam ? Vector3(0.72f, 0.70f, 0.63f) : Vector3(0.61f, 0.57f, 0.49f)) * h.intensity;
                objectLight.ambientColor = Vector3::Zero;
                objectLight.fogEnabled = true;
                objectLight.fogColor = Vector3::Zero;
                objectLight.fogStart = 10.0f;
                objectLight.fogEnd = beamRange;
                auto& cardLight = beamCardRenderer.Lighting();
                cardLight.fogEnabled = true;
                cardLight.fogColor = Vector3::Zero;
                cardLight.fogStart = 6.0f;
                cardLight.fogEnd = beamRange;
                engine.AddPass(Graphics::PassPoint::AfterOpaque, "headlights",
                               [&](GraphicsDevice&, const Graphics::Camera& passCamera, Graphics::RenderStatistics& stats) {
                                   beamObjectRenderer.Render(beamObjects, passCamera, &stats);
                                   beamCardRenderer.Render(beamCards, passCamera, &stats);
                               });
            }
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

            const Pilot::ImageDifference image = Pilot::Compare(carPixels, enginePixels);
            const bool withinTolerance = Pilot::Within(image);
            const std::string prefix = name.empty() ? "" : name + "-";
            const auto carPng =
                Graphics::WritePng(device, carPixels, Width, Height, (output / (prefix + "car-simulator.png")).string());
            const auto enginePng =
                Graphics::WritePng(device, enginePixels, Width, Height, (output / (prefix + "engine.png")).string());

            std::printf("--- view: %s (%.1f h, lamp factor %.2f)\n", name.empty() ? "day" : name.c_str(),
                        rig.timeOfDayHours, lampFactor);
            std::printf("world: %zu terrain chunks, %zu road batches, %zu object batches, %zu tree batches; built in %.0f ms\n",
                        chunks.size(), roads.size(), objects.size(), trees.size(), buildMs);
            std::printf("drawn (car-simulator / engine rules with Intersects / engine):\n");
            std::printf("  terrain chunks %d / %d / %d\n", carStats.terrainChunksDrawn, xnaTerrain, terrainDrawn);
            std::printf("  road batches   %d / %d / %d\n", carStats.roadBatchesDrawn, xnaRoads, roadsDrawn);
            std::printf("  object batches %d / %d / %d\n", carStats.objectBatchesDrawn, xnaObjects, objectsDrawn);
            std::printf("  tree batches   %d / %d / %d\n", carStats.treeBatchesDrawn, xnaTrees, treesDrawn);
            std::printf("  sign batches   (not counted) / %d / %d\n", xnaSigns, signsDrawn);
            std::printf("  lamp lights    (not counted) / %d / %d\n", xnaLamps, lampsDrawn);
            if (headlights && !mirrored) {
                std::printf("  headlight fill (not counted) / - / %d\n", beamDrawn);
            }
            std::printf("draw calls: car-simulator %d (all world passes), engine %llu (compared geometry)\n",
                        carStats.drawCalls, static_cast<unsigned long long>(engineStats.drawCalls));
            std::printf("cpu frame: car-simulator %.1f ms, engine %.1f ms (software renderer, Debug)\n", carMs, engineMs);
            std::printf("image difference: %.2f / 255 mean per channel, %d / 255 largest, %zu of %zu pixels differ,"
                        " %zu visibly (> %d / 255) — %s the agreed tolerance\n",
                        image.mean, image.largest, image.differing, image.pixels, image.visible, Pilot::VisibleStep,
                        withinTolerance ? "within" : "OUTSIDE");
            for (const auto& fallback : engine.LastFallbacks()) {
                std::printf("fallback: %s\n", fallback.c_str());
            }
            std::printf("images: %s %s\n", carPng.path.c_str(), enginePng.path.c_str());
            std::fflush(stdout);

            // Since car-simulator adopted the engine (2026-10-20) its
            // WorldRenderer culls with the engine's rules itself, so its
            // counts must equal the engine's own here; the `xna*` counts
            // keep showing what XNA's GJK test would have kept.
            const bool same = carStats.terrainChunksDrawn == terrainDrawn && carStats.roadBatchesDrawn == roadsDrawn
                              && carStats.objectBatchesDrawn == objectsDrawn && carStats.treeBatchesDrawn == treesDrawn;
            if (!same) {
                std::fprintf(stderr, "%s: visibility differs from car-simulator's\n", name.c_str());
                return false;
            }
            if (!withinTolerance) {
                std::fprintf(stderr, "%s: image outside the agreed tolerance\n", name.c_str());
            }
            return withinTolerance && carPng.success && enginePng.success;
        };

        bool ok = compare("", pose, false, 0.0f);
        // After dark: lit windows and the street lamps' light.
        rig.SetTimeOfDay(22.0f);
        renderer.ApplyLighting();
        ok = compare("night", pose, false, 0.0f) && ok;
        // At night with the low beams on, from just below the driver's eye.
        Vector3 ahead = pose.target - pose.position;
        ahead.Y = 0.0f;
        ahead.Normalize();
        headlights = Headlights{pose.position - Vector3(0.0f, 0.6f, 0.0f), ahead, 1.0f, false};
        renderer.SetHeadlights(headlights->position, headlights->forward, headlights->intensity,
                               headlights->highBeam);
        ok = compare("headlights", pose, false, 0.0f) && ok;
        // A rear-view mirror by day: looking back from the driver's eye,
        // mirrored, with the mirror's 320 m cap (car-simulator's MirrorView).
        rig.SetTimeOfDay(10.5f);
        renderer.ApplyLighting();
        renderer.SetHeadlights(headlights->position, headlights->forward, 0.0f, false);
        headlights.reset();
        CarSim::Render::CameraPose mirror = pose;
        mirror.target = pose.position - (pose.target - pose.position);
        mirror.fieldOfViewDeg = 30.0f;
        mirror.nearPlane = 0.5f;
        mirror.farPlane = 320.0f;
        ok = compare("mirror", mirror, true, 320.0f) && ok;
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "car-simulator scene comparison: %s\n", e.what());
        return 1;
    }
}
