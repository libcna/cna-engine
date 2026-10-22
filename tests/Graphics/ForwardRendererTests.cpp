// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CubeMapFace.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/CullMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureFilter.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
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

    TEST_F(Fixture, MaterialsSetTheirSamplersAndRenderRestoresTheDevices)
    {
        using Microsoft::Xna::Framework::Graphics::SamplerState;
        using Microsoft::Xna::Framework::Graphics::TextureFilter;
        // Two texels stretched over the quad: at the quad's edge, wrapping
        // blends in the opposite texel, clamping does not.
        Texture2D texture(device, 2, 1);
        const Color texels[2] = {Color(255, 0, 0, 255), Color(0, 0, 255, 255)};
        texture.SetData(texels, 2);
        Material stripes = Unlit("stripes", Vector3(1.0f, 1.0f, 1.0f));
        stripes.baseTexture = &texture;
        RenderQueue queue;
        queue.Submit(Item(stripes, 0.0f));

        // Whoever drew before left point sampling behind.
        device.getSamplerStatesProperty()[0] = SamplerState::PointClamp;
        const Color wrapped = At(Draw(queue), 1, Size / 2);
        EXPECT_GT(wrapped.getRProperty(), 40) << "LinearWrap by default, not the device's PointClamp";
        EXPECT_GT(wrapped.getBProperty(), 40);
        EXPECT_EQ(device.getSamplerStatesProperty()[0].getFilterProperty(), TextureFilter::Point) << "restored";

        stripes.sampler = &SamplerState::LinearClamp;
        const Color clamped = At(Draw(queue), 1, Size / 2);
        EXPECT_LT(std::min(clamped.getRProperty(), clamped.getBProperty()), 5) << "one texel only";
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

    // Baked light keeps (1 - share) and the key light gives the share back
    // as N.L allows: the color as baked where the normal faces the light.
    TEST_F(Fixture, BakedLightKeepsAllButTheKeyLightsShare)
    {
        MeshBuilder builder;
        builder.AddBox(Vector3(-1.0f, -1.0f, -0.1f), Vector3(1.0f, 1.0f, 0.1f));
        const GpuMesh baked = GpuMesh::Upload(device, builder.Mesh(), VertexLayout::PositionNormalColorTexture(),
                                              GpuMeshOptions{.missingAttributes = MissingAttributePolicy::UseDefaults});
        renderer.Lighting().defaultLighting = false;
        renderer.Lighting().lightColor = Vector3(1.0f, 1.0f, 1.0f);
        renderer.Lighting().ambientColor = Vector3(0.1f, 0.1f, 0.1f);
        renderer.Lighting().fillLights = {{Vector3(0.0f, 0.0f, -1.0f), Vector3(0.5f, 0.5f, 0.5f), Vector3::Zero}};
        Material wall = Unlit("wall", Vector3(0.8f, 0.8f, 0.8f));
        wall.vertexColors = true;
        wall.bakedKeyLight = 0.5f;
        Material sunlit = Unlit("sunlit", Vector3(1.0f, 1.0f, 1.0f));
        sunlit.shading = ShadingModel::Lit;

        auto draw = [&](const GpuMesh& mesh, const Material& material) {
            RenderQueue queue;
            RenderItem item;
            item.mesh = &mesh;
            item.material = &material;
            item.bounds = WorldBounds(mesh, item.world);
            queue.Submit(item);
            return At(Draw(queue), Size / 2, Size / 2);
        };
        // Lit through the key light alone: the fill light does not add.
        renderer.Lighting().lightDirection = Vector3(0.0f, 0.0f, -1.0f);
        EXPECT_TRUE(Near(draw(baked, wall), 204, 204, 204)) << "facing the light: as baked";
        renderer.Lighting().lightDirection = Vector3(1.0f, 0.0f, 0.0f);
        EXPECT_TRUE(Near(draw(baked, wall), 102, 102, 102)) << "edge-on: the share is gone";
        EXPECT_TRUE(renderer.LastFallbacks().empty());
        // The next lit draw gets the frame's key light, fill and ambient back.
        renderer.Lighting().lightDirection = Vector3(0.0f, 0.0f, -1.0f);
        const Color bright = draw(baked, sunlit);
        EXPECT_GT(bright.getRProperty(), 250);
        renderer.Lighting().lightDirection = Vector3(1.0f, 0.0f, 0.0f);
        const Color edge = draw(baked, sunlit);
        (void)draw(baked, wall);
        EXPECT_EQ(draw(baked, sunlit), edge) << "fill and ambient restored";
        EXPECT_TRUE(Near(edge, 153, 153, 153)) << "ambient 0.1 + fill 0.5";

        // Without normals the share cannot be lit: drawn unlit, reported.
        Material flat = wall;
        flat.vertexColors = false;
        EXPECT_TRUE(Near(draw(quad, flat), 204, 204, 204));
        ASSERT_EQ(renderer.LastFallbacks().size(), 1u);
        EXPECT_EQ(renderer.LastFallbacks().front(), "material 'wall': drawn unlit: mesh has no normals");
        // A material without the key light keeps its baked color.
        flat.keyLight = false;
        EXPECT_TRUE(Near(draw(baked, flat), 204, 204, 204));
        EXPECT_TRUE(renderer.LastFallbacks().empty());
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
        auto render = [](Microsoft::Xna::Framework::Graphics::GraphicsProfile profile, RenderStatistics& stats,
                         bool plainList = false) {
            GraphicsDevice device;
            device.SetGraphicsProfileEXT(profile);
            RenderTarget2D target(device, Size, Size, false, SurfaceFormat::Color, DepthFormat::Depth24);
            MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
            builder.SetColor(Color(255, 64, 0, 255));
            builder.AddBox(Vector3(-0.15f, -0.15f, -0.15f), Vector3(0.15f, 0.15f, 0.15f));
            GpuMesh box = GpuMesh::Upload(device, builder.Mesh(), VertexLayout::PositionColor());
            // The same triangles as a non-indexed list, which XNA cannot instance.
            std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> list;
            for (const auto index : builder.Mesh().indices) {
                list.emplace_back(builder.Mesh().positions[index], builder.Mesh().colors[index]);
            }
            Microsoft::Xna::Framework::Graphics::VertexBuffer listBuffer(
                device, Microsoft::Xna::Framework::Graphics::VertexPositionColor::getVertexDeclarationStatic(),
                static_cast<int>(list.size()), Microsoft::Xna::Framework::Graphics::BufferUsage::WriteOnly);
            listBuffer.SetData(list.data(), static_cast<int>(list.size()));
            if (plainList) {
                box = GpuMesh::Borrow(listBuffer, static_cast<int>(list.size() / 3), box.Bounds(),
                                      AttributeMask{VertexAttribute::Position, VertexAttribute::Color});
            }
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

        RenderStatistics listStats;
        const auto list = render(Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef, listStats, true);
        EXPECT_EQ(listStats.drawCalls, 3u) << "a non-indexed mesh is drawn once per instance on HiDef too";
        EXPECT_EQ(listStats.primitives, perInstance.primitives);
        EXPECT_EQ(list, reach);
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

    TEST_F(Fixture, TheBinderChoosesTheBlendStateOfBlendedItems)
    {
        // A binder whose effect writes straight alpha would answer
        // NonPremultiplied; this one answers Additive to make it visible.
        class AdditiveBinder final : public IMaterialBinder
        {
        public:
            explicit AdditiveBinder(std::shared_ptr<StockMaterialBinder> stock) : stock_(std::move(stock)) {}
            void BeginFrame(GraphicsDevice& device, const Camera& camera, const ForwardLighting& lighting) override
            {
                stock_->BeginFrame(device, camera, lighting);
            }
            bool Apply(GraphicsDevice& device, const RenderItem& item, RenderBucket bucket, const Camera& camera,
                       const FallbackSink& fallback) override
            {
                return stock_->Apply(device, item, bucket, camera, fallback);
            }
            const BlendState& BlendStateFor(const RenderItem& item) const override
            {
                asked.push_back(item.material->name);
                return BlendState::Additive;
            }
            mutable std::vector<std::string> asked;

        private:
            std::shared_ptr<StockMaterialBinder> stock_;
        };

        const Material red = Unlit("red", Vector3(1.0f, 0.0f, 0.0f));
        Material glass = Unlit("glass", Vector3(0.0f, 0.0f, 1.0f));
        glass.blend = BlendMode::AlphaBlend;
        const Material label = Unlit("label", Vector3(0.0f, 1.0f, 0.0f));
        auto binder = std::make_shared<AdditiveBinder>(renderer.StockBinder());
        renderer.SetMaterialBinder(binder);

        RenderQueue queue;
        queue.Submit(Item(red, 0.0f));
        queue.Submit(Item(glass, 1.0f, 0.5f));
        queue.Submit(Item(label, 2.0f, 0.25f), RenderBucket::Overlay);
        const auto pixels = Draw(queue);
        EXPECT_EQ(binder->asked, (std::vector<std::string>{"glass", "label"})) << "blended buckets only";
        EXPECT_TRUE(Near(At(pixels, Size / 2, Size / 2), 255, 255, 255)) << "red + blue + green, added";
        EXPECT_TRUE(Near(At(pixels, Size / 2 - 6, Size / 2), 255, 0, 255)) << "red + blue";
    }

    TEST_F(Fixture, SequencedItemsDrawInSubmissionOrderWithTheirMaterialsStates)
    {
        // Red, then half-green glass in front of it, then blue between the two:
        // the glass writes no depth, so blue drawn after it covers it.
        class RecordingBinder final : public IMaterialBinder
        {
        public:
            explicit RecordingBinder(std::shared_ptr<StockMaterialBinder> stock) : stock_(std::move(stock)) {}
            void BeginFrame(GraphicsDevice& device, const Camera& camera, const ForwardLighting& lighting) override
            {
                stock_->BeginFrame(device, camera, lighting);
            }
            bool Apply(GraphicsDevice& device, const RenderItem& item, RenderBucket bucket, const Camera& camera,
                       const FallbackSink& fallback) override
            {
                buckets.push_back(bucket);
                return stock_->Apply(device, item, bucket, camera, fallback);
            }
            std::vector<RenderBucket> buckets;

        private:
            std::shared_ptr<StockMaterialBinder> stock_;
        };

        const Material red = Unlit("red", Vector3(1.0f, 0.0f, 0.0f));
        Material glass = Unlit("glass", Vector3(0.0f, 1.0f, 0.0f));
        glass.blend = BlendMode::AlphaBlend;
        glass.alpha = 0.5f;
        const Material blue = Unlit("blue", Vector3(0.0f, 0.0f, 1.0f));
        auto binder = std::make_shared<RecordingBinder>(renderer.StockBinder());
        renderer.SetMaterialBinder(binder);
        const auto submit = [&](RenderQueue& queue, std::optional<RenderBucket> bucket) {
            queue.Submit(Item(red, 0.0f), bucket);
            queue.Submit(Item(glass, 1.0f, 0.5f), bucket);
            queue.Submit(Item(blue, 0.5f, 0.25f), bucket);
        };

        RenderQueue sequenced;
        submit(sequenced, RenderBucket::Sequenced);
        device.setBlendStateProperty(BlendState::Additive);
        const auto pixels = Draw(sequenced);
        EXPECT_TRUE(Near(At(pixels, Size / 2, Size / 2), 0, 0, 255)) << "blue over the glass";
        EXPECT_TRUE(Near(At(pixels, Size / 2 - 6, Size / 2), 128, 128, 0, 4)) << "glass over red";
        EXPECT_EQ(binder->buckets, (std::vector<RenderBucket>{RenderBucket::Opaque, RenderBucket::Transparent,
                                                              RenderBucket::Opaque}))
            << "the binder sees each material's bucket";
        EXPECT_EQ(device.getBlendStateProperty().getColorDestinationBlendProperty(),
                  BlendState::Additive.getColorDestinationBlendProperty())
            << "restored";

        RenderQueue bucketed;
        submit(bucketed, std::nullopt);
        EXPECT_TRUE(Near(At(Draw(bucketed), Size / 2, Size / 2), 0, 128, 128, 4))
            << "by buckets the glass comes last";
    }

    TEST_F(Fixture, CounterClockwiseMaterialsCullTheOtherFaces)
    {
        // The fixture's quad faces the camera with XNA's clockwise winding.
        Material ccw = Unlit("ccw", Vector3(0.0f, 1.0f, 0.0f));
        ccw.frontFace = FrontFace::CounterClockwise;
        RenderQueue queue;
        queue.Submit(Item(ccw, 0.0f));
        EXPECT_TRUE(Near(At(Draw(queue), Size / 2, Size / 2), 0, 0, 0)) << "seen from behind: culled";

        // Seen from the other side the same triangles wind counter-clockwise.
        camera.SetLookAt(Vector3(0.0f, 0.0f, -5.0f), Vector3::Zero, Vector3::Up);
        EXPECT_TRUE(Near(At(Draw(queue), Size / 2, Size / 2), 0, 255, 0)) << "front face: drawn";
        const Material cw = Unlit("cw", Vector3(0.0f, 1.0f, 0.0f));
        RenderQueue plain;
        plain.Submit(Item(cw, 0.0f));
        EXPECT_TRUE(Near(At(Draw(plain), Size / 2, Size / 2), 0, 0, 0)) << "XNA's winding is culled from there";

        camera.SetMirrored(true);
        EXPECT_TRUE(Near(At(Draw(queue), Size / 2, Size / 2), 0, 255, 0)) << "a mirror keeps front faces";
        ccw.depthBias = -0.00002f;
        EXPECT_TRUE(Near(At(Draw(queue), Size / 2, Size / 2), 0, 255, 0)) << "with a depth bias too";
    }

    TEST_F(Fixture, StraightAlphaMaterialsBlendWithTheirAlpha)
    {
        // A texel with straight alpha: its color is not multiplied by its 50 %.
        Texture2D texture(device, 1, 1);
        const Color texel(200, 100, 40, 128);
        texture.SetData(&texel, 1);
        Material pane = Unlit("pane", Vector3(1.0f, 1.0f, 1.0f));
        pane.blend = BlendMode::AlphaBlend;
        pane.baseTexture = &texture;
        RenderQueue queue;
        queue.Submit(Item(pane, 0.0f));

        EXPECT_TRUE(Near(At(Draw(queue), Size / 2, Size / 2), 200, 100, 40)) << "read as premultiplied: too bright";
        pane.straightAlpha = true;
        EXPECT_TRUE(Near(At(Draw(queue), Size / 2, Size / 2), 100, 50, 20)) << "color * alpha over black";
        EXPECT_TRUE(renderer.LastFallbacks().empty());

        pane.alpha = 0.5f;
        (void)Draw(queue);
        ASSERT_EQ(renderer.LastFallbacks().size(), 1u);
        EXPECT_EQ(renderer.LastFallbacks()[0],
                  "material 'pane': straight alpha with alpha < 1: stock effects premultiply it, so it counts twice");
    }

    TEST_F(Fixture, StockEffectsReportPremultipliedTransparency)
    {
        Material pane = Unlit("pane", Vector3(0.0f, 0.0f, 1.0f));
        pane.blend = BlendMode::PremultipliedAlpha;
        RenderQueue queue;
        queue.Submit(Item(pane, 0.0f));
        (void)Draw(queue);
        EXPECT_TRUE(renderer.LastFallbacks().empty()) << "opaque alpha: nothing is lost";

        pane.alpha = 0.5f;
        const auto pixels = Draw(queue);
        ASSERT_EQ(renderer.LastFallbacks().size(), 1u);
        EXPECT_EQ(renderer.LastFallbacks()[0],
                  "material 'pane': premultiplied blending drawn as AlphaBlend (stock effects scale the color by alpha)");
        EXPECT_TRUE(Near(At(pixels, Size / 2, Size / 2), 0, 0, 128, 4)) << "half blue over black";
    }

    TEST_F(Fixture, SkinnedItemsFollowTheirBonePalette)
    {
        // The front face of a box, every vertex bound to bone 0 (the builder's default).
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord0,
                             VertexAttribute::BlendIndices, VertexAttribute::BlendWeights});
        builder.AddBox(Vector3(-1.0f, -1.0f, 0.0f), Vector3(1.0f, 1.0f, 0.0f));
        MeshData data = builder.Take();
        data.indices.resize(6);
        const GpuMesh skinned = GpuMesh::Upload(device, data, VertexLayout::PositionNormalTextureSkinned());
        Material glow;
        glow.name = "glow";
        glow.emissiveColor = Vector3(1.0f, 1.0f, 1.0f);

        const std::vector<Matrix> shifted{Matrix::CreateTranslation(1.0f, 0.0f, 0.0f)};
        auto draw = [&](const GpuMesh& mesh, const std::vector<Matrix>* bones) {
            RenderQueue queue;
            RenderItem item;
            item.mesh = &mesh;
            item.material = &glow;
            item.bounds = BoundingSphere(Vector3::Zero, 3.0f);
            item.bones = bones;
            queue.Submit(item);
            return Draw(queue);
        };
        const auto moved = draw(skinned, &shifted);
        EXPECT_TRUE(Near(At(moved, 4, Size / 2), 0, 0, 0)) << "the bone moved the quad off the left half";
        EXPECT_TRUE(Near(At(moved, Size - 4, Size / 2), 255, 255, 255));
        EXPECT_TRUE(renderer.LastFallbacks().empty());

        // A palette on a rigid mesh is reported and the mesh drawn as it is
        // (the fixture's quad has no normals either, so it is drawn unlit).
        const auto rigid = draw(quad, &shifted);
        ASSERT_EQ(renderer.LastFallbacks().size(), 2u);
        EXPECT_EQ(renderer.LastFallbacks()[0], "material 'glow': bone palette ignored: mesh has no BlendIndices and BlendWeights");
        EXPECT_EQ(renderer.LastFallbacks()[1], "material 'glow': drawn unlit: mesh has no normals");
        EXPECT_FALSE(Near(At(rigid, 4, Size / 2), 0, 0, 0));

        const std::vector<Matrix> tooMany(73, Matrix::getIdentityProperty());
        const auto none = draw(skinned, &tooMany);
        ASSERT_EQ(renderer.LastFallbacks().size(), 1u);
        EXPECT_EQ(renderer.LastFallbacks()[0], "material 'glow': not drawn: more bones than XNA SkinnedEffect's 72");
        EXPECT_TRUE(Near(At(none, Size / 2, Size / 2), 0, 0, 0));
    }

    TEST_F(Fixture, MaterialsCanOptOutOfTheFog)
    {
        // Fog that has fully set in at the quad's distance, colored red.
        auto& lighting = renderer.Lighting();
        lighting.fogEnabled = true;
        lighting.fogColor = Vector3(1.0f, 0.0f, 0.0f);
        lighting.fogStart = 0.0f;
        lighting.fogEnd = 1.0f;
        Material glow = Unlit("glow", Vector3(1.0f, 1.0f, 1.0f));
        auto draw = [&] {
            RenderQueue queue;
            queue.Submit(Item(glow, 0.0f));
            return At(Draw(queue), Size / 2, Size / 2);
        };
        EXPECT_TRUE(Near(draw(), 255, 0, 0)) << "fogged";
        glow.fog = false;
        EXPECT_TRUE(Near(draw(), 255, 255, 255)) << "a glow keeps its color";
    }

    TEST_F(Fixture, BlendedMaterialsCanDiscardBelowTheCutoff)
    {
        // Left texel opaque, right texel 40 % alpha; a half-bright glow added
        // over gray.
        Texture2D texture(device, 2, 1);
        const Color texels[2] = {Color(255, 255, 255, 255), Color(255, 255, 255, 102)};
        texture.SetData(texels, 2);
        const Material gray = Unlit("gray", Vector3(0.25f, 0.25f, 0.25f));
        Material glow = Unlit("glow", Vector3(0.5f, 0.5f, 0.5f));
        glow.blend = BlendMode::Additive;
        glow.baseTexture = &texture;
        glow.sampler = &Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp;
        auto draw = [&] {
            RenderQueue queue;
            queue.Submit(Item(gray, 0.0f));
            queue.Submit(Item(glow, 1.0f));
            return Draw(queue);
        };
        // Additive weights the glow by its alpha: 64 + 128 and 64 + 0.4 * 128.
        const auto blended = draw();
        EXPECT_TRUE(Near(At(blended, 4, Size / 2), 192, 192, 192));
        EXPECT_TRUE(Near(At(blended, Size - 4, Size / 2), 115, 115, 115));

        glow.alphaCutoff = 0.5f;
        glow.discardBelowCutoff = true;
        const auto cut = draw();
        EXPECT_TRUE(Near(At(cut, 4, Size / 2), 192, 192, 192)) << "kept and added";
        EXPECT_TRUE(Near(At(cut, Size - 4, Size / 2), 64, 64, 64)) << "below the cutoff: discarded";
        EXPECT_TRUE(renderer.LastFallbacks().empty()) << "unlit: nothing lost";
    }

    TEST_F(Fixture, MirroredCamerasFlipTheImageAndKeepFrontFaces)
    {
        // Red left half, blue right half; one-sided.
        Texture2D texture(device, 2, 1);
        const Color texels[2] = {Color(255, 0, 0, 255), Color(0, 0, 255, 255)};
        texture.SetData(texels, 2);
        Material halves = Unlit("halves", Vector3(1.0f, 1.0f, 1.0f));
        halves.baseTexture = &texture;
        halves.sampler = &Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp;
        Material marking = Unlit("marking", Vector3(1.0f, 1.0f, 1.0f));
        marking.depthBias = -0.00002f;
        RenderQueue queue;
        queue.Submit(Item(halves, 0.0f));
        RenderItem small = Item(marking, 0.01f, 0.25f);
        queue.Submit(small);

        const auto plain = Draw(queue);
        EXPECT_TRUE(Near(At(plain, 2, Size / 2), 255, 0, 0));
        EXPECT_TRUE(Near(At(plain, Size - 3, Size / 2), 0, 0, 255));

        camera.SetMirrored(true);
        EXPECT_TRUE(camera.Mirrored());
        const auto mirrored = Draw(queue);
        EXPECT_TRUE(Near(At(mirrored, 2, Size / 2), 0, 0, 255)) << "left and right swap";
        EXPECT_TRUE(Near(At(mirrored, Size - 3, Size / 2), 255, 0, 0));
        EXPECT_TRUE(Near(At(mirrored, Size / 2, Size / 2), 255, 255, 255)) << "biased, one-sided items still draw";
        EXPECT_EQ(camera.Frustum().Contains(Vector3(0.9f, 0.0f, 0.0f)),
                  Microsoft::Xna::Framework::ContainmentType::Contains)
            << "the volume is unchanged";
    }

    TEST_F(Fixture, EnvironmentMappedMaterialsMatchXnasEnvironmentMapEffect)
    {
        using Microsoft::Xna::Framework::Graphics::CubeMapFace;
        using Microsoft::Xna::Framework::Graphics::EnvironmentMapEffect;
        using Microsoft::Xna::Framework::Graphics::SamplerState;
        using Microsoft::Xna::Framework::Graphics::TextureCube;
        MeshBuilder builder;
        builder.AddSphere(Vector3::Zero, 0.8f, 24, 16);
        const GpuMesh sphere = GpuMesh::Upload(device, builder.Take(), VertexLayout::PositionNormalTexture());

        // Six differently colored faces; alpha is the glint mask.
        TextureCube sky(device, 4, false, Microsoft::Xna::Framework::Graphics::SurfaceFormat::Color);
        const Color faces[6] = {Color(200, 60, 60, 255), Color(60, 200, 60, 0),   Color(60, 60, 200, 255),
                                Color(200, 200, 60, 0),  Color(60, 200, 200, 255), Color(200, 60, 200, 0)};
        for (int face = 0; face < 6; ++face) {
            const std::vector<Color> texels(16, faces[face]);
            sky.SetData(static_cast<CubeMapFace>(face), texels.data(), 16);
        }
        // car-simulator's paint (VehicleRenderer): a little reflection, a sun glint, Fresnel 2.2.
        Material paint;
        paint.name = "paint";
        paint.diffuseColor = Vector3(0.55f, 0.08f, 0.06f);
        paint.environmentMap = &sky;
        paint.environmentMapAmount = 0.22f;
        paint.environmentMapSpecular = Vector3(0.9f, 0.86f, 0.78f);
        paint.fresnelFactor = 2.2f;

        RenderQueue queue;
        RenderItem item;
        item.mesh = &sphere;
        item.material = &paint;
        item.bounds = WorldBounds(sphere, item.world);
        queue.Submit(item);
        const auto engine = Draw(queue);
        EXPECT_TRUE(renderer.LastFallbacks().empty());

        // The same draw with XNA's effect set up by hand.
        Texture2D white(device, 1, 1);
        const Color one(255, 255, 255, 255);
        white.SetData(&one, 1);
        EnvironmentMapEffect effect(device);
        effect.EnableDefaultLighting();
        effect.setAmbientLightColorProperty(renderer.Lighting().ambientColor);
        effect.setWorldProperty(Matrix::getIdentityProperty());
        effect.setViewProperty(camera.View());
        effect.setProjectionProperty(camera.Projection());
        effect.setDiffuseColorProperty(paint.diffuseColor);
        effect.setTextureProperty(&white);
        effect.setEnvironmentMapProperty(&sky);
        effect.setEnvironmentMapAmountProperty(0.22f);
        effect.setEnvironmentMapSpecularProperty(paint.environmentMapSpecular);
        effect.setFresnelFactorProperty(2.2f);
        effect.setFogEnabledProperty(false);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        device.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
        device.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        device.getSamplerStatesProperty()[1] = SamplerState::LinearWrap;
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        sphere.Draw(device);
        device.SetRenderTarget(nullptr);
        std::vector<Color> direct(static_cast<std::size_t>(Size * Size));
        target.GetData(direct.data(), static_cast<int>(direct.size()));

        int differing = 0;
        for (std::size_t i = 0; i < direct.size(); ++i) {
            differing += direct[i] != engine[i] ? 1 : 0;
        }
        EXPECT_EQ(differing, 0) << "pixel-identical to the hand-configured effect";
        EXPECT_NE(At(engine, Size / 2, Size / 2), Color(0, 0, 0, 255));

        // On a mesh without normals the surface is unlit and the map is reported.
        const Material& reflective = paint;
        RenderQueue unlitQueue;
        unlitQueue.Submit(Item(reflective, 0.0f));
        (void)Draw(unlitQueue);
        ASSERT_EQ(renderer.LastFallbacks().size(), 2u);
        EXPECT_EQ(renderer.LastFallbacks()[1], "material 'paint': environment map ignored: the surface is drawn unlit");
    }
}
