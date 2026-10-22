// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Collision/StaticWorld.hpp"

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>

namespace CnaEngine::Collision
{
    /// How a character's body moves. The defaults are the middle of what the
    /// games use (docs/game-facilities-survey.md): a 0.3 m by 1.8 m body,
    /// steps up to 0.35 m, slopes up to 45 degrees, 3 m/s walking.
    struct CharacterSettings
    {
        float radius = 0.3f;
        float height = 1.8f;
        /// The eye above the feet (Eye()).
        float eyeHeight = 1.68f;
        /// The highest ledge walked onto without jumping.
        float stepHeight = 0.35f;
        /// How far the feet follow the ground down (stairs, kerbs) instead of falling.
        float stepDown = 0.4f;
        /// Steeper ground is a wall.
        float maxSlopeDegrees = 45.0f;
        float gravity = 9.81f;
        float terminalSpeed = 50.0f;
        /// Upward speed of a jump (v² / 2g high: 1.03 m at the defaults).
        float jumpSpeed = 4.5f;
        /// How quickly the horizontal speed approaches the wish, per second
        /// (exponential), on the ground; deceleration when the wish is slower.
        float acceleration = 9.0f;
        float deceleration = 13.0f;
        /// The share of the acceleration available in the air.
        float airControl = 0.3f;
        /// The distance between strides (StrideCount; footsteps, head bob).
        float strideLength = 0.75f;
        /// Collision passes per movement substep.
        int iterations = 4;
    };

    /// The horizontal velocity for camera-relative movement input (X right,
    /// Z forward, as Graphics::CameraControlInput::move) seen with `yaw` (the
    /// camera's: 0 looks along -Z), at `speed` for a full push; diagonals are
    /// not faster, a half-deflected stick walks at half speed. The basis is
    /// WalkCameraController's.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 WishVelocity(const Microsoft::Xna::Framework::Vector3& move,
                                                                  float yaw, float speed) noexcept;

    /// Moves an upright capsule through a StaticWorld: walking with sliding
    /// along walls, stepping up ledges, following the ground down, slopes up
    /// to a limit, gravity and jumping. Collide-and-slide in substeps of at
    /// most half the radius, then pushing out of overlaps, as school, car,
    /// island and backrooms do it; steps and slope limits as house and
    /// island do it.
    ///
    /// Call Step once per fixed simulation step: the same steps with the
    /// same input give the same positions (FixedStepClock).
    class CharacterController
    {
    public:
        /// Throws std::invalid_argument for a body that cannot exist
        /// (radius <= 0, height < 2 * radius, a negative step, ...).
        explicit CharacterController(CharacterSettings settings = {},
                                     const Microsoft::Xna::Framework::Vector3& feet = Microsoft::Xna::Framework::Vector3::Zero);

        /// Advances `seconds`: the horizontal velocity approaches `wish`
        /// (world units per second; its Y is ignored), `jump` starts a jump
        /// from the ground.
        void Step(const StaticWorld& world, const Microsoft::Xna::Framework::Vector3& wish, bool jump, float seconds);

        /// Puts the feet at `feet` with no velocity, out of any overlap.
        void Teleport(const StaticWorld& world, const Microsoft::Xna::Framework::Vector3& feet);

        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Feet() const noexcept { return feet_; }
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Eye() const noexcept;
        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Velocity() const noexcept { return velocity_; }
        [[nodiscard]] bool Grounded() const noexcept { return grounded_; }
        /// The ground's normal while grounded (Up otherwise).
        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& GroundNormal() const noexcept { return groundNormal_; }
        /// The tag of the ground's shape while grounded (a surface kind for footsteps).
        [[nodiscard]] std::uint32_t GroundTag() const noexcept { return groundTag_; }
        [[nodiscard]] Capsule Body() const noexcept;
        [[nodiscard]] const CharacterSettings& Settings() const noexcept { return settings_; }

        /// Horizontal distance walked on the ground since construction.
        [[nodiscard]] double DistanceWalked() const noexcept { return walked_; }
        /// Whole strides walked; changes once per `strideLength` on the ground.
        [[nodiscard]] std::uint64_t StrideCount() const noexcept;
        /// The last Step completed a stride (a footstep).
        [[nodiscard]] bool StrideTaken() const noexcept { return strideTaken_; }
        /// A head-bob offset for the eye: `amplitude * |sin|` over each stride
        /// (two bobs per left-right pair), fading as the body slows.
        [[nodiscard]] float HeadBob(float amplitude) const noexcept;
        /// The last Step was stopped by a wall (or stepped up a ledge).
        [[nodiscard]] bool HitWall() const noexcept { return hitWall_; }
        [[nodiscard]] bool SteppedUp() const noexcept { return steppedUp_; }

    private:
        struct Resolved
        {
            Microsoft::Xna::Framework::Vector3 feet;
            bool wall = false;
            Microsoft::Xna::Framework::Vector3 wallNormal = Microsoft::Xna::Framework::Vector3::Zero;
            bool ground = false;
            Microsoft::Xna::Framework::Vector3 groundNormal = Microsoft::Xna::Framework::Vector3::Up;
            std::uint32_t groundTag = 0;
            bool ceiling = false;
        };

        /// Pushes the body at `feet` out of the world. With `steps`, an edge
        /// below the lower sphere's centre and at most stepHeight above
        /// `stepBase` with walkable ground behind it supports the body
        /// instead of stopping it: the sphere rides up over the edge.
        [[nodiscard]] Resolved Resolve(const StaticWorld& world, Microsoft::Xna::Framework::Vector3 feet,
                                       bool horizontalOnly, bool steps = false, float stepBase = 0.0f) const;
        [[nodiscard]] bool LedgeBehind(const StaticWorld& world, const Contact& contact) const;
        /// Moves across in substeps; edges ride up only to stepBase + stepHeight.
        [[nodiscard]] Resolved MoveHorizontal(const StaticWorld& world, const Microsoft::Xna::Framework::Vector3& from,
                                              const Microsoft::Xna::Framework::Vector3& motion, float stepBase) const;
        /// Lowers the body up to `distance` onto the first ground (or edge no
        /// higher than stepBase + stepHeight); false, unchanged, when there is none.
        bool ProbeGround(const StaticWorld& world, float distance, float stepBase);

        CharacterSettings settings_;
        float walkableY_;
        Microsoft::Xna::Framework::Vector3 feet_;
        Microsoft::Xna::Framework::Vector3 velocity_ = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 groundNormal_ = Microsoft::Xna::Framework::Vector3::Up;
        std::uint32_t groundTag_ = 0;
        bool grounded_ = false;
        bool hitWall_ = false;
        bool steppedUp_ = false;
        bool strideTaken_ = false;
        double walked_ = 0.0;
    };
}
