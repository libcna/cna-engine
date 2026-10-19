// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// Points in the frame where a game can insert its own drawing.
    enum class PassPoint : std::uint8_t
    {
        BeforeOpaque,
        AfterOpaque,
        AfterTransparent,
        AfterOverlay,
    };

    /// Scene-wide lighting for the stock effects.
    struct ForwardLighting
    {
        /// XNA's default three-light rig (BasicEffect::EnableDefaultLighting).
        /// When false the lights are left as configured through LitEffect().
        bool defaultLighting = true;
        Microsoft::Xna::Framework::Vector3 ambientColor = Microsoft::Xna::Framework::Vector3(0.05333332f, 0.09882354f,
                                                                                             0.1819608f);
        bool fogEnabled = false;
        Microsoft::Xna::Framework::Vector3 fogColor = Microsoft::Xna::Framework::Vector3::Zero;
        float fogStart = 0.0f;
        float fogEnd = 1.0f;
    };

    /// A small forward renderer over XNA's stock effects (BasicEffect and
    /// AlphaTestEffect), usable by strict-XNA games.
    ///
    /// Pass order: [BeforeOpaque] Opaque, AlphaTested [AfterOpaque]
    /// Transparent [AfterTransparent] Overlay [AfterOverlay]. Custom passes run
    /// in insertion order at their point and may draw anything with CNA
    /// directly.
    ///
    /// State contract: before each bucket the renderer sets blend, depth and
    /// rasterizer states it needs (opaque/alpha-tested: Opaque blend, Default
    /// depth, counter-clockwise culling; transparent: material blend, DepthRead;
    /// overlay: AlphaBlend, no depth); double-sided materials disable culling.
    /// Custom passes receive the device in the state of the preceding bucket.
    /// After Render() the blend, depth-stencil and rasterizer states are
    /// restored to what they were before the call.
    ///
    /// Fallbacks are deterministic and reported in LastFallbacks():
    ///  - Lit/PhysicallyBased without normals in the mesh draws Unlit;
    ///  - PhysicallyBased draws Lit (stock effects have no PBR);
    ///  - a texture on a mesh without TexCoord0, or a disposed texture, is ignored;
    ///  - vertex colors on a mesh without a Color attribute are ignored;
    ///  - AlphaTest draws through AlphaTestEffect, which is unlit in XNA.
    class ForwardRenderer
    {
    public:
        using PassCallback = std::function<void(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                                const Camera& camera, RenderStatistics& statistics)>;

        explicit ForwardRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        [[nodiscard]] ForwardLighting& Lighting() noexcept { return lighting_; }
        /// The BasicEffect used for Opaque/Transparent/Overlay items, for custom
        /// light setups (escape hatch; per-item parameters are overwritten).
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::BasicEffect& LitEffect() noexcept { return *basic_; }

        void AddPass(PassPoint point, std::string name, PassCallback callback);
        /// Removes every custom pass called `name`; returns how many were removed.
        std::size_t RemovePass(const std::string& name);

        /// Draws a queue (call RenderQueue::Sort first) seen by `camera`.
        void Render(const RenderQueue& queue, const Camera& camera, RenderStatistics* statistics = nullptr);

        /// Distinct fallback descriptions from the last Render() call.
        [[nodiscard]] const std::vector<std::string>& LastFallbacks() const noexcept { return fallbacks_; }

    private:
        struct Pass
        {
            PassPoint point;
            std::string name;
            PassCallback callback;
        };

        void RunPasses(PassPoint point, const Camera& camera, RenderStatistics& statistics);
        void DrawBucket(std::span<const RenderItem> items, RenderBucket bucket, const Camera& camera,
                        RenderStatistics& statistics);
        void DrawItem(const RenderItem& item, RenderBucket bucket, const Camera& camera, RenderStatistics& statistics);
        void Fallback(const Material& material, const std::string& what);

        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> basic_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::AlphaTestEffect> alphaTest_;
        /// 1x1 white texture for untextured alpha-tested materials.
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> white_;
        ForwardLighting lighting_;
        std::vector<Pass> passes_;
        std::vector<std::string> fallbacks_;
    };
}
