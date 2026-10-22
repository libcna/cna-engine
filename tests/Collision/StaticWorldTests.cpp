// SPDX-License-Identifier: MIT
#include "CnaEngine/Collision/StaticWorld.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace CnaEngine::Collision
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Ray;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        void ExpectNear(const Vector3& actual, const Vector3& expected, const float tolerance = 1e-4f)
        {
            EXPECT_NEAR(actual.X, expected.X, tolerance);
            EXPECT_NEAR(actual.Y, expected.Y, tolerance);
            EXPECT_NEAR(actual.Z, expected.Z, tolerance);
        }
    }

    TEST(StaticWorldTest, TheNearestOfSeveralBoxesWins)
    {
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(10.0f, 0.0f, -1.0f), Vector3(11.0f, 2.0f, 1.0f)), 2);
        const ShapeId near = world.AddBox(BoundingBox(Vector3(5.0f, 0.0f, -1.0f), Vector3(6.0f, 2.0f, 1.0f)), 1);
        const auto hit = world.Raycast(Ray(Vector3(0.0f, 0.5f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_FLOAT_EQ(hit->distance, 5.0f);
        EXPECT_EQ(hit->shape, near);
        EXPECT_EQ(hit->tag, 1u);
        ExpectNear(hit->normal, Vector3(-1.0f, 0.0f, 0.0f));
        ExpectNear(hit->point, Vector3(5.0f, 0.5f, 0.0f));
        EXPECT_FALSE(world.Raycast(Ray(Vector3(0.0f, 0.5f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)), 4.0f).has_value());
        EXPECT_FALSE(world.Raycast(Ray(Vector3(0.0f, 0.5f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f))).has_value());
    }

    TEST(StaticWorldTest, ARayStartingInsideAWallHitsAtOnce)
    {
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f)));
        const auto hit = world.Raycast(Ray(Vector3::Zero, Vector3(0.0f, 0.0f, -1.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_FLOAT_EQ(hit->distance, 0.0f);
        ExpectNear(hit->normal, Vector3(0.0f, 0.0f, 1.0f));
    }

    TEST(StaticWorldTest, LayersFilterQueries)
    {
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(2.0f, 0.0f, -1.0f), Vector3(3.0f, 2.0f, 1.0f)), 0, 2u);
        const Ray ray(Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
        EXPECT_TRUE(world.Raycast(ray, 100.0f, 2u).has_value());
        EXPECT_FALSE(world.Raycast(ray, 100.0f, 1u).has_value());
    }

    TEST(StaticWorldTest, TrianglesAreTwoSidedAndARayInTheirPlaneMisses)
    {
        StaticWorld world;
        world.AddTriangle(Vector3(-2.0f, -2.0f, 3.0f), Vector3(2.0f, -2.0f, 3.0f), Vector3(0.0f, 2.0f, 3.0f), 7);
        auto hit = world.Raycast(Ray(Vector3::Zero, Vector3(0.0f, 0.0f, 1.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_FLOAT_EQ(hit->distance, 3.0f);
        ExpectNear(hit->normal, Vector3(0.0f, 0.0f, -1.0f));
        EXPECT_EQ(hit->tag, 7u);
        hit = world.Raycast(Ray(Vector3(0.0f, 0.0f, 6.0f), Vector3(0.0f, 0.0f, -1.0f)));
        ASSERT_TRUE(hit.has_value());
        ExpectNear(hit->normal, Vector3(0.0f, 0.0f, 1.0f));
        EXPECT_FALSE(world.Raycast(Ray(Vector3(-5.0f, 0.0f, 3.0f), Vector3(1.0f, 0.0f, 0.0f))).has_value());
    }

    TEST(StaticWorldTest, CylindersAreHitOnTheSideAndOnTop)
    {
        StaticWorld world;
        world.AddCylinder(Vector3(4.0f, 0.0f, 0.0f), 0.5f, 2.0f);
        auto hit = world.Raycast(Ray(Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_NEAR(hit->distance, 3.5f, 1e-5f);
        ExpectNear(hit->normal, Vector3(-1.0f, 0.0f, 0.0f));
        hit = world.Raycast(Ray(Vector3(4.2f, 5.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_NEAR(hit->distance, 3.0f, 1e-5f);
        ExpectNear(hit->normal, Vector3::Up);
        EXPECT_FALSE(world.Raycast(Ray(Vector3(0.0f, 3.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f))).has_value());
    }

    TEST(StaticWorldTest, HeightFieldsAreTwoTrianglesPerCell)
    {
        HeightField field;
        field.origin = Vector3(-4.0f, 1.0f, -4.0f);
        field.cellSize = 2.0f;
        field.columns = 5;
        field.rows = 5;
        // A plane rising along X: h = 0.5 * x (local).
        for (int z = 0; z < 5; ++z) {
            for (int x = 0; x < 5; ++x) {
                field.heights.push_back(0.5f * static_cast<float>(x) * 2.0f);
            }
        }
        field.tag = 9;
        ASSERT_TRUE(field.HeightAt(0.0f, 0.0f).has_value());
        EXPECT_NEAR(*field.HeightAt(0.0f, 0.0f), 1.0f + 2.0f, 1e-5f);
        EXPECT_NEAR(*field.HeightAt(1.0f, 0.5f), 1.0f + 2.5f, 1e-5f);
        EXPECT_FALSE(field.HeightAt(10.0f, 0.0f).has_value());
        const Vector3 normal = field.NormalAt(1.0f, 1.0f);
        const float length = std::sqrt(0.25f + 1.0f);
        ExpectNear(normal, Vector3(-0.5f / length, 1.0f / length, 0.0f));

        StaticWorld world;
        world.SetHeightField(field);
        const auto hit = world.Raycast(Ray(Vector3(1.0f, 20.0f, 0.5f), Vector3(0.0f, -1.0f, 0.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->shape, StaticWorld::TerrainId);
        EXPECT_EQ(hit->tag, 9u);
        EXPECT_NEAR(hit->point.Y, 3.5f, 1e-4f);
        // A slanted ray across several cells.
        const auto slanted = world.Raycast(Ray(Vector3(-3.0f, 10.0f, -3.0f), Vector3(0.5f, -1.0f, 0.4f)));
        ASSERT_TRUE(slanted.has_value());
        EXPECT_NEAR(slanted->point.Y, *field.HeightAt(slanted->point.X, slanted->point.Z), 1e-3f);
    }

    TEST(StaticWorldTest, ContactsSayHowToGetOut)
    {
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(1.0f, 0.0f, -2.0f), Vector3(2.0f, 3.0f, 2.0f)));    // a wall at +X
        world.AddBox(BoundingBox(Vector3(-5.0f, -1.0f, -5.0f), Vector3(5.0f, 0.0f, 5.0f)));  // the floor
        // Touching the wall by 0.1, standing 0.05 into the floor.
        const Capsule body{Vector3(0.8f, -0.05f, 0.0f), 0.3f, 1.8f};
        const auto contacts = world.Contacts(body);
        ASSERT_EQ(contacts.size(), 2u);
        bool wall = false;
        bool floor = false;
        for (const auto& contact : contacts) {
            if (contact.normal.X < -0.99f) {
                wall = true;
                EXPECT_NEAR(contact.depth, 0.1f, 1e-5f);
            }
            if (contact.normal.Y > 0.99f) {
                floor = true;
                EXPECT_NEAR(contact.depth, 0.05f, 1e-5f);
            }
        }
        EXPECT_TRUE(wall);
        EXPECT_TRUE(floor);
        EXPECT_GE(contacts.front().depth, contacts.back().depth);
        EXPECT_FALSE(world.Overlaps(Capsule{Vector3(0.0f, 0.5f, 0.0f), 0.3f, 1.8f}));
    }

    TEST(StaticWorldTest, AnUpperFloorDoesNotTouchTheFloorBelow)
    {
        // museum-simulator's case: a wall upstairs over a walker downstairs.
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(-1.0f, 3.0f, -1.0f), Vector3(1.0f, 6.0f, 1.0f)));
        EXPECT_FALSE(world.Overlaps(Capsule{Vector3(0.0f, 0.0f, 0.0f), 0.3f, 1.8f}));
        EXPECT_TRUE(world.Overlaps(Capsule{Vector3(0.0f, 1.5f, 0.0f), 0.3f, 1.8f}));
    }

    TEST(StaticWorldTest, ACapsuleInsideABoxOrCylinderLeavesTheShortestWay)
    {
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 4.0f, 1.0f)));
        auto contacts = world.Contacts(Capsule{Vector3(0.9f, 1.0f, 0.0f), 0.3f, 1.8f});
        ASSERT_FALSE(contacts.empty());
        ExpectNear(contacts.front().normal, Vector3(1.0f, 0.0f, 0.0f));
        EXPECT_NEAR(contacts.front().depth, 0.4f, 1e-5f);

        StaticWorld posts;
        posts.AddCylinder(Vector3::Zero, 0.5f, 0.2f);
        contacts = posts.Contacts(Capsule{Vector3(0.1f, 0.1f, 0.0f), 0.3f, 1.8f});
        ASSERT_FALSE(contacts.empty());
        ExpectNear(contacts.front().normal, Vector3::Up); // onto its low top
        EXPECT_NEAR(contacts.front().depth, 0.1f, 1e-5f);
    }

    TEST(StaticWorldTest, ShapesOverManyCellsAreReportedOnce)
    {
        StaticWorld world(1.0f);
        world.AddBox(BoundingBox(Vector3(-20.0f, 0.0f, -0.5f), Vector3(20.0f, 1.0f, 0.5f)));
        EXPECT_EQ(world.Contacts(Capsule{Vector3(0.0f, 0.5f, 0.0f), 0.3f, 1.8f}).size(), 1u);
        const auto hit = world.Raycast(Ray(Vector3(-30.0f, 0.5f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_FLOAT_EQ(hit->distance, 10.0f);
    }

    TEST(StaticWorldTest, MeshesRemovalAndBadInput)
    {
        StaticWorld world;
        const std::vector<Vector3> positions{Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 0, 1), Vector3(2, 0, 2)};
        const std::vector<std::uint32_t> indices{0, 1, 2, 0, 0, 1, 1, 3, 2};
        EXPECT_EQ(world.AddTriangles(positions, indices, Matrix::CreateTranslation(0.0f, 2.0f, 0.0f)), 2u);
        EXPECT_EQ(world.ShapeCount(), 2u);
        const auto hit = world.Raycast(Ray(Vector3(0.2f, 5.0f, 0.2f), Vector3(0.0f, -1.0f, 0.0f)));
        ASSERT_TRUE(hit.has_value());
        EXPECT_NEAR(hit->point.Y, 2.0f, 1e-5f);
        EXPECT_TRUE(world.Remove(hit->shape));
        EXPECT_FALSE(world.Remove(hit->shape));
        EXPECT_FALSE(world.Raycast(Ray(Vector3(0.2f, 5.0f, 0.2f), Vector3(0.0f, -1.0f, 0.0f))).has_value());
        world.Clear();
        EXPECT_EQ(world.ShapeCount(), 0u);

        EXPECT_THROW(StaticWorld(0.0f), std::invalid_argument);
        EXPECT_THROW(world.AddBox(BoundingBox(Vector3(1, 1, 1), Vector3(0, 0, 0))), std::invalid_argument);
        EXPECT_THROW(world.AddTriangle(Vector3::Zero, Vector3::Zero, Vector3(1, 0, 0)), std::invalid_argument);
        EXPECT_THROW(world.AddCylinder(Vector3::Zero, 0.0f, 1.0f), std::invalid_argument);
        EXPECT_THROW((void)world.Raycast(Ray(Vector3::Zero, Vector3::Zero)), std::invalid_argument);
        HeightField bad;
        bad.columns = 3;
        bad.rows = 3;
        EXPECT_THROW(world.SetHeightField(bad), std::invalid_argument);
    }
}
