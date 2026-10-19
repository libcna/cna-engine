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
    };

    /// Builds a CameraControlInput from ActionMap axes and an optional "fast"
    /// action. Look axes are multiplied by `lookScale` (radians per axis unit),
    /// so mouse-delta axes and stick axes can share one controller.
    struct CameraInputAxes
    {
        Input::AxisId moveRight = 0;
        Input::AxisId moveUp = 0;
        Input::AxisId moveForward = 0;
        Input::AxisId lookYaw = 0;
        Input::AxisId lookPitch = 0;
        std::optional<Input::ActionId> fast;
        bool hasMoveUp = false;
        float lookScale = 1.0f;
    };
    [[nodiscard]] CameraControlInput ReadCameraInput(const Input::ActionMap& actions, const CameraInputAxes& axes);

    /// Policy that moves a Camera; the camera stays a passive state object.
    /// Game-specific controllers (chase, cockpit, cinematic) implement this in
    /// the game and are not part of the engine.
    class ICameraController
    {
    public:
        virtual ~ICameraController() = default;
        virtual void Update(Camera& camera, const CameraControlInput& input, float elapsedSeconds) = 0;
    };

    /// Shared yaw/pitch bookkeeping of the first-person controllers.
    struct FirstPersonSettings
    {
        float speed = 5.0f;           ///< world units per second
        float fastMultiplier = 4.0f;
        float maxPitch = 1.53f;       ///< radians, just under 90 degrees
    };

    /// Free flight: moves along the camera's own axes, no gravity, no ground.
    class FlyCameraController final : public ICameraController
    {
    public:
        explicit FlyCameraController(FirstPersonSettings settings = {});

        /// Adopts the camera's current heading so the first update does not snap.
        void SyncFrom(const Camera& camera);
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

        void SyncFrom(const Camera& camera);
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
