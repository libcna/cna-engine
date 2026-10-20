// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/CameraControllers.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        // Inverse of Quaternion::CreateFromYawPitchRoll(yaw, pitch, 0) applied to -Z:
        // forward = (-sin(yaw) cos(pitch), sin(pitch), -cos(yaw) cos(pitch)).
        void HeadingOf(const Camera& camera, float& yaw, float& pitch)
        {
            const Vector3 forward = camera.Forward();
            pitch = std::asin(std::clamp(forward.Y, -1.0f, 1.0f));
            yaw = std::atan2(-forward.X, -forward.Z);
        }

        // Keeps yaw within (-pi, pi] so long sessions do not lose precision.
        float WrapAngle(float angle)
        {
            angle = std::remainder(angle, MathHelper::TwoPi);
            return angle <= -MathHelper::Pi ? angle + MathHelper::TwoPi : angle;
        }

        float Speed(const FirstPersonSettings& settings, const CameraControlInput& input)
        {
            return settings.speed * (input.fast ? settings.fastMultiplier : 1.0f)
                   * (input.slow ? settings.slowMultiplier : 1.0f);
        }

        void Turn(const FirstPersonSettings& settings, const CameraControlInput& input, float& yaw, float& pitch)
        {
            yaw = WrapAngle(yaw + input.yaw);
            pitch = std::clamp(pitch + input.pitch, -settings.maxPitch, settings.maxPitch);
        }
    }

    CameraControlInput ReadCameraInput(const Input::ActionMap& actions, const CameraInputAxes& axes,
                                       const float elapsedSeconds)
    {
        CameraControlInput input;
        input.move = Vector3(actions.Axis(axes.moveRight), axes.hasMoveUp ? actions.Axis(axes.moveUp) : 0.0f,
                             actions.Axis(axes.moveForward));
        input.yaw = actions.Axis(axes.lookYaw) * axes.lookScale;
        input.pitch = actions.Axis(axes.lookPitch) * axes.lookScale;
        const float rate = axes.lookRate * elapsedSeconds;
        if (axes.lookYawRate) {
            input.yaw += actions.Axis(*axes.lookYawRate) * rate;
        }
        if (axes.lookPitchRate) {
            input.pitch += actions.Axis(*axes.lookPitchRate) * rate;
        }
        input.fast = axes.fast && actions.Held(*axes.fast);
        input.slow = axes.slow && actions.Held(*axes.slow);
        return input;
    }

    FlyCameraController::FlyCameraController(const FirstPersonSettings settings) : settings_(settings) {}

    void FlyCameraController::SyncFrom(const Camera& camera) { HeadingOf(camera, yaw_, pitch_); }

    void FlyCameraController::Update(Camera& camera, const CameraControlInput& input, const float elapsedSeconds)
    {
        Turn(settings_, input, yaw_, pitch_);
        camera.SetYawPitchRoll(yaw_, pitch_, 0.0f);
        const Vector3 up = settings_.worldUpVertical ? Vector3::Up : camera.Up();
        const Vector3 direction = camera.Right() * input.move.X + up * input.move.Y + camera.Forward() * input.move.Z;
        const float lengthSquared = direction.LengthSquared();
        if (lengthSquared > 1e-12f) {
            const float amount = std::min(1.0f, input.move.Length());
            const Vector3 step = direction * (amount * Speed(settings_, input) * elapsedSeconds / std::sqrt(lengthSquared));
            camera.SetPosition(camera.Position() + step);
        }
    }

    WalkCameraController::WalkCameraController(const FirstPersonSettings settings, const float eyeHeight,
                                               GroundHeight ground)
        : settings_(settings), eyeHeight_(eyeHeight), ground_(std::move(ground))
    {
    }

    void WalkCameraController::SyncFrom(const Camera& camera) { HeadingOf(camera, yaw_, pitch_); }

    void WalkCameraController::Update(Camera& camera, const CameraControlInput& input, const float elapsedSeconds)
    {
        Turn(settings_, input, yaw_, pitch_);
        camera.SetYawPitchRoll(yaw_, pitch_, 0.0f);

        // Horizontal basis from yaw alone: looking up or down never changes walking speed.
        const Vector3 forward(-std::sin(yaw_), 0.0f, -std::cos(yaw_));
        const Vector3 right(std::cos(yaw_), 0.0f, -std::sin(yaw_));
        Vector3 planar = right * input.move.X + forward * input.move.Z;
        const float length = planar.Length();
        if (length > 1.0f) {
            planar = planar * (1.0f / length); // diagonal walking is not faster
        }
        Vector3 position = camera.Position() + planar * (Speed(settings_, input) * elapsedSeconds);
        if (ground_) {
            position.Y = ground_(position.X, position.Z) + eyeHeight_;
        }
        camera.SetPosition(position);
    }

    OrbitCameraController::OrbitCameraController(const OrbitSettings settings) : settings_(settings) {}

    Vector3 OrbitCameraController::Position() const
    {
        const float cosPitch = std::cos(pitch_);
        const float sinPitch = std::sin(pitch_);
        const float cosYaw = std::cos(yaw_);
        const float sinYaw = std::sin(yaw_);
        return Vector3(target_.X + distance_ * cosPitch * sinYaw, target_.Y + distance_ * sinPitch,
                       target_.Z + distance_ * cosPitch * cosYaw);
    }

    void OrbitCameraController::Apply(Camera& camera) const { camera.SetLookAt(Position(), target_, Vector3::Up); }

    void OrbitCameraController::SyncFrom(const Camera& camera)
    {
        const Vector3 offset = camera.Position() - target_;
        const float distance = offset.Length();
        if (distance < 1e-6f) {
            return;
        }
        distance_ = distance;
        pitch_ = std::asin(std::clamp(offset.Y / distance, -1.0f, 1.0f));
        yaw_ = std::atan2(offset.X, offset.Z);
    }

    void OrbitCameraController::Update(Camera& camera, const CameraControlInput& input, const float elapsedSeconds)
    {
        float speed = settings_.zoomSpeed;
        if (input.fast) {
            speed *= settings_.fastMultiplier;
        }
        if (input.slow) {
            speed *= settings_.slowMultiplier;
        }
        yaw_ += input.yaw;
        pitch_ = std::clamp(pitch_ - input.pitch, -settings_.maxPitch, settings_.maxPitch);
        distance_ = std::clamp(distance_ - input.move.Z * speed * elapsedSeconds, settings_.minDistance,
                               settings_.maxDistance);
        Apply(camera);
    }

    FollowCameraController::FollowCameraController(const Vector3& offset, const float smoothing)
        : offset_(offset), smoothing_(smoothing)
    {
    }

    void FollowCameraController::Snap() noexcept { position_ = target_ + offset_; }

    void FollowCameraController::SyncFrom(const Camera& camera) { position_ = camera.Position(); }

    void FollowCameraController::Update(Camera& camera, const CameraControlInput& input, const float elapsedSeconds)
    {
        (void)input;
        // The share of the way closed per 1/60 s, compounded over the elapsed time.
        constexpr float referenceFramesPerSecond = 60.0f;
        const Vector3 desired(target_.X + offset_.X, target_.Y + offset_.Y, target_.Z + offset_.Z);
        float t = 0.0f;
        if (smoothing_ >= 1.0f) {
            t = 1.0f;
        } else if (smoothing_ > 0.0f) {
            t = 1.0f - std::pow(1.0f - smoothing_, elapsedSeconds * referenceFramesPerSecond);
        }
        position_ = Vector3::Lerp(position_, desired, t);
        if (Vector3::DistanceSquared(position_, target_) > 1e-12f) {
            camera.SetLookAt(position_, target_, Vector3::Up);
        }
    }
}
