// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/MaterialBinder.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

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

    /// A small forward renderer: ordering, buckets, render states and custom
    /// passes. How a material becomes effect parameters is delegated to an
    /// IMaterialBinder — by default the StockMaterialBinder over XNA's
    /// BasicEffect/AlphaTestEffect, so strict-XNA games can use it as is.
    ///
    /// Pass order: [BeforeOpaque] Opaque, AlphaTested [AfterOpaque]
    /// Transparent [AfterTransparent] Overlay [AfterOverlay]. Custom passes run
    /// in insertion order at their point and may draw anything with CNA
    /// directly.
    ///
    /// State contract: before each bucket the renderer sets the blend, depth
    /// and rasterizer states it needs (opaque/alpha-tested: Opaque blend,
    /// Default depth, counter-clockwise culling; transparent: material blend,
    /// DepthRead; overlay: AlphaBlend, no depth); double-sided materials
    /// disable culling and biased materials get a biased rasterizer state. Custom passes receive the device in the state of the
    /// preceding bucket. After Render() the blend, depth-stencil and rasterizer
    /// states are restored to what they were before the call.
    ///
    /// Fallbacks taken by the binder are deterministic and reported, once each,
    /// in LastFallbacks().
    class ForwardRenderer
    {
    public:
        using PassCallback = std::function<void(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                                const Camera& camera, RenderStatistics& statistics)>;

        explicit ForwardRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        [[nodiscard]] ForwardLighting& Lighting() noexcept { return lighting_; }

        /// Replaces the material binder; null restores the stock binder.
        void SetMaterialBinder(std::shared_ptr<IMaterialBinder> binder);
        [[nodiscard]] IMaterialBinder& MaterialBinder() noexcept { return *binder_; }
        /// The built-in stock binder (also when another binder is active);
        /// enhanced binders delegate unsupported materials to it.
        [[nodiscard]] const std::shared_ptr<StockMaterialBinder>& StockBinder() const noexcept { return stock_; }
        /// The stock BasicEffect, for custom light setups (escape hatch).
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::BasicEffect& LitEffect() noexcept
        {
            return stock_->LitEffect();
        }

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

        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
        std::shared_ptr<StockMaterialBinder> stock_;
        std::shared_ptr<IMaterialBinder> binder_;
        ForwardLighting lighting_;
        std::vector<Pass> passes_;
        std::vector<std::string> fallbacks_;
        FallbackSink sink_;
        /// Rasterizer states for biased materials, created on first use.
        struct BiasedState
        {
            bool doubleSided;
            float depthBias;
            float slopeScale;
            std::unique_ptr<Microsoft::Xna::Framework::Graphics::RasterizerState> state;
        };
        std::vector<BiasedState> biasedStates_;
        const Microsoft::Xna::Framework::Graphics::RasterizerState& RasterizerFor(const Material& material);
    };
}
