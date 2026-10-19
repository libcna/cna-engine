// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/MeshData.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Vector4;

    namespace
    {
        // A unit quad in the XY plane facing +Z (clockwise as seen from +Z).
        MeshData QuadFacingPositiveZ()
        {
            MeshData mesh;
            mesh.positions = {Vector3(-1.0f, 1.0f, 0.0f), Vector3(1.0f, 1.0f, 0.0f), Vector3(1.0f, -1.0f, 0.0f),
                              Vector3(-1.0f, -1.0f, 0.0f)};
            mesh.texCoords0 = {Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f)};
            mesh.indices = {0, 1, 2, 0, 2, 3};
            return mesh;
        }

        // Axis-aligned unit cube, every face clockwise when seen from outside.
        MeshData Cube()
        {
            MeshData mesh;
            for (int i = 0; i < 8; ++i) {
                mesh.positions.emplace_back((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);
            }
            // Corners: bit0 = +X, bit1 = +Y, bit2 = +Z.
            const std::uint32_t faces[6][4] = {
                {6, 7, 5, 4}, // +Z seen from +Z: TL, TR, BR, BL
                {3, 2, 0, 1}, // -Z seen from -Z
                {7, 3, 1, 5}, // +X seen from +X
                {2, 6, 4, 0}, // -X seen from -X
                {2, 3, 7, 6}, // +Y seen from above
                {4, 5, 1, 0}, // -Y seen from below
            };
            for (const auto& f : faces) {
                mesh.indices.insert(mesh.indices.end(), {f[0], f[1], f[2], f[0], f[2], f[3]});
            }
            return mesh;
        }

        void ExpectNear(const Vector3& actual, const Vector3& expected, float tolerance = 1e-5f)
        {
            EXPECT_NEAR(actual.X, expected.X, tolerance);
            EXPECT_NEAR(actual.Y, expected.Y, tolerance);
            EXPECT_NEAR(actual.Z, expected.Z, tolerance);
        }
    }

    TEST(MeshDataTests, AttributesReflectNonEmptyStreams)
    {
        MeshData mesh = QuadFacingPositiveZ();
        EXPECT_EQ(mesh.Attributes(), (AttributeMask{VertexAttribute::Position, VertexAttribute::TexCoord0}));
        mesh.AddAttribute(VertexAttribute::Color);
        EXPECT_TRUE(mesh.Has(VertexAttribute::Color));
        EXPECT_EQ(mesh.colors[2], Color(255, 255, 255, 255));
        mesh.RemoveAttribute(VertexAttribute::TexCoord0);
        EXPECT_FALSE(mesh.Has(VertexAttribute::TexCoord0));
        EXPECT_EQ(mesh.VertexCount(), 4u);
        EXPECT_EQ(mesh.TriangleCount(), 2u);
    }

    TEST(MeshDataTests, ValidateReportsEveryInconsistency)
    {
        MeshData mesh = QuadFacingPositiveZ();
        EXPECT_TRUE(mesh.Validate().empty());
        mesh.normals.resize(2);
        mesh.indices.push_back(9);
        const auto problems = mesh.Validate();
        ASSERT_EQ(problems.size(), 3u);
        EXPECT_NE(problems[0].find("Normal stream has 2 elements for 4 vertices"), std::string::npos);
        EXPECT_NE(problems[1].find("not a multiple of 3"), std::string::npos);
        EXPECT_NE(problems[2].find("out of range"), std::string::npos);
    }

    TEST(MeshDataTests, BoundsAndSphereEncloseAllVertices)
    {
        const MeshData mesh = Cube();
        const auto box = mesh.Bounds();
        ExpectNear(box.Min, Vector3(-1.0f, -1.0f, -1.0f));
        ExpectNear(box.Max, Vector3(1.0f, 1.0f, 1.0f));
        const auto sphere = mesh.Sphere();
        ExpectNear(sphere.Center, Vector3::Zero);
        EXPECT_NEAR(sphere.Radius, std::sqrt(3.0f), 1e-5f);

        const MeshData empty;
        ExpectNear(empty.Bounds().Min, Vector3::Zero);
        EXPECT_EQ(empty.Sphere().Radius, 0.0f);
    }

    TEST(MeshDataTests, FaceNormalFollowsClockwiseFrontFaces)
    {
        const MeshData quad = QuadFacingPositiveZ();
        ExpectNear(Vector3::Normalize(quad.FaceNormal(0)), Vector3(0.0f, 0.0f, 1.0f));
        EXPECT_GT(Cube().SignedVolume(), 0.0f) << "front faces point outward";
        EXPECT_NEAR(Cube().SignedVolume(), 8.0f, 1e-4f);
    }

    TEST(MeshDataTests, FlipWindingInvertsFacing)
    {
        MeshData cube = Cube();
        cube.FlipWinding();
        EXPECT_NEAR(cube.SignedVolume(), -8.0f, 1e-4f);
    }

    TEST(MeshDataTests, ComputeNormalsSmoothsSharedVertices)
    {
        MeshData quad = QuadFacingPositiveZ();
        quad.ComputeNormals();
        ASSERT_EQ(quad.normals.size(), 4u);
        for (const auto& n : quad.normals) {
            ExpectNear(n, Vector3(0.0f, 0.0f, 1.0f));
        }
        MeshData cube = Cube();
        cube.ComputeNormals();
        const float s = 1.0f / std::sqrt(3.0f);
        ExpectNear(cube.normals[7], Vector3(s, s, s), 1e-4f);
    }

    TEST(MeshDataTests, ComputeTangentsFollowsTextureU)
    {
        MeshData quad = QuadFacingPositiveZ();
        EXPECT_THROW(quad.ComputeTangents(), std::logic_error);
        quad.ComputeNormals();
        quad.ComputeTangents();
        ASSERT_EQ(quad.tangents.size(), 4u);
        for (const auto& t : quad.tangents) {
            ExpectNear(Vector3(t.X, t.Y, t.Z), Vector3(1.0f, 0.0f, 0.0f));
            // V grows downwards (-Y) while cross(N, T) = +Y: left-handed frame.
            EXPECT_EQ(t.W, -1.0f);
        }
    }

    TEST(MeshDataTests, AppendRebasesIndicesAndRequiresMatchingAttributes)
    {
        MeshData a = QuadFacingPositiveZ();
        const MeshData b = QuadFacingPositiveZ();
        a.Append(b, Matrix::CreateTranslation(0.0f, 0.0f, 5.0f));
        EXPECT_EQ(a.VertexCount(), 8u);
        EXPECT_EQ(a.indices[6], 4u);
        EXPECT_NEAR(a.positions[4].Z, 5.0f, 1e-6f);
        EXPECT_TRUE(a.Validate().empty());

        MeshData colored = QuadFacingPositiveZ();
        colored.SetUniformColor(Color(255, 0, 0, 255));
        EXPECT_THROW(a.Append(colored), std::invalid_argument);

        MeshData empty;
        empty.Append(colored);
        EXPECT_EQ(empty.VertexCount(), 4u) << "an empty mesh adopts the appended attributes";
    }

    TEST(MeshDataTests, MirroringTransformPreservesFrontFaces)
    {
        MeshData cube = Cube();
        cube.ComputeNormals();
        cube.AddAttribute(VertexAttribute::Tangent);
        cube.Transform(Matrix::CreateScale(-1.0f, 1.0f, 1.0f));
        EXPECT_NEAR(cube.SignedVolume(), 8.0f, 1e-4f);
        EXPECT_EQ(cube.tangents[0].W, -1.0f) << "mirroring flips tangent handedness";
        // Corner 7 (+X,+Y,+Z) moved to -X; its normal must follow.
        EXPECT_LT(cube.normals[7].X, 0.0f);
    }

    TEST(MeshDataTests, NonUniformScaleKeepsNormalsPerpendicular)
    {
        MeshData quad = QuadFacingPositiveZ();
        quad.ComputeNormals();
        quad.Transform(Matrix::CreateRotationX(0.5f) * Matrix::CreateScale(1.0f, 3.0f, 0.5f));
        quad.ComputeNormals();
        const Vector3 recomputed = quad.normals[0];
        MeshData again = QuadFacingPositiveZ();
        again.ComputeNormals();
        again.Transform(Matrix::CreateRotationX(0.5f) * Matrix::CreateScale(1.0f, 3.0f, 0.5f));
        ExpectNear(again.normals[0], recomputed, 1e-4f);
    }

    TEST(MeshDataTests, SkinStreamsAreOrdinaryAttributes)
    {
        MeshData mesh = QuadFacingPositiveZ();
        mesh.AddAttribute(VertexAttribute::BlendIndices);
        mesh.AddAttribute(VertexAttribute::BlendWeights);
        EXPECT_TRUE(mesh.Has(VertexAttribute::BlendIndices));
        EXPECT_TRUE(mesh.Has(VertexAttribute::BlendWeights));
        EXPECT_EQ(mesh.blendIndices[3], (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
        EXPECT_EQ(mesh.blendWeights[3].X, 1.0f) << "bound rigidly to bone 0";
        EXPECT_TRUE(mesh.Validate().empty());

        mesh.blendIndices[1] = {1, 2, 0, 0};
        MeshData twice = mesh;
        twice.Append(mesh);
        ASSERT_EQ(twice.blendIndices.size(), 8u);
        EXPECT_EQ(twice.blendIndices[5], (std::array<std::uint8_t, 4>{1, 2, 0, 0})) << "bone indices are not rebased";

        mesh.blendWeights.pop_back();
        mesh.blendIndices[0][0] = 7;
        const auto problems = mesh.Validate();
        ASSERT_EQ(problems.size(), 1u);
        EXPECT_EQ(problems[0], "BlendWeights stream has 3 elements for 4 vertices");
        mesh.blendWeights.push_back(Vector4(std::nanf(""), 0.0f, 0.0f, 0.0f));
        ASSERT_EQ(mesh.Validate().size(), 1u);
        EXPECT_EQ(mesh.Validate()[0], "bone weight 3 is not finite");

        mesh.RemoveAttribute(VertexAttribute::BlendIndices);
        EXPECT_FALSE(mesh.Has(VertexAttribute::BlendIndices));
    }
}
