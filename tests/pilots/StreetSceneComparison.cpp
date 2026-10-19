// SPDX-License-Identifier: MIT
//
// MILESTONE-2 comparison: one real street-simulator scene drawn by Street's own
// SceneRenderer and by the engine (ForwardRenderer + PbrMaterialBinder +
// engine culling) over the *same* GPU buffers and materials.
//
// Street's core is compiled read-only (tests/pilots/CMakeLists.txt). The scene
// is Street's procedural CityScene; nothing is modified. The registered scene
// items are read from SceneRenderer's private lists with the standard
// explicit-instantiation access idiom, because Street offers no accessor and
// may not be changed from this repository.
//
// Output: a report on stdout (visible items and instances, draw calls, CPU
// milliseconds, image difference) and both images as PNG files. The program
// fails when the engine's visibility decisions differ from Street's: the
// rules are the same and must give the same sets. Images are expected to
// differ (Street adds sky, image-based lighting, reflection probes and
// exposure that the engine path does not reproduce); their difference is
// reported, not asserted.
//
// Usage: cna_engine_street_scene_comparison <output-directory> [seed]
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"

#include "CnaStreet/Assets/ModelLibrary.hpp"
#include "CnaStreet/Render/Camera.hpp"
#include "CnaStreet/Render/GpuMesh.hpp"
#include "CnaStreet/Render/MaterialLibrary.hpp"
#include "CnaStreet/Render/RenderSettings.hpp"
#include "CnaStreet/Render/SceneRenderer.hpp"
#include "CnaStreet/Scene/CityScene.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaModeEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

// --- Read-only access to SceneRenderer's registered scene ----------------------
namespace
{
    template <typename Tag, typename Tag::type Member>
    struct Expose
    {
        friend typename Tag::type Get(Tag) { return Member; }
    };
    struct ItemsTag
    {
        using type = std::vector<CnaStreet::SceneItem> CnaStreet::SceneRenderer::*;
        friend type Get(ItemsTag);
    };
    struct GroupsTag
    {
        using type = std::vector<CnaStreet::InstanceGroup> CnaStreet::SceneRenderer::*;
        friend type Get(GroupsTag);
    };
    struct DynamicTag
    {
        using type = std::vector<CnaStreet::SceneItem> CnaStreet::SceneRenderer::*;
        friend type Get(DynamicTag);
    };
    template struct Expose<ItemsTag, &CnaStreet::SceneRenderer::items_>;
    template struct Expose<DynamicTag, &CnaStreet::SceneRenderer::dynamic_>;
    template struct Expose<GroupsTag, &CnaStreet::SceneRenderer::groups_>;
}

namespace
{
    using namespace CnaEngine;
    using Clock = std::chrono::steady_clock;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::AlphaModeEXT;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;

    constexpr int Width = 480;
    constexpr int Height = 270;

    double Milliseconds(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

    // Street's material description to the engine's (field by field).
    Graphics::Material ToEngine(const CnaStreet::Material& m)
    {
        Graphics::Material e;
        e.name = m.name;
        e.shading = Graphics::ShadingModel::PhysicallyBased;
        switch (m.alphaMode) {
            case AlphaModeEXT::Opaque: e.blend = Graphics::BlendMode::Opaque; break;
            case AlphaModeEXT::Mask: e.blend = Graphics::BlendMode::AlphaTest; break;
            case AlphaModeEXT::Blend: e.blend = Graphics::BlendMode::AlphaBlend; break;
        }
        e.alpha = m.alpha;
        e.alphaCutoff = m.alphaCutoff;
        e.doubleSided = m.doubleSided;
        e.keyLight = m.sunlit;
        e.diffuseColor = m.baseColour;
        e.emissiveColor = m.emissiveFactor;
        e.metallic = m.metallic;
        e.roughness = m.roughness;
        e.normalScale = m.normalScale;
        e.occlusionStrength = m.occlusionStrength;
        e.ior = m.ior;
        e.specularFactor = m.specular;
        e.uvScale = m.uvScale;
        e.uvOffset = m.uvOffset;
        e.baseTexture = m.albedo;
        e.normalTexture = m.normal;
        e.occlusionRoughnessMetallicTexture = m.orm;
        e.emissiveTexture = m.emissive;
        return e;
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
        const auto seed = argc > 2 ? static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 0u;

        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);
        device.SetRenderTarget(&target);

        // --- Street's scene, built by Street. ------------------------------
        CnaStreet::RenderSettings settings;
        settings.applyPreset(CnaStreet::QualityPreset::Low);
        settings.reflectionProbes = false; // a probe bake would dominate the run time
        settings.traffic = false;          // static city only: movers are submitted per frame
        settings.pedestrians = false;
        if (seed != 0) {
            settings.seed = seed;
        }

        const auto buildStart = Clock::now();
        CnaStreet::MaterialLibrary materials(&device);
        CnaStreet::ModelLibrary models(device, materials);
        CnaStreet::SceneRenderer streetRenderer(device, materials);
        streetRenderer.resize(Width, Height);
        streetRenderer.initialise(settings);
        CnaStreet::CityScene scene(device, streetRenderer, materials, models);
        scene.build(settings);
        const double buildMs = Milliseconds(Clock::now() - buildStart);

        const auto& viewpoints = scene.viewpoints();
        if (viewpoints.empty()) {
            std::fprintf(stderr, "Street produced no viewpoints\n");
            return 1;
        }
        const auto& viewpoint = viewpoints.front();
        CnaStreet::Camera streetCamera;
        streetCamera.setPerspective(viewpoint.fov, static_cast<float>(Width) / Height, settings.nearPlane,
                                    settings.farPlane);
        streetCamera.setPosition(viewpoint.position);
        streetCamera.setOrientation(viewpoint.yaw, viewpoint.pitch);

        // --- Street frame. ------------------------------------------------
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        const auto streetStart = Clock::now();
        streetRenderer.beginFrame();
        scene.submit(settings, streetCamera.position());
        streetRenderer.render(streetCamera, settings, 0.0f);
        const double streetMs = Milliseconds(Clock::now() - streetStart);
        // Street may have switched targets; read the capture target unbound.
        device.SetRenderTarget(nullptr);
        const auto streetPixels = Pixels(target);
        const auto& streetStats = streetRenderer.stats();

        // --- The same scene through the engine. ----------------------------
        Graphics::Camera camera;
        camera.SetPosition(streetCamera.position());
        camera.SetYawPitchRoll(-streetCamera.yaw(), streetCamera.pitch());
        camera.SetPerspective(streetCamera.verticalFov(), streetCamera.aspect(), streetCamera.nearPlane(),
                              streetCamera.farPlane());

        const auto& items = streetRenderer.*Get(ItemsTag{});
        const auto& dynamic = streetRenderer.*Get(DynamicTag{});
        const auto& groups = streetRenderer.*Get(GroupsTag{});
        const Graphics::AttributeMask pntt{Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Normal,
                                           Graphics::VertexAttribute::Tangent, Graphics::VertexAttribute::TexCoord0};
        std::map<const CnaStreet::GpuMesh*, Graphics::GpuMesh> meshes;
        std::map<const CnaStreet::Material*, Graphics::Material> engineMaterials;
        auto meshFor = [&](const CnaStreet::GpuMesh* mesh) -> const Graphics::GpuMesh& {
            auto it = meshes.find(mesh);
            if (it == meshes.end()) {
                it = meshes.emplace(mesh, Graphics::GpuMesh::Borrow(*mesh->part(), mesh->bounds(), pntt)).first;
            }
            return it->second;
        };
        auto materialFor = [&](const CnaStreet::Material* material) -> const Graphics::Material& {
            auto it = engineMaterials.find(material);
            if (it == engineMaterials.end()) {
                it = engineMaterials.emplace(material, ToEngine(*material)).first;
            }
            return it->second;
        };

        const auto engineStart = Clock::now();
        Graphics::RenderQueue queue;
        // Street's visible-item count covers registered items (shadow-only
        // ones included: they are culled like any other) and this frame's
        // movers (box distance against the prop cull distance, shadow-only
        // movers skipped). The engine applies the same rules; only drawable
        // items go to the queue.
        Graphics::CullStatistics itemCulling;
        auto convert = [&](const CnaStreet::SceneItem& item, float cullDistance) {
            Graphics::RenderItem e;
            e.mesh = &meshFor(item.mesh);
            e.material = &materialFor(item.material);
            e.world = item.world;
            e.bounds = item.worldSphere;
            e.box = item.worldBounds;
            e.cullDistance = cullDistance;
            return e;
        };
        for (const auto& item : items) {
            const auto e = convert(item, item.cullDistance > 0.0f ? std::min(item.cullDistance, settings.propCullDistance)
                                                                  : 0.0f);
            ++itemCulling.considered;
            if (Graphics::Classify(e, camera, Graphics::VisibilitySettings{}) == Graphics::Visibility::Visible) {
                ++itemCulling.visible;
                if (!item.shadowOnly) {
                    queue.Submit(e);
                }
            }
        }
        for (const auto& item : dynamic) {
            if (item.shadowOnly) {
                continue;
            }
            const auto e = convert(item, settings.propCullDistance);
            ++itemCulling.considered;
            if (Graphics::Classify(e, camera, Graphics::VisibilitySettings{}) == Graphics::Visibility::Visible) {
                ++itemCulling.visible;
                queue.Submit(e);
            }
        }

        std::uint64_t visibleInstances = 0;
        std::uint64_t totalInstances = 0;
        Graphics::CullStatistics instanceCulling;
        for (const auto& group : groups) {
            if (group.shadowOnly) {
                continue;
            }
            totalInstances += group.transforms.size();
            Graphics::VisibilitySettings visibility;
            visibility.maxDistance = group.cullDistance > 0.0f ? std::min(group.cullDistance, settings.propCullDistance)
                                                               : settings.propCullDistance;
            visibility.minDistance = group.minDistance;
            std::vector<std::size_t> visible;
            float nearest = 1e30f;
            for (std::size_t i = 0; i < group.transforms.size(); ++i) {
                ++instanceCulling.considered;
                if (Graphics::Classify(group.spheres[i], 1, camera, visibility) == Graphics::Visibility::Visible) {
                    visible.push_back(i);
                    nearest = std::min(nearest, Vector3::Distance(camera.Position(), group.spheres[i].Center)
                                                    - group.spheres[i].Radius);
                }
            }
            visibleInstances += visible.size();
            // Street's batch LOD: one decision per group from its nearest instance.
            const CnaStreet::GpuMesh* mesh =
                group.lodMesh != nullptr && group.lodDistance > 0.0f && nearest > group.lodDistance ? group.lodMesh
                                                                                                     : group.mesh;
            for (const std::size_t i : visible) {
                Graphics::RenderItem e;
                e.mesh = &meshFor(mesh);
                e.material = &materialFor(group.material);
                e.world = group.transforms[i];
                e.bounds = group.spheres[i];
                queue.Submit(e);
            }
        }
        queue.Sort(camera.Position());

        Graphics::ForwardRenderer renderer(device);
        renderer.SetMaterialBinder(std::make_shared<Enhanced::PbrMaterialBinder>(device, renderer.StockBinder()));
        renderer.Lighting().defaultLighting = false;
        renderer.Lighting().lightDirection = -streetRenderer.sky().sunDirection();
        renderer.Lighting().lightColor = streetRenderer.sky().sunColour();
        renderer.Lighting().ambientColor = Vector3(0.25f, 0.27f, 0.3f);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        Graphics::RenderStatistics engineStats;
        renderer.Render(queue, camera, &engineStats);
        const double engineMs = Milliseconds(Clock::now() - engineStart);
        device.SetRenderTarget(nullptr);
        const auto enginePixels = Pixels(target);

        // --- Images and report. --------------------------------------------
        double difference = 0.0;
        for (std::size_t i = 0; i < streetPixels.size(); ++i) {
            difference += std::abs(streetPixels[i].getRProperty() - enginePixels[i].getRProperty())
                          + std::abs(streetPixels[i].getGProperty() - enginePixels[i].getGProperty())
                          + std::abs(streetPixels[i].getBProperty() - enginePixels[i].getBProperty());
        }
        difference /= static_cast<double>(streetPixels.size() * 3);
        const auto streetPng = Graphics::WritePng(device, streetPixels, Width, Height, (output / "street.png").string());
        const auto enginePng = Graphics::WritePng(device, enginePixels, Width, Height, (output / "engine.png").string());

        std::printf("scene: %zu static items, %zu instance groups (%llu instances), built in %.0f ms\n", items.size(),
                    groups.size(), static_cast<unsigned long long>(totalInstances), buildMs);
        std::printf("visible items:     street %d, engine %llu\n", streetStats.visibleItems,
                    static_cast<unsigned long long>(itemCulling.visible));
        std::printf("visible instances: street %d, engine %llu\n", streetStats.visibleInstances,
                    static_cast<unsigned long long>(visibleInstances));
        std::printf("draw calls:        street %d (instanced %d), engine %llu (per-instance path)\n",
                    streetStats.drawCalls, streetStats.instancedDrawCalls,
                    static_cast<unsigned long long>(engineStats.drawCalls));
        std::printf("cpu frame:         street %.1f ms, engine %.1f ms (software renderer, Debug)\n", streetMs, engineMs);
        std::printf("image difference:  %.1f / 255 mean per channel\n", difference);
        for (const auto& fallback : renderer.LastFallbacks()) {
            std::printf("fallback: %s\n", fallback.c_str());
        }
        std::printf("images: %s %s\n", streetPng.path.c_str(), enginePng.path.c_str());

        std::fflush(stdout);
        const bool sameVisibility = static_cast<std::uint64_t>(streetStats.visibleItems) == itemCulling.visible
                                    && static_cast<std::uint64_t>(streetStats.visibleInstances) == visibleInstances;
        if (!sameVisibility) {
            std::fprintf(stderr, "visibility differs from Street's\n");
            return 1;
        }
        return streetPng.success && enginePng.success ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "street scene comparison: %s\n", e.what());
        return 1;
    }
}
