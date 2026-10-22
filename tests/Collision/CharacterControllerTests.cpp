// SPDX-License-Identifier: MIT
#include "CnaEngine/Collision/CharacterController.hpp"

#include "CnaEngine/Core/FixedStepClock.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

namespace CnaEngine::Collision
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        constexpr float Step = 1.0f / 120.0f;

        StaticWorld Floor()
        {
            StaticWorld world;
            world.AddBox(BoundingBox(Vector3(-50.0f, -1.0f, -50.0f), Vector3(50.0f, 0.0f, 50.0f)), 1);
            return world;
        }

        void Walk(CharacterController& body, const StaticWorld& world, const Vector3& wish, const float seconds,
                 const bool jump = false)
        {
            const int steps = static_cast<int>(std::lround(seconds / Step));
            for (int i = 0; i < steps; ++i) {
                body.Step(world, wish, jump && i == 0, Step);
            }
        }

        CharacterController Standing(const StaticWorld& world, const Vector3& feet = Vector3::Zero)
        {
            CharacterController body;
            body.Teleport(world, feet);
            return body;
        }
    }

    TEST(CharacterControllerTest, FallsAndLandsOnTheFloor)
    {
        const auto world = Floor();
        CharacterController body({}, Vector3(0.0f, 3.0f, 0.0f));
        Walk(body, world, Vector3::Zero, 2.0f);
        EXPECT_TRUE(body.Grounded());
        EXPECT_NEAR(body.Feet().Y, 0.0f, 1e-3f);
        EXPECT_EQ(body.GroundTag(), 1u);
        EXPECT_NEAR(body.Velocity().Y, 0.0f, 1e-6f);
        EXPECT_NEAR(body.Eye().Y, 1.68f, 1e-3f);
    }

    TEST(CharacterControllerTest, AWallStopsTheBodyAndTakesItsSpeed)
    {
        auto world = Floor();
        world.AddBox(BoundingBox(Vector3(3.0f, 0.0f, -10.0f), Vector3(4.0f, 3.0f, 10.0f)));
        auto body = Standing(world);
        Walk(body, world, Vector3(3.0f, 0.0f, 0.0f), 3.0f);
        EXPECT_NEAR(body.Feet().X, 2.7f, 0.01f);
        EXPECT_TRUE(body.HitWall());
        EXPECT_NEAR(body.Velocity().X, 0.0f, 0.05f);
        EXPECT_TRUE(body.Grounded());
    }

    TEST(CharacterControllerTest, ItSlidesAlongAWall)
    {
        auto world = Floor();
        world.AddBox(BoundingBox(Vector3(3.0f, 0.0f, -20.0f), Vector3(4.0f, 3.0f, 20.0f)));
        auto body = Standing(world);
        Walk(body, world, Vector3(2.0f, 0.0f, 2.0f), 4.0f);
        EXPECT_NEAR(body.Feet().X, 2.7f, 0.01f);
        EXPECT_GT(body.Feet().Z, 6.0f); // the along-wall half of the wish is kept
    }

    TEST(CharacterControllerTest, ItStepsOntoAKerbButNotOntoABlock)
    {
        auto world = Floor();
        world.AddBox(BoundingBox(Vector3(2.0f, 0.0f, -5.0f), Vector3(6.0f, 0.2f, 5.0f)));   // kerb
        world.AddBox(BoundingBox(Vector3(2.0f, 0.0f, 10.0f), Vector3(6.0f, 0.6f, 20.0f))); // block
        auto kerb = Standing(world);
        Walk(kerb, world, Vector3(2.0f, 0.0f, 0.0f), 2.0f);
        EXPECT_GT(kerb.Feet().X, 3.0f);
        EXPECT_NEAR(kerb.Feet().Y, 0.2f, 1e-3f);
        EXPECT_TRUE(kerb.Grounded());

        auto block = Standing(world, Vector3(0.0f, 0.0f, 15.0f));
        Walk(block, world, Vector3(2.0f, 0.0f, 0.0f), 2.0f);
        EXPECT_NEAR(block.Feet().X, 1.7f, 0.01f);
        EXPECT_NEAR(block.Feet().Y, 0.0f, 1e-3f);
    }

    TEST(CharacterControllerTest, ItClimbsStairsAndAStepHigherThanItsRadius)
    {
        StaticWorld world;
        world.AddBox(BoundingBox(Vector3(-5.0f, -1.0f, -2.0f), Vector3(0.0f, 0.0f, 2.0f)));
        // Five 0.17 m risers, 0.28 m treads, then a landing.
        for (int i = 0; i < 5; ++i) {
            world.AddBox(BoundingBox(Vector3(0.28f * static_cast<float>(i), -1.0f, -2.0f),
                                     Vector3(0.28f * static_cast<float>(i + 1), 0.17f * static_cast<float>(i + 1), 2.0f)));
        }
        world.AddBox(BoundingBox(Vector3(1.4f, -1.0f, -2.0f), Vector3(6.0f, 0.85f, 2.0f)));
        auto body = Standing(world, Vector3(-1.0f, 0.0f, 0.0f));
        Walk(body, world, Vector3(1.5f, 0.0f, 0.0f), 3.0f);
        EXPECT_GT(body.Feet().X, 2.0f);
        EXPECT_NEAR(body.Feet().Y, 0.85f, 1e-3f);
        EXPECT_TRUE(body.Grounded());

        // A single 0.33 m step: above the lower sphere's centre, so the body
        // lifts by the step height and walks on.
        StaticWorld ledge;
        ledge.AddBox(BoundingBox(Vector3(-5.0f, -1.0f, -2.0f), Vector3(1.0f, 0.0f, 2.0f)));
        ledge.AddBox(BoundingBox(Vector3(1.0f, -1.0f, -2.0f), Vector3(6.0f, 0.33f, 2.0f)));
        auto climber = Standing(ledge);
        Walk(climber, ledge, Vector3(1.5f, 0.0f, 0.0f), 2.0f);
        EXPECT_GT(climber.Feet().X, 1.5f);
        EXPECT_NEAR(climber.Feet().Y, 0.33f, 1e-3f);
    }

    TEST(CharacterControllerTest, ItWalksDownStairsWithoutFalling)
    {
        StaticWorld world;
        // Five 0.15 m steps down along +X, 0.3 m deep, then the floor.
        for (int i = 0; i < 5; ++i) {
            const float top = 0.75f - 0.15f * static_cast<float>(i);
            world.AddBox(BoundingBox(Vector3(0.3f * static_cast<float>(i) - 5.0f, -1.0f, -2.0f),
                                     Vector3(0.3f * static_cast<float>(i + 1), top, 2.0f)));
        }
        world.AddBox(BoundingBox(Vector3(1.5f, -1.0f, -2.0f), Vector3(10.0f, 0.0f, 2.0f)));
        auto body = Standing(world, Vector3(-0.5f, 0.75f, 0.0f));
        ASSERT_TRUE(body.Grounded());
        bool alwaysGrounded = true;
        for (int i = 0; i < 360; ++i) {
            body.Step(world, Vector3(1.5f, 0.0f, 0.0f), false, Step);
            alwaysGrounded = alwaysGrounded && body.Grounded();
        }
        EXPECT_TRUE(alwaysGrounded);
        EXPECT_GT(body.Feet().X, 2.5f);
        EXPECT_NEAR(body.Feet().Y, 0.0f, 1e-3f);
    }

    TEST(CharacterControllerTest, ItFallsOffALedgeAndLands)
    {
        auto world = Floor();
        world.AddBox(BoundingBox(Vector3(-5.0f, 0.0f, -5.0f), Vector3(1.0f, 2.0f, 5.0f)));
        auto body = Standing(world, Vector3(0.0f, 2.0f, 0.0f));
        ASSERT_NEAR(body.Feet().Y, 2.0f, 1e-3f);
        bool fell = false;
        for (int i = 0; i < 240; ++i) {
            body.Step(world, Vector3(2.0f, 0.0f, 0.0f), false, Step);
            fell = fell || !body.Grounded();
        }
        EXPECT_TRUE(fell);
        EXPECT_TRUE(body.Grounded());
        EXPECT_NEAR(body.Feet().Y, 0.0f, 1e-3f);
        EXPECT_GT(body.Feet().X, 1.3f);
    }

    TEST(CharacterControllerTest, GentleSlopesAreWalkedAndSteepOnesAreWalls)
    {
        const auto ramp = [](const float degrees) {
            StaticWorld world;
            world.AddBox(BoundingBox(Vector3(-10.0f, -1.0f, -5.0f), Vector3(1.0f, 0.0f, 5.0f)));
            const float rise = 10.0f * std::tan(degrees * 3.14159265f / 180.0f);
            world.AddTriangle(Vector3(1.0f, 0.0f, -5.0f), Vector3(11.0f, rise, -5.0f), Vector3(1.0f, 0.0f, 5.0f));
            world.AddTriangle(Vector3(11.0f, rise, -5.0f), Vector3(11.0f, rise, 5.0f), Vector3(1.0f, 0.0f, 5.0f));
            return world;
        };
        const auto gentle = ramp(20.0f);
        auto walker = Standing(gentle);
        Walk(walker, gentle, Vector3(2.0f, 0.0f, 0.0f), 3.0f);
        EXPECT_GT(walker.Feet().X, 4.0f);
        EXPECT_NEAR(walker.Feet().Y, (walker.Feet().X - 1.0f) * std::tan(20.0f * 3.14159265f / 180.0f), 0.05f);
        EXPECT_TRUE(walker.Grounded());

        const auto steep = ramp(60.0f);
        auto climber = Standing(steep);
        Walk(climber, steep, Vector3(2.0f, 0.0f, 0.0f), 3.0f);
        EXPECT_LT(climber.Feet().X, 1.3f);
        EXPECT_LT(climber.Feet().Y, 0.6f);
    }

    TEST(CharacterControllerTest, AJumpReachesItsHeight)
    {
        const auto world = Floor();
        auto body = Standing(world);
        float highest = 0.0f;
        body.Step(world, Vector3::Zero, true, Step);
        for (int i = 0; i < 240; ++i) {
            body.Step(world, Vector3::Zero, false, Step);
            highest = std::max(highest, body.Feet().Y);
        }
        const float expected = 4.5f * 4.5f / (2.0f * 9.81f);
        EXPECT_NEAR(highest, expected, expected * 0.05f);
        EXPECT_TRUE(body.Grounded());
        EXPECT_NEAR(body.Feet().Y, 0.0f, 1e-3f);
    }

    TEST(CharacterControllerTest, TheSameStepsGiveTheSamePlaceAtAnyFrameRate)
    {
        auto world = Floor();
        world.AddBox(BoundingBox(Vector3(2.0f, 0.0f, -1.0f), Vector3(3.0f, 0.2f, 1.0f)));
        world.AddCylinder(Vector3(4.0f, 0.0f, 1.0f), 0.4f, 2.0f);
        Vector3 ends[3];
        const double frames[3] = {1.0 / 30.0, 1.0 / 60.0, 1.0 / 144.0};
        for (int run = 0; run < 3; ++run) {
            auto body = Standing(world);
            FixedStepClock clock(1.0 / 120.0);
            int taken = 0;
            while (taken < 480) {
                clock.Run(frames[run], [&](const double step) {
                    if (taken < 480) {
                        ++taken;
                        body.Step(world, Vector3(2.0f, 0.0f, 0.4f), false, static_cast<float>(step));
                    }
                });
            }
            ends[run] = body.Feet();
        }
        for (int run = 1; run < 3; ++run) {
            EXPECT_EQ(ends[run].X, ends[0].X);
            EXPECT_EQ(ends[run].Y, ends[0].Y);
            EXPECT_EQ(ends[run].Z, ends[0].Z);
        }
    }

    TEST(CharacterControllerTest, StridesAreCountedOnTheGround)
    {
        const auto world = Floor();
        auto body = Standing(world);
        int footsteps = 0;
        for (int i = 0; i < 240; ++i) {
            body.Step(world, Vector3(1.5f, 0.0f, 0.0f), false, Step);
            footsteps += body.StrideTaken() ? 1 : 0;
        }
        EXPECT_NEAR(body.DistanceWalked(), body.Feet().X, 1e-3);
        EXPECT_EQ(static_cast<std::uint64_t>(footsteps), body.StrideCount());
        EXPECT_EQ(body.StrideCount(), static_cast<std::uint64_t>(body.Feet().X / 0.75f));
        EXPECT_GE(body.HeadBob(0.02f), 0.0f);
        EXPECT_LE(body.HeadBob(0.02f), 0.02f);
    }

    TEST(CharacterControllerTest, ItFollowsAHeightField)
    {
        StaticWorld world;
        HeightField hills;
        hills.origin = Vector3(-20.0f, 0.0f, -20.0f);
        hills.cellSize = 1.0f;
        hills.columns = 41;
        hills.rows = 41;
        for (int z = 0; z < 41; ++z) {
            for (int x = 0; x < 41; ++x) {
                hills.heights.push_back(0.5f * std::sin(static_cast<float>(x) * 0.3f));
            }
        }
        world.SetHeightField(hills);
        auto body = Standing(world, Vector3(0.0f, 2.0f, 0.0f));
        Walk(body, world, Vector3(2.0f, 0.0f, 0.0f), 3.0f);
        ASSERT_TRUE(world.Terrain()->HeightAt(body.Feet().X, body.Feet().Z).has_value());
        EXPECT_NEAR(body.Feet().Y, *world.Terrain()->HeightAt(body.Feet().X, body.Feet().Z), 0.03f);
        EXPECT_TRUE(body.Grounded());
    }

    TEST(CharacterControllerTest, WishesFollowTheViewAndDiagonalsAreNotFaster)
    {
        const Vector3 ahead = WishVelocity(Vector3(0.0f, 0.0f, 1.0f), 0.0f, 3.0f);
        EXPECT_NEAR(ahead.Z, -3.0f, 1e-6f);
        const Vector3 left = WishVelocity(Vector3(0.0f, 0.0f, 1.0f), 1.5707963f, 3.0f); // yaw 90: looking along -X
        EXPECT_NEAR(left.X, -3.0f, 1e-5f);
        const Vector3 diagonal = WishVelocity(Vector3(1.0f, 0.0f, 1.0f), 0.3f, 3.0f);
        EXPECT_NEAR(std::sqrt(diagonal.X * diagonal.X + diagonal.Z * diagonal.Z), 3.0f, 1e-5f);
        const Vector3 half = WishVelocity(Vector3(0.0f, 0.0f, 0.5f), 0.0f, 3.0f);
        EXPECT_NEAR(half.Z, -1.5f, 1e-6f);
        EXPECT_EQ(half.Y, 0.0f);
    }

    TEST(CharacterControllerTest, ImpossibleBodiesAreRejected)
    {
        CharacterSettings settings;
        settings.height = 0.5f; // shorter than its two half spheres
        EXPECT_THROW(CharacterController{settings}, std::invalid_argument);
        settings = {};
        settings.maxSlopeDegrees = 90.0f;
        EXPECT_THROW(CharacterController{settings}, std::invalid_argument);
    }
}
