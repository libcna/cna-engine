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
// Three views: Street's first viewpoint, a walk driven by engine input, and a
// close-up of a pedestrian (the skinned path). For each, a report on stdout
// (visible items, instances and figures, draw calls, CPU milliseconds, image
// difference, the engine's diagnostics) and both images as PNG files; then
// GPU resource estimates. The program fails when the engine's visibility
// decisions differ from Street's (the rules are the same and must give the
// same sets) or when an image leaves the agreed screenshot tolerance
// (ImageTolerance.hpp, docs/pilots/screenshot-tolerances.md).
//
// Usage: cna_engine_street_scene_comparison <output-directory> [seed] [probes]
// ("probes" bakes Street's reflection probes and lights items from them, as
// Street's Medium preset does; it is slow, so the test runs without).
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/DiagnosticsReport.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMemory.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"
#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include "ImageTolerance.hpp"

#include "CnaStreet/Assets/ModelLibrary.hpp"
#include "CnaStreet/Render/Camera.hpp"
#include "CnaStreet/Render/GpuMesh.hpp"
#include "CnaStreet/Render/MaterialLibrary.hpp"
#include "CnaStreet/Render/RenderSettings.hpp"
#include "CnaStreet/Render/SkinnedGpuMesh.hpp"
#include "CnaStreet/Render/SceneRenderer.hpp"
#include "CnaStreet/Scene/CityScene.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaModeEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTangentTexture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <map>
#include <set>
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
    struct SkinnedTag
    {
        using type = std::vector<CnaStreet::SkinnedItem> CnaStreet::SceneRenderer::*;
        friend type Get(SkinnedTag);
    };
    template struct Expose<ItemsTag, &CnaStreet::SceneRenderer::items_>;
    template struct Expose<DynamicTag, &CnaStreet::SceneRenderer::dynamic_>;
    template struct Expose<GroupsTag, &CnaStreet::SceneRenderer::groups_>;
    template struct Expose<SkinnedTag, &CnaStreet::SceneRenderer::skinned_>;

    // SkinnedGpuMesh keeps its (possibly shared) buffers to itself.
    template <typename Member>
    struct SkinnedMeshTag
    {
        using type = Member CnaStreet::SkinnedGpuMesh::*;
    };
    struct VertexBufferTag : SkinnedMeshTag<Microsoft::Xna::Framework::Graphics::VertexBuffer*>
    {
        friend type Get(VertexBufferTag);
    };
    struct IndexBufferTag : SkinnedMeshTag<Microsoft::Xna::Framework::Graphics::IndexBuffer*>
    {
        friend type Get(IndexBufferTag);
    };
    struct VertexOffsetTag : SkinnedMeshTag<int>
    {
        friend type Get(VertexOffsetTag);
    };
    struct StartIndexTag : SkinnedMeshTag<int>
    {
        friend type Get(StartIndexTag);
    };
    struct VertexCountTag : SkinnedMeshTag<int>
    {
        friend type Get(VertexCountTag);
    };
    template struct Expose<VertexBufferTag, &CnaStreet::SkinnedGpuMesh::vertexBuffer_>;
    template struct Expose<IndexBufferTag, &CnaStreet::SkinnedGpuMesh::indexBuffer_>;
    template struct Expose<VertexOffsetTag, &CnaStreet::SkinnedGpuMesh::vertexOffset_>;
    template struct Expose<StartIndexTag, &CnaStreet::SkinnedGpuMesh::startIndex_>;
    template struct Expose<VertexCountTag, &CnaStreet::SkinnedGpuMesh::vertexCount_>;
}

namespace
{
    using namespace CnaEngine;
    using Clock = std::chrono::steady_clock;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::AlphaModeEXT;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
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
            case AlphaModeEXT::Blend:
                e.blend = m.premultipliedBlend ? Graphics::BlendMode::PremultipliedAlpha : Graphics::BlendMode::AlphaBlend;
                break;
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
        const bool probes = argc > 3 && std::string(argv[3]) == "probes";

        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        RenderTarget2D target(device, Width, Height, false, SurfaceFormat::Color, DepthFormat::Depth24);
        device.SetRenderTarget(&target);

        // --- Street's scene, built by Street. ------------------------------
        CnaStreet::RenderSettings settings;
        settings.applyPreset(CnaStreet::QualityPreset::Low);
        // A probe bake would dominate the run time, so reflection probes are
        // opt-in (third argument "probes": Medium's 32-pixel probes every 32 m).
        settings.reflectionProbes = probes;
        if (probes) {
            settings.probeFaceSize = 32;
            settings.probeSpacing = 32.0f;
        }
        settings.traffic = true;           // moving vehicles: dynamic items, drivers as skinned figures
        settings.pedestrians = true;       // skinned figures (ENG-032)
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
        scene.update(0.5f, settings); // poses the pedestrians (their bone palettes)
        const double buildMs = Milliseconds(Clock::now() - buildStart);

        const auto& viewpoints = scene.viewpoints();
        if (viewpoints.empty()) {
            std::fprintf(stderr, "Street produced no viewpoints\n");
            return 1;
        }

        // --- The engine side, set up once. ----------------------------------
        const auto& items = streetRenderer.*Get(ItemsTag{});
        const auto& dynamic = streetRenderer.*Get(DynamicTag{});
        const auto& groups = streetRenderer.*Get(GroupsTag{});
        const auto& skinnedItems = streetRenderer.*Get(SkinnedTag{});
        const Graphics::AttributeMask pntt{Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Normal,
                                           Graphics::VertexAttribute::Tangent, Graphics::VertexAttribute::TexCoord0};
        const Graphics::AttributeMask skinnedAttributes{
            Graphics::VertexAttribute::Position,     Graphics::VertexAttribute::Normal,
            Graphics::VertexAttribute::Tangent,      Graphics::VertexAttribute::TexCoord0,
            Graphics::VertexAttribute::BlendIndices, Graphics::VertexAttribute::BlendWeights};
        std::map<const CnaStreet::GpuMesh*, Graphics::GpuMesh> meshes;
        std::map<const CnaStreet::SkinnedGpuMesh*, Graphics::GpuMesh> skinnedMeshes;
        std::map<const CnaStreet::Material*, Graphics::Material> engineMaterials;
        auto meshFor = [&](const CnaStreet::GpuMesh* mesh) -> const Graphics::GpuMesh& {
            auto it = meshes.find(mesh);
            if (it == meshes.end()) {
                it = meshes.emplace(mesh, Graphics::GpuMesh::Borrow(*mesh->part(), mesh->bounds(), pntt)).first;
            }
            return it->second;
        };
        auto skinnedMeshFor = [&](const CnaStreet::SkinnedGpuMesh* mesh) -> const Graphics::GpuMesh& {
            auto it = skinnedMeshes.find(mesh);
            if (it == skinnedMeshes.end()) {
                const CnaStreet::SkinnedGpuMesh& m = *mesh;
                it = skinnedMeshes
                         .emplace(mesh, Graphics::GpuMesh::Borrow(*(m.*Get(VertexBufferTag{})),
                                                                  *(m.*Get(IndexBufferTag{})), m.*Get(VertexCountTag{}),
                                                                  m.triangleCount(), m.bounds(), skinnedAttributes,
                                                                  m.*Get(VertexOffsetTag{}), m.*Get(StartIndexTag{})))
                         .first;
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

        // Street's lighting (SceneRenderer::applyLighting/applyEnvironment):
        // its sun, sky ambient and image-based environment, all under its
        // exposure, and fog matched to the horizon haze.
        auto& sky = streetRenderer.sky();
        const float exposure = std::max(settings.exposure, 0.0f);
        Graphics::ForwardRenderer renderer(device);
        auto pbr = std::make_shared<Enhanced::PbrMaterialBinder>(device, renderer.StockBinder());
        pbr->Settings().exposure = exposure;
        if (sky.hasImageBasedLighting() && settings.imageBasedLighting) {
            auto& environment = pbr->Settings().environment;
            environment.Irradiance = sky.irradiance();
            environment.PrefilteredSpecular = sky.prefiltered();
            environment.PrefilteredMipCount = sky.prefilteredMipCount();
            environment.BrdfLut = sky.brdfLut();
            environment.Intensity = sky.environmentScale() * settings.iblIntensity;
        }
        // Street's applyEnvironment: an item's own probe (prefiltered
        // specular, and its irradiance when probeIrradiance is set) with the
        // sky's BRDF table and scale; otherwise the sky's environment.
        std::map<const CnaStreet::ReflectionProbe*, Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT> probeLights;
        pbr->SetEnvironmentSelector(
            [&](const Graphics::RenderItem& item) -> const Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT* {
                const auto* probe = static_cast<const CnaStreet::ReflectionProbe*>(item.userData);
                if (probe == nullptr || probe->prefiltered == nullptr || !sky.hasImageBasedLighting()
                    || !settings.imageBasedLighting) {
                    return nullptr;
                }
                auto it = probeLights.find(probe);
                if (it == probeLights.end()) {
                    Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT light;
                    light.PrefilteredSpecular = probe->prefiltered.get();
                    light.PrefilteredMipCount = probe->prefilteredMips;
                    light.Irradiance = settings.probeIrradiance && probe->irradiance != nullptr ? probe->irradiance.get()
                                                                                                : sky.irradiance();
                    light.BrdfLut = sky.brdfLut();
                    light.Intensity = sky.environmentScale() * settings.iblIntensity;
                    it = probeLights.emplace(probe, light).first;
                }
                return &it->second;
            });
        renderer.SetMaterialBinder(pbr);
        auto& lighting = renderer.Lighting();
        lighting.defaultLighting = false;
        lighting.lightDirection = sky.lightDirection();
        lighting.lightColor = sky.sunColour();
        lighting.ambientColor = sky.ambientColour();
        lighting.fogEnabled = settings.heightFog;
        lighting.fogColor = sky.ambientColour();
        lighting.fogStart = settings.farPlane * 0.30f;
        lighting.fogEnd = settings.farPlane * 0.98f;
        // Street's sky is the game's own drawing: a custom pass before the
        // opaque geometry, as Street draws it.
        renderer.AddPass(Graphics::PassPoint::BeforeOpaque, "street-sky",
                         [&](GraphicsDevice& passDevice, const Graphics::Camera& passCamera, Graphics::RenderStatistics&) {
                             passDevice.setDepthStencilStateProperty(DepthStencilState::None);
                             passDevice.setBlendStateProperty(BlendState::Opaque);
                             sky.draw(passCamera.View(), passCamera.Projection(), Width, Height, 0.0f, exposure, true);
                         });

        // One view drawn by Street and by the engine; returns false when the
        // visibility decisions differ or an image cannot be written.
        auto compare = [&](const CnaStreet::Camera& streetCamera, const std::string& name) -> bool {
            // --- Street frame. --------------------------------------------
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
            const auto streetStats = streetRenderer.stats();

            // --- The same view through the engine. ------------------------
            Graphics::Camera camera;
            camera.SetPosition(streetCamera.position());
            camera.SetYawPitchRoll(-streetCamera.yaw(), streetCamera.pitch());
            camera.SetPerspective(streetCamera.verticalFov(), streetCamera.aspect(), streetCamera.nearPlane(),
                                  streetCamera.farPlane());

            const auto engineStart = Clock::now();
            Graphics::RenderQueue queue;
            // Street's visible-item count covers registered items (shadow-only
            // ones included: they are culled like any other) and this frame's
            // movers (box distance against the prop cull distance, shadow-only
            // movers skipped). The engine applies the same rules; only
            // drawable items go to the queue.
            Graphics::CullStatistics itemCulling;
            auto convert = [&](const CnaStreet::SceneItem& item, float cullDistance) {
                Graphics::RenderItem e;
                e.mesh = &meshFor(item.mesh);
                e.material = &materialFor(item.material);
                e.world = item.world;
                e.bounds = item.worldSphere;
                e.box = item.worldBounds;
                e.cullDistance = cullDistance;
                e.userData = item.probe; // Street's nearest reflection probe, if any
                return e;
            };
            for (const auto& item : items) {
                const auto e = convert(item, item.cullDistance > 0.0f
                                                 ? std::min(item.cullDistance, settings.propCullDistance)
                                                 : 0.0f);
                const auto result = Graphics::Classify(e, camera, Graphics::VisibilitySettings{});
                itemCulling.Record(result);
                if (result == Graphics::Visibility::Visible && !item.shadowOnly) {
                    queue.Submit(e);
                }
            }
            for (const auto& item : dynamic) {
                if (item.shadowOnly) {
                    continue;
                }
                const auto e = convert(item, settings.propCullDistance);
                const auto result = Graphics::Classify(e, camera, Graphics::VisibilitySettings{});
                itemCulling.Record(result);
                if (result == Graphics::Visibility::Visible) {
                    queue.Submit(e);
                }
            }

            // Skinned figures: Street's own limit (pedestrian cull distance,
            // sphere against the frustum), one skinned render item each.
            Graphics::CullStatistics skinnedCulling;
            Graphics::RenderQueue figuresOnly;
            for (const auto& item : skinnedItems) {
                Graphics::VisibilitySettings visibility;
                visibility.maxDistance = settings.pedestrianCullDistance;
                const auto result = Graphics::Classify(item.worldSphere, 1, camera, visibility);
                skinnedCulling.Record(result);
                if (result != Graphics::Visibility::Visible) {
                    continue;
                }
                Graphics::RenderItem e;
                e.mesh = &skinnedMeshFor(item.mesh);
                e.material = &materialFor(item.material);
                e.world = item.world;
                e.bounds = item.worldSphere;
                e.bones = item.bones;
                queue.Submit(e);
                figuresOnly.Submit(e);
            }

            std::uint64_t visibleInstances = 0;
            std::uint64_t totalInstances = 0;
            Graphics::CullStatistics instanceCulling;
            Graphics::LodStatistics groupLod;
            // One instanced render item per group, as Street draws them.
            std::vector<Graphics::InstanceSet> sets(groups.size());
            for (std::size_t g = 0; g < groups.size(); ++g) {
                const auto& group = groups[g];
                if (group.shadowOnly) {
                    continue;
                }
                totalInstances += group.transforms.size();
                Graphics::VisibilitySettings visibility;
                visibility.maxDistance = group.cullDistance > 0.0f
                                             ? std::min(group.cullDistance, settings.propCullDistance)
                                             : settings.propCullDistance;
                visibility.minDistance = group.minDistance;
                std::vector<std::size_t> visible;
                float nearest = 1e30f;
                for (std::size_t i = 0; i < group.transforms.size(); ++i) {
                    const auto result = Graphics::Classify(group.spheres[i], 1, camera, visibility);
                    instanceCulling.Record(result);
                    if (result == Graphics::Visibility::Visible) {
                        visible.push_back(i);
                        nearest = std::min(nearest, Vector3::Distance(camera.Position(), group.spheres[i].Center)
                                                        - group.spheres[i].Radius);
                    }
                }
                visibleInstances += visible.size();
                if (visible.empty()) {
                    continue;
                }
                // Street's batch LOD: one decision per group from its nearest
                // instance, past lodDistance (no hysteresis).
                int level = 0;
                if (group.lodMesh != nullptr && group.lodDistance > 0.0f) {
                    level = Graphics::LodSelector({group.lodDistance}, 0.0f).Select(nearest);
                    groupLod.Record(level);
                }
                const CnaStreet::GpuMesh* mesh = level == 1 ? group.lodMesh : group.mesh;
                Graphics::InstanceSet& set = sets[g];
                set.SetMesh(&meshFor(mesh));
                for (const std::size_t i : visible) {
                    set.Add(group.transforms[i]);
                }
                Graphics::RenderItem e;
                e.mesh = set.Mesh();
                e.material = &materialFor(group.material);
                e.bounds = group.spheres[visible.front()];
                e.instances = &set;
                queue.Submit(e);
            }
            queue.Sort(camera.Position());

            device.SetRenderTarget(&target);
            device.Clear(Color(0, 0, 0, 255));
            Graphics::RenderStatistics engineStats;
            renderer.Render(queue, camera, &engineStats);
            const double engineMs = Milliseconds(Clock::now() - engineStart);
            device.SetRenderTarget(nullptr);
            const auto enginePixels = Pixels(target);
            const auto fallbacks = renderer.LastFallbacks();

            // How much of the frame the figures cover on their own (shows
            // that the skinned path is actually exercised).
            figuresOnly.Sort(camera.Position());
            device.SetRenderTarget(&target);
            device.Clear(Color(0, 0, 0, 0));
            renderer.Render(figuresOnly, camera);
            device.SetRenderTarget(nullptr);
            std::vector<bool> figureMask;
            std::size_t figurePixels = 0;
            for (const auto& c : Pixels(target)) {
                figureMask.push_back(c.getAProperty() > 0);
                figurePixels += figureMask.back() ? 1u : 0u;
            }

            // --- Images and report. ---------------------------------------
            const Pilot::ImageDifference image = Pilot::Compare(streetPixels, enginePixels);
            std::size_t differingOnFigures = 0;
            for (std::size_t i = 0; i < streetPixels.size(); ++i) {
                differingOnFigures += figureMask[i] && streetPixels[i] != enginePixels[i] ? 1u : 0u;
            }
            const bool withinTolerance = Pilot::Within(image);
            const std::string prefix = name.empty() ? "" : name + "-";
            const auto streetPng =
                Graphics::WritePng(device, streetPixels, Width, Height, (output / (prefix + "street.png")).string());
            const auto enginePng =
                Graphics::WritePng(device, enginePixels, Width, Height, (output / (prefix + "engine.png")).string());

            std::printf("--- view: %s\n", name.empty() ? "first viewpoint" : name.c_str());
            std::printf("scene: %zu static items, %zu instance groups (%llu instances), built in %.0f ms\n",
                        items.size(), groups.size(), static_cast<unsigned long long>(totalInstances), buildMs);
            std::printf("visible items:     street %d, engine %llu\n", streetStats.visibleItems,
                        static_cast<unsigned long long>(itemCulling.visible));
            std::printf("visible instances: street %d, engine %llu\n", streetStats.visibleInstances,
                        static_cast<unsigned long long>(visibleInstances));
            std::printf("visible figures:   street %d (%d skinned draws), engine %llu of %zu; %zu pixels of figures,"
                        " %zu of them differ\n",
                        streetStats.visibleCharacters, streetStats.skinnedDrawCalls,
                        static_cast<unsigned long long>(skinnedCulling.visible), skinnedItems.size(), figurePixels,
                        differingOnFigures);
            std::printf("draw calls:        street %d (instanced %d), engine %llu\n", streetStats.drawCalls,
                        streetStats.instancedDrawCalls, static_cast<unsigned long long>(engineStats.drawCalls));
            std::printf("cpu frame:         street %.1f ms, engine %.1f ms (software renderer)\n", streetMs,
                        engineMs);
            std::printf("image difference:  %.2f / 255 mean per channel, %d / 255 largest, %zu of %zu pixels differ,"
                        " %zu visibly (> %d / 255) — %s the agreed tolerance\n",
                        image.mean, image.largest, image.differing, image.pixels, image.visible, Pilot::VisibleStep,
                        withinTolerance ? "within" : "OUTSIDE");
            // Culling and LOD counts through the engine's diagnostics (ENG-033).
            Graphics::DiagnosticsSources itemSources;
            itemSources.render = &engineStats;
            itemSources.culling = &itemCulling;
            itemSources.lod = &groupLod;
            itemSources.includeGpuMemory = false;
            for (const auto& line : Graphics::FormatDiagnostics(itemSources)) {
                std::printf("engine diagnostics: %s\n", line.c_str());
            }
            Graphics::DiagnosticsSources instanceSources;
            instanceSources.culling = &instanceCulling;
            instanceSources.includeGpuMemory = false;
            for (const auto& line : Graphics::FormatDiagnostics(instanceSources)) {
                std::printf("engine diagnostics (instances): %s\n", line.c_str());
            }
            Graphics::DiagnosticsSources skinnedSources;
            skinnedSources.culling = &skinnedCulling;
            skinnedSources.includeGpuMemory = false;
            for (const auto& line : Graphics::FormatDiagnostics(skinnedSources)) {
                std::printf("engine diagnostics (figures): %s\n", line.c_str());
            }
            for (const auto& fallback : fallbacks) {
                std::printf("fallback: %s\n", fallback.c_str());
            }
            std::printf("images: %s %s\n", streetPng.path.c_str(), enginePng.path.c_str());
            std::fflush(stdout);

            const bool sameVisibility =
                static_cast<std::uint64_t>(streetStats.visibleItems) == itemCulling.visible
                && static_cast<std::uint64_t>(streetStats.visibleInstances) == visibleInstances
                && static_cast<std::uint64_t>(streetStats.visibleCharacters) == skinnedCulling.visible;
            if (!sameVisibility) {
                std::fprintf(stderr, "%s: visibility differs from Street's\n", name.c_str());
            }
            if (!withinTolerance) {
                std::fprintf(stderr, "%s: image outside the agreed tolerance\n", name.c_str());
            }
            return sameVisibility && withinTolerance && streetPng.success && enginePng.success;
        };

        const auto& viewpoint = viewpoints.front();
        CnaStreet::Camera streetCamera;
        streetCamera.setPerspective(viewpoint.fov, static_cast<float>(Width) / Height, settings.nearPlane,
                                    settings.farPlane);
        streetCamera.setPosition(viewpoint.position);
        streetCamera.setOrientation(viewpoint.yaw, viewpoint.pitch);
        bool ok = compare(streetCamera, "");

        // Engine input drives the camera (MILESTONE-2): from Street's
        // viewpoint an ActionMap and a FlyCameraController walk the camera
        // under scripted keys -- forward, then forward while turning left --
        // and Street and the engine both render the pose the engine reached.
        {
            using Microsoft::Xna::Framework::Input::KeyboardState;
            using Microsoft::Xna::Framework::Input::Keys;
            constexpr float Step = 1.0f / 60.0f;
            Input::ActionMap actions;
            Graphics::CameraInputAxes axes;
            axes.moveRight = actions.AddAxis("MoveRight");
            axes.moveForward = actions.AddAxis("MoveForward");
            axes.lookYaw = actions.AddAxis("LookYaw");
            axes.lookPitch = actions.AddAxis("LookPitch");
            axes.lookScale = 0.4f * Step; // 0.4 rad per second of turning
            actions.BindAxis(axes.moveRight, Input::AxisBinding::ForKeys(Keys::A, Keys::D));
            actions.BindAxis(axes.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
            actions.BindAxis(axes.lookYaw, Input::AxisBinding::ForKeys(Keys::Right, Keys::Left));
            actions.BindAxis(axes.lookPitch, Input::AxisBinding::ForKeys(Keys::Down, Keys::Up));

            Graphics::Camera walker;
            walker.SetPosition(viewpoint.position);
            walker.SetYawPitchRoll(-viewpoint.yaw, viewpoint.pitch);
            Graphics::FlyCameraController controller(Graphics::FirstPersonSettings{3.0f, 2.0f, 1.4f});
            controller.SyncFrom(walker);
            for (int update = 0; update < 120; ++update) {
                Input::InputSnapshot snapshot;
                snapshot.keyboard = update < 60 ? KeyboardState({Keys::W}) : KeyboardState({Keys::W, Keys::Left});
                actions.Update(snapshot);
                controller.Update(walker, Graphics::ReadCameraInput(actions, axes), Step);
            }
            CnaStreet::Camera walkCamera;
            walkCamera.setPerspective(viewpoint.fov, static_cast<float>(Width) / Height, settings.nearPlane,
                                      settings.farPlane);
            walkCamera.setPosition(walker.Position());
            walkCamera.setOrientation(-controller.Yaw(), controller.Pitch());
            std::printf("walk: %.2f m from the viewpoint, turned %.2f rad\n",
                        Vector3::Distance(viewpoint.position, walker.Position()),
                        controller.Yaw() + viewpoint.yaw);
            ok = compare(walkCamera, "walk") && ok;
        }

        // A close-up of a pedestrian: 3.5 m in front of the first figure
        // whose eye point is in the open, looking at it.
        bool closeUp = false;
        for (const auto& figure : skinnedItems) {
            Vector3 facing(-figure.world.M31, 0.0f, -figure.world.M33);
            if (facing.LengthSquared() < 1e-6f) {
                continue;
            }
            facing.Normalize();
            const Vector3 target3 = figure.worldSphere.Center;
            const Vector3 eye = target3 + facing * 3.5f + Vector3(0.0f, 0.4f, 0.0f);
            if (scene.isSolid(eye)) {
                continue;
            }
            const Vector3 look = Vector3::Normalize(target3 - eye);
            // The engine camera's convention (positive yaw turns left from
            // -Z), and Street's yaw is its negation (see the conversion above).
            const float engineYaw = std::atan2(-look.X, -look.Z);
            const float pitch = std::asin(std::clamp(look.Y, -1.0f, 1.0f));
            CnaStreet::Camera figureCamera;
            figureCamera.setPerspective(viewpoint.fov, static_cast<float>(Width) / Height, settings.nearPlane,
                                        settings.farPlane);
            figureCamera.setPosition(eye);
            figureCamera.setOrientation(-engineYaw, pitch);
            ok = compare(figureCamera, "figure") && ok;
            closeUp = true;
            break;
        }
        if (!closeUp) {
            std::fprintf(stderr, "no pedestrian could be framed for the close-up\n");
            return 1;
        }

        // GPU resource estimates (MILESTONE-2): the engine's, measured on the
        // resources the scene registers, next to Street's own accounting
        // (Street counts the meshes it uploads itself and nothing it borrows
        // from models, and its texture library's loads).
        Graphics::GpuMemoryEstimate registered;
        Graphics::GpuMemoryEstimate streetOwned;
        std::uint64_t streetOwnedBytes = 0;
        std::set<const CnaStreet::GpuMesh*> counted;
        auto addMesh = [&](const CnaStreet::GpuMesh* mesh) {
            if (mesh == nullptr) {
                return;
            }
            registered.Add(meshFor(mesh));
            if (mesh->gpuBytes() > 0 && counted.insert(mesh).second) {
                streetOwned.Add(meshFor(mesh));
                streetOwnedBytes += mesh->gpuBytes();
            }
        };
        for (const auto* list : {&items, &dynamic}) {
            for (const auto& item : *list) {
                addMesh(item.mesh);
                registered.Add(materialFor(item.material));
            }
        }
        for (const auto& group : groups) {
            addMesh(group.mesh);
            addMesh(group.lodMesh);
            registered.Add(materialFor(group.material));
        }
        for (const auto& figure : skinnedItems) {
            registered.Add(skinnedMeshFor(figure.mesh));
            registered.Add(materialFor(figure.material));
        }
        Graphics::DiagnosticsSources memorySources;
        memorySources.memoryEstimate = &registered;
        memorySources.includeGpuMemory = false;
        std::printf("--- GPU resources\n");
        for (const auto& line : Graphics::FormatDiagnostics(memorySources)) {
            std::printf("engine diagnostics: %s (everything the scene registers)\n", line.c_str());
        }
        // Street sizes vertices with sizeof() of CNA's vertex struct, the GPU
        // with the declaration's stride; the difference per vertex shows it.
        const std::uint64_t ownedVertices = streetOwned.VertexBytes() / sizeof(float) / 12; // 48-byte stride
        std::printf("meshes Street uploads itself: street %llu bytes, engine estimate %llu bytes "
                    "(vertex %llu, index %llu; %llu vertices, %.2f bytes per vertex apart; "
                    "sizeof(VertexPositionNormalTangentTexture) = %zu, declared stride %d)\n",
                    static_cast<unsigned long long>(streetOwnedBytes),
                    static_cast<unsigned long long>(streetOwned.VertexBytes() + streetOwned.IndexBytes()),
                    static_cast<unsigned long long>(streetOwned.VertexBytes()),
                    static_cast<unsigned long long>(streetOwned.IndexBytes()),
                    static_cast<unsigned long long>(ownedVertices),
                    ownedVertices == 0 ? 0.0
                                       : static_cast<double>(streetOwnedBytes - streetOwned.VertexBytes()
                                                             - streetOwned.IndexBytes())
                                             / static_cast<double>(ownedVertices),
                    sizeof(Microsoft::Xna::Framework::Graphics::VertexPositionNormalTangentTexture),
                    Microsoft::Xna::Framework::Graphics::VertexPositionNormalTangentTexture::getVertexDeclarationStatic()
                        .getVertexStrideProperty());
        std::printf("street's own totals: meshes %llu bytes, textures %llu bytes (library)\n",
                    static_cast<unsigned long long>(scene.buildStats().meshBytes),
                    static_cast<unsigned long long>(materials.textureBytes()));
        std::fflush(stdout);
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "street scene comparison: %s\n", e.what());
        return 1;
    }
}
