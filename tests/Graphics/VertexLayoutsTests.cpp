// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/VertexLayouts.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Vector4;
    using Microsoft::Xna::Framework::Graphics::VertexElementFormat;
    using Microsoft::Xna::Framework::Graphics::VertexElementUsage;

    namespace
    {
        MeshData Triangle()
        {
            MeshData mesh;
            mesh.positions = {Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f)};
            mesh.normals = {Vector3::UnitZ, Vector3::UnitZ, Vector3::UnitZ};
            mesh.texCoords0 = {Vector2(0.5f, 0.0f), Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f)};
            mesh.colors = {Color(255, 0, 0, 255), Color(0, 255, 0, 255), Color(0, 0, 255, 128)};
            mesh.indices = {0, 1, 2};
            return mesh;
        }
    }

    TEST(VertexLayoutsTests, StockLayoutsMatchXnaStrides)
    {
        EXPECT_EQ(VertexLayout::PositionColor().Stride(), 16);
        EXPECT_EQ(VertexLayout::PositionTexture().Stride(), 20);
        EXPECT_EQ(VertexLayout::PositionColorTexture().Stride(), 24);
        EXPECT_EQ(VertexLayout::PositionNormalTexture().Stride(), 32);
        EXPECT_EQ(VertexLayout::PositionNormalColorTexture().Stride(), 36);
        EXPECT_EQ(VertexLayout::PositionNormalDualTexture().Stride(), 40);
        EXPECT_EQ(VertexLayout::PositionNormalTangentTexture().Stride(), 48);
        EXPECT_EQ(VertexLayout::PositionColor().Declaration().getVertexStrideProperty(), 16);
    }

    TEST(VertexLayoutsTests, InterleavedBytesFollowTheDeclaration)
    {
        const MeshData mesh = Triangle();
        const auto pc = Interleave(mesh, VertexLayout::PositionColor());
        ASSERT_EQ(pc.bytes.size(), 3u * 16u);
        float position[3];
        std::uint32_t packed = 0;
        std::memcpy(position, pc.bytes.data() + 16, sizeof(position));
        std::memcpy(&packed, pc.bytes.data() + 16 + 12, sizeof(packed));
        EXPECT_EQ(position[0], 1.0f);
        EXPECT_EQ(position[1], 0.0f);
        EXPECT_EQ(packed, Color(0, 255, 0, 255).getPackedValueProperty());
        EXPECT_EQ(pc.bytes[16 + 12 + 1], 255) << "packed color is R, G, B, A in memory";
        EXPECT_EQ(pc.dropped, (AttributeMask{VertexAttribute::Normal, VertexAttribute::TexCoord0}));
        EXPECT_TRUE(pc.missing.Empty());

        const auto pnt = Interleave(mesh, VertexLayout::PositionNormalTexture());
        float vertex[8];
        std::memcpy(vertex, pnt.bytes.data() + 2 * 32, sizeof(vertex));
        const float expected[8] = {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f};
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(vertex[i], expected[i]) << "float " << i;
        }
    }

    TEST(VertexLayoutsTests, MissingAttributesFailOrUseReportedDefaults)
    {
        MeshData mesh = Triangle();
        mesh.colors.clear();
        EXPECT_THROW((void)Interleave(mesh, VertexLayout::PositionColor()), std::invalid_argument);
        const auto filled = Interleave(mesh, VertexLayout::PositionColor(), MissingAttributePolicy::UseDefaults);
        EXPECT_EQ(filled.missing, AttributeMask{VertexAttribute::Color});
        std::uint32_t packed = 0;
        std::memcpy(&packed, filled.bytes.data() + 12, sizeof(packed));
        EXPECT_EQ(packed, 0xFFFFFFFFu);
    }

    TEST(VertexLayoutsTests, NarrowingIsReported)
    {
        MeshData mesh = Triangle();
        mesh.tangents.assign(3, Vector4(1.0f, 0.0f, 0.0f, -1.0f));
        const VertexLayout layout("PositionTangent3",
                                  {{VertexAttribute::Position, VertexElementFormat::Vector3, VertexElementUsage::Position, 0},
                                   {VertexAttribute::Tangent, VertexElementFormat::Vector3, VertexElementUsage::Tangent, 0}});
        const auto result = Interleave(mesh, layout);
        EXPECT_EQ(result.narrowed, AttributeMask{VertexAttribute::Tangent});
        const auto full = Interleave(mesh, VertexLayout::PositionNormalTangentTexture());
        EXPECT_TRUE(full.narrowed.Empty());
        float w = 0.0f;
        std::memcpy(&w, full.bytes.data() + 24 + 12, sizeof(float));
        EXPECT_EQ(w, -1.0f) << "handedness survives the 48-byte layout";
    }

    TEST(VertexLayoutsTests, InvalidLayoutsAreRejected)
    {
        using E = VertexLayoutElement;
        EXPECT_THROW(VertexLayout("NoPosition", {E{VertexAttribute::Color, VertexElementFormat::Color,
                                                   VertexElementUsage::Color, 0}}),
                     std::invalid_argument);
        EXPECT_THROW(VertexLayout("BadFormat", {E{VertexAttribute::Position, VertexElementFormat::Vector2,
                                                  VertexElementUsage::Position, 0}}),
                     std::invalid_argument);
        EXPECT_THROW(VertexLayout("Duplicate", {E{}, E{}}), std::invalid_argument);
        EXPECT_EQ(Describe(AttributeMask{VertexAttribute::Normal, VertexAttribute::TexCoord1}), "Normal, TexCoord1");
    }

    TEST(VertexLayoutsTests, InvalidMeshIsRejected)
    {
        MeshData mesh = Triangle();
        mesh.normals.pop_back();
        EXPECT_THROW((void)Interleave(mesh, VertexLayout::PositionNormalTexture()), std::invalid_argument);
    }
}
