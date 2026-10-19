// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Camera.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingFrustum;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Quaternion;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        bool Finite(const Vector3& v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); }

        void ValidateDepthRange(const float nearPlane, const float farPlane, const bool allowZeroNear)
        {
            if (!std::isfinite(nearPlane) || !std::isfinite(farPlane) || farPlane <= nearPlane
                || (allowZeroNear ? nearPlane < 0.0f : nearPlane <= 0.0f)) {
                throw std::invalid_argument("Camera: invalid near/far planes");
            }
        }

        void ValidatePositive(const float value, const char* message)
        {
            if (!std::isfinite(value) || value <= 0.0f) {
                throw std::invalid_argument(message);
            }
        }
    }

    Camera::Camera()
        : position_(Vector3::Zero),
          orientation_(Quaternion::Identity),
          view_(Matrix::getIdentityProperty()),
          projection_(Matrix::getIdentityProperty()),
          viewProjection_(Matrix::getIdentityProperty()),
          frustum_(Matrix::getIdentityProperty())
    {
        SetPerspective(MathHelper::PiOver4, 16.0f / 9.0f, 0.1f, 1000.0f);
        revision_ = 0;
    }

    void Camera::Invalidate() noexcept
    {
        dirty_ = true;
        ++revision_;
    }

    void Camera::SetPosition(const Vector3& position)
    {
        if (!Finite(position)) {
            throw std::invalid_argument("Camera: position must be finite");
        }
        position_ = position;
        Invalidate();
    }

    void Camera::SetOrientation(const Quaternion& orientation)
    {
        const float lengthSquared = orientation.LengthSquared();
        if (!std::isfinite(lengthSquared) || lengthSquared < 1e-12f) {
            throw std::invalid_argument("Camera: orientation must be a finite non-zero quaternion");
        }
        orientation_ = Quaternion::Normalize(orientation);
        Invalidate();
    }

    void Camera::SetYawPitchRoll(const float yaw, const float pitch, const float roll)
    {
        SetOrientation(Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll));
    }

    void Camera::LookAt(const Vector3& target, const Vector3& up)
    {
        const Vector3 direction = target - position_;
        if (!Finite(target) || direction.LengthSquared() < 1e-12f) {
            throw std::invalid_argument("Camera: LookAt target must be finite and differ from the position");
        }
        const Vector3 forward = Vector3::Normalize(direction);
        Vector3 upHint = up.LengthSquared() > 1e-12f ? Vector3::Normalize(up) : Vector3::Up;
        if (std::fabs(Vector3::Dot(forward, upHint)) > 0.9999f) {
            // Looking straight along the up hint: any perpendicular up works;
            // prefer the one that keeps "screen up" pointing towards -Z/+Z.
            upHint = std::fabs(forward.Y) > 0.5f ? (forward.Y > 0.0f ? Vector3::Backward : Vector3::Forward)
                                                 : Vector3::Up;
        }
        const Vector3 right = Vector3::Normalize(Vector3::Cross(forward, upHint));
        const Vector3 trueUp = Vector3::Cross(right, forward);
        // Rotation matrix whose Forward (-Z row) is `forward` and Up row is `trueUp`.
        Matrix rotation = Matrix::getIdentityProperty();
        rotation.M11 = right.X;
        rotation.M12 = right.Y;
        rotation.M13 = right.Z;
        rotation.M21 = trueUp.X;
        rotation.M22 = trueUp.Y;
        rotation.M23 = trueUp.Z;
        rotation.M31 = -forward.X;
        rotation.M32 = -forward.Y;
        rotation.M33 = -forward.Z;
        SetOrientation(Quaternion::CreateFromRotationMatrix(rotation));
    }

    Vector3 Camera::Forward() const { return Vector3::Normalize(Vector3::Transform(Vector3::Forward, orientation_)); }
    Vector3 Camera::Right() const { return Vector3::Normalize(Vector3::Transform(Vector3::Right, orientation_)); }
    Vector3 Camera::Up() const { return Vector3::Normalize(Vector3::Transform(Vector3::Up, orientation_)); }

    void Camera::SetPerspective(const float fieldOfView, const float aspectRatio, const float nearPlane,
                                const float farPlane)
    {
        if (!std::isfinite(fieldOfView) || fieldOfView <= 0.0f || fieldOfView >= MathHelper::Pi) {
            throw std::invalid_argument("Camera: field of view must be within (0, pi)");
        }
        ValidatePositive(aspectRatio, "Camera: aspect ratio must be positive");
        ValidateDepthRange(nearPlane, farPlane, false);
        kind_ = ProjectionKind::Perspective;
        fieldOfView_ = fieldOfView;
        aspectRatio_ = aspectRatio;
        nearPlane_ = nearPlane;
        farPlane_ = farPlane;
        Invalidate();
    }

    void Camera::SetOrthographic(const float width, const float height, const float nearPlane, const float farPlane)
    {
        ValidatePositive(width, "Camera: orthographic width must be positive");
        ValidatePositive(height, "Camera: orthographic height must be positive");
        ValidateDepthRange(nearPlane, farPlane, true);
        kind_ = ProjectionKind::Orthographic;
        orthoWidth_ = width;
        orthoHeight_ = height;
        aspectRatio_ = width / height;
        nearPlane_ = nearPlane;
        farPlane_ = farPlane;
        Invalidate();
    }

    void Camera::SetAspectRatio(const float aspectRatio)
    {
        ValidatePositive(aspectRatio, "Camera: aspect ratio must be positive");
        aspectRatio_ = aspectRatio;
        if (kind_ == ProjectionKind::Orthographic) {
            orthoWidth_ = orthoHeight_ * aspectRatio;
        }
        Invalidate();
    }

    void Camera::UpdateDerived() const
    {
        if (!dirty_) {
            return;
        }
        view_ = Matrix::CreateLookAt(position_, position_ + Forward(), Up());
        projection_ = kind_ == ProjectionKind::Perspective
                          ? Matrix::CreatePerspectiveFieldOfView(fieldOfView_, aspectRatio_, nearPlane_, farPlane_)
                          : Matrix::CreateOrthographic(orthoWidth_, orthoHeight_, nearPlane_, farPlane_);
        viewProjection_ = view_ * projection_;
        frustum_ = BoundingFrustum(viewProjection_);
        dirty_ = false;
    }

    const Matrix& Camera::View() const
    {
        UpdateDerived();
        return view_;
    }

    const Matrix& Camera::Projection() const
    {
        UpdateDerived();
        return projection_;
    }

    const Matrix& Camera::ViewProjection() const
    {
        UpdateDerived();
        return viewProjection_;
    }

    const BoundingFrustum& Camera::Frustum() const
    {
        UpdateDerived();
        return frustum_;
    }
}
