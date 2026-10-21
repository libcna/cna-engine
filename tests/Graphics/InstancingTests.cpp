// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Instancing.hpp"

#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;

    namespace
    {
        constexpr int Size = 48;

        GpuMesh SmallBox(GraphicsDevice& device)
        {
            MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
            builder.SetColor(Color(255, 255, 0, 255));
            builder.AddBox(Vector3(-0.2f, -0.2f, -0.2f), Vector3(0.2f, 0.2f, 0.2f));
            return GpuMesh::Upload(device, builder.Mesh(), VertexLayout::PositionColor());
        }

        int CountLit(const std::vector<Color>& pixels)
        {
            int lit = 0;
            for (const auto& p : pixels) {
                lit += p.getRProperty() > 128 ? 1 : 0;
            }
            return lit;
        }
    }

    TEST(InstancingTests, PathSelectionFollowsProfileAndEffect)
    {
        GraphicsDevice device;
        EXPECT_EQ(InstanceSet::ChoosePath(device, true), InstancingPath::PerInstance) << "Reach";
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        EXPECT_EQ(InstanceSet::ChoosePath(device, true), InstancingPath::Hardware);
        EXPECT_EQ(InstanceSet::ChoosePath(device, false), InstancingPath::PerInstance);
        EXPECT_EQ(InstanceSet::InstanceDeclaration().getVertexStrideProperty(), 64);
    }

    TEST(InstancingTests, CullingSelectsVisibleInstances)
    {
        GraphicsDevice device;
        const GpuMesh box = SmallBox(device);
        InstanceSet set(&box);
        for (const float x : {0.0f, 1.0f, -1.0f, 100.0f}) {
            set.Add(Matrix::CreateTranslation(x, 0.0f, -5.0f));
        }
        EXPECT_EQ(set.VisibleCount(), 4u) << "without culling every instance is drawn";
        Camera camera;
        camera.SetPerspective(MathHelper::PiOver4, 1.0f, 0.1f, 100.0f);
        CullStatistics stats;
        set.Cull(camera, VisibilitySettings{}, stats);
        EXPECT_EQ(set.VisibleCount(), 3u);
        EXPECT_EQ(stats.byFrustum, 1u);
        set.UseAll();
        EXPECT_EQ(set.VisibleCount(), 4u);
    }

    TEST(InstancingTests, PerInstanceFallbackDrawsEveryVisibleInstance)
    {
        GraphicsDevice device;
        const GpuMesh box = SmallBox(device);
        InstanceSet set(&box);
        for (int i = 0; i < 3; ++i) {
            set.Add(Matrix::CreateTranslation(-0.6f + 0.6f * static_cast<float>(i), 0.0f, 0.0f));
        }
        RenderTarget2D target(device, Size, Size);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        device.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
        BasicEffect effect(device);
        effect.setVertexColorEnabledProperty(true);
        effect.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 3.0f), Vector3::Zero, Vector3::Up));
        effect.setProjectionProperty(Matrix::CreateOrthographic(2.0f, 2.0f, 0.1f, 10.0f));
        RenderStatistics stats;
        const auto drawn = set.DrawPerInstance(
            device, effect, [&] { effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply(); }, &stats);
        device.SetRenderTarget(nullptr);
        EXPECT_EQ(drawn, 3u);
        EXPECT_EQ(stats.drawCalls, 3u);
        EXPECT_EQ(effect.getWorldProperty(), Matrix::getIdentityProperty()) << "World restored";
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        // Three separate boxes along the middle row.
        const auto row = [&](int x) { return pixels[static_cast<std::size_t>(Size / 2 * Size + x)].getRProperty(); };
        EXPECT_GT(row(Size / 2), 128);
        EXPECT_GT(row(Size / 2 - static_cast<int>(0.3f * Size)), 128);
        EXPECT_GT(row(Size / 2 + static_cast<int>(0.3f * Size)), 128);
        EXPECT_LT(row(Size / 2 - static_cast<int>(0.15f * Size)), 128) << "gap between boxes";
    }

    TEST(InstancingTests, HardwarePathIssuesOneDrawForAllInstances)
    {
        GraphicsDevice device;
        const GpuMesh box = SmallBox(device);
        InstanceSet set(&box);
        for (int i = 0; i < 5; ++i) {
            set.Add(Matrix::CreateTranslation(static_cast<float>(i), 0.0f, 0.0f));
        }
        EXPECT_THROW(set.DrawHardware(device), std::logic_error) << "Reach has no instancing";

        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        RenderTarget2D target(device, Size, Size);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        // Instances 1..4 are beyond the right edge; this checks buffers,
        // bindings and counts (StockEffectsReadTheInstanceStream checks placement).
        BasicEffect effect(device);
        effect.setVertexColorEnabledProperty(true);
        effect.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 3.0f), Vector3::Zero, Vector3::Up));
        effect.setProjectionProperty(Matrix::CreateOrthographic(2.0f, 2.0f, 0.1f, 10.0f));
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        RenderStatistics stats;
        EXPECT_TRUE(set.DrawHardware(device, &stats));
        device.SetRenderTarget(nullptr);
        EXPECT_EQ(stats.drawCalls, 1u);
        EXPECT_EQ(stats.primitives, 5u * 12u);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        EXPECT_GT(CountLit(pixels), 0);

        // Updating and drawing again must work: the instance stream was
        // unbound after the first draw, so the upload is allowed.
        set.Set(0, Matrix::CreateTranslation(0.5f, 0.0f, 0.0f));
        RenderStatistics again;
        EXPECT_TRUE(set.DrawHardware(device, &again));
        EXPECT_EQ(again.drawCalls, 1u);

        InstanceSet empty(&box);
        EXPECT_FALSE(empty.DrawHardware(device));

        // XNA's DrawInstancedPrimitives reads indices: a plain list cannot be instanced.
        const GpuMesh list = GpuMesh::Borrow(*box.Vertices(), 8, box.Bounds(),
                                             AttributeMask{VertexAttribute::Position, VertexAttribute::Color});
        InstanceSet plain(&list);
        plain.Add(Matrix::getIdentityProperty());
        EXPECT_THROW(plain.DrawHardware(device), std::logic_error);
    }

    TEST(InstancingTests, StockEffectsReadTheInstanceStream)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        const GpuMesh box = SmallBox(device);
        InstanceSet set(&box);
        for (const float x : {-0.6f, 0.0f, 0.6f}) {
            set.Add(Matrix::CreateTranslation(x, 0.0f, 0.0f));
        }
        RenderTarget2D target(device, Size, Size);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        BasicEffect effect(device);
        effect.setVertexColorEnabledProperty(true);
        effect.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 3.0f), Vector3::Zero, Vector3::Up));
        effect.setProjectionProperty(Matrix::CreateOrthographic(2.0f, 2.0f, 0.1f, 10.0f));
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        ASSERT_TRUE(set.DrawHardware(device));
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        const auto row = [&](int x) { return pixels[static_cast<std::size_t>(Size / 2 * Size + x)].getRProperty(); };
        // Three separate boxes: CNA applies the per-instance world (TextureCoordinate 1-4).
        EXPECT_GT(row(Size / 2), 128);
        EXPECT_GT(row(Size / 2 - static_cast<int>(0.3f * Size)), 128);
        EXPECT_GT(row(Size / 2 + static_cast<int>(0.3f * Size)), 128);
        EXPECT_LT(row(Size / 2 - static_cast<int>(0.15f * Size)), 128) << "gap between boxes";
    }
}
