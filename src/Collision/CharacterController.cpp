// SPDX-License-Identifier: MIT
#include "CnaEngine/Collision/CharacterController.hpp"

#include "CollisionMath.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Collision
{
    using Microsoft::Xna::Framework::Vector3;
    using namespace Detail;

    namespace
    {
        // Pushes leave this much air, so a body pushed out of a surface does
        // not touch it again by rounding.
        constexpr float Skin = 1e-4f;

        Vector3 Horizontal(const Vector3& v) noexcept { return Vector3(v.X, 0.0f, v.Z); }
    }

    Vector3 WishVelocity(const Vector3& move, const float yaw, const float speed) noexcept
    {
        const Vector3 forward(-std::sin(yaw), 0.0f, -std::cos(yaw));
        const Vector3 right(std::cos(yaw), 0.0f, -std::sin(yaw));
        Vector3 planar = right * move.X + forward * move.Z;
        const float length = Length(planar);
        if (length > 1.0f) {
            planar = planar * (1.0f / length);
        }
        return planar * speed;
    }

    CharacterController::CharacterController(CharacterSettings settings, const Vector3& feet)
        : settings_(settings), feet_(feet)
    {
        const auto& s = settings_;
        if (!(s.radius > 0.0f) || !(s.height >= 2.0f * s.radius) || !(s.eyeHeight > 0.0f) ||
            !(s.eyeHeight <= s.height) || !(s.stepHeight >= 0.0f) || !(s.stepDown >= 0.0f) ||
            !(s.maxSlopeDegrees > 0.0f && s.maxSlopeDegrees < 90.0f) || !(s.gravity >= 0.0f) ||
            !(s.terminalSpeed > 0.0f) || !(s.jumpSpeed >= 0.0f) || !(s.acceleration >= 0.0f) ||
            !(s.deceleration >= 0.0f) || !(s.airControl >= 0.0f) || !(s.strideLength > 0.0f) || s.iterations < 1) {
            throw std::invalid_argument("CharacterController: settings out of range");
        }
        walkableY_ = std::cos(s.maxSlopeDegrees * 3.14159265358979f / 180.0f);
    }

    Vector3 CharacterController::Eye() const noexcept
    {
        return Vector3(feet_.X, feet_.Y + settings_.eyeHeight, feet_.Z);
    }

    Capsule CharacterController::Body() const noexcept
    {
        return Capsule{feet_, settings_.radius, settings_.height};
    }

    std::uint64_t CharacterController::StrideCount() const noexcept
    {
        return static_cast<std::uint64_t>(walked_ / static_cast<double>(settings_.strideLength));
    }

    float CharacterController::HeadBob(const float amplitude) const noexcept
    {
        const double phase = walked_ / static_cast<double>(settings_.strideLength) * 3.14159265358979;
        const float speed = Length(Horizontal(velocity_));
        const float fade = grounded_ ? std::min(1.0f, speed) : 0.0f;
        return amplitude * static_cast<float>(std::fabs(std::sin(phase))) * fade;
    }

    bool CharacterController::LedgeBehind(const StaticWorld& world, const Contact& contact) const
    {
        // A short ray down just behind the edge: walkable ground there (a
        // kerb's top, a stair's tread) makes the edge a step; a wall's inside
        // or a steep face does not.
        const Vector3 into = Normalized(Horizontal(contact.normal)) * -0.02f;
        const Microsoft::Xna::Framework::Ray probe(
            Vector3(contact.point.X + into.X, contact.point.Y + 0.05f, contact.point.Z + into.Z), Vector3(0.0f, -1.0f, 0.0f));
        const auto hit = world.Raycast(probe, 0.1f);
        return hit && hit->distance > 0.0f && hit->normal.Y >= walkableY_;
    }

    CharacterController::Resolved CharacterController::Resolve(const StaticWorld& world, Vector3 feet,
                                                               const bool horizontalOnly, const bool steps,
                                                               const float stepBase) const
    {
        Resolved result;
        for (int i = 0; i < settings_.iterations; ++i) {
            const Capsule body{feet, settings_.radius, settings_.height};
            const auto contacts = world.Contacts(body);
            if (contacts.empty()) {
                break;
            }
            const Contact& contact = contacts.front(); // the deepest
            const Vector3& n = contact.normal;
            const Vector3 centre = body.Bottom();
            if (steps && n.Y < walkableY_ && contact.point.Y < centre.Y - 0.01f &&
                contact.point.Y - stepBase <= settings_.stepHeight + 1e-4f && LedgeBehind(world, contact)) {
                // Ride up the edge: the lower sphere rests on it.
                const float dx = contact.point.X - centre.X;
                const float dz = contact.point.Z - centre.Z;
                const float across = std::min(dx * dx + dz * dz, settings_.radius * settings_.radius);
                const float rest = contact.point.Y + std::sqrt(settings_.radius * settings_.radius - across);
                feet.Y += rest - centre.Y + Skin;
                result.ground = true;
                result.groundNormal = Vector3::Up;
                result.groundTag = contact.tag;
                continue;
            }
            if (n.Y >= walkableY_) {
                // Ground: straight up, so walking up a ramp keeps its pace.
                result.ground = true;
                result.groundNormal = n;
                result.groundTag = contact.tag;
                feet.Y += contact.depth / n.Y + Skin;
            } else if (n.Y <= -walkableY_ && !horizontalOnly) {
                result.ceiling = true;
                feet = feet + n * (contact.depth + Skin);
            } else {
                // A wall, or ground too steep to stand on: out sideways only,
                // so a steep slope cannot be climbed by pushing into it.
                const Vector3 side = Horizontal(n);
                const float sideLength = Length(side);
                if (sideLength < 1e-4f) {
                    feet = feet + n * (contact.depth + Skin);
                    continue;
                }
                const Vector3 direction = side * (1.0f / sideLength);
                result.wall = true;
                result.wallNormal = direction;
                feet = feet + direction * (std::min(contact.depth / sideLength, contact.depth * 8.0f) + Skin);
            }
        }
        result.feet = feet;
        return result;
    }

    CharacterController::Resolved CharacterController::MoveHorizontal(const StaticWorld& world, const Vector3& from,
                                                                       const Vector3& motion, const float stepBase) const
    {
        // Only a body on the ground climbs edges; one in the air meets them as walls.
        const bool steps = grounded_;
        const float length = Length(motion);
        const int substeps = std::max(1, static_cast<int>(std::ceil(length / (settings_.radius * 0.5f))));
        const Vector3 step = motion * (1.0f / static_cast<float>(substeps));
        Resolved total;
        total.feet = from;
        for (int i = 0; i < substeps; ++i) {
            const Resolved moved = Resolve(world, total.feet + step, true, steps, stepBase);
            total.feet = moved.feet;
            if (moved.wall) {
                total.wall = true;
                total.wallNormal = moved.wallNormal;
            }
            if (moved.ground) {
                total.ground = true;
                total.groundNormal = moved.groundNormal;
                total.groundTag = moved.groundTag;
            }
        }
        return total;
    }

    bool CharacterController::ProbeGround(const StaticWorld& world, const float distance, const float stepBase)
    {
        // A small first drop finds the ground the body stands on (also an
        // edge it rests on), larger ones the ground below a step.
        const float increment = settings_.radius * 0.5f;
        for (float lowered = std::min(0.02f, distance); lowered <= distance + 1e-6f;) {
            const Resolved settled = Resolve(world, Vector3(feet_.X, feet_.Y - lowered, feet_.Z), false, true, stepBase);
            if (settled.ground) {
                feet_ = settled.feet;
                groundNormal_ = settled.groundNormal;
                groundTag_ = settled.groundTag;
                return true;
            }
            if (lowered >= distance) {
                break;
            }
            lowered = std::min(lowered + increment, distance);
        }
        return false;
    }

    void CharacterController::Step(const StaticWorld& world, const Vector3& wish, const bool jump, const float seconds)
    {
        hitWall_ = false;
        steppedUp_ = false;
        strideTaken_ = false;
        if (!(seconds > 0.0f) || !std::isfinite(seconds)) {
            return;
        }
        const std::uint64_t stridesBefore = StrideCount();

        // Horizontal velocity eases towards the wish.
        Vector3 horizontal = Horizontal(velocity_);
        const Vector3 wished = std::isfinite(wish.X) && std::isfinite(wish.Z) ? Horizontal(wish) : Vector3::Zero;
        const float rate = (LengthSquared(wished) >= LengthSquared(horizontal) ? settings_.acceleration
                                                                               : settings_.deceleration) *
                           (grounded_ ? 1.0f : settings_.airControl);
        horizontal = horizontal + (wished - horizontal) * (1.0f - std::exp(-rate * seconds));

        float vertical = velocity_.Y;
        if (jump && grounded_) {
            vertical = settings_.jumpSpeed;
            grounded_ = false;
        }
        if (grounded_) {
            vertical = 0.0f;
        } else {
            vertical = std::max(vertical - settings_.gravity * seconds, -settings_.terminalSpeed);
        }

        // Across, with a step up when a ledge stops a body on the ground.
        const Vector3 start = feet_;
        const Vector3 motion = horizontal * seconds;
        Resolved moved = MoveHorizontal(world, feet_, motion, feet_.Y);
        // A ledge above the lower sphere's centre (higher than the radius)
        // is no edge to ride up: lift by the step height and go on at least a
        // radius, so the body's axis gets over the ledge (a wall took the
        // speed, so the motion alone may be millimetres).
        const Vector3 heading = LengthSquared(motion) > 1e-12f ? Normalized(motion) : Normalized(wished);
        if (moved.wall && grounded_ && settings_.stepHeight > 0.0f && LengthSquared(heading) > 0.0f) {
            const Vector3 raised(feet_.X, feet_.Y + settings_.stepHeight, feet_.Z);
            if (!world.Overlaps(Capsule{raised, settings_.radius, settings_.height})) {
                const float travel = std::max(Length(motion), settings_.radius + 0.01f);
                const Resolved across = MoveHorizontal(world, raised, heading * travel, start.Y);
                const float plainProgress = LengthSquared(Horizontal(moved.feet - start));
                const float stepProgress = LengthSquared(Horizontal(across.feet - start));
                if (stepProgress > plainProgress + 1e-8f) {
                    const Vector3 saved = feet_;
                    feet_ = across.feet;
                    if (ProbeGround(world, settings_.stepHeight + settings_.radius * 0.5f, start.Y) &&
                        feet_.Y > start.Y + 1e-3f) {
                        moved.feet = feet_;
                        moved.wall = across.wall;
                        moved.wallNormal = across.wallNormal;
                        steppedUp_ = true;
                    }
                    feet_ = saved;
                }
            }
        }
        feet_ = moved.feet;
        if (moved.wall && !steppedUp_) {
            hitWall_ = true;
            // Speed into the wall is lost; along it is kept (sliding).
            const float into = Dot(horizontal, moved.wallNormal);
            if (into < 0.0f) {
                horizontal = horizontal - moved.wallNormal * into;
            }
        }

        // Up and down.
        if (grounded_) {
            if (!ProbeGround(world, settings_.stepDown, feet_.Y)) {
                grounded_ = false; // walked off an edge: fall from here
                groundNormal_ = Vector3::Up;
            }
        } else {
            const float dy = vertical * seconds;
            const int substeps = std::max(1, static_cast<int>(std::ceil(std::fabs(dy) / (settings_.radius * 0.5f))));
            for (int i = 0; i < substeps; ++i) {
                const Resolved resolved =
                    Resolve(world, Vector3(feet_.X, feet_.Y + dy / static_cast<float>(substeps), feet_.Z), false);
                feet_ = resolved.feet;
                if (resolved.ground && vertical <= 0.0f) {
                    grounded_ = true;
                    groundNormal_ = resolved.groundNormal;
                    groundTag_ = resolved.groundTag;
                    vertical = 0.0f;
                    break;
                }
                if (resolved.ceiling && vertical > 0.0f) {
                    vertical = 0.0f;
                }
                if (resolved.wall) {
                    const float into = Dot(horizontal, resolved.wallNormal);
                    if (into < 0.0f) {
                        horizontal = horizontal - resolved.wallNormal * into;
                    }
                }
            }
        }

        velocity_ = Vector3(horizontal.X, vertical, horizontal.Z);
        if (grounded_) {
            walked_ += static_cast<double>(Length(Horizontal(feet_ - start)));
            strideTaken_ = StrideCount() != stridesBefore;
        }
    }

    void CharacterController::Teleport(const StaticWorld& world, const Vector3& feet)
    {
        feet_ = Resolve(world, feet, false).feet;
        velocity_ = Vector3::Zero;
        grounded_ = ProbeGround(world, settings_.radius * 0.5f, feet_.Y);
        if (!grounded_) {
            groundNormal_ = Vector3::Up;
        }
    }
}
