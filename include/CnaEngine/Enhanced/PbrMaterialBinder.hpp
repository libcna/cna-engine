// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001 addendum): this header uses CNA graphics
// extensions (PbrEffect) and must never be included from CnaEngine::Core or
// CnaEngine::Graphics. It does not depend on cna-extended.
#pragma once

#include "CnaEngine/Graphics/MaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"

#include <memory>

namespace CnaEngine::Enhanced
{
    /// Settings shared by every draw of a PbrMaterialBinder.
    struct PbrSettings
    {
        /// The effect encodes its output to sRGB (8-bit targets and the back
        /// buffer). Turn off when rendering into a linear HDR target.
        bool encodeOutputToSrgb = true;
        /// Multiplies the directional light's color (exposure).
        float lightIntensity = 1.0f;
    };

    /// Material binder over CNA's metallic-roughness PbrEffect, for enhanced
    /// renderers. Material fields map one to one to the glTF-style effect
    /// parameters (base color and texture, metallic/roughness factors and the
    /// occlusion-roughness-metallic map, normal map and scale, emissive,
    /// IOR/specular, alpha mode and cutoff, double-sidedness, UV transform).
    ///
    /// Lighting: ForwardLighting's single directional light (switched off for
    /// materials with `keyLight == false`) and ambient color
    /// (the default three-light rig of the stock effects is approximated by
    /// its key light). Image-based lighting, shadows and environment probes
    /// are not set; a game that needs them derives its own binder.
    ///
    /// Delegation and fallbacks:
    ///  - Unlit materials, vertex-colored materials and meshes without
    ///    normals are drawn by the stock binder (meshes without normals
    ///    report the fallback);
    ///  - a normal map on a mesh without Tangent is ignored and reported;
    ///  - textures on a mesh without TexCoord0 are ignored and reported;
    ///  - the alpha mode follows the render bucket (Opaque, Mask for
    ///    AlphaTested, Blend for Transparent/Overlay).
    class PbrMaterialBinder final : public Graphics::IMaterialBinder
    {
    public:
        PbrMaterialBinder(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                          std::shared_ptr<Graphics::StockMaterialBinder> fallback, PbrSettings settings = {});

        void BeginFrame(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Graphics::Camera& camera,
                        const Graphics::ForwardLighting& lighting) override;
        bool Apply(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Graphics::RenderItem& item,
                   Graphics::RenderBucket bucket, const Graphics::Camera& camera,
                   const Graphics::FallbackSink& fallback) override;

        [[nodiscard]] PbrSettings& Settings() noexcept { return settings_; }
        /// The effect, for setups the binder does not manage (IBL, shadows).
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::PbrEffect& Effect() noexcept { return *effect_; }

    private:
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::PbrEffect> effect_;
        std::shared_ptr<Graphics::StockMaterialBinder> fallback_;
        PbrSettings settings_;
        Microsoft::Xna::Framework::Vector3 keyColor_ = Microsoft::Xna::Framework::Vector3::Zero;
    };
}
