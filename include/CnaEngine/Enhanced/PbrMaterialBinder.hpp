// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001 addendum): this header uses CNA graphics
// extensions (PbrEffect) and must never be included from CnaEngine::Core or
// CnaEngine::Graphics. It does not depend on cna-extended.
#pragma once

#include "CnaEngine/Graphics/MaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Graphics/ImageBasedLightEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedPbrEffect.hpp"

#include <memory>

namespace CnaEngine::Enhanced
{
    /// Settings shared by every draw of a PbrMaterialBinder.
    struct PbrSettings
    {
        /// The effect encodes its output to sRGB (8-bit targets and the back
        /// buffer). Turn off when rendering into a linear HDR target.
        bool encodeOutputToSrgb = true;
        /// Multiplies every light the binder sets: the directional lights,
        /// the ambient color and the environment's intensity. Emissive
        /// surfaces and the fog color are not scaled.
        float exposure = 1.0f;
        /// Image-based lighting for every PBR draw: irradiance and
        /// prefiltered specular cubes with a BRDF table (textures borrowed;
        /// they must outlive the frames that use them). Empty by default.
        /// CNA's PbrEffect replaces the flat ambient term with an
        /// environment rather than adding the two.
        Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT environment{};
    };

    /// Material binder over CNA's metallic-roughness PbrEffect, for enhanced
    /// renderers. Material fields map one to one to the glTF-style effect
    /// parameters (base color and texture, metallic/roughness factors and the
    /// occlusion-roughness-metallic map, normal map and scale, emissive,
    /// IOR/specular, alpha mode and cutoff, double-sidedness, UV transform).
    ///
    /// Lighting: ForwardLighting's key light (switched off for materials
    /// with `keyLight == false`), fill lights and ambient color (the default
    /// three-light rig of the stock effects is approximated by its key
    /// light), all scaled by PbrSettings::exposure, and the optional
    /// image-based environment of PbrSettings. Shadows and per-item
    /// environment probes are not set; a game that needs them implements its
    /// own IMaterialBinder (Effect() exposes the effect for setups the binder
    /// leaves alone).
    ///
    /// Skinned items (RenderItem::bones) draw through CNA's SkinnedPbrEffect
    /// with the same parameters; a palette on a mesh without
    /// BlendIndices/BlendWeights is ignored and reported, more than 72 bones
    /// is reported and not drawn. Delegated skinned materials use the stock
    /// binder's SkinnedEffect.
    ///
    /// Blending: PbrEffect writes straight (not premultiplied) color, so
    /// BlendMode::AlphaBlend blends with XNA's `NonPremultiplied` state and
    /// BlendMode::PremultipliedAlpha with `AlphaBlend`; delegated materials
    /// keep the stock binder's blend states.
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
        [[nodiscard]] const Microsoft::Xna::Framework::Graphics::BlendState&
        BlendStateFor(const Graphics::RenderItem& item) const override;

        /// Changes take effect at the next frame (BeginFrame).
        [[nodiscard]] PbrSettings& Settings() noexcept { return settings_; }
        /// The effect, for setups the binder does not manage (IBL, shadows).
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::PbrEffect& Effect() noexcept { return *effect_; }

    private:
        /// Items the stock binder draws instead (unlit, vertex-colored, no normals).
        [[nodiscard]] static bool Delegated(const Graphics::RenderItem& item);

        std::unique_ptr<Microsoft::Xna::Framework::Graphics::PbrEffect> effect_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SkinnedPbrEffect> skinned_;
        std::shared_ptr<Graphics::StockMaterialBinder> fallback_;
        PbrSettings settings_;
        Microsoft::Xna::Framework::Vector3 keyColor_ = Microsoft::Xna::Framework::Vector3::Zero;
    };
}
