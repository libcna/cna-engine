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

        bool Invertible(const Matrix& m)
        {
            const float elements[] = {m.M11, m.M12, m.M13, m.M14, m.M21, m.M22, m.M23, m.M24,
                                      m.M31, m.M32, m.M33, m.M34, m.M41, m.M42, m.M43, m.M44};
            for (const float e : elements) {
                if (!std::isfinite(e)) {
                    return false;
                }
            }
            const float determinant = m.Determinant();
            return std::isfinite(determinant) && determinant != 0.0f;
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
        hasCustomView_ = false;
        Invalidate();
    }

    void Camera::SetOrientation(const Quaternion& orientation)
    {
        const float lengthSquared = orientation.LengthSquared();
        if (!std::isfinite(lengthSquared) || lengthSquared < 1e-12f) {
            throw std::invalid_argument("Camera: orientation must be a finite non-zero quaternion");
        }
        orientation_ = Quaternion::Normalize(orientation);
        hasCustomView_ = false;
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
        if (kind_ == ProjectionKind::Custom) {
            throw std::invalid_argument("Camera: a custom projection has no aspect ratio to change");
        }
        ValidatePositive(aspectRatio, "Camera: aspect ratio must be positive");
        aspectRatio_ = aspectRatio;
        if (kind_ == ProjectionKind::Orthographic) {
            orthoWidth_ = orthoHeight_ * aspectRatio;
        }
        Invalidate();
    }

    void Camera::SetMatrices(const Matrix& view, const Matrix& projection)
    {
        if (!Invertible(view) || !Invertible(projection)) {
            throw std::invalid_argument("Camera: custom matrices must be finite and invertible");
        }
        // The pose the view stands for: the camera's world matrix is its inverse.
        const Matrix world = Matrix::Invert(view);
        const Vector3 right = Vector3::Normalize(Vector3(world.M11, world.M12, world.M13));
        const Vector3 up = Vector3::Normalize(Vector3(world.M21, world.M22, world.M23));
        const Vector3 backward = Vector3::Normalize(Vector3(world.M31, world.M32, world.M33));
        Matrix rotation = Matrix::getIdentityProperty();
        rotation.M11 = right.X;
        rotation.M12 = right.Y;
        rotation.M13 = right.Z;
        rotation.M21 = up.X;
        rotation.M22 = up.Y;
        rotation.M23 = up.Z;
        rotation.M31 = backward.X;
        rotation.M32 = backward.Y;
        rotation.M33 = backward.Z;
        position_ = world.getTranslationProperty();
        orientation_ = Quaternion::Normalize(Quaternion::CreateFromRotationMatrix(rotation));
        customView_ = view;
        customProjection_ = projection;
        hasCustomView_ = true;
        kind_ = ProjectionKind::Custom;
        // CreatePerspectiveFieldOfView: M11 = M22 / aspect, M22 = 1 / tan(fov / 2),
        // M33 = far / (near - far), M34 = -1, M43 = near * far / (near - far), the rest 0.
        const bool perspective = projection.M34 == -1.0f && projection.M44 == 0.0f && projection.M12 == 0.0f
                                 && projection.M21 == 0.0f && projection.M31 == 0.0f && projection.M32 == 0.0f
                                 && projection.M11 > 0.0f && projection.M22 > 0.0f && projection.M33 < 0.0f;
        fieldOfView_ = perspective ? 2.0f * std::atan(1.0f / projection.M22) : 0.0f;
        aspectRatio_ = perspective ? projection.M22 / projection.M11 : 0.0f;
        nearPlane_ = perspective ? projection.M43 / projection.M33 : 0.0f;
        farPlane_ = perspective && projection.M33 != -1.0f ? projection.M43 / (projection.M33 + 1.0f) : 0.0f;
        orthoWidth_ = 0.0f;
        orthoHeight_ = 0.0f;
        Invalidate();
    }

    void Camera::SetMirrored(const bool mirrored)
    {
        mirrored_ = mirrored;
        Invalidate();
    }

    void Camera::UpdateDerived() const
    {
        if (!dirty_) {
            return;
        }
        view_ = hasCustomView_ ? customView_ : Matrix::CreateLookAt(position_, position_ + Forward(), Up());
        switch (kind_) {
            case ProjectionKind::Perspective:
                projection_ = Matrix::CreatePerspectiveFieldOfView(fieldOfView_, aspectRatio_, nearPlane_, farPlane_);
                break;
            case ProjectionKind::Orthographic:
                projection_ = Matrix::CreateOrthographic(orthoWidth_, orthoHeight_, nearPlane_, farPlane_);
                break;
            case ProjectionKind::Custom:
                projection_ = customProjection_;
                break;
        }
        // The volume is the unmirrored one; the mirror only flips the image.
        frustum_ = BoundingFrustum(view_ * projection_);
        if (mirrored_) {
            projection_ = projection_ * Matrix::CreateScale(-1.0f, 1.0f, 1.0f);
        }
        viewProjection_ = view_ * projection_;
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
