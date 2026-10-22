// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ShadowMap.hpp"

#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/PackedVector/HalfSingle.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        std::vector<float> ReadColour(ShadowMap& map, const int count)
        {
            std::vector<Microsoft::Xna::Framework::Color> colors(static_cast<std::size_t>(count));
            map.Map().GetData(colors.data(), count);
            std::vector<float> red(colors.size());
            for (std::size_t i = 0; i < colors.size(); ++i) {
                red[i] = static_cast<float>(colors[i].getRProperty()) / 255.0f;
            }
            return red;
        }

        // The red channel of every texel, whatever the map's format.
        std::vector<float> Read(ShadowMap& map)
        {
            const int count = map.Size() * map.Size();
            std::vector<float> red(static_cast<std::size_t>(count));
            if (map.Format() == Xna::SurfaceFormat::Single) {
                map.Map().GetData(red.data(), count);
                return red;
            }
            if (map.Format() == Xna::SurfaceFormat::HalfSingle) {
                std::vector<Xna::PackedVector::HalfSingle> half(static_cast<std::size_t>(count));
                map.Map().GetData(half.data(), count);
                for (std::size_t i = 0; i < half.size(); ++i) {
                    red[i] = half[i].ToSingle();
                }
                return red;
            }
            return ReadColour(map, count);
        }
    }

    // Looking straight down from 10 m over a floor at 0 and a box whose top
    // is at 1 m: the light's depth runs 0..20 m, stored as z * 0.5 + 0.5.
    TEST(ShadowMapTest, TheMapHoldsTheNearestCastersDepth)
    {
        Xna::GraphicsDevice device;
        ShadowMap map(device, 64);
        map.Fit(Vector3(0.0f, 1.0f, 0.0f), Vector3::Zero, 5.0f);

        const auto& layout = VertexLayout::PositionNormalTexture();
        MeshBuilder builder;
        builder.AddBox(Vector3(-5.0f, -0.5f, -5.0f), Vector3(5.0f, 0.0f, 5.0f));
        const GpuMesh floor = GpuMesh::Upload(device, builder.Take(), layout);
        builder.AddBox(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
        const GpuMesh box = GpuMesh::Upload(device, builder.Take(), layout);
        Material solid;
        Material glass;
        glass.blend = BlendMode::AlphaBlend;

        auto item = [](const GpuMesh& mesh, const Material& material) {
            RenderItem result;
            result.mesh = &mesh;
            result.material = &material;
            result.world = Matrix::getIdentityProperty();
            result.bounds = WorldBounds(mesh, result.world);
            return result;
        };
        const std::vector<RenderItem> items{item(floor, solid), item(box, solid), item(box, glass)};
        EXPECT_FALSE(ShadowMap::Casts(items[2]));
        map.Render(device, items);
        EXPECT_EQ(map.CastersDrawn(), 2);

        const auto red = Read(map);
        const float tolerance = 0.006f; // a Color map on a Reach device keeps 8 bits
        const int middle = map.Size() / 2;
        // Under the box: its top, 9 m from the light.
        EXPECT_NEAR(red[static_cast<std::size_t>(middle * map.Size() + middle)], 9.0f / 20.0f * 0.5f + 0.5f, tolerance);
        // Beside it: the floor, 10 m away.
        EXPECT_NEAR(red[static_cast<std::size_t>(middle * map.Size() + 4)], 10.0f / 20.0f * 0.5f + 0.5f, tolerance);
        EXPECT_GT(map.SuggestedBias(), 0.0f);

        // The view moves in whole texels.
        const Matrix before = map.LightViewProjection();
        map.Fit(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.001f, 0.0f, 0.0f), 5.0f);
        EXPECT_FLOAT_EQ(map.LightViewProjection().M41, before.M41);
        EXPECT_THROW(map.Fit(Vector3::Zero, Vector3::Zero, 5.0f), std::invalid_argument);
        EXPECT_THROW(ShadowMap(device, 8), std::invalid_argument);
    }
}
