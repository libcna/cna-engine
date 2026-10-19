// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/WorldCameraSync.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Extended
{
    using CNA::Extended::World3DEXT::Camera3DEXT;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Vector3;

    TEST(WorldCameraSyncTests, EngineCameraDrivesWorldCameraMatrices)
    {
        Graphics::Camera camera;
        camera.SetPosition(Vector3(2.0f, 3.0f, 10.0f));
        camera.LookAt(Vector3(0.0f, 1.0f, 0.0f));
        camera.SetPerspective(MathHelper::PiOver4, 1.25f, 0.25f, 300.0f);

        Camera3DEXT world;
        EXPECT_TRUE(SyncToWorldCamera(camera, world));
        const auto view = world.GetViewMatrixEXT();
        const auto expected = camera.View();
        EXPECT_NEAR(view.M11, expected.M11, 1e-4f);
        EXPECT_NEAR(view.M32, expected.M32, 1e-4f);
        EXPECT_NEAR(view.M43, expected.M43, 1e-4f);
        EXPECT_FLOAT_EQ(world.getFieldOfViewProperty(), MathHelper::PiOver4);
        EXPECT_FLOAT_EQ(world.getFarPlaneProperty(), 300.0f);
    }

    TEST(WorldCameraSyncTests, OrthographicCameraIsReportedAsUnsupported)
    {
        Graphics::Camera camera;
        camera.SetOrthographic(10.0f, 5.0f, 0.0f, 50.0f);
        Camera3DEXT world;
        EXPECT_FALSE(SyncToWorldCamera(camera, world));
        EXPECT_FLOAT_EQ(world.getAspectRatioProperty(), 2.0f);
    }

    TEST(WorldCameraSyncTests, RoundTripFromWorldCamera)
    {
        Camera3DEXT world;
        world.setPositionProperty(Vector3(0.0f, 5.0f, 5.0f));
        world.setTargetProperty(Vector3::Zero);
        world.setFieldOfViewProperty(1.0f);
        world.setAspectRatioProperty(2.0f);
        world.setNearPlaneProperty(0.5f);
        world.setFarPlaneProperty(50.0f);

        Graphics::Camera camera;
        SyncFromWorldCamera(world, camera);
        const Vector3 forward = camera.Forward();
        EXPECT_NEAR(forward.Y, forward.Z, 1e-5f); // looking down and towards -Z at 45 degrees
        EXPECT_LT(forward.Y, 0.0f);
        EXPECT_FLOAT_EQ(camera.FieldOfView(), 1.0f);
        EXPECT_FLOAT_EQ(camera.AspectRatio(), 2.0f);
    }
}
