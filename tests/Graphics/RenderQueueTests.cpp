// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        // The queue never touches the mesh, so an empty one is enough here.
        const GpuMesh& Mesh()
        {
            static const GpuMesh mesh;
            return mesh;
        }

        RenderItem Item(const Material& material, float z, std::int32_t order = 0)
        {
            RenderItem item;
            item.mesh = &Mesh();
            item.material = &material;
            item.bounds = BoundingSphere(Vector3(0.0f, 0.0f, z), 1.0f);
            item.order = order;
            return item;
        }

        std::vector<float> Depths(std::span<const RenderItem> items)
        {
            std::vector<float> z;
            for (const auto& item : items) {
                z.push_back(item.bounds.Center.Z);
            }
            return z;
        }
    }

    TEST(RenderQueueTests, MaterialsChooseBuckets)
    {
        Material opaque;
        Material cutout;
        cutout.blend = BlendMode::AlphaTest;
        Material glass;
        glass.blend = BlendMode::AlphaBlend;
        Material glow;
        glow.blend = BlendMode::Additive;
        EXPECT_EQ(BucketFor(opaque), RenderBucket::Opaque);
        EXPECT_EQ(BucketFor(cutout), RenderBucket::AlphaTested);
        EXPECT_EQ(BucketFor(glass), RenderBucket::Transparent);
        EXPECT_EQ(BucketFor(glow), RenderBucket::Transparent);

        RenderQueue queue;
        queue.Submit(Item(opaque, 0.0f));
        queue.Submit(Item(glass, 0.0f));
        queue.Submit(Item(glass, 0.0f), RenderBucket::Overlay);
        EXPECT_EQ(queue.Count(), 3u);
        EXPECT_EQ(queue.Items(RenderBucket::Overlay).size(), 1u);
        EXPECT_THROW(queue.Submit(RenderItem{}), std::invalid_argument);
        queue.Clear();
        EXPECT_EQ(queue.Count(), 0u);
    }

    TEST(RenderQueueTests, OpaqueGroupsByMaterialThenFrontToBack)
    {
        Material a;
        Material b;
        RenderQueue queue;
        queue.Submit(Item(b, -5.0f));
        queue.Submit(Item(a, -9.0f));
        queue.Submit(Item(b, -1.0f));
        queue.Submit(Item(a, -3.0f));
        queue.Submit(Item(a, -20.0f, -1)); // explicit order wins
        queue.Sort(Vector3::Zero);
        const auto items = queue.Items(RenderBucket::Opaque);
        EXPECT_EQ(Depths(items), (std::vector<float>{-20.0f, -1.0f, -5.0f, -3.0f, -9.0f}));
        EXPECT_EQ(items[1].material, &b) << "material b was submitted first";
    }

    TEST(RenderQueueTests, TransparentDrawsBackToFrontWithStableTies)
    {
        Material glass;
        glass.blend = BlendMode::AlphaBlend;
        RenderQueue queue;
        RenderItem first = Item(glass, -4.0f);
        first.world = Matrix::CreateTranslation(1.0f, 0.0f, 0.0f);
        queue.Submit(first);
        queue.Submit(Item(glass, -2.0f));
        queue.Submit(Item(glass, -8.0f));
        queue.Submit(Item(glass, -4.0f));
        queue.Sort(Vector3::Zero);
        const auto items = queue.Items(RenderBucket::Transparent);
        EXPECT_EQ(Depths(items), (std::vector<float>{-8.0f, -4.0f, -4.0f, -2.0f}));
        EXPECT_EQ(items[1].world.M41, 1.0f) << "equal distances keep submission order";
    }

    TEST(RenderQueueTests, OverlayKeepsSubmissionOrder)
    {
        Material ui;
        RenderQueue queue;
        queue.Submit(Item(ui, -9.0f), RenderBucket::Overlay);
        queue.Submit(Item(ui, -1.0f), RenderBucket::Overlay);
        queue.Submit(Item(ui, -5.0f, -1), RenderBucket::Overlay);
        queue.Sort(Vector3::Zero);
        EXPECT_EQ(Depths(queue.Items(RenderBucket::Overlay)), (std::vector<float>{-5.0f, -9.0f, -1.0f}));
    }

    TEST(RenderQueueTests, SequencedKeepsSubmissionOrderAcrossMaterials)
    {
        Material opaque;
        Material glass;
        glass.blend = BlendMode::AlphaBlend;
        RenderQueue queue;
        queue.Submit(Item(opaque, -9.0f), RenderBucket::Sequenced);
        queue.Submit(Item(glass, -1.0f), RenderBucket::Sequenced);
        queue.Submit(Item(opaque, -5.0f), RenderBucket::Sequenced);
        queue.Submit(Item(glass, -7.0f, -1), RenderBucket::Sequenced); // explicit order wins
        queue.Submit(Item(opaque, -3.0f));
        queue.Sort(Vector3::Zero);
        EXPECT_EQ(Depths(queue.Items(RenderBucket::Sequenced)), (std::vector<float>{-7.0f, -9.0f, -1.0f, -5.0f}));
        EXPECT_EQ(queue.Items(RenderBucket::Opaque).size(), 1u) << "only explicit submissions are sequenced";
    }
}
