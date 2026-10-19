// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Visibility.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        Camera LookingDownNegativeZ()
        {
            Camera camera;
            camera.SetPerspective(MathHelper::PiOver2, 1.0f, 0.1f, 100.0f);
            return camera;
        }
    }

    TEST(VisibilityTests, PoliciesAreIndependentAndConservative)
    {
        const Camera camera = LookingDownNegativeZ();
        VisibilitySettings settings;
        const BoundingSphere ahead(Vector3(0.0f, 0.0f, -10.0f), 1.0f);
        const BoundingSphere behind(Vector3(0.0f, 0.0f, 10.0f), 1.0f);
        const BoundingSphere straddling(Vector3(0.0f, 0.0f, 0.5f), 1.0f);

        EXPECT_EQ(Classify(ahead, 1, camera, settings), Visibility::Visible);
        EXPECT_EQ(Classify(behind, 1, camera, settings), Visibility::RejectedByFrustum);
        EXPECT_EQ(Classify(straddling, 1, camera, settings), Visibility::Visible) << "partially inside is visible";

        settings.frustum = false;
        EXPECT_EQ(Classify(behind, 1, camera, settings), Visibility::Visible);

        settings.maxDistance = 9.5f;
        EXPECT_EQ(Classify(ahead, 1, camera, settings), Visibility::Visible) << "the near edge is at 9";
        settings.maxDistance = 8.5f;
        EXPECT_EQ(Classify(ahead, 1, camera, settings), Visibility::RejectedByDistance);

        settings.layerMask = 0b10;
        EXPECT_EQ(Classify(ahead, 0b01, camera, settings), Visibility::RejectedByLayer) << "layer is tested first";
    }

    TEST(VisibilityTests, CullIntoSubmitsVisibleItemsAndCounts)
    {
        const Camera camera = LookingDownNegativeZ();
        const GpuMesh mesh;
        Material material;
        std::vector<RenderItem> candidates;
        for (const float z : {-5.0f, 5.0f, -50.0f, -150.0f}) {
            RenderItem item;
            item.mesh = &mesh;
            item.material = &material;
            item.bounds = BoundingSphere(Vector3(0.0f, 0.0f, z), 1.0f);
            candidates.push_back(item);
        }
        candidates[2].layers = 0b100;
        VisibilitySettings settings;
        settings.layerMask = 0b1;

        RenderQueue queue;
        CullStatistics cull;
        RenderStatistics render;
        CullInto(candidates, camera, settings, queue, cull, &render);
        EXPECT_EQ(queue.Count(), 1u);
        EXPECT_EQ(cull.considered, 4u);
        EXPECT_EQ(cull.visible, 1u);
        EXPECT_EQ(cull.byLayer, 1u);
        EXPECT_EQ(cull.byFrustum, 2u) << "behind the camera and beyond the far plane";
        EXPECT_EQ(render.submitted, 4u);
        EXPECT_EQ(render.culled, 3u);
    }

    TEST(VisibilityTests, LodSelectionWithoutHistory)
    {
        const LodSelector lod({10.0f, 30.0f, 90.0f});
        EXPECT_EQ(lod.LevelCount(), 4);
        EXPECT_EQ(lod.Select(0.0f), 0);
        EXPECT_EQ(lod.Select(10.0f), 0);
        EXPECT_EQ(lod.Select(10.5f), 1);
        EXPECT_EQ(lod.Select(45.0f), 2);
        EXPECT_EQ(lod.Select(1000.0f), 3);
    }

    TEST(VisibilityTests, LodHysteresisPreventsFlicker)
    {
        const LodSelector lod({10.0f, 30.0f}, 0.1f);
        int level = lod.Select(9.0f);
        ASSERT_EQ(level, 0);
        // Jitter around the 10-unit threshold never flips the level...
        for (const float d : {10.5f, 9.5f, 10.9f, 9.2f, 10.99f}) {
            level = lod.Select(d, level);
            EXPECT_EQ(level, 0) << d;
        }
        // ...until the hysteresis band is left.
        level = lod.Select(11.1f, level);
        EXPECT_EQ(level, 1);
        for (const float d : {10.0f, 9.1f, 10.5f}) {
            level = lod.Select(d, level);
            EXPECT_EQ(level, 1) << d;
        }
        EXPECT_EQ(lod.Select(8.9f, level), 0);
        EXPECT_EQ(lod.Select(200.0f, 0), 2) << "large jumps cross several levels";
    }

    TEST(VisibilityTests, LodRejectsInvalidThresholds)
    {
        EXPECT_THROW(LodSelector({10.0f, 5.0f}), std::invalid_argument);
        EXPECT_THROW(LodSelector({0.0f}), std::invalid_argument);
        EXPECT_THROW(LodSelector({10.0f}, 1.0f), std::invalid_argument);
        EXPECT_NO_THROW(LodSelector({}, 0.0f));
    }

    TEST(VisibilityTests, ItemsUseTheirBoxAndOwnCullDistance)
    {
        Camera camera = LookingDownNegativeZ();
        const GpuMesh mesh;
        Material material;
        // A long facade along X, 20 units ahead: its sphere reaches far to the
        // sides, its box is thin.
        RenderItem facade;
        facade.mesh = &mesh;
        facade.material = &material;
        facade.bounds = BoundingSphere(Vector3(0.0f, 0.0f, -20.0f), 30.0f);
        facade.box = Microsoft::Xna::Framework::BoundingBox(Vector3(-30.0f, 0.0f, -21.0f), Vector3(30.0f, 5.0f, -19.0f));

        VisibilitySettings settings;
        settings.maxDistance = 25.0f;
        EXPECT_EQ(Classify(facade, camera, settings), Visibility::Visible) << "nearest point of the box is 19 away";
        facade.cullDistance = 15.0f;
        EXPECT_EQ(Classify(facade, camera, settings), Visibility::RejectedByDistance) << "own limit is smaller";
        facade.cullDistance = 0.0f;
        settings.minDistance = 20.0f;
        EXPECT_EQ(Classify(facade, camera, settings), Visibility::RejectedByDistance) << "closer than minDistance";
        settings.minDistance = 0.0f;

        // Behind the camera: the sphere would still touch the frustum, the box does not.
        facade.box = Microsoft::Xna::Framework::BoundingBox(Vector3(-30.0f, 0.0f, 19.0f), Vector3(30.0f, 5.0f, 21.0f));
        facade.bounds = BoundingSphere(Vector3(0.0f, 0.0f, 20.0f), 30.0f);
        EXPECT_EQ(Classify(facade.bounds, 1, camera, VisibilitySettings{}), Visibility::Visible);
        EXPECT_EQ(Classify(facade, camera, VisibilitySettings{}), Visibility::RejectedByFrustum);
        EXPECT_FLOAT_EQ(DistanceToBox(Vector3(0.0f, 2.0f, 0.0f), *facade.box), 19.0f);
    }
}
