// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CullMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::CullMode;
    using Microsoft::Xna::Framework::Graphics::DepthFormat;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        constexpr int Size = 32;

        // Unit quad in the XY plane facing the camera at +Z, with UVs.
        MeshData Quad()
        {
            MeshBuilder builder({VertexAttribute::Position, VertexAttribute::TexCoord0});
            builder.AddBox(Vector3(-1.0f, -1.0f, 0.0f), Vector3(1.0f, 1.0f, 0.0f));
            MeshData quad = builder.Take();
            // Keep only the +Z face (the first six indices of AddBox).
            quad.indices.resize(6);
            return quad;
        }

        class Fixture : public ::testing::Test
        {
        protected:
            Fixture()
                : target(device, Size, Size, false, SurfaceFormat::Color, DepthFormat::Depth24),
                  quad(GpuMesh::Upload(device, Quad(), VertexLayout::PositionTexture())),
                  renderer(device)
            {
                camera.SetOrthographic(2.0f, 2.0f, 0.1f, 10.0f);
                camera.SetPosition(Vector3(0.0f, 0.0f, 5.0f));
            }

            RenderItem Item(const Material& material, float z, float scale = 1.0f)
            {
                RenderItem item;
                item.mesh = &quad;
                item.material = &material;
                item.world = Matrix::CreateScale(scale) * Matrix::CreateTranslation(0.0f, 0.0f, z);
                item.bounds = WorldBounds(quad, item.world);
                return item;
            }

            std::vector<Color> Draw(RenderQueue& queue, RenderStatistics* stats = nullptr)
            {
                device.SetRenderTarget(&target);
                device.Clear(Color(0, 0, 0, 255));
                queue.Sort(camera.Position());
                renderer.Render(queue, camera, stats);
                device.SetRenderTarget(nullptr);
                std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
                target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                return pixels;
            }

            static Color At(const std::vector<Color>& pixels, int x, int y)
            {
                return pixels[static_cast<std::size_t>(y * Size + x)];
            }

            static bool Near(const Color& c, int r, int g, int b, int tolerance = 3)
            {
                return std::abs(c.getRProperty() - r) <= tolerance && std::abs(c.getGProperty() - g) <= tolerance
                       && std::abs(c.getBProperty() - b) <= tolerance;
            }

            GraphicsDevice device;
            RenderTarget2D target;
            GpuMesh quad;
            ForwardRenderer renderer;
            Camera camera;
        };

        Material Unlit(const std::string& name, Vector3 color)
        {
            Material m;
            m.name = name;
            m.shading = ShadingModel::Unlit;
            m.diffuseColor = color;
            return m;
        }
    }

    TEST_F(Fixture, OpaqueThenTransparentBlendsOverIt)
    {
        const Material red = Unlit("red", Vector3(1.0f, 0.0f, 0.0f));
        Material glass = Unlit("glass", Vector3(0.0f, 0.0f, 1.0f));
        glass.blend = BlendMode::AlphaBlend;
        glass.alpha = 0.5f;

        RenderQueue queue;
        // Submitted in the "wrong" order: the queue and renderer fix it.
        queue.Submit(Item(glass, 1.0f, 0.5f));
        queue.Submit(Item(red, 0.0f));
        RenderStatistics stats;
        const auto pixels = Draw(queue, &stats);
        EXPECT_EQ(stats.drawCalls, 2u);
        EXPECT_TRUE(Near(At(pixels, Size / 2, Size / 2), 128, 0, 128, 4)) << "half blue over red";
        EXPECT_TRUE(Near(At(pixels, 2, 2), 255, 0, 0)) << "outside the glass";
        EXPECT_TRUE(renderer.LastFallbacks().empty());
    }

    TEST_F(Fixture, AlphaTestDiscardsBelowCutoff)
    {
        // Left half opaque white, right half transparent.
        Texture2D texture(device, 2, 1);
        const Color texels[2] = {Color(255, 255, 255, 255), Color(255, 255, 255, 0)};
        texture.SetData(texels, 2);
        Material cutout = Unlit("cutout", Vector3(0.0f, 1.0f, 0.0f));
        cutout.blend = BlendMode::AlphaTest;
        cutout.baseTexture = &texture;

        RenderQueue queue;
        queue.Submit(Item(cutout, 0.0f));
        const auto pixels = Draw(queue);
        EXPECT_TRUE(Near(At(pixels, 4, Size / 2), 0, 255, 0)) << "kept texel";
        EXPECT_TRUE(Near(At(pixels, Size - 4, Size / 2), 0, 0, 0)) << "discarded texel";
    }

    TEST_F(Fixture, RenderRestoresDeviceStates)
    {
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.setBlendStateProperty(BlendState::Additive);
        const Material red = Unlit("red", Vector3(1.0f, 0.0f, 0.0f));
        RenderQueue queue;
        queue.Submit(Item(red, 0.0f));
        (void)Draw(queue);
        EXPECT_EQ(device.getRasterizerStateProperty().getCullModeProperty(), CullMode::None);
        EXPECT_EQ(device.getBlendStateProperty().getColorDestinationBlendProperty(),
                  BlendState::Additive.getColorDestinationBlendProperty());
    }

    TEST_F(Fixture, CustomPassesRunAtTheirPointsInOrder)
    {
        std::vector<std::string> calls;
        auto record = [&](const char* name) {
            return [&calls, name](GraphicsDevice&, const Camera&, RenderStatistics&) { calls.emplace_back(name); };
        };
        renderer.AddPass(PassPoint::AfterOverlay, "hud", record("hud"));
        renderer.AddPass(PassPoint::BeforeOpaque, "sky", record("sky"));
        renderer.AddPass(PassPoint::AfterOpaque, "decals", record("decals"));
        renderer.AddPass(PassPoint::AfterOpaque, "outline", record("outline"));
        renderer.AddPass(PassPoint::AfterTransparent, "bloom", record("bloom"));
        RenderQueue queue;
        (void)Draw(queue);
        EXPECT_EQ(calls, (std::vector<std::string>{"sky", "decals", "outline", "bloom", "hud"}));
        EXPECT_EQ(renderer.RemovePass("decals"), 1u);
        calls.clear();
        (void)Draw(queue);
        EXPECT_EQ(calls.size(), 4u);
    }

    TEST_F(Fixture, UnsupportedFeaturesFallBackDeterministically)
    {
        Material lit = Unlit("lit", Vector3(1.0f, 1.0f, 1.0f));
        lit.shading = ShadingModel::Lit; // the quad has no normals
        lit.vertexColors = true;         // ... and no colors
        Material pbr = lit;
        pbr.name = "pbr";
        pbr.shading = ShadingModel::PhysicallyBased;
        RenderQueue queue;
        queue.Submit(Item(lit, 0.0f));
        queue.Submit(Item(lit, 0.1f));
        queue.Submit(Item(pbr, 0.2f));
        const auto pixels = Draw(queue);
        const auto& fallbacks = renderer.LastFallbacks();
        ASSERT_EQ(fallbacks.size(), 4u) << "each distinct fallback is reported once";
        EXPECT_NE(std::find(fallbacks.begin(), fallbacks.end(), "material 'lit': drawn unlit: mesh has no normals"),
                  fallbacks.end());
        EXPECT_NE(std::find(fallbacks.begin(), fallbacks.end(),
                            "material 'lit': vertex colors ignored: mesh has no Color attribute"),
                  fallbacks.end());
        EXPECT_TRUE(Near(At(pixels, Size / 2, Size / 2), 255, 255, 255)) << "still drawn";
    }

    TEST_F(Fixture, WorldBoundsAreConservative)
    {
        const BoundingSphere local = quad.Sphere();
        const BoundingSphere world =
            WorldBounds(quad, Matrix::CreateScale(1.0f, 4.0f, 2.0f) * Matrix::CreateTranslation(10.0f, 0.0f, 0.0f));
        EXPECT_FLOAT_EQ(world.Radius, local.Radius * 4.0f);
        EXPECT_FLOAT_EQ(world.Center.X, 10.0f);
    }

    TEST_F(Fixture, KeyLightCanBeSwitchedOffPerMaterial)
    {
        MeshBuilder builder;
        builder.AddBox(Vector3(-1.0f, -1.0f, -0.1f), Vector3(1.0f, 1.0f, 0.1f));
        const GpuMesh lit = GpuMesh::Upload(device, builder.Mesh(), VertexLayout::PositionNormalTexture());
        renderer.Lighting().defaultLighting = false;
        renderer.Lighting().lightDirection = Vector3(0.0f, 0.0f, -1.0f);
        renderer.Lighting().lightColor = Vector3(1.0f, 1.0f, 1.0f);
        renderer.Lighting().ambientColor = Vector3(0.1f, 0.1f, 0.1f);
        Material sunlit = Unlit("sunlit", Vector3(1.0f, 1.0f, 1.0f));
        sunlit.shading = ShadingModel::Lit;
        Material interior = sunlit;
        interior.name = "interior";
        interior.keyLight = false;

        auto draw = [&](const Material& material) {
            RenderQueue queue;
            RenderItem item;
            item.mesh = &lit;
            item.material = &material;
            item.bounds = WorldBounds(lit, item.world);
            queue.Submit(item);
            return Draw(queue);
        };
        const Color bright = At(draw(sunlit), Size / 2, Size / 2);
        const Color dark = At(draw(interior), Size / 2, Size / 2);
        EXPECT_GT(bright.getRProperty(), 200);
        EXPECT_LT(dark.getRProperty(), 60) << "ambient only";
        EXPECT_TRUE(renderer.LastFallbacks().empty());
        // The next sunlit draw gets its light back.
        EXPECT_EQ(At(draw(sunlit), Size / 2, Size / 2), bright);

        // Fill lights stay on for interiors: only the key light is switched off.
        renderer.Lighting().fillLights = {{Vector3(0.0f, 0.0f, -1.0f), Vector3(0.5f, 0.5f, 0.5f), Vector3::Zero}};
        const Color filled = At(draw(interior), Size / 2, Size / 2);
        EXPECT_GT(filled.getRProperty(), dark.getRProperty() + 60) << "lit by the fill light";
        EXPECT_LT(filled.getRProperty(), bright.getRProperty());
    }

    TEST_F(Fixture, SecondaryTextureUsesDualTextureEffect)
    {
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::TexCoord0, VertexAttribute::TexCoord1});
        builder.AddBox(Vector3(-1.0f, -1.0f, -0.1f), Vector3(1.0f, 1.0f, 0.1f));
        const GpuMesh dual = GpuMesh::Upload(device, builder.Mesh(), VertexLayout::PositionNormalDualTexture(),
                                             GpuMeshOptions{.missingAttributes = MissingAttributePolicy::UseDefaults});
        Texture2D white(device, 1, 1);
        const Color w(255, 255, 255, 255);
        white.SetData(&w, 1);
        Texture2D grey(device, 1, 1);
        const Color g(64, 64, 64, 255);
        grey.SetData(&g, 1);
        Material terrain = Unlit("terrain", Vector3(1.0f, 1.0f, 1.0f));
        terrain.baseTexture = &white;
        terrain.secondaryTexture = &grey;

        RenderQueue queue;
        RenderItem item;
        item.mesh = &dual;
        item.material = &terrain;
        item.bounds = WorldBounds(dual, item.world);
        queue.Submit(item);
        const auto pixels = Draw(queue);
        // XNA's DualTextureEffect modulates 2x: white * 64/255 * 2 is about 128.
        EXPECT_TRUE(Near(At(pixels, Size / 2, Size / 2), 128, 128, 128, 6))
            << static_cast<int>(At(pixels, Size / 2, Size / 2).getRProperty());
        EXPECT_TRUE(renderer.LastFallbacks().empty());

        // Without a second UV set the secondary texture is ignored, and said so.
        RenderQueue plain;
        RenderItem quadItem = Item(terrain, 0.0f);
        plain.Submit(quadItem);
        (void)Draw(plain);
        ASSERT_EQ(renderer.LastFallbacks().size(), 1u);
        EXPECT_EQ(renderer.LastFallbacks()[0], "material 'terrain': secondary texture ignored: mesh has no TexCoord1");
    }

    // Instanced items draw identically on both paths: one hardware draw on
    // HiDef, one draw per instance on Reach, instances relative to the item's
    // world in both.
    TEST(ForwardRendererInstancingTests, InstancedItemsMatchAcrossProfiles)
    {
        auto render = [](Microsoft::Xna::Framework::Graphics::GraphicsProfile profile, RenderStatistics& stats) {
            GraphicsDevice device;
            device.SetGraphicsProfileEXT(profile);
            RenderTarget2D target(device, Size, Size, false, SurfaceFormat::Color, DepthFormat::Depth24);
            MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
            builder.SetColor(Color(255, 64, 0, 255));
            builder.AddBox(Vector3(-0.15f, -0.15f, -0.15f), Vector3(0.15f, 0.15f, 0.15f));
            const GpuMesh box = GpuMesh::Upload(device, builder.Mesh(), VertexLayout::PositionColor());
            InstanceSet instances(&box);
            for (const float x : {-0.6f, 0.0f, 0.6f}) {
                instances.Add(Matrix::CreateTranslation(x, 0.0f, 0.0f));
            }
            Material material;
            material.name = "boxes";
            material.shading = ShadingModel::Unlit;
            material.vertexColors = true;
            RenderItem item;
            item.mesh = &box;
            item.material = &material;
            item.world = Matrix::CreateTranslation(0.0f, 0.4f, 0.0f); // shared base
            item.bounds = Microsoft::Xna::Framework::BoundingSphere(Vector3(0.0f, 0.4f, 0.0f), 1.0f);
            item.instances = &instances;
            RenderQueue queue;
            queue.Submit(item);
            Camera camera;
            camera.SetOrthographic(2.0f, 2.0f, 0.1f, 10.0f);
            camera.SetPosition(Vector3(0.0f, 0.0f, 5.0f));
            ForwardRenderer renderer(device);
            device.SetRenderTarget(&target);
            device.Clear(Color(0, 0, 0, 255));
            renderer.Render(queue, camera, &stats);
            device.SetRenderTarget(nullptr);
            std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
            target.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return pixels;
        };
        RenderStatistics hardware;
        RenderStatistics perInstance;
        const auto hiDef = render(Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef, hardware);
        const auto reach = render(Microsoft::Xna::Framework::Graphics::GraphicsProfile::Reach, perInstance);
        EXPECT_EQ(hardware.drawCalls, 1u);
        EXPECT_EQ(perInstance.drawCalls, 3u);
        EXPECT_EQ(hardware.primitives, perInstance.primitives);
        int lit = 0;
        for (const auto& p : reach) {
            lit += p.getRProperty() > 128 ? 1 : 0;
        }
        EXPECT_GT(lit, 20);
        // Boxes sit 0.4 up (the base) at three x positions.
        const auto at = [&](const std::vector<Color>& pixels, int x, int y) {
            return pixels[static_cast<std::size_t>(y * Size + x)];
        };
        const int y = Size / 2 - static_cast<int>(0.4f * Size / 2.0f);
        EXPECT_GT(at(reach, Size / 2, y).getRProperty(), 128) << "per-instance: base applied";
        EXPECT_EQ(hiDef, reach) << "hardware and per-instance paths agree";
    }

    TEST_F(Fixture, DepthBiasPullsDecalsInFront)
    {
        // A white "marking" a hair behind the grey "road": hidden without
        // bias, visible with the bias car-simulator uses for markings.
        const Material road = Unlit("road", Vector3(0.3f, 0.3f, 0.3f));
        Material marking = Unlit("marking", Vector3(1.0f, 1.0f, 1.0f));
        auto draw = [&](const Material& decal) {
            RenderQueue queue;
            queue.Submit(Item(road, 0.0f));
            RenderItem item = Item(decal, -0.0001f, 0.5f);
            item.order = 1;
            queue.Submit(item);
            return At(Draw(queue), Size / 2, Size / 2);
        };
        EXPECT_TRUE(Near(draw(marking), 77, 77, 77)) << "behind the road";
        marking.depthBias = -0.00002f;
        marking.slopeScaleDepthBias = -1.0f;
        EXPECT_TRUE(Near(draw(marking), 255, 255, 255)) << "biased in front";
        EXPECT_EQ(device.getRasterizerStateProperty().getDepthBiasProperty(), 0.0f) << "state restored";
    }
}
