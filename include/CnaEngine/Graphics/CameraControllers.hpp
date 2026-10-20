// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Input/ActionMap.hpp"

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <functional>
#include <optional>

namespace CnaEngine::Graphics
{
    /// Device-independent intent for one controller update. Controllers never
    /// read devices or action names themselves, so the same controller works
    /// with keyboard/mouse, a game pad, a replay or a test.
    struct CameraControlInput
    {
        /// Camera-relative movement: X right, Y up, Z forward. Components are
        /// normally within [-1, 1]; the controller scales them by its speed.
        Microsoft::Xna::Framework::Vector3 move = Microsoft::Xna::Framework::Vector3::Zero;
        /// Radians to turn this update; positive yaw turns left, positive pitch looks up.
        float yaw = 0.0f;
        float pitch = 0.0f;
        /// Applies the controller's fast multiplier.
        bool fast = false;
        /// Applies the controller's slow multiplier (combines with `fast`).
        bool slow = false;
    };

    /// Which ActionMap axes and actions feed a camera controller.
    ///
    /// Look comes from two kinds of axes: per-update deltas (`lookYaw`,
    /// `lookPitch`, typically mouse axes) multiplied by `lookScale` radians
    /// per unit, and rates (`lookYawRate`, `lookPitchRate`, typically keys or
    /// sticks) multiplied by `lookRate` radians per second and the elapsed
    /// time. Either kind may be absent.
    struct CameraInputAxes
    {
        Input::AxisId moveRight = 0;
        Input::AxisId moveUp = 0;
        Input::AxisId moveForward = 0;
        Input::AxisId lookYaw = 0;
        Input::AxisId lookPitch = 0;
        std::optional<Input::AxisId> lookYawRate;
        std::optional<Input::AxisId> lookPitchRate;
        std::optional<Input::ActionId> fast;
        std::optional<Input::ActionId> slow;
        bool hasMoveUp = false;
        float lookScale = 1.0f;
        float lookRate = 1.5f;
    };
    [[nodiscard]] CameraControlInput ReadCameraInput(const Input::ActionMap& actions, const CameraInputAxes& axes,
                                                     float elapsedSeconds = 0.0f);

    /// Policy that moves a Camera; the camera stays a passive state object.
    /// The engine ships the general ones (fly, walk, orbit, follow);
    /// game-specific controllers (chase, cockpit, cinematic) implement this in
    /// the game.
    class ICameraController
    {
    public:
        virtual ~ICameraController() = default;
        virtual void Update(Camera& camera, const CameraControlInput& input, float elapsedSeconds) = 0;
        /// Adopts the camera's current state so the next Update() does not
        /// snap; called when a controller takes over a camera. Default: no-op.
        virtual void SyncFrom(const Camera& camera) { (void)camera; }
    };

    /// Shared yaw/pitch bookkeeping of the first-person controllers.
    struct FirstPersonSettings
    {
        float speed = 5.0f;           ///< world units per second
        float fastMultiplier = 4.0f;
        float maxPitch = 1.53f;       ///< radians, just under 90 degrees
        float slowMultiplier = 0.25f;
        /// Fly: climb along world +Y (true) or along the camera's up vector.
        bool worldUpVertical = true;
    };

    /// Free flight, no gravity, no ground.
    ///
    /// The direction is right * move.X + up * move.Y + forward * move.Z
    /// (forward follows pitch; up is world +Y or the camera's up), normalized,
    /// and the speed is scaled by min(1, |move|): combined digital keys are
    /// never faster than one key, while a half-deflected stick moves at half
    /// speed.
    class FlyCameraController final : public ICameraController
    {
    public:
        explicit FlyCameraController(FirstPersonSettings settings = {});

        /// Adopts the camera's current heading so the first update does not snap.
        void SyncFrom(const Camera& camera) override;
        void Update(Camera& camera, const CameraControlInput& input, float elapsedSeconds) override;

        [[nodiscard]] float Yaw() const noexcept { return yaw_; }
        [[nodiscard]] float Pitch() const noexcept { return pitch_; }
        [[nodiscard]] FirstPersonSettings& Settings() noexcept { return settings_; }

    private:
        FirstPersonSettings settings_;
        float yaw_ = 0.0f;
        float pitch_ = 0.0f;
    };

    /// Walking: moves on the horizontal plane regardless of pitch; the eye
    /// height follows an optional ground function (world X, Z -> ground Y).
    /// Without one, the camera keeps its height and move.Y is ignored.
    class WalkCameraController final : public ICameraController
    {
    public:
        using GroundHeight = std::function<float(float x, float z)>;

        explicit WalkCameraController(FirstPersonSettings settings = {}, float eyeHeight = 1.7f,
                                      GroundHeight ground = {});

        void SyncFrom(const Camera& camera) override;
        void Update(Camera& camera, const CameraControlInput& input, float elapsedSeconds) override;

        [[nodiscard]] float Yaw() const noexcept { return yaw_; }
        [[nodiscard]] float Pitch() const noexcept { return pitch_; }
        [[nodiscard]] FirstPersonSettings& Settings() noexcept { return settings_; }
        void SetGround(GroundHeight ground) { ground_ = std::move(ground); }

    private:
        FirstPersonSettings settings_;
        float eyeHeight_;
        GroundHeight ground_;
        float yaw_ = 0.0f;
        float pitch_ = 0.0f;
    };

    /// Distance limits and speeds of an OrbitCameraController.
    struct OrbitSettings
    {
        float minDistance = 0.5f;
        float maxDistance = 1000.0f;
        /// Radians; the camera never goes past straight above or below.
        float maxPitch = 1.53f;
        /// World units per second at move.Z = 1 (forward zooms in).
        float zoomSpeed = 10.0f;
        float fastMultiplier = 4.0f;
        float slowMultiplier = 0.25f;
    };

    /// Circles a target point at a distance -- model viewers, editors,
    /// strategy maps.
    ///
    /// The camera sits at target + distance * (cos(pitch) sin(yaw),
    /// sin(pitch), cos(pitch) cos(yaw)) and looks at the target: yaw 0 views
    /// from +Z, positive pitch from above. Input yaw turns the view as in the
    /// first-person controllers (positive looks left, so the camera moves
    /// towards +X); input pitch looks up by lowering the camera; move.Z zooms.
    class OrbitCameraController final : public ICameraController
    {
    public:
        explicit OrbitCameraController(OrbitSettings settings = {});

        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Target() const noexcept { return target_; }
        void SetTarget(const Microsoft::Xna::Framework::Vector3& target) noexcept { target_ = target; }
        [[nodiscard]] float Yaw() const noexcept { return yaw_; }
        void SetYaw(float radians) noexcept { yaw_ = radians; }
        [[nodiscard]] float Pitch() const noexcept { return pitch_; }
        void SetPitch(float radians) noexcept { pitch_ = radians; }
        [[nodiscard]] float Distance() const noexcept { return distance_; }
        void SetDistance(float distance) noexcept { distance_ = distance; }
        [[nodiscard]] OrbitSettings& Settings() noexcept { return settings_; }

        /// Where the camera stands for the current target, yaw, pitch and distance.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Position() const;
        /// Places `camera` at Position() looking at the target (Camera::SetLookAt).
        void Apply(Camera& camera) const;

        /// Takes yaw, pitch and distance from where the camera stands
        /// relative to the target.
        void SyncFrom(const Camera& camera) override;
        void Update(Camera& camera, const CameraControlInput& input, float elapsedSeconds) override;

    private:
        OrbitSettings settings_;
        Microsoft::Xna::Framework::Vector3 target_ = Microsoft::Xna::Framework::Vector3::Zero;
        float yaw_ = 0.0f;
        float pitch_ = 0.3f;
        float distance_ = 10.0f;
    };

    /// Trails a moving target at a fixed offset and looks at it -- simple
    /// third-person views.
    ///
    /// Each update moves the camera towards target + offset by
    /// 1 - (1 - smoothing)^(60 * elapsed seconds) of the way, so the
    /// smoothing means "the share closed per 1/60 s" at any frame rate;
    /// smoothing 1 follows rigidly, 0 never moves. Input is ignored.
    class FollowCameraController final : public ICameraController
    {
    public:
        explicit FollowCameraController(
            const Microsoft::Xna::Framework::Vector3& offset = Microsoft::Xna::Framework::Vector3(0.0f, 5.0f, 10.0f),
            float smoothing = 0.15f);

        /// What the camera follows; set it before each Update().
        void SetTargetPosition(const Microsoft::Xna::Framework::Vector3& target) noexcept { target_ = target; }
        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& TargetPosition() const noexcept { return target_; }
        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Offset() const noexcept { return offset_; }
        void SetOffset(const Microsoft::Xna::Framework::Vector3& offset) noexcept { offset_ = offset; }
        [[nodiscard]] float Smoothing() const noexcept { return smoothing_; }
        void SetSmoothing(float smoothing) noexcept { smoothing_ = smoothing; }
        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Position() const noexcept { return position_; }
        /// Jumps to target + offset (a cut instead of a pan).
        void Snap() noexcept;

        /// Starts from the camera's current position.
        void SyncFrom(const Camera& camera) override;
        void Update(Camera& camera, const CameraControlInput& input, float elapsedSeconds) override;

    private:
        Microsoft::Xna::Framework::Vector3 position_ = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 target_ = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 offset_;
        float smoothing_;
    };
}
