// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace CnaEngine::Graphics
{
    /// A lamp: a point light, or a spot light within a cone.
    struct LocalLight
    {
        enum class Kind : std::uint8_t { Point, Spot };
        Kind kind = Kind::Point;
        Microsoft::Xna::Framework::Vector3 position = Microsoft::Xna::Framework::Vector3::Zero;
        /// Where a spot light points.
        Microsoft::Xna::Framework::Vector3 direction = Microsoft::Xna::Framework::Vector3(0.0f, -1.0f, 0.0f);
        Microsoft::Xna::Framework::Vector3 color = Microsoft::Xna::Framework::Vector3(1.0f, 0.85f, 0.6f);
        /// Nothing is lit beyond it.
        float range = 8.0f;
        /// A spot light's full and fading cone, half angles in radians.
        float innerAngle = 0.35f;
        float outerAngle = 0.5f;
    };

    /// How strongly a light reaches a sphere: its colour's brightness, faded
    /// by distance to the sphere's nearest point over the light's range (and
    /// for a spot light by the cone). Zero out of reach.
    [[nodiscard]] float LightReach(const LocalLight& light, const Microsoft::Xna::Framework::BoundingSphere& bounds) noexcept;

    /// The light that matters most for an item (CNA's lit effects take one
    /// point or spot light per draw); nothing when none reaches it.
    [[nodiscard]] std::optional<std::size_t> StrongestLight(std::span<const LocalLight> lights,
                                                            const Microsoft::Xna::Framework::BoundingSphere& bounds) noexcept;

    /// Light on the ground for strict games, whose stock effects take no local
    /// light: an additive, unlit disc fading from the light's colour at the
    /// centre to nothing at the edge, drawn on the ground below each lamp --
    /// car-simulator's lamp pools, island's lamp glow. Light pools add, so
    /// they work best on darker scenes (night, interiors).
    class LightPools
    {
    public:
        explicit LightPools(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        /// Appends a pool under each light whose ground (the light's position
        /// minus `height`) is given by `groundY` -- for a flat world, its height.
        /// The items borrow this object's mesh and materials until the next call.
        void Append(std::span<const LocalLight> lights, float groundY, std::vector<RenderItem>& items,
                    float brightness = 0.6f);

    private:
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> gradient_;
        GpuMesh disc_;
        std::vector<Material> materials_;
    };
}
