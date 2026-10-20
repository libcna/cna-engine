// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>

namespace CnaEngine::Graphics
{
    enum class ProjectionKind : std::uint8_t
    {
        Perspective,
        Orthographic,
    };

    /// Camera state: a pose (position + orientation) and a projection.
    ///
    /// Conventions are XNA's: right-handed, +Y up, the identity orientation
    /// looks down -Z. Yaw turns about +Y (positive turns left), pitch about the
    /// camera's right axis (positive looks up).
    ///
    /// Invalidation contract: every setter marks the derived values stale and
    /// increments Revision(); View(), Projection(), ViewProjection() and
    /// Frustum() recompute lazily on the next read. References returned by
    /// those getters stay valid for the camera's lifetime but their values
    /// change after a setter. Setters validate their arguments and throw
    /// std::invalid_argument without modifying the camera.
    ///
    /// The camera owns no input or behaviour; controllers (CameraControllers.hpp)
    /// and game code drive it through the setters.
    class Camera
    {
    public:
        /// Perspective, 45 degree field of view, aspect 16:9, near 0.1, far 1000,
        /// at the origin looking down -Z.
        Camera();

        // --- Pose -----------------------------------------------------------
        void SetPosition(const Microsoft::Xna::Framework::Vector3& position);
        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Position() const noexcept { return position_; }

        /// Sets a unit orientation (the quaternion is normalized; a zero
        /// quaternion is rejected).
        void SetOrientation(const Microsoft::Xna::Framework::Quaternion& orientation);
        [[nodiscard]] const Microsoft::Xna::Framework::Quaternion& Orientation() const noexcept { return orientation_; }

        void SetYawPitchRoll(float yaw, float pitch, float roll = 0.0f);

        /// Orients the camera towards `target`. When the view direction is
        /// (nearly) parallel to `up`, a perpendicular up vector is chosen so the
        /// result is always a valid orientation. Throws when target == position.
        void LookAt(const Microsoft::Xna::Framework::Vector3& target,
                    const Microsoft::Xna::Framework::Vector3& up = Microsoft::Xna::Framework::Vector3::Up);

        /// Unit basis vectors of the current orientation.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Forward() const;
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Right() const;
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Up() const;

        // --- Projection -----------------------------------------------------
        /// `fieldOfView` is vertical, in radians, within (0, pi).
        void SetPerspective(float fieldOfView, float aspectRatio, float nearPlane, float farPlane);
        /// View volume of `width` x `height` world units centred on the view axis.
        void SetOrthographic(float width, float height, float nearPlane, float farPlane);
        /// Perspective: replaces the aspect ratio. Orthographic: keeps the
        /// height and derives the width, so a resize never squashes the image.
        void SetAspectRatio(float aspectRatio);

        [[nodiscard]] ProjectionKind Kind() const noexcept { return kind_; }
        [[nodiscard]] float FieldOfView() const noexcept { return fieldOfView_; }
        [[nodiscard]] float AspectRatio() const noexcept { return aspectRatio_; }
        [[nodiscard]] float OrthographicWidth() const noexcept { return orthoWidth_; }
        [[nodiscard]] float OrthographicHeight() const noexcept { return orthoHeight_; }
        [[nodiscard]] float NearPlane() const noexcept { return nearPlane_; }
        [[nodiscard]] float FarPlane() const noexcept { return farPlane_; }

        /// Mirrors the image horizontally (rear-view mirrors, reflections):
        /// Projection() and ViewProjection() flip clip-space X, which makes
        /// front faces counter-clockwise on screen, so renderers swap their
        /// culling (ForwardRenderer does). Frustum() is unaffected: a mirrored
        /// view sees the same volume.
        void SetMirrored(bool mirrored);
        [[nodiscard]] bool Mirrored() const noexcept { return mirrored_; }

        // --- Derived values (lazy) -------------------------------------------
        [[nodiscard]] const Microsoft::Xna::Framework::Matrix& View() const;
        [[nodiscard]] const Microsoft::Xna::Framework::Matrix& Projection() const;
        [[nodiscard]] const Microsoft::Xna::Framework::Matrix& ViewProjection() const;
        [[nodiscard]] const Microsoft::Xna::Framework::BoundingFrustum& Frustum() const;

        /// Incremented by every successful setter; lets dependants (culling
        /// caches, shadow setups) detect a change without comparing matrices.
        [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }

    private:
        void Invalidate() noexcept;
        void UpdateDerived() const;

        Microsoft::Xna::Framework::Vector3 position_;
        Microsoft::Xna::Framework::Quaternion orientation_;
        ProjectionKind kind_ = ProjectionKind::Perspective;
        float fieldOfView_ = 0.0f;
        float aspectRatio_ = 0.0f;
        float orthoWidth_ = 0.0f;
        float orthoHeight_ = 0.0f;
        float nearPlane_ = 0.0f;
        float farPlane_ = 0.0f;
        bool mirrored_ = false;
        std::uint64_t revision_ = 0;

        mutable bool dirty_ = true;
        mutable Microsoft::Xna::Framework::Matrix view_;
        mutable Microsoft::Xna::Framework::Matrix projection_;
        mutable Microsoft::Xna::Framework::Matrix viewProjection_;
        mutable Microsoft::Xna::Framework::BoundingFrustum frustum_;
    };
}
