// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/CameraControllers.hpp"

#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    TEST(CameraControllersTests, FlyMovesAlongViewDirection)
    {
        Camera camera;
        FlyCameraController fly(FirstPersonSettings{2.0f, 3.0f, 1.5f});
        CameraControlInput input;
        input.pitch = MathHelper::PiOver2; // clamped to maxPitch
        input.move = Vector3(0.0f, 0.0f, 1.0f);
        fly.Update(camera, input, 0.5f);
        EXPECT_FLOAT_EQ(fly.Pitch(), 1.5f);
        EXPECT_NEAR(camera.Position().Length(), 1.0f, 1e-5f);
        EXPECT_GT(camera.Position().Y, 0.99f) << "flying follows the pitched view";

        input = CameraControlInput{};
        input.move = Vector3(0.0f, 0.0f, 1.0f);
        input.fast = true;
        const Vector3 before = camera.Position();
        fly.Update(camera, input, 0.5f);
        EXPECT_NEAR(Vector3::Distance(before, camera.Position()), 3.0f, 1e-5f);
    }

    TEST(CameraControllersTests, WalkStaysOnGroundAndIgnoresPitchForSpeed)
    {
        Camera camera;
        WalkCameraController walk(FirstPersonSettings{1.0f, 2.0f, 1.5f}, 1.5f,
                                  [](float x, float) { return x * 0.5f; });
        CameraControlInput input;
        input.pitch = -1.0f;
        input.yaw = -MathHelper::PiOver2; // turn right: forward becomes +X
        input.move = Vector3(0.0f, 5.0f, 1.0f);
        walk.Update(camera, input, 2.0f);
        EXPECT_NEAR(camera.Position().X, 2.0f, 1e-5f);
        EXPECT_NEAR(camera.Position().Z, 0.0f, 1e-5f);
        EXPECT_NEAR(camera.Position().Y, 1.0f + 1.5f, 1e-5f) << "ground(x) + eye height; move.Y ignored";

        input = CameraControlInput{};
        input.move = Vector3(1.0f, 0.0f, 1.0f);
        const Vector3 before = camera.Position();
        walk.Update(camera, input, 1.0f);
        const Vector3 delta = camera.Position() - before;
        EXPECT_NEAR(std::sqrt(delta.X * delta.X + delta.Z * delta.Z), 1.0f, 1e-5f) << "diagonal is not faster";
    }

    TEST(CameraControllersTests, SyncFromAdoptsCameraHeading)
    {
        Camera camera;
        camera.SetYawPitchRoll(0.7f, -0.3f);
        FlyCameraController fly;
        fly.SyncFrom(camera);
        EXPECT_NEAR(fly.Yaw(), 0.7f, 1e-5f);
        EXPECT_NEAR(fly.Pitch(), -0.3f, 1e-5f);
        const Vector3 forward = camera.Forward();
        fly.Update(camera, CameraControlInput{}, 0.016f);
        EXPECT_NEAR(Vector3::Distance(forward, camera.Forward()), 0.0f, 1e-5f) << "no snap on the first update";
    }

    TEST(CameraControllersTests, ReadCameraInputMapsAxesAndFastAction)
    {
        Input::ActionMap actions;
        CameraInputAxes axes;
        axes.moveRight = actions.AddAxis("MoveRight");
        axes.moveForward = actions.AddAxis("MoveForward");
        axes.lookYaw = actions.AddAxis("LookYaw");
        axes.lookPitch = actions.AddAxis("LookPitch");
        axes.fast = actions.AddAction("Fast");
        axes.lookScale = 0.5f;
        actions.BindAxis(axes.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
        actions.BindAxis(axes.lookYaw, Input::AxisBinding::ForKeys(Keys::Right, Keys::Left));
        actions.Bind(*axes.fast, Input::Binding::ForKey(Keys::LeftShift));

        Input::InputSnapshot snapshot;
        snapshot.keyboard = KeyboardState({Keys::W, Keys::Left, Keys::LeftShift});
        actions.Update(snapshot);
        const CameraControlInput input = ReadCameraInput(actions, axes);
        EXPECT_FLOAT_EQ(input.move.Z, 1.0f);
        EXPECT_FLOAT_EQ(input.move.Y, 0.0f);
        EXPECT_FLOAT_EQ(input.yaw, 0.5f);
        EXPECT_TRUE(input.fast);
    }

    TEST(CameraControllersTests, OrbitCirclesItsTargetAndLooksAtIt)
    {
        using Microsoft::Xna::Framework::Matrix;
        Camera camera;
        OrbitCameraController orbit(OrbitSettings{1.0f, 50.0f, 1.2f, 10.0f});
        orbit.SetTarget(Vector3(1.0f, 2.0f, 3.0f));
        orbit.SetYaw(0.0f);
        orbit.SetPitch(0.0f);
        orbit.SetDistance(5.0f);
        orbit.Apply(camera);
        EXPECT_EQ(camera.Position(), Vector3(1.0f, 2.0f, 8.0f)) << "yaw 0 views from +Z";
        EXPECT_EQ(camera.View().M11, Matrix::CreateLookAt(Vector3(1.0f, 2.0f, 8.0f), Vector3(1.0f, 2.0f, 3.0f), Vector3::Up).M11);

        CameraControlInput input;
        input.yaw = MathHelper::PiOver2;      // look left: the camera moves to +X
        input.pitch = -0.5f;                  // look down: the camera rises
        input.move = Vector3(0.0f, 0.0f, 1.0f);
        orbit.Update(camera, input, 0.1f);
        EXPECT_NEAR(camera.Position().X, 1.0f + 4.0f * std::cos(0.5f), 1e-4f);
        EXPECT_GT(camera.Position().Y, 2.0f);
        EXPECT_NEAR(orbit.Distance(), 4.0f, 1e-5f) << "forward zooms in";
        EXPECT_NEAR(Vector3::Dot(camera.Forward(), Vector3::Normalize(orbit.Target() - camera.Position())), 1.0f, 1e-5f);

        input = CameraControlInput{};
        input.pitch = -10.0f;
        input.move = Vector3(0.0f, 0.0f, 100.0f);
        orbit.Update(camera, input, 1.0f);
        EXPECT_FLOAT_EQ(orbit.Pitch(), 1.2f) << "pitch is clamped";
        EXPECT_FLOAT_EQ(orbit.Distance(), 1.0f) << "distance is clamped";

        // Adopting a camera's position keeps the view where it is.
        OrbitCameraController adopted;
        adopted.SetTarget(orbit.Target());
        adopted.SyncFrom(camera);
        EXPECT_NEAR(Vector3::Distance(adopted.Position(), camera.Position()), 0.0f, 1e-4f);
    }

    TEST(CameraControllersTests, FollowClosesTheGapAtAnyFrameRate)
    {
        Camera camera;
        FollowCameraController follow(Vector3(0.0f, 2.0f, 6.0f), 0.5f);
        follow.SetTargetPosition(Vector3(10.0f, 0.0f, 0.0f));
        // Two 1/60 s steps close as much as one 1/30 s step.
        FollowCameraController coarse = follow;
        follow.Update(camera, {}, 1.0f / 60.0f);
        follow.Update(camera, {}, 1.0f / 60.0f);
        coarse.Update(camera, {}, 1.0f / 30.0f);
        EXPECT_NEAR(Vector3::Distance(follow.Position(), coarse.Position()), 0.0f, 1e-5f);
        EXPECT_NEAR(follow.Position().X, 10.0f * 0.75f, 1e-4f);
        EXPECT_NEAR(Vector3::Dot(camera.Forward(), Vector3::Normalize(follow.TargetPosition() - camera.Position())), 1.0f,
                    1e-5f);
        follow.Snap();
        EXPECT_EQ(follow.Position(), Vector3(10.0f, 2.0f, 6.0f));
        FollowCameraController rigid(Vector3(0.0f, 1.0f, 1.0f), 1.0f);
        rigid.SetTargetPosition(Vector3(4.0f, 0.0f, 0.0f));
        rigid.Update(camera, {}, 0.001f);
        EXPECT_EQ(rigid.Position(), Vector3(4.0f, 1.0f, 1.0f));
    }

    TEST(CameraControllersTests, FollowCutsAndAppliesWithoutMoving)
    {
        using Microsoft::Xna::Framework::Matrix;
        Camera camera;
        FollowCameraController follow(Vector3(0.0f, 2.0f, 6.0f), 0.5f);
        follow.SetTargetPosition(Vector3(10.0f, 0.0f, 0.0f));
        follow.SetPosition(Vector3(-3.0f, 4.0f, 5.0f));
        follow.Apply(camera);
        EXPECT_EQ(follow.Position(), Vector3(-3.0f, 4.0f, 5.0f)) << "Apply does not move the camera on";
        EXPECT_EQ(camera.Position(), Vector3(-3.0f, 4.0f, 5.0f));
        EXPECT_EQ(camera.View(), Matrix::CreateLookAt(Vector3(-3.0f, 4.0f, 5.0f), Vector3(10.0f, 0.0f, 0.0f),
                                                      Vector3::Up));
        const auto revision = camera.Revision();
        follow.SetPosition(follow.TargetPosition());
        follow.Apply(camera);
        EXPECT_EQ(camera.Revision(), revision) << "no view while the camera stands on its target";
    }
}

