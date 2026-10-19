// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

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
}
