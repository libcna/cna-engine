// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001 addendum): this header uses CNA's punctual-light
// extension (PunctualLightEXT) and must never be included from the strict
// modules. It does not depend on cna-extended.
#pragma once

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/LocalLights.hpp"
#include "CnaEngine/Graphics/MaterialBinder.hpp"

#include <memory>
#include <vector>

namespace CnaEngine::Enhanced
{
    /// Lamps for lit materials: wraps the renderer's binder and, before each
    /// draw, gives the effects it draws with (the stock BasicEffect and
    /// SkinnedEffect, a PbrMaterialBinder's PbrEffect and SkinnedPbrEffect)
    /// the local light that reaches the item most (Graphics::StrongestLight;
    /// CNA's effects take one point or spot light per draw). More lamps on
    /// one item than one -- museum-simulator draws extra passes for them --
    /// are not added up: the strongest wins.
    class LocalLightBinder final : public Graphics::IMaterialBinder
    {
    public:
        /// Wraps `renderer`'s current binder; install with
        /// renderer.SetMaterialBinder (InstallLocalLights does both).
        explicit LocalLightBinder(Graphics::ForwardRenderer& renderer);

        /// This frame's lamps.
        std::vector<Graphics::LocalLight> lights;

        void BeginFrame(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Graphics::Camera& camera,
                        const Graphics::ForwardLighting& lighting) override;
        bool Apply(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Graphics::RenderItem& item,
                   Graphics::RenderBucket bucket, const Graphics::Camera& camera,
                   const Graphics::FallbackSink& fallback) override;
        [[nodiscard]] const Microsoft::Xna::Framework::Graphics::BlendState& BlendStateFor(
            const Graphics::RenderItem& item) const override;

        /// The light given to the last item (none: -1), for diagnostics.
        [[nodiscard]] int LastLight() const noexcept { return last_; }

    private:
        Graphics::ForwardRenderer& renderer_;
        std::shared_ptr<Graphics::IMaterialBinder> inner_;
        int last_ = -1;
    };

    /// Puts a LocalLightBinder around the renderer's binder and returns it
    /// (fill its `lights` each frame). Call after choosing the binder (a
    /// PbrMaterialBinder first, when the game uses one).
    LocalLightBinder& InstallLocalLights(Graphics::ForwardRenderer& renderer);
}
