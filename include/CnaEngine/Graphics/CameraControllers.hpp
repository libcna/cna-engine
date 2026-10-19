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
    /// Game-specific controllers (chase, cockpit, cinematic) implement this in
    /// the game and are not part of the engine.
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
}
