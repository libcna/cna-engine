// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/LocalLights.hpp"

#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    TEST(LocalLightsTest, ReachFadesWithDistanceAndTheCone)
    {
        LocalLight lamp;
        lamp.position = Vector3::Zero;
        lamp.color = Vector3(1.0f, 0.5f, 0.5f);
        lamp.range = 10.0f;
        EXPECT_FLOAT_EQ(LightReach(lamp, BoundingSphere(Vector3(0, 0, 0), 1.0f)), 1.0f);
        EXPECT_NEAR(LightReach(lamp, BoundingSphere(Vector3(6, 0, 0), 1.0f)), 0.25f, 1e-6f); // 5 m of 10: (1/2)^2
        EXPECT_FLOAT_EQ(LightReach(lamp, BoundingSphere(Vector3(12, 0, 0), 1.0f)), 0.0f);

        LocalLight spot = lamp;
        spot.kind = LocalLight::Kind::Spot;
        spot.direction = Vector3(0, -1, 0);
        spot.innerAngle = 0.3f;
        spot.outerAngle = 0.5f;
        EXPECT_GT(LightReach(spot, BoundingSphere(Vector3(0, -4, 0), 0.1f)), 0.3f); // straight below
        EXPECT_FLOAT_EQ(LightReach(spot, BoundingSphere(Vector3(4, -1, 0), 0.1f)), 0.0f); // far outside the cone
        EXPECT_FLOAT_EQ(LightReach(spot, BoundingSphere(Vector3(0, 4, 0), 0.1f)), 0.0f); // behind it
    }

    TEST(LocalLightsTest, TheStrongestLightIsChosen)
    {
        std::vector<LocalLight> lamps(3);
        lamps[0].position = Vector3(10, 0, 0);
        lamps[1].position = Vector3(2, 0, 0);
        lamps[2].position = Vector3(-2, 0, 0);
        lamps[2].color = Vector3(0.2f, 0.2f, 0.2f); // near but dim
        EXPECT_EQ(StrongestLight(lamps, BoundingSphere(Vector3::Zero, 0.5f)), 1u);
        EXPECT_FALSE(StrongestLight(lamps, BoundingSphere(Vector3(0, 0, 50), 0.5f)).has_value());
        EXPECT_FALSE(StrongestLight({}, BoundingSphere(Vector3::Zero, 0.5f)).has_value());
    }

    // A pool under a lamp brightens a dark floor at its centre and not far away.
    TEST(LocalLightsTest, APoolLightsTheGroundUnderTheLamp)
    {
        Xna::GraphicsDevice device;
        Xna::RenderTarget2D target(device, 64, 64, false, Xna::SurfaceFormat::Color, Xna::DepthFormat::Depth24);
        LightPools pools(device);
        MeshBuilder builder;
        builder.AddGrid(Vector3::Zero, 20.0f, 20.0f, 4, 4, 1.0f);
        const GpuMesh floor = GpuMesh::Upload(device, builder.Take(), VertexLayout::PositionNormalTexture());
        Material dark;
        dark.shading = ShadingModel::Unlit;
        dark.diffuseColor = Vector3(0.1f, 0.1f, 0.1f);
        std::vector<RenderItem> items;
        RenderItem ground;
        ground.mesh = &floor;
        ground.material = &dark;
        ground.bounds = WorldBounds(floor, ground.world);
        items.push_back(ground);
        LocalLight lamp;
        lamp.position = Vector3(0, 2, 0);
        lamp.range = 6.0f;
        lamp.color = Vector3(1.0f, 0.8f, 0.4f);
        pools.Append(std::span<const LocalLight>(&lamp, 1), 0.0f, items);
        ASSERT_EQ(items.size(), 2u);
        EXPECT_EQ(items[1].material->blend, BlendMode::Additive);

        Camera camera;
        camera.SetOrthographic(20.0f, 20.0f, 0.1f, 50.0f);
        camera.SetLookAt(Vector3(0, 10, 0.001f), Vector3::Zero);
        ForwardRenderer renderer(device);
        RenderQueue queue;
        for (const auto& item : items) {
            queue.Submit(item);
        }
        queue.Sort(camera.Position());
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        renderer.Render(queue, camera);
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(64 * 64);
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        const Color centre = pixels[32 * 64 + 32];
        const Color edge = pixels[2 * 64 + 2];
        EXPECT_GT(centre.getRProperty(), edge.getRProperty() + 60);
        EXPECT_NEAR(edge.getRProperty(), 26, 4); // the floor alone
    }
}
