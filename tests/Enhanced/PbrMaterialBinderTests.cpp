// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace CnaEngine::Enhanced
{
    using Graphics::Material;
    using Graphics::RenderItem;
    using Graphics::RenderQueue;
    using Graphics::ShadingModel;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        constexpr int Size = 48;
        const Color Background(10, 10, 10, 255);

        // Constructed before the fixture members so they see a HiDef device.
        struct HiDefDevice
        {
            HiDefDevice() { device.SetGraphicsProfileEXT(GraphicsProfile::HiDef); }
            GraphicsDevice device;
        };

        class PbrFixture : public ::testing::Test, protected HiDefDevice
        {
        protected:
            PbrFixture()
                : target(device, Size, Size, false, SurfaceFormat::Color, DepthFormat::Depth24),
                  renderer(device),
                  binder(std::make_shared<PbrMaterialBinder>(device, renderer.StockBinder()))
            {
                Graphics::MeshBuilder builder;
                builder.AddSphere(Vector3::Zero, 1.0f, 32, 20);
                Graphics::MeshData plain = builder.Take();
                sphere = Graphics::GpuMesh::Upload(device, plain, Graphics::VertexLayout::PositionNormalTexture());
                plain.ComputeTangents();
                tangentSphere =
                    Graphics::GpuMesh::Upload(device, plain, Graphics::VertexLayout::PositionNormalTangentTexture());

                camera.SetPerspective(0.9f, 1.0f, 0.1f, 20.0f);
                camera.SetPosition(Vector3(0.0f, 0.0f, 3.5f));
                renderer.Lighting().defaultLighting = false;
                renderer.Lighting().lightDirection = Vector3(-0.4f, -0.5f, -1.0f);
                renderer.Lighting().lightColor = Vector3(1.0f, 1.0f, 1.0f);
                renderer.Lighting().ambientColor = Vector3(0.05f, 0.05f, 0.05f);
                renderer.SetMaterialBinder(binder);
            }

            std::vector<Color> Draw(const Material& material, const Graphics::GpuMesh& mesh)
            {
                RenderQueue queue;
                RenderItem item;
                item.mesh = &mesh;
                item.material = &material;
                item.bounds = Graphics::WorldBounds(mesh, item.world);
                queue.Submit(item);
                queue.Sort(camera.Position());
                device.SetRenderTarget(&target);
                device.Clear(Background);
                renderer.Render(queue, camera);
                device.SetRenderTarget(nullptr);
                std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
                target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                return pixels;
            }

            static Color At(const std::vector<Color>& pixels, int x, int y)
            {
                return pixels[static_cast<std::size_t>(y * Size + x)];
            }

            static int Distance(const Color& a, const Color& b)
            {
                return std::abs(a.getRProperty() - b.getRProperty()) + std::abs(a.getGProperty() - b.getGProperty())
                       + std::abs(a.getBProperty() - b.getBProperty());
            }

            static Material Pbr(const std::string& name, Vector3 color, float metallic, float roughness)
            {
                Material m;
                m.name = name;
                m.shading = ShadingModel::PhysicallyBased;
                m.diffuseColor = color;
                m.metallic = metallic;
                m.roughness = roughness;
                return m;
            }

            RenderTarget2D target;
            Graphics::ForwardRenderer renderer;
            std::shared_ptr<PbrMaterialBinder> binder;
            Graphics::GpuMesh sphere;
            Graphics::GpuMesh tangentSphere;
            Graphics::Camera camera;
        };
    }

    TEST_F(PbrFixture, LitSphereIsShadedAcrossItsSurface)
    {
        const Material clay = Pbr("clay", Vector3(0.8f, 0.3f, 0.2f), 0.0f, 0.8f);
        const auto pixels = Draw(clay, sphere);
        EXPECT_TRUE(renderer.LastFallbacks().empty());
        EXPECT_EQ(At(pixels, 1, 1), Background);
        const Color centre = At(pixels, Size / 2, Size / 2);
        EXPECT_NE(centre, Background);
        EXPECT_GT(centre.getRProperty(), centre.getBProperty()) << "base color shows";
        // The key light comes from the upper left behind the camera: the
        // upper-left limb is brighter than the lower-right one.
        const Color lit = At(pixels, Size / 2 - 8, Size / 2 - 8);
        const Color shaded = At(pixels, Size / 2 + 8, Size / 2 + 8);
        EXPECT_GT(lit.getRProperty(), shaded.getRProperty());
    }

    TEST_F(PbrFixture, MetallicAndRoughnessChangeTheResult)
    {
        const auto dielectric = Draw(Pbr("dielectric", Vector3(0.9f, 0.9f, 0.9f), 0.0f, 0.5f), sphere);
        const auto metal = Draw(Pbr("metal", Vector3(0.9f, 0.9f, 0.9f), 1.0f, 0.5f), sphere);
        const auto glossy = Draw(Pbr("glossy", Vector3(0.9f, 0.9f, 0.9f), 0.0f, 0.1f), sphere);
        int differentMetal = 0;
        int differentGloss = 0;
        for (int i = 0; i < Size * Size; ++i) {
            differentMetal += Distance(dielectric[static_cast<std::size_t>(i)], metal[static_cast<std::size_t>(i)]) > 6;
            differentGloss += Distance(dielectric[static_cast<std::size_t>(i)], glossy[static_cast<std::size_t>(i)]) > 6;
        }
        EXPECT_GT(differentMetal, 50);
        EXPECT_GT(differentGloss, 10);
    }

    TEST_F(PbrFixture, NormalMapNeedsTangents)
    {
        Texture2D flat(device, 1, 1);
        const Color up(128, 128, 255, 255);
        flat.SetData(&up, 1);
        Material bumpy = Pbr("bumpy", Vector3(0.5f, 0.5f, 0.5f), 0.0f, 0.6f);
        bumpy.normalTexture = &flat;

        (void)Draw(bumpy, sphere);
        ASSERT_EQ(renderer.LastFallbacks().size(), 1u);
        EXPECT_EQ(renderer.LastFallbacks()[0], "material 'bumpy': normal map ignored: mesh has no Tangent attribute");

        const auto withTangents = Draw(bumpy, tangentSphere);
        EXPECT_TRUE(renderer.LastFallbacks().empty());
        // A flat normal map leaves the shading as it was without one.
        const auto reference = Draw(Pbr("bumpy", Vector3(0.5f, 0.5f, 0.5f), 0.0f, 0.6f), tangentSphere);
        int far = 0;
        for (std::size_t i = 0; i < reference.size(); ++i) {
            far += Distance(reference[i], withTangents[i]) > 9;
        }
        EXPECT_LT(far, Size * Size / 50);
    }

    TEST_F(PbrFixture, UnlitMaterialsDelegateToTheStockBinder)
    {
        Material red;
        red.name = "red";
        red.shading = ShadingModel::Unlit;
        red.diffuseColor = Vector3(1.0f, 0.0f, 0.0f);
        const auto pixels = Draw(red, sphere);
        EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255));
        EXPECT_TRUE(renderer.LastFallbacks().empty()) << "delegation is a designed path, not a fallback";
    }

    TEST_F(PbrFixture, MeshWithoutNormalsFallsBackAndReports)
    {
        Graphics::MeshBuilder builder({Graphics::VertexAttribute::Position});
        builder.AddSphere(Vector3::Zero, 1.0f, 16, 10);
        const auto bare = Graphics::GpuMesh::Upload(device, builder.Mesh(), Graphics::VertexLayout::PositionColor(),
                                                    Graphics::GpuMeshOptions{
                                                        .missingAttributes = Graphics::MissingAttributePolicy::UseDefaults});
        const auto pixels = Draw(Pbr("bare", Vector3(0.0f, 1.0f, 0.0f), 0.0f, 0.5f), bare);
        EXPECT_NE(At(pixels, Size / 2, Size / 2), Background) << "still drawn";
        ASSERT_FALSE(renderer.LastFallbacks().empty());
        EXPECT_EQ(renderer.LastFallbacks()[0], "material 'bare': drawn unlit: mesh has no normals");
    }

    TEST_F(PbrFixture, BlendedSurfacesUseStraightAlpha)
    {
        // Emission only (no light reaches it), so the surface's color is
        // exactly its emissive red.
        renderer.Lighting().ambientColor = Vector3::Zero;
        Material glow = Pbr("glow", Vector3::Zero, 0.0f, 1.0f);
        glow.keyLight = false;
        glow.emissiveColor = Vector3(1.0f, 0.0f, 0.0f);
        glow.alpha = 0.5f;

        glow.blend = Graphics::BlendMode::AlphaBlend;
        const Color filtered = At(Draw(glow, sphere), Size / 2, Size / 2);
        // color * alpha + behind * (1 - alpha) = 255 * 0.5 + 10 * 0.5.
        EXPECT_NEAR(filtered.getRProperty(), 132, 2);
        EXPECT_NEAR(filtered.getGProperty(), 5, 2);

        glow.blend = Graphics::BlendMode::PremultipliedAlpha;
        const Color glass = At(Draw(glow, sphere), Size / 2, Size / 2);
        // color + behind * (1 - alpha): the surface's own color at full strength.
        EXPECT_EQ(glass.getRProperty(), 255);
        EXPECT_NEAR(glass.getGProperty(), 5, 2);
        EXPECT_TRUE(renderer.LastFallbacks().empty());
    }

    TEST_F(PbrFixture, DelegatedMaterialsKeepTheStockBlend)
    {
        Material tint;
        tint.name = "tint";
        tint.shading = ShadingModel::Unlit;
        tint.diffuseColor = Vector3(1.0f, 0.0f, 0.0f);
        tint.alpha = 0.5f;
        tint.blend = Graphics::BlendMode::AlphaBlend;
        // BasicEffect premultiplies: 255 * 0.5 + 10 * 0.5, not 255 * 0.25 + 10 * 0.5.
        EXPECT_NEAR(At(Draw(tint, sphere), Size / 2, Size / 2).getRProperty(), 132, 2);
    }

    TEST_F(PbrFixture, ExposureScalesEveryLight)
    {
        renderer.Lighting().ambientColor = Vector3(0.05f, 0.05f, 0.05f);
        const Material matte = Pbr("matte", Vector3(1.0f, 1.0f, 1.0f), 0.0f, 1.0f);
        const Color normal = At(Draw(matte, sphere), Size / 2 + 8, Size / 2 + 8);
        binder->Settings().exposure = 3.0f;
        const Color bright = At(Draw(matte, sphere), Size / 2 + 8, Size / 2 + 8);
        binder->Settings().exposure = 0.0f;
        const Color dark = At(Draw(matte, sphere), Size / 2 + 8, Size / 2 + 8);
        EXPECT_GT(bright.getRProperty(), normal.getRProperty() + 20);
        EXPECT_EQ(dark, Color(0, 0, 0, 255)) << "no light at all, ambient included";
    }

    TEST_F(PbrFixture, EnvironmentReplacesTheFlatAmbient)
    {
        using Microsoft::Xna::Framework::Graphics::CubeMapFace;
        using Microsoft::Xna::Framework::Graphics::TextureCube;
        renderer.Lighting().ambientColor = Vector3(0.4f, 0.0f, 0.0f);
        Material matte = Pbr("matte", Vector3(1.0f, 1.0f, 1.0f), 0.0f, 1.0f);
        matte.keyLight = false;
        const Color flat = At(Draw(matte, sphere), Size / 2, Size / 2);
        EXPECT_GT(flat.getRProperty(), 40);
        EXPECT_LT(flat.getGProperty(), 5);

        // A uniform green environment; a zero BRDF table leaves only its diffuse part.
        TextureCube irradiance(device, 1, false, SurfaceFormat::Color);
        TextureCube prefiltered(device, 1, false, SurfaceFormat::Color);
        const Color green(0, 160, 0, 255);
        for (int face = 0; face < 6; ++face) {
            irradiance.SetData(static_cast<CubeMapFace>(face), &green, 1);
            prefiltered.SetData(static_cast<CubeMapFace>(face), &green, 1);
        }
        Texture2D lut(device, 1, 1);
        const Color zero(0, 0, 0, 255);
        lut.SetData(&zero, 1);
        binder->Settings().environment.Irradiance = &irradiance;
        binder->Settings().environment.PrefilteredSpecular = &prefiltered;
        binder->Settings().environment.BrdfLut = &lut;
        const Color lit = At(Draw(matte, sphere), Size / 2, Size / 2);
        EXPECT_GT(lit.getGProperty(), 40);
        EXPECT_LT(lit.getRProperty(), 5) << "the environment replaces the red ambient";

        binder->Settings().environment = {};
        EXPECT_EQ(At(Draw(matte, sphere), Size / 2, Size / 2), flat) << "and leaves no trace";
    }

    TEST_F(PbrFixture, SkinnedItemsDrawThroughSkinnedPbrEffect)
    {
        Graphics::MeshBuilder builder({Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Normal,
                                       Graphics::VertexAttribute::TexCoord0, Graphics::VertexAttribute::BlendIndices,
                                       Graphics::VertexAttribute::BlendWeights});
        builder.AddSphere(Vector3::Zero, 1.0f, 32, 20);
        const auto skinned =
            Graphics::GpuMesh::Upload(device, builder.Mesh(), Graphics::VertexLayout::PositionNormalTextureSkinned());
        const Material clay = Pbr("clay", Vector3(0.8f, 0.3f, 0.2f), 0.0f, 0.8f);
        const std::vector<Matrix> raised{Matrix::CreateTranslation(0.0f, 1.2f, 0.0f)};

        RenderQueue queue;
        RenderItem item;
        item.mesh = &skinned;
        item.material = &clay;
        item.bounds = Microsoft::Xna::Framework::BoundingSphere(Vector3::Zero, 3.0f);
        item.bones = &raised;
        queue.Submit(item);
        device.SetRenderTarget(&target);
        device.Clear(Background);
        renderer.Render(queue, camera);
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        EXPECT_TRUE(renderer.LastFallbacks().empty());

        // Raised by 1.2: the lower part of the frame is empty, the upper shaded.
        EXPECT_EQ(At(pixels, Size / 2, Size - 8), Background);
        const Color top = At(pixels, Size / 2, 12);
        EXPECT_NE(top, Background);
        EXPECT_GT(top.getRProperty(), top.getBProperty()) << "the material's base color";
        // Same image as the rigid sphere moved by its world matrix.
        const Color rigidTop = [&] {
            RenderQueue rigidQueue;
            RenderItem rigidItem;
            rigidItem.mesh = &sphere;
            rigidItem.material = &clay;
            rigidItem.world = raised[0];
            rigidItem.bounds = Graphics::WorldBounds(sphere, rigidItem.world);
            rigidQueue.Submit(rigidItem);
            device.SetRenderTarget(&target);
            device.Clear(Background);
            renderer.Render(rigidQueue, camera);
            device.SetRenderTarget(nullptr);
            std::vector<Color> rigidPixels(static_cast<std::size_t>(Size * Size));
            target.GetData(rigidPixels.data(), static_cast<int>(rigidPixels.size()));
            return At(rigidPixels, Size / 2, 12);
        }();
        EXPECT_LE(Distance(top, rigidTop), 6);
    }

    TEST_F(PbrFixture, EnvironmentSelectorLightsItemsFromTheirProbes)
    {
        using Microsoft::Xna::Framework::Graphics::CubeMapFace;
        using Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT;
        using Microsoft::Xna::Framework::Graphics::TextureCube;
        renderer.Lighting().ambientColor = Vector3::Zero;
        // Uniform cubes and a zero BRDF table: only each environment's diffuse part lights.
        auto uniform = [&](TextureCube& cube, const Color& color) {
            for (int face = 0; face < 6; ++face) {
                cube.SetData(static_cast<CubeMapFace>(face), &color, 1);
            }
        };
        TextureCube green(device, 1, false, SurfaceFormat::Color);
        TextureCube red(device, 1, false, SurfaceFormat::Color);
        uniform(green, Color(0, 160, 0, 255));
        uniform(red, Color(160, 0, 0, 255));
        Texture2D lut(device, 1, 1);
        const Color zero(0, 0, 0, 255);
        lut.SetData(&zero, 1);
        binder->Settings().environment = ImageBasedLightEXT{&green, &green, &lut, 1, 1.0f};
        const ImageBasedLightEXT probe{&red, &red, &lut, 1, 1.0f};
        const int inProbe = 1;
        int asked = 0;
        binder->SetEnvironmentSelector([&](const RenderItem& item) -> const ImageBasedLightEXT* {
            ++asked;
            return item.userData == &inProbe ? &probe : nullptr;
        });

        Material matte = Pbr("matte", Vector3(1.0f, 1.0f, 1.0f), 0.0f, 1.0f);
        matte.keyLight = false;
        RenderQueue queue;
        for (const float x : {-1.2f, 1.2f, 3.0f}) {
            RenderItem item;
            item.mesh = &sphere;
            item.material = &matte;
            item.world = Matrix::CreateScale(0.5f) * Matrix::CreateTranslation(x, 0.0f, 0.0f);
            item.bounds = Graphics::WorldBounds(sphere, item.world);
            item.userData = x > 0.0f && x < 2.0f ? &inProbe : nullptr;
            queue.Submit(item);
        }
        queue.Sort(camera.Position());
        device.SetRenderTarget(&target);
        device.Clear(Background);
        renderer.Render(queue, camera);
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        EXPECT_EQ(asked, 3);

        const Color left = At(pixels, Size / 2 - 11, Size / 2);
        const Color right = At(pixels, Size / 2 + 11, Size / 2);
        EXPECT_GT(left.getGProperty(), 40) << "the frame's green environment";
        EXPECT_LT(left.getRProperty(), 5);
        EXPECT_GT(right.getRProperty(), 40) << "the probe's red environment";
        EXPECT_LT(right.getGProperty(), 5);

        binder->SetEnvironmentSelector({});
        binder->Settings().environment = {};
        device.SetRenderTarget(&target);
        renderer.Render(queue, camera); // leaves no texture of this test bound
        device.SetRenderTarget(nullptr);
    }
}
