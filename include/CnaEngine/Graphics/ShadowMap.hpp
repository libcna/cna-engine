// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <memory>
#include <span>

namespace CnaEngine::Graphics
{
    /// The directional (sun) shadow map in the representation CNA's shadow
    /// receivers sample (IShadowReceiverEXT, implemented by BasicEffect,
    /// SkinnedEffect, PbrEffect and SkinnedPbrEffect): an orthographic light
    /// view, and per texel in the red channel the nearest caster's light-space
    /// depth z/w * 0.5 + 0.5, cleared to 1 where nothing casts.
    ///
    /// It is drawn with XNA's own effects only -- the depth is the linear fog
    /// of a BasicEffect (SkinnedEffect for skinned casters) from the light's
    /// near to its far plane, which in an orthographic view is exactly the
    /// projected depth -- so the producer is strict. Making effects receive it
    /// takes CNA's extension API: Enhanced/ShadowReception.hpp. CNA removed
    /// its own producers upstream (room-simulator keeps a copy, island wrote
    /// its own; docs/game-facilities-survey.md).
    ///
    /// The map is a Single render target, else HalfSingle, else Color (whose
    /// 8 bits band visibly); Format() tells which one the device took.
    class ShadowMap
    {
    public:
        /// Throws std::invalid_argument for a size below 16.
        explicit ShadowMap(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, int size = 2048);

        /// Places the light's view so it covers a sphere of `radius` around
        /// `focus`, seen from `directionToLight` (towards the sun). The view is
        /// moved in whole texels, so shadows do not crawl as the focus moves.
        void Fit(const Microsoft::Xna::Framework::Vector3& directionToLight,
                 const Microsoft::Xna::Framework::Vector3& focus, float radius);

        /// Whether an item casts: an opaque mesh (alpha-tested, blended and
        /// overlay items do not; an alpha-tested leaf card would cast its
        /// whole quad).
        [[nodiscard]] static bool Casts(const RenderItem& item) noexcept;

        /// Draws the depth of every caster among `items` whose bounds reach
        /// the light's view (instanced items per instance, skinned items with
        /// their palette). Restores the device's render target and states.
        void Render(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, std::span<const RenderItem> items,
                    RenderStatistics* statistics = nullptr);

        [[nodiscard]] Microsoft::Xna::Framework::Graphics::RenderTarget2D& Map() noexcept { return *target_; }
        [[nodiscard]] const Microsoft::Xna::Framework::Matrix& LightViewProjection() const noexcept { return viewProjection_; }
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::SurfaceFormat Format() const noexcept { return format_; }
        [[nodiscard]] int Size() const noexcept { return size_; }
        /// A depth bias for the receivers: three texels' slope at 45 degrees
        /// in the map's units, plus one and a half steps of the precision the
        /// map really keeps (measured when it is created: an 8-bit map needs
        /// a bias that lifts shadows visibly off the ground).
        [[nodiscard]] float SuggestedBias() const noexcept;
        /// The bits of depth the map keeps: 23 (float), 11 (half), 8 (colour).
        [[nodiscard]] int PrecisionBits() const noexcept { return precisionBits_; }
        /// Casters drawn by the last Render.
        [[nodiscard]] int CastersDrawn() const noexcept { return drawn_; }

    private:
        [[nodiscard]] Microsoft::Xna::Framework::Matrix DrawingProjection() const;

        int size_;
        Microsoft::Xna::Framework::Graphics::SurfaceFormat format_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> target_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> rigid_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SkinnedEffect> skinned_;
        Microsoft::Xna::Framework::Matrix view_;
        Microsoft::Xna::Framework::Matrix projection_;
        Microsoft::Xna::Framework::Matrix viewProjection_;
        int precisionBits_ = 8;
        float radius_ = 1.0f;
        float depth_ = 1.0f;
        int drawn_ = 0;
    };
}
