// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Camera.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/ContainmentType.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Quaternion;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        void ExpectNear(const Vector3& actual, const Vector3& expected, float tolerance = 1e-5f)
        {
            EXPECT_NEAR(actual.X, expected.X, tolerance);
            EXPECT_NEAR(actual.Y, expected.Y, tolerance);
            EXPECT_NEAR(actual.Z, expected.Z, tolerance);
        }

        void ExpectNear(const Matrix& actual, const Matrix& expected, float tolerance = 1e-5f)
        {
            const float a[] = {actual.M11, actual.M12, actual.M13, actual.M14, actual.M21, actual.M22,
                               actual.M23, actual.M24, actual.M31, actual.M32, actual.M33, actual.M34,
                               actual.M41, actual.M42, actual.M43, actual.M44};
            const float e[] = {expected.M11, expected.M12, expected.M13, expected.M14, expected.M21, expected.M22,
                               expected.M23, expected.M24, expected.M31, expected.M32, expected.M33, expected.M34,
                               expected.M41, expected.M42, expected.M43, expected.M44};
            for (int i = 0; i < 16; ++i) {
                EXPECT_NEAR(a[i], e[i], tolerance) << "element " << i;
            }
        }
    }

    TEST(CameraTests, DefaultLooksDownNegativeZ)
    {
        const Camera camera;
        ExpectNear(camera.Forward(), Vector3(0.0f, 0.0f, -1.0f));
        ExpectNear(camera.Right(), Vector3(1.0f, 0.0f, 0.0f));
        ExpectNear(camera.Up(), Vector3(0.0f, 1.0f, 0.0f));
        EXPECT_EQ(camera.Kind(), ProjectionKind::Perspective);
        EXPECT_EQ(camera.Revision(), 0u);
        ExpectNear(camera.View(), Matrix::getIdentityProperty());
    }

    TEST(CameraTests, MatricesMatchXnaFactories)
    {
        Camera camera;
        camera.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
        camera.LookAt(Vector3(4.0f, 0.0f, -5.0f));
        camera.SetPerspective(MathHelper::PiOver4, 1.5f, 0.5f, 200.0f);

        const Matrix view = Matrix::CreateLookAt(Vector3(1.0f, 2.0f, 3.0f), Vector3(4.0f, 0.0f, -5.0f), Vector3::Up);
        const Matrix projection = Matrix::CreatePerspectiveFieldOfView(MathHelper::PiOver4, 1.5f, 0.5f, 200.0f);
        ExpectNear(camera.View(), view, 1e-4f);
        ExpectNear(camera.Projection(), projection);
        ExpectNear(camera.ViewProjection(), view * projection, 1e-4f);
    }

    TEST(CameraTests, YawAndPitchFollowDocumentedSigns)
    {
        Camera camera;
        camera.SetYawPitchRoll(MathHelper::PiOver2, 0.0f);
        ExpectNear(camera.Forward(), Vector3(-1.0f, 0.0f, 0.0f)); // positive yaw turns left
        camera.SetYawPitchRoll(0.0f, MathHelper::PiOver4);
        EXPECT_GT(camera.Forward().Y, 0.7f) << "positive pitch looks up";
    }

    TEST(CameraTests, LookAtAlongUpStaysValid)
    {
        Camera camera;
        camera.SetPosition(Vector3(0.0f, 10.0f, 0.0f));
        camera.LookAt(Vector3::Zero);
        ExpectNear(camera.Forward(), Vector3(0.0f, -1.0f, 0.0f));
        EXPECT_NEAR(Vector3::Dot(camera.Forward(), camera.Up()), 0.0f, 1e-5f);
        EXPECT_NEAR(camera.Up().Length(), 1.0f, 1e-5f);
        EXPECT_THROW(camera.LookAt(Vector3(0.0f, 10.0f, 0.0f)), std::invalid_argument);
    }

    TEST(CameraTests, SettersInvalidateDerivedValuesAndBumpRevision)
    {
        Camera camera;
        const Matrix before = camera.View();
        const auto revision = camera.Revision();
        camera.SetPosition(Vector3(0.0f, 0.0f, 5.0f));
        EXPECT_GT(camera.Revision(), revision);
        EXPECT_NE(camera.View().M43, before.M43);

        const auto beforeProjection = camera.Projection().M11;
        camera.SetAspectRatio(1.0f);
        EXPECT_NE(camera.Projection().M11, beforeProjection);
    }

    TEST(CameraTests, InvalidArgumentsLeaveCameraUnchanged)
    {
        Camera camera;
        const auto revision = camera.Revision();
        EXPECT_THROW(camera.SetPerspective(0.0f, 1.0f, 0.1f, 10.0f), std::invalid_argument);
        EXPECT_THROW(camera.SetPerspective(MathHelper::Pi, 1.0f, 0.1f, 10.0f), std::invalid_argument);
        EXPECT_THROW(camera.SetPerspective(1.0f, 0.0f, 0.1f, 10.0f), std::invalid_argument);
        EXPECT_THROW(camera.SetPerspective(1.0f, 1.0f, 0.0f, 10.0f), std::invalid_argument);
        EXPECT_THROW(camera.SetPerspective(1.0f, 1.0f, 5.0f, 5.0f), std::invalid_argument);
        EXPECT_THROW(camera.SetOrthographic(0.0f, 1.0f, 0.0f, 1.0f), std::invalid_argument);
        EXPECT_THROW(camera.SetOrientation(Quaternion(0.0f, 0.0f, 0.0f, 0.0f)), std::invalid_argument);
        EXPECT_EQ(camera.Revision(), revision);
        EXPECT_EQ(camera.Kind(), ProjectionKind::Perspective);
    }

    TEST(CameraTests, OrthographicKeepsHeightOnResize)
    {
        Camera camera;
        camera.SetOrthographic(20.0f, 10.0f, 0.0f, 100.0f);
        EXPECT_EQ(camera.Kind(), ProjectionKind::Orthographic);
        ExpectNear(camera.Projection(), Matrix::CreateOrthographic(20.0f, 10.0f, 0.0f, 100.0f));
        camera.SetAspectRatio(1.0f);
        EXPECT_FLOAT_EQ(camera.OrthographicHeight(), 10.0f);
        EXPECT_FLOAT_EQ(camera.OrthographicWidth(), 10.0f);
    }

    TEST(CameraTests, FrustumContainsWhatTheCameraSees)
    {
        Camera camera;
        camera.SetPosition(Vector3(0.0f, 0.0f, 10.0f));
        camera.LookAt(Vector3::Zero);
        camera.SetPerspective(MathHelper::PiOver4, 1.0f, 0.1f, 100.0f);
        EXPECT_EQ(camera.Frustum().Contains(BoundingSphere(Vector3::Zero, 1.0f)), ContainmentType::Contains);
        EXPECT_EQ(camera.Frustum().Contains(BoundingSphere(Vector3(0.0f, 0.0f, 20.0f), 1.0f)),
                  ContainmentType::Disjoint)
            << "behind the camera";
        EXPECT_EQ(camera.Frustum().Contains(BoundingSphere(Vector3(0.0f, 0.0f, -95.0f), 10.0f)),
                  ContainmentType::Intersects)
            << "straddling the far plane";
    }
}
