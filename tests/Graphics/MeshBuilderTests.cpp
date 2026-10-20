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

    TEST(MeshBuilderTests, BoxFacesTakeTheirRegionsAndHiddenFacesAreLeftOut)
    {
        using Microsoft::Xna::Framework::Vector2;
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::TexCoord0});
        BoxFaces faces;
        faces[static_cast<std::size_t>(BoxFace::PositiveY)].uv = UvRect{0.5f, 0.0f, 0.75f, 0.25f};
        faces[static_cast<std::size_t>(BoxFace::NegativeY)].visible = false;
        faces[static_cast<std::size_t>(BoxFace::NegativeX)].visible = false;
        builder.AddBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), faces);
        const MeshData& box = builder.Mesh();
        EXPECT_EQ(box.VertexCount(), 16u) << "four visible faces";
        EXPECT_EQ(box.TriangleCount(), 8u);
        ExpectOutwardFaces(box, Vector3(0.5f, 0.5f, 0.5f));
        // The top face is the fifth in BoxFace order, the fourth one emitted
        // (-X is hidden); its corners are top-left, top-right, bottom-right,
        // bottom-left as seen from outside.
        EXPECT_EQ(box.texCoords0[12], Vector2(0.5f, 0.0f));
        EXPECT_EQ(box.texCoords0[14], Vector2(0.75f, 0.25f));
        EXPECT_EQ(box.positions[12], Vector3(0.0f, 1.0f, 0.0f));
        EXPECT_EQ(box.positions[14], Vector3(1.0f, 1.0f, 1.0f));
        // Corners are the bounds exactly, not reconstructed from a centre.
        const Vector3 min(0.1f, -0.7f, 3.3f);
        const Vector3 max(0.9f, 0.2f, 4.1f);
        MeshBuilder exact({VertexAttribute::Position});
        exact.AddBox(min, max);
        for (const Vector3& p : exact.Mesh().positions) {
            EXPECT_TRUE(p.X == min.X || p.X == max.X);
            EXPECT_TRUE(p.Y == min.Y || p.Y == max.Y);
            EXPECT_TRUE(p.Z == min.Z || p.Z == max.Z);
        }
    }

    TEST(MeshBuilderTests, PlatesAreDoubleSidedAlongTheirAxis)
    {
        for (const PlateAxis axis : {PlateAxis::Z, PlateAxis::X, PlateAxis::Y}) {
            MeshBuilder builder;
            builder.AddPlate(Vector3(1.0f, 2.0f, 3.0f), 2.0f, 4.0f, axis);
            const MeshData& plate = builder.Mesh();
            ASSERT_EQ(plate.VertexCount(), 8u);
            ASSERT_EQ(plate.TriangleCount(), 4u);
            const Vector3 front = Vector3::Normalize(plate.FaceNormal(0));
            const Vector3 back = Vector3::Normalize(plate.FaceNormal(2));
            EXPECT_NEAR(Vector3::Dot(front, back), -1.0f, 1e-5f) << "one copy per side";
            const Vector3 expected = axis == PlateAxis::Z ? Vector3::UnitZ : axis == PlateAxis::X ? Vector3::UnitX
                                                                                                 : Vector3::UnitY;
            EXPECT_NEAR(std::fabs(Vector3::Dot(front, expected)), 1.0f, 1e-5f);
            for (std::size_t i = 0; i < 4; ++i) {
                EXPECT_EQ(plate.positions[i], plate.positions[i + 4]) << "the same corners twice";
                EXPECT_GT(Vector3::Dot(plate.normals[i], front), 0.99f);
                EXPECT_GT(Vector3::Dot(plate.normals[i + 4], back), 0.99f);
            }
        }
    }

    TEST(MeshBuilderTests, CrossedPlatesTurnAboutY)
    {
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::TexCoord0});
        builder.AddCrossedPlates(Vector3::Zero, 2.0f, 1.0f);
        const MeshData& cross = builder.Mesh();
        EXPECT_EQ(cross.VertexCount(), 24u) << "three double-sided plates";
        EXPECT_EQ(cross.TriangleCount(), 12u);
        // The second plate is turned 60 degrees: its first corner is at -cos/-sin of 60.
        EXPECT_NEAR(cross.positions[8].X, -std::cos(MathHelper::ToRadians(60.0f)), 1e-6f);
        EXPECT_NEAR(cross.positions[8].Z, -std::sin(MathHelper::ToRadians(60.0f)), 1e-6f);
        EXPECT_THROW(builder.AddCrossedPlates(Vector3::Zero, 1.0f, 1.0f, 0), std::invalid_argument);
    }

    TEST(MeshBuilderTests, BillboardFacesTheCamera)
    {
        using Microsoft::Xna::Framework::Vector2;
        // A camera looking down -Z: right +X, up +Y; the quad must face +Z.
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord0});
        builder.AddBillboard(Vector3(0.0f, 0.0f, -5.0f), Vector2(2.0f, 1.0f), Vector3::UnitX, Vector3::UnitY,
                             UvRect{0.0f, 0.0f, 0.5f, 0.5f});
        const MeshData& quad = builder.Mesh();
        ASSERT_EQ(quad.VertexCount(), 4u);
        EXPECT_GT(Vector3::Dot(Vector3::Normalize(quad.FaceNormal(0)), Vector3::UnitZ), 0.99f);
        EXPECT_GT(Vector3::Dot(Vector3::Normalize(quad.FaceNormal(1)), Vector3::UnitZ), 0.99f);
        EXPECT_GT(Vector3::Dot(quad.normals[0], Vector3::UnitZ), 0.99f);
        EXPECT_EQ(quad.positions[0], Vector3(-1.0f, -0.5f, -5.0f)) << "bottom-left";
        EXPECT_EQ(quad.positions[2], Vector3(1.0f, 0.5f, -5.0f)) << "top-right";
        EXPECT_EQ(quad.texCoords0[0], Vector2(0.0f, 0.5f)) << "the region's bottom-left";
        EXPECT_EQ(quad.texCoords0[2], Vector2(0.5f, 0.0f));
        // A pivot at the bottom centre and a quarter turn.
        MeshBuilder turned({VertexAttribute::Position});
        turned.AddBillboard(Vector3::Zero, Vector2(2.0f, 1.0f), Vector3::UnitX, Vector3::UnitY, UvRect::Full(),
                            Vector2(0.5f, 0.0f), MathHelper::PiOver2);
        EXPECT_NEAR(turned.Mesh().positions[1].Y, 1.0f, 1e-6f) << "bottom-right swings up";
        EXPECT_NEAR(turned.Mesh().positions[1].X, 0.0f, 1e-6f);
    }
}

