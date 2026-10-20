// SPDX-License-Identifier: MIT
//
// Read-only equivalence probe: what cna-engine took over from easy-3d
// (openeggbert/easy-3d, compiled read-only) produces exactly what easy-3d
// produces -- the same vertices, texture coordinates, indices and matrices,
// bit for bit -- so galaxy-eggbert can move from easy-3d to the engine
// without its picture moving.
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/TextureAtlas.hpp"

#include "Easy3D/BillboardMesh.hpp"
#include "Easy3D/Camera3D.hpp"
#include "Easy3D/CubeMesh.hpp"
#include "Easy3D/FollowCamera.hpp"
#include "Easy3D/OrbitCamera.hpp"
#include "Easy3D/TextureAtlas.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace CnaEngine::Pilot
{
    using Graphics::MeshBuilder;
    using Graphics::VertexAttribute;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        struct Random
        {
            std::mt19937 engine{20261020u};
            float operator()(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(engine); }
            Vector3 Point(float range) { return Vector3((*this)(-range, range), (*this)(-range, range), (*this)(-range, range)); }
            Easy3D::UvRect Uv()
            {
                const float u = (*this)(0.0f, 0.5f);
                const float v = (*this)(0.0f, 0.5f);
                return Easy3D::UvRect{u, v, u + (*this)(0.01f, 0.5f), v + (*this)(0.01f, 0.5f)};
            }
        };

        Graphics::UvRect ToEngine(const Easy3D::UvRect& uv) { return Graphics::UvRect{uv.U0, uv.V0, uv.U1, uv.V1}; }

        bool Same(const Vector3& a, const Vector3& b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z; }
        bool Same(const Vector2& a, const Vector2& b) { return a.X == b.X && a.Y == b.Y; }
        bool Same(const Matrix& a, const Matrix& b)
        {
            const float x[] = {a.M11, a.M12, a.M13, a.M14, a.M21, a.M22, a.M23, a.M24,
                               a.M31, a.M32, a.M33, a.M34, a.M41, a.M42, a.M43, a.M44};
            const float y[] = {b.M11, b.M12, b.M13, b.M14, b.M21, b.M22, b.M23, b.M24,
                               b.M31, b.M32, b.M33, b.M34, b.M41, b.M42, b.M43, b.M44};
            for (int i = 0; i < 16; ++i) {
                if (x[i] != y[i]) {
                    return false;
                }
            }
            return true;
        }

        // easy-3d's vertex/index lists against the engine builder's mesh, exactly.
        template <typename Vertex>
        void ExpectIdentical(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices,
                             const MeshBuilder& builder)
        {
            const auto& mesh = builder.Mesh();
            ASSERT_EQ(mesh.positions.size(), vertices.size());
            ASSERT_EQ(mesh.indices, indices);
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                ASSERT_TRUE(Same(mesh.positions[i], vertices[i].Position)) << "vertex " << i;
                ASSERT_TRUE(Same(mesh.texCoords0[i], vertices[i].Uv)) << "vertex " << i;
            }
        }

        MeshBuilder TexturedBuilder() { return MeshBuilder({VertexAttribute::Position, VertexAttribute::TexCoord0}); }
    }

    TEST(Easy3DEquivalenceTests, TextureAtlasRegionsAndGrids)
    {
        Easy3D::TextureAtlas theirs(512, 256);
        Graphics::TextureAtlas ours(512, 256);
        theirs.Add("sky", Easy3D::AtlasRect{3, 5, 100, 70});
        ours.Add("sky", Graphics::AtlasRect{3, 5, 100, 70});
        theirs.AddGrid("tile", 32, 16, 7, 5, 1, 2, 3, 4);
        ours.AddGrid("tile", 32, 16, 7, 5, 1, 2, 3, 4);
        ASSERT_EQ(ours.Count(), theirs.Count());
        for (int i = 0; i < 35; ++i) {
            const std::string name = "tile_" + std::to_string(i);
            const auto a = theirs.GetUv(name);
            const auto b = ours.Uv(name);
            EXPECT_TRUE(a.U0 == b.u0 && a.V0 == b.v0 && a.U1 == b.u1 && a.V1 == b.v1) << name;
        }
        const auto sky = theirs.GetUvOrDefault("missing", Easy3D::UvRect{0.1f, 0.2f, 0.3f, 0.4f});
        const auto skyOurs = ours.UvOrDefault("missing", Graphics::UvRect{0.1f, 0.2f, 0.3f, 0.4f});
        EXPECT_TRUE(sky.U0 == skyOurs.u0 && sky.V1 == skyOurs.v1);
    }

    TEST(Easy3DEquivalenceTests, CubesAndDirectionalCubes)
    {
        Random random;
        for (int n = 0; n < 200; ++n) {
            const Vector3 center = random.Point(500.0f);
            const Vector3 size(random(0.1f, 4.0f), random(0.1f, 4.0f), random(0.1f, 4.0f));
            Easy3D::DirectionalCubeItem item;
            item.Center = center;
            item.Size = size;
            Graphics::BoxFaces faces;
            for (int f = 0; f < 6; ++f) {
                item.Faces[f].Visible = random(0.0f, 1.0f) > 0.3f;
                item.Faces[f].Uv = random.Uv();
                faces[static_cast<std::size_t>(f)].visible = item.Faces[f].Visible;
                faces[static_cast<std::size_t>(f)].uv = ToEngine(item.Faces[f].Uv);
            }
            std::vector<Easy3D::CubeVertex> vertices;
            std::vector<std::uint32_t> indices;
            Easy3D::AppendDirectionalCubeMesh(item, vertices, indices);
            // easy-3d's own bounds arithmetic, so the engine sees the same corners.
            const Vector3 half = item.Size * 0.5f;
            MeshBuilder builder = TexturedBuilder();
            builder.AddBox(item.Center - half, item.Center + half, faces);
            ExpectIdentical(vertices, indices, builder);

            Easy3D::CubeItem plain{center, size, random.Uv()};
            vertices.clear();
            indices.clear();
            Easy3D::AppendCubeMesh(plain, vertices, indices);
            Graphics::BoxFaces same;
            same.fill(Graphics::BoxFaceStyle{true, ToEngine(plain.Uv)});
            MeshBuilder cube = TexturedBuilder();
            cube.AddBox(plain.Center - plain.Size * 0.5f, plain.Center + plain.Size * 0.5f, same);
            ExpectIdentical(vertices, indices, cube);
        }
    }

    TEST(Easy3DEquivalenceTests, PlatesAndCrossedPlates)
    {
        Random random;
        for (int n = 0; n < 200; ++n) {
            Easy3D::PlateItem plate;
            plate.Center = random.Point(300.0f);
            plate.Width = random(0.1f, 5.0f);
            plate.Height = random(0.1f, 5.0f);
            plate.Uv = random.Uv();
            plate.Axis = static_cast<Easy3D::PlateAxis>(n % 3);
            std::vector<Easy3D::CubeVertex> vertices;
            std::vector<std::uint32_t> indices;
            Easy3D::AppendPlateMesh(plate, vertices, indices);
            MeshBuilder builder = TexturedBuilder();
            builder.AddPlate(plate.Center, plate.Width, plate.Height, static_cast<Graphics::PlateAxis>(n % 3),
                             ToEngine(plate.Uv));
            ExpectIdentical(vertices, indices, builder);

            Easy3D::TripleCrossItem cross;
            cross.Center = plate.Center;
            cross.Width = plate.Width;
            cross.Height = plate.Height;
            cross.Uv = plate.Uv;
            vertices.clear();
            indices.clear();
            Easy3D::AppendTripleCrossMesh(cross, vertices, indices);
            MeshBuilder crossed = TexturedBuilder();
            crossed.AddCrossedPlates(cross.Center, cross.Width, cross.Height, 3, ToEngine(cross.Uv));
            ExpectIdentical(vertices, indices, crossed);
        }
    }

    TEST(Easy3DEquivalenceTests, Billboards)
    {
        Random random;
        for (int n = 0; n < 200; ++n) {
            const Matrix view = Matrix::CreateLookAt(random.Point(50.0f), random.Point(50.0f), Vector3::Up);
            const Matrix world = Matrix::Invert(view);
            const Vector3 right(world.M11, world.M12, world.M13);
            const Vector3 up(world.M21, world.M22, world.M23);
            Easy3D::BillboardItem item;
            item.Position = random.Point(100.0f);
            item.Size = Vector2(random(0.1f, 8.0f), random(0.1f, 8.0f));
            item.Uv = random.Uv();
            item.Origin = Vector2(random(0.0f, 1.0f), random(0.0f, 1.0f));
            item.RotationRadians = random(-3.0f, 3.0f);
            std::vector<Easy3D::BillboardVertex> vertices;
            std::vector<std::uint32_t> indices;
            Easy3D::AppendBillboardMesh(item, right, up, vertices, indices);
            MeshBuilder builder = TexturedBuilder();
            builder.AddBillboard(item.Position, item.Size, right, up, ToEngine(item.Uv), item.Origin,
                                 item.RotationRadians);
            ExpectIdentical(vertices, indices, builder);
        }
    }

    TEST(Easy3DEquivalenceTests, CameraMatricesAndControllers)
    {
        Random random;
        for (int n = 0; n < 200; ++n) {
            Easy3D::Camera3D theirs;
            theirs.SetPosition(random.Point(200.0f));
            theirs.SetTarget(random.Point(200.0f));
            theirs.SetFieldOfView(random(0.3f, 2.0f));
            theirs.SetAspectRatio(random(0.5f, 3.0f));
            theirs.SetNearPlane(random(0.01f, 1.0f));
            theirs.SetFarPlane(random(100.0f, 5000.0f));
            Graphics::Camera ours;
            ours.SetPerspective(theirs.GetFieldOfView(), theirs.GetAspectRatio(), theirs.GetNearPlane(),
                                theirs.GetFarPlane());
            ours.SetLookAt(theirs.GetPosition(), theirs.GetTarget(), theirs.GetUp());
            ASSERT_TRUE(Same(ours.View(), theirs.GetViewMatrix())) << n;
            ASSERT_TRUE(Same(ours.Projection(), theirs.GetProjectionMatrix())) << n;
        }

        Easy3D::OrbitCamera orbit;
        Graphics::OrbitCameraController orbitOurs;
        for (int n = 0; n < 100; ++n) {
            const Vector3 target = random.Point(50.0f);
            const float yaw = random(-6.0f, 6.0f);
            const float pitch = random(-1.4f, 1.4f);
            const float distance = random(0.5f, 80.0f);
            orbit.SetTarget(target);
            orbit.SetYaw(yaw);
            orbit.SetPitch(pitch);
            orbit.SetDistance(distance);
            orbitOurs.SetTarget(target);
            orbitOurs.SetYaw(yaw);
            orbitOurs.SetPitch(pitch);
            orbitOurs.SetDistance(distance);
            ASSERT_TRUE(Same(orbit.ComputePosition(), orbitOurs.Position())) << n;
        }

        Easy3D::FollowCamera follow;
        Graphics::FollowCameraController followOurs(follow.GetOffset(), follow.GetSmoothing());
        Graphics::Camera camera;
        for (int n = 0; n < 300; ++n) {
            const Vector3 target = random.Point(20.0f);
            const float dt = random(0.001f, 0.05f);
            follow.Update(target, dt);
            followOurs.SetTargetPosition(target);
            followOurs.Update(camera, {}, dt);
            ASSERT_TRUE(Same(follow.GetPosition(), followOurs.Position())) << n;
        }
    }
}
