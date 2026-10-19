// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        // Every face normal points away from the shape's centre.
        void ExpectOutwardFaces(const MeshData& mesh, const Vector3& center)
        {
            for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
                const Vector3 n = mesh.FaceNormal(t);
                if (n.LengthSquared() < 1e-12f) {
                    continue;
                }
                const Vector3 a = mesh.positions[mesh.indices[t * 3]];
                const Vector3 b = mesh.positions[mesh.indices[t * 3 + 1]];
                const Vector3 c = mesh.positions[mesh.indices[t * 3 + 2]];
                const Vector3 centroid = (a + b + c) * (1.0f / 3.0f);
                EXPECT_GT(Vector3::Dot(n, centroid - center), 0.0f) << "triangle " << t;
            }
        }
    }

    TEST(MeshBuilderTests, StoresOnlyMaskedAttributes)
    {
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
        BuilderVertex v;
        v.color = Color(1, 2, 3, 4);
        builder.AddVertex(v);
        builder.AddVertex(v);
        builder.AddVertex(v);
        builder.AddTriangle(0, 1, 2);
        EXPECT_EQ(builder.Mesh().Attributes(), (AttributeMask{VertexAttribute::Position, VertexAttribute::Color}));
        EXPECT_EQ(builder.Mesh().colors[1], Color(1, 2, 3, 4));
        EXPECT_THROW(builder.AddTriangle(0, 1, 3), std::out_of_range);

        const MeshData taken = builder.Take();
        EXPECT_EQ(taken.VertexCount(), 3u);
        EXPECT_EQ(builder.Mesh().VertexCount(), 0u) << "Take() resets the builder";
    }

    TEST(MeshBuilderTests, BoxIsClosedAndOutward)
    {
        MeshBuilder builder;
        builder.AddBox(Vector3(-1.0f, -2.0f, -3.0f), Vector3(1.0f, 2.0f, 3.0f));
        const MeshData& box = builder.Mesh();
        EXPECT_EQ(box.VertexCount(), 24u);
        EXPECT_EQ(box.TriangleCount(), 12u);
        EXPECT_TRUE(box.Validate().empty());
        EXPECT_NEAR(box.SignedVolume(), 2.0f * 4.0f * 6.0f, 1e-3f);
        ExpectOutwardFaces(box, Vector3::Zero);
        for (std::size_t i = 0; i < box.VertexCount(); ++i) {
            // Stored normals agree with the geometric face orientation.
            EXPECT_GT(Vector3::Dot(box.normals[i], box.positions[i]), 0.0f);
        }
    }

    TEST(MeshBuilderTests, GridFacesUpAndHasExpectedTopology)
    {
        MeshBuilder builder;
        builder.AddGrid(Vector3(0.0f, 1.0f, 0.0f), 4.0f, 2.0f, 4, 2, 2.0f);
        const MeshData& grid = builder.Mesh();
        EXPECT_EQ(grid.VertexCount(), 15u);
        EXPECT_EQ(grid.TriangleCount(), 16u);
        for (std::size_t t = 0; t < grid.TriangleCount(); ++t) {
            EXPECT_GT(grid.FaceNormal(t).Y, 0.0f);
        }
        const auto bounds = grid.Bounds();
        EXPECT_FLOAT_EQ(bounds.Min.X, -2.0f);
        EXPECT_FLOAT_EQ(bounds.Max.Z, 1.0f);
        EXPECT_FLOAT_EQ(grid.texCoords0.back().X, 2.0f);
        EXPECT_THROW(builder.AddGrid(Vector3::Zero, 1.0f, 1.0f, 0, 1), std::invalid_argument);
    }

    TEST(MeshBuilderTests, SphereIsClosedOutwardAndOnTheSurface)
    {
        MeshBuilder builder;
        builder.AddSphere(Vector3(1.0f, 2.0f, 3.0f), 2.0f, 24, 16);
        const MeshData& sphere = builder.Mesh();
        EXPECT_TRUE(sphere.Validate().empty());
        for (const auto& p : sphere.positions) {
            EXPECT_NEAR(Vector3::Distance(p, Vector3(1.0f, 2.0f, 3.0f)), 2.0f, 1e-4f);
        }
        ExpectOutwardFaces(sphere, Vector3(1.0f, 2.0f, 3.0f));
        // A 24x16 UV sphere holds a little less than the true volume.
        const float exact = 4.0f / 3.0f * MathHelper::Pi * 8.0f;
        EXPECT_GT(sphere.SignedVolume(), 0.95f * exact);
        EXPECT_LT(sphere.SignedVolume(), exact);
    }

    TEST(MeshBuilderTests, CylinderWithCapsIsClosed)
    {
        MeshBuilder builder;
        builder.AddCylinder(Vector3::Zero, 1.0f, 2.0f, 32);
        const MeshData& cylinder = builder.Mesh();
        ExpectOutwardFaces(cylinder, Vector3(0.0f, 1.0f, 0.0f));
        const float exact = MathHelper::Pi * 2.0f;
        EXPECT_GT(cylinder.SignedVolume(), 0.98f * exact);
        EXPECT_LT(cylinder.SignedVolume(), exact);

        MeshBuilder open;
        open.AddCylinder(Vector3::Zero, 1.0f, 2.0f, 8, false);
        EXPECT_EQ(open.Mesh().TriangleCount(), 16u);
    }

    TEST(MeshBuilderTests, ColorAppliesToProceduralShapes)
    {
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Color});
        builder.SetColor(Color(10, 20, 30, 255));
        builder.AddBox(Vector3::Zero, Vector3(1.0f, 1.0f, 1.0f));
        for (const auto& c : builder.Mesh().colors) {
            EXPECT_EQ(c, Color(10, 20, 30, 255));
        }
        EXPECT_FALSE(builder.Mesh().Has(VertexAttribute::Normal));
    }
}
