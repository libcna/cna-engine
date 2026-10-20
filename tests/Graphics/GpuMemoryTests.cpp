// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/DiagnosticsReport.hpp"
#include "CnaEngine/Graphics/GpuMemory.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    TEST(GpuMemoryTests, TextureBytesCountEveryLevelAtItsFormat)
    {
        GraphicsDevice device;
        const Texture2D plain(device, 8, 4);
        EXPECT_EQ(TextureBytes(plain), 8u * 4u * 4u);
        const Texture2D mipmapped(device, 8, 4, true, SurfaceFormat::Color);
        // 8x4 + 4x2 + 2x1 + 1x1 pixels.
        EXPECT_EQ(TextureBytes(mipmapped), (32u + 8u + 2u + 1u) * 4u);
        const Texture2D packed(device, 16, 16, false, SurfaceFormat::Bgr565);
        bool exact = false;
        EXPECT_EQ(TextureBytes(packed, &exact), 512u);
        EXPECT_TRUE(exact);
    }

    TEST(GpuMemoryTests, SharedResourcesAreCountedOnce)
    {
        GraphicsDevice device;
        MeshBuilder builder;
        builder.AddBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
        const MeshData box = builder.Take();
        const GpuMesh owned = GpuMesh::Upload(device, box, VertexLayout::PositionNormalTexture());
        // A second mesh over the same buffers (as model parts share them).
        const GpuMesh borrowed = GpuMesh::Borrow(*owned.Vertices(), *owned.Indices(), owned.VertexCount(),
                                                 owned.PrimitiveCount(), owned.Bounds());

        Texture2D texture(device, 4, 4);
        Material a;
        a.baseTexture = &texture;
        Material b = a;
        b.secondaryTexture = &texture;

        GpuMemoryEstimate estimate;
        estimate.Add(owned);
        estimate.Add(borrowed);
        estimate.Add(a);
        estimate.Add(b);
        EXPECT_EQ(estimate.VertexBuffers(), 1u);
        EXPECT_EQ(estimate.IndexBuffers(), 1u);
        EXPECT_EQ(estimate.VertexBytes(), box.VertexCount() * 32u) << "PositionNormalTexture is 32 bytes";
        EXPECT_EQ(estimate.IndexBytes(), box.indices.size() * 2u) << "16-bit indices";
        EXPECT_EQ(estimate.Textures(), 1u);
        EXPECT_EQ(estimate.TextureBytes(), 64u);
        EXPECT_EQ(estimate.TotalBytes(), estimate.VertexBytes() + estimate.IndexBytes() + 64u);

        // A queue reaches the same resources through its items.
        RenderQueue queue;
        RenderItem item;
        item.mesh = &borrowed;
        item.material = &b;
        queue.Submit(item);
        GpuMemoryEstimate fromQueue;
        fromQueue.Add(queue);
        EXPECT_EQ(fromQueue.TotalBytes(), estimate.TotalBytes());

        DiagnosticsSources sources;
        sources.memoryEstimate = &estimate;
        sources.includeGpuMemory = false;
        const auto lines = FormatDiagnostics(sources);
        ASSERT_EQ(lines.size(), 1u);
        EXPECT_EQ(lines[0].rfind("gpu estimate 0.0 MiB: vertex 0.0 MiB (1 buffers), index 0.0 MiB (1), textures 0.0 MiB (1)", 0), 0u)
            << lines[0];

        estimate.Clear();
        EXPECT_EQ(estimate.TotalBytes(), 0u);
        estimate.Add(owned);
        EXPECT_EQ(estimate.VertexBuffers(), 1u) << "Clear forgets what was seen";
    }
}
