// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/GpuMesh.hpp"

#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::IndexElementSize;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;

    namespace
    {
        constexpr int Size = 64;

        // A red quad covering the middle of the view, facing the camera.
        MeshData RedQuad()
        {
            MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
            builder.SetColor(Color(255, 0, 0, 255));
            builder.AddBox(Vector3(-0.5f, -0.5f, -0.01f), Vector3(0.5f, 0.5f, 0.01f));
            return builder.Take();
        }

        std::vector<Color> Render(GraphicsDevice& device, const GpuMesh& mesh, RenderStatistics* stats,
                                  bool* drawn = nullptr)
        {
            RenderTarget2D target(device, Size, Size);
            device.SetRenderTarget(&target);
            device.Clear(Color(0, 0, 0, 255));
            device.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);

            BasicEffect effect(device);
            effect.setVertexColorEnabledProperty(true);
            effect.setWorldProperty(Matrix::getIdentityProperty());
            effect.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 2.0f), Vector3::Zero, Vector3::Up));
            effect.setProjectionProperty(Matrix::CreateOrthographic(2.0f, 2.0f, 0.1f, 10.0f));
            effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
            const bool result = mesh.Draw(device, stats);
            if (drawn != nullptr) {
                *drawn = result;
            }

            device.SetRenderTarget(nullptr);
            std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
            target.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return pixels;
        }

        Color At(const std::vector<Color>& pixels, int x, int y)
        {
            return pixels[static_cast<std::size_t>(y * Size + x)];
        }
    }

    TEST(GpuMeshTests, UploadsAndDrawsWithStatistics)
    {
        GraphicsDevice device;
        const auto memoryBefore = CurrentGpuMemory();
        {
            const GpuMesh mesh = GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor());
            EXPECT_TRUE(mesh.IsDrawable());
            EXPECT_TRUE(mesh.OwnsBuffers());
            EXPECT_EQ(mesh.VertexCount(), 24);
            EXPECT_EQ(mesh.PrimitiveCount(), 12);
            EXPECT_EQ(mesh.IndexSize(), IndexElementSize::SixteenBits);
            EXPECT_EQ(mesh.VertexBytes(), 24 * 16);
            EXPECT_EQ(mesh.IndexBytes(), 36 * 2);
            EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes + 1);
            EXPECT_EQ(CurrentGpuMemory().vertexBytes, memoryBefore.vertexBytes + 24 * 16);

            RenderStatistics stats;
            const auto pixels = Render(device, mesh, &stats);
            EXPECT_EQ(stats.drawCalls, 1u);
            EXPECT_EQ(stats.primitives, 12u);
            EXPECT_EQ(stats.vertices, 24u);
            EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255)) << "quad covers the centre";
            EXPECT_EQ(At(pixels, 2, 2), Color(0, 0, 0, 255)) << "corner stays clear";
        }
        EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes) << "destructor releases the tally";
        EXPECT_EQ(CurrentGpuMemory().vertexBytes, memoryBefore.vertexBytes);
    }

    TEST(GpuMeshTests, ThirtyTwoBitIndicesWhenRequestedOrNeeded)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef); // test setup: Reach has no 32-bit indices
        GpuMeshOptions options;
        options.preferSixteenBitIndices = false;
        const GpuMesh mesh = GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor(), options);
        EXPECT_EQ(mesh.IndexSize(), IndexElementSize::ThirtyTwoBits);
        EXPECT_EQ(mesh.IndexBytes(), 36 * 4);
        const auto pixels = Render(device, mesh, nullptr);
        EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255));

        MeshBuilder big({VertexAttribute::Position});
        big.AddGrid(Vector3::Zero, 1.0f, 1.0f, 300, 300); // 301 * 301 > 65535 vertices
        const GpuMesh large = GpuMesh::Upload(device, big.Mesh(), VertexLayout::PositionColor(),
                                              GpuMeshOptions{.missingAttributes = MissingAttributePolicy::UseDefaults});
        EXPECT_EQ(large.IndexSize(), IndexElementSize::ThirtyTwoBits);
        EXPECT_TRUE(large.ConversionReport().missing.Has(VertexAttribute::Color));
    }

    TEST(GpuMeshTests, ReachProfileRejectsThirtyTwoBitIndicesUpFront)
    {
        GraphicsDevice device;
        ASSERT_EQ(device.getGraphicsProfileProperty(), GraphicsProfile::Reach);
        const auto memoryBefore = CurrentGpuMemory();
        MeshBuilder big({VertexAttribute::Position, VertexAttribute::Color});
        big.AddGrid(Vector3::Zero, 1.0f, 1.0f, 300, 300);
        EXPECT_THROW((void)GpuMesh::Upload(device, big.Mesh(), VertexLayout::PositionColor()), std::invalid_argument);
        GpuMeshOptions options;
        options.preferSixteenBitIndices = false;
        EXPECT_THROW((void)GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor(), options),
                     std::invalid_argument);
        EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes);
    }

    TEST(GpuMeshTests, RejectsInvalidInputBeforeAllocating)
    {
        GraphicsDevice device;
        const auto memoryBefore = CurrentGpuMemory();
        EXPECT_THROW((void)GpuMesh::Upload(device, MeshData{}, VertexLayout::PositionColor()), std::invalid_argument);
        MeshData noColor = RedQuad();
        noColor.colors.clear();
        EXPECT_THROW((void)GpuMesh::Upload(device, noColor, VertexLayout::PositionColor()), std::invalid_argument);
        MeshData broken = RedQuad();
        broken.indices.push_back(1000);
        broken.indices.push_back(0);
        broken.indices.push_back(1);
        EXPECT_THROW((void)GpuMesh::Upload(device, broken, VertexLayout::PositionColor()), std::invalid_argument);
        EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes);
    }

    TEST(GpuMeshTests, MoveTransfersOwnershipAndResetReleases)
    {
        GraphicsDevice device;
        const auto memoryBefore = CurrentGpuMemory();
        GpuMesh a = GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor());
        GpuMesh b = std::move(a);
        EXPECT_FALSE(a.IsDrawable()); // NOLINT(bugprone-use-after-move): moved-from state is specified
        EXPECT_TRUE(b.IsDrawable());
        EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes + 1);
        b.Reset();
        EXPECT_FALSE(b.IsDrawable());
        EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes);

        bool drawn = true;
        RenderStatistics stats;
        (void)Render(device, b, &stats, &drawn);
        EXPECT_FALSE(drawn) << "an empty mesh draws nothing";
        EXPECT_EQ(stats.drawCalls, 0u);
    }

    TEST(GpuMeshTests, BorrowedBuffersAreNotReleasedOrCounted)
    {
        GraphicsDevice device;
        GpuMesh owner = GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor());
        const auto memoryBefore = CurrentGpuMemory();
        {
            const GpuMesh view = GpuMesh::Borrow(*owner.Vertices(), *owner.Indices(), owner.VertexCount(),
                                                 owner.PrimitiveCount(), owner.Bounds());
            EXPECT_FALSE(view.OwnsBuffers());
            EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes);
            const auto pixels = Render(device, view, nullptr);
            EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255));
        }
        EXPECT_TRUE(owner.IsDrawable()) << "dropping the borrowed view leaves the owner intact";
        EXPECT_THROW((void)GpuMesh::Borrow(*owner.Vertices(), *owner.Indices(), 0, 1, BoundingBox()),
                     std::invalid_argument);
    }

    TEST(GpuMeshTests, BorrowedNonIndexedListsDrawFromTheirStartVertex)
    {
        using Microsoft::Xna::Framework::Graphics::VertexPositionColor;
        GraphicsDevice device;
        // Two quads as plain triangle lists: a blue one off screen, then the
        // red one, wound clockwise as seen from the camera (XNA front faces).
        std::vector<VertexPositionColor> vertices;
        const auto quad = [&](float x0, float y0, float x1, float y1, Color color) {
            const std::pair<float, float> corners[] = {{x0, y1}, {x1, y1}, {x1, y0}, {x0, y1}, {x1, y0}, {x0, y0}};
            for (const auto& [x, y] : corners) {
                vertices.emplace_back(Vector3(x, y, 0.0f), color);
            }
        };
        quad(5.0f, 5.0f, 6.0f, 6.0f, Color(0, 0, 255, 255));
        quad(-0.5f, -0.5f, 0.5f, 0.5f, Color(255, 0, 0, 255));
        Microsoft::Xna::Framework::Graphics::VertexBuffer buffer(
            device, VertexPositionColor::getVertexDeclarationStatic(), static_cast<int>(vertices.size()),
            Microsoft::Xna::Framework::Graphics::BufferUsage::WriteOnly);
        buffer.SetData(vertices.data(), static_cast<int>(vertices.size()));

        const auto memoryBefore = CurrentGpuMemory();
        const GpuMesh red = GpuMesh::Borrow(buffer, 2, BoundingBox(Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f)),
                                            AttributeMask{VertexAttribute::Position, VertexAttribute::Color}, 6);
        EXPECT_FALSE(red.Indexed());
        EXPECT_TRUE(red.IsDrawable());
        EXPECT_FALSE(red.OwnsBuffers());
        EXPECT_EQ(red.VertexCount(), 6);
        EXPECT_EQ(red.BaseVertex(), 6);
        EXPECT_EQ(CurrentGpuMemory().meshes, memoryBefore.meshes);
        RenderStatistics stats;
        const auto pixels = Render(device, red, &stats);
        EXPECT_EQ(stats.drawCalls, 1u);
        EXPECT_EQ(stats.primitives, 2u);
        EXPECT_EQ(stats.vertices, 6u);
        EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255)) << "drawn from the start vertex";
        EXPECT_EQ(At(pixels, 2, 2), Color(0, 0, 0, 255));

        EXPECT_THROW((void)GpuMesh::Borrow(buffer, 0, BoundingBox()), std::invalid_argument);
        EXPECT_THROW((void)GpuMesh::Borrow(buffer, 1, BoundingBox(), AttributeMask{VertexAttribute::Position}, -1),
                     std::invalid_argument);
        buffer.Dispose();
        EXPECT_FALSE(red.IsDrawable());
    }

    TEST(GpuMeshTests, DisposedBuffersAreNotDrawn)
    {
        GraphicsDevice device;
        GpuMesh mesh = GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor());
        mesh.Vertices()->Dispose();
        EXPECT_FALSE(mesh.IsDrawable());
        bool drawn = true;
        const auto pixels = Render(device, mesh, nullptr, &drawn);
        EXPECT_FALSE(drawn);
        EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(0, 0, 0, 255));
    }

    TEST(GpuMeshTests, TangentLayoutUploads)
    {
        GraphicsDevice device;
        MeshBuilder builder;
        builder.AddSphere(Vector3::Zero, 0.5f, 12, 8);
        MeshData sphere = builder.Take();
        sphere.ComputeTangents();
        const GpuMesh mesh = GpuMesh::Upload(device, sphere, VertexLayout::PositionNormalTangentTexture());
        EXPECT_EQ(mesh.VertexBytes(), static_cast<std::int64_t>(sphere.VertexCount()) * 48);
        EXPECT_TRUE(mesh.ConversionReport().missing.Empty());
        EXPECT_TRUE(mesh.ConversionReport().dropped.Empty());
    }

    TEST(GpuMeshTests, BorrowedModelPartsHonourOffsets)
    {
        GraphicsDevice device;
        // Two quads in one buffer: a blue one off screen, then the red one.
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
        builder.SetColor(Color(0, 0, 255, 255));
        builder.AddBox(Vector3(5.0f, 5.0f, -0.01f), Vector3(6.0f, 6.0f, 0.01f));
        builder.SetColor(Color(255, 0, 0, 255));
        builder.AddBox(Vector3(-0.5f, -0.5f, -0.01f), Vector3(0.5f, 0.5f, 0.01f));
        MeshData both = builder.Take();
        // Make the second box's indices local to its own vertices, as a
        // content-pipeline part would store them.
        for (std::size_t i = 36; i < both.indices.size(); ++i) {
            both.indices[i] -= 24;
        }
        // Upload cannot validate local indices; build the buffers through a
        // valid mesh and reuse them.
        MeshData valid = both;
        for (std::size_t i = 36; i < valid.indices.size(); ++i) {
            valid.indices[i] += 24;
        }
        const GpuMesh owner = GpuMesh::Upload(device, valid, VertexLayout::PositionColor());
        // Rewrite the second half of the index buffer with part-local indices.
        std::vector<std::uint16_t> local(both.indices.begin(), both.indices.end());
        owner.Indices()->SetData(local.data(), static_cast<int>(local.size()));

        const Microsoft::Xna::Framework::Graphics::ModelMeshPart part(owner.Vertices(), owner.Indices(), 24, 12, 36, 24);
        const GpuMesh redOnly = GpuMesh::Borrow(part, BoundingBox(), AttributeMask{VertexAttribute::Position,
                                                                                  VertexAttribute::Color});
        EXPECT_EQ(redOnly.BaseVertex(), 24);
        EXPECT_EQ(redOnly.StartIndex(), 36);
        const auto pixels = Render(device, redOnly, nullptr);
        EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255));
    }

    TEST(GpuMeshTests, DrawWithAnEffectAppliesItsPasses)
    {
        GraphicsDevice device;
        const GpuMesh mesh = GpuMesh::Upload(device, RedQuad(), VertexLayout::PositionColor());
        RenderTarget2D target(device, Size, Size);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        BasicEffect effect(device);
        effect.setVertexColorEnabledProperty(true);
        effect.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 2.0f), Vector3::Zero, Vector3::Up));
        effect.setProjectionProperty(Matrix::CreateOrthographic(2.0f, 2.0f, 0.1f, 10.0f));
        RenderStatistics stats;
        EXPECT_TRUE(mesh.Draw(device, effect, &stats));
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        EXPECT_EQ(At(pixels, Size / 2, Size / 2), Color(255, 0, 0, 255));
        EXPECT_EQ(stats.drawCalls, 1u) << "BasicEffect has one pass";
        GpuMesh empty;
        EXPECT_FALSE(empty.Draw(device, effect));
    }
}

