// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// A directional light: `direction` points from the light into the scene.
    struct DirectionalLightDescription
    {
        Microsoft::Xna::Framework::Vector3 direction = Microsoft::Xna::Framework::Vector3(0.0f, -1.0f, 0.0f);
        Microsoft::Xna::Framework::Vector3 diffuse = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 specular = Microsoft::Xna::Framework::Vector3::Zero;
    };

    /// Scene-wide lighting shared by material binders.
    struct ForwardLighting
    {
        /// XNA's default three-light rig (BasicEffect::EnableDefaultLighting).
        /// When false, the key light (`lightDirection`, `lightColor` for
        /// diffuse and specular) plus up to two `fillLights` (XNA effects
        /// have three directional lights; sky fill and ground bounce are the
        /// usual pair) are used.
        bool defaultLighting = true;
        Microsoft::Xna::Framework::Vector3 lightDirection =
            Microsoft::Xna::Framework::Vector3(-0.5265408f, -0.5735765f, -0.6275069f);
        Microsoft::Xna::Framework::Vector3 lightColor = Microsoft::Xna::Framework::Vector3(1.0f, 0.9607844f, 0.8078432f);
        /// At most two are used; more are ignored.
        std::vector<DirectionalLightDescription> fillLights;
        /// BasicEffect::PreferPerPixelLighting.
        bool perPixelLighting = false;
        Microsoft::Xna::Framework::Vector3 ambientColor =
            Microsoft::Xna::Framework::Vector3(0.05333332f, 0.09882354f, 0.1819608f);
        bool fogEnabled = false;
        Microsoft::Xna::Framework::Vector3 fogColor = Microsoft::Xna::Framework::Vector3::Zero;
        float fogStart = 0.0f;
        float fogEnd = 1.0f;
    };

    /// Receives a description of a deterministic fallback taken for a material.
    using FallbackSink = std::function<void(const Material& material, const std::string& what)>;

    /// Translates a Material into effect parameters for one draw.
    ///
    /// The renderer owns ordering, buckets and blend/depth/rasterizer states;
    /// a binder owns the effect: it chooses one, sets its parameters from the
    /// material, the item and the camera, and applies a pass. Games replace
    /// the binder to use their own effects (environment probes, custom shaders)
    /// without touching the renderer.
    class IMaterialBinder
    {
    public:
        virtual ~IMaterialBinder() = default;

        /// Called once per frame before any item is applied.
        virtual void BeginFrame(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Camera& camera,
                                const ForwardLighting& lighting)
        {
            (void)device;
            (void)camera;
            (void)lighting;
        }

        /// Configures an effect for `item` and applies its pass. Returns
        /// false to skip the item (nothing is drawn).
        virtual bool Apply(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const RenderItem& item,
                           RenderBucket bucket, const Camera& camera, const FallbackSink& fallback) = 0;

        /// The blend state the renderer sets for `item` in the blended
        /// buckets (Transparent, Overlay). It depends on what the binder's
        /// effect writes: XNA's stock effects multiply their color by the
        /// material alpha (premultiplied output), CNA's PbrEffect does not
        /// (straight output). The default is XNA's convention: `Additive`
        /// for BlendMode::Additive, `AlphaBlend` otherwise.
        [[nodiscard]] virtual const Microsoft::Xna::Framework::Graphics::BlendState&
        BlendStateFor(const RenderItem& item) const;
    };

    /// Binder over XNA's stock effects (BasicEffect, AlphaTestEffect,
    /// DualTextureEffect, SkinnedEffect); the
    /// strict default.
    ///
    /// Materials with a `secondaryTexture` (on meshes with TexCoord0 and
    /// TexCoord1) draw through XNA's DualTextureEffect, which is unlit.
    ///
    /// Fallbacks (reported through the sink):
    ///  - Lit/PhysicallyBased without normals in the mesh draws Unlit;
    ///  - a secondary texture on a mesh without TexCoord1 is ignored; a lit
    ///    material with a secondary texture draws unlit (DualTextureEffect);
    ///  - PhysicallyBased draws Lit (stock effects have no PBR);
    ///  - a texture on a mesh without TexCoord0, or a disposed texture, is ignored;
    ///  - vertex colors on a mesh without a Color attribute are ignored;
    ///  - AlphaTest draws through AlphaTestEffect, which is unlit in XNA;
    ///  - PremultipliedAlpha with alpha < 1 draws as AlphaBlend (stock
    ///    effects scale their color by the alpha).
    ///
    /// Skinned items (RenderItem::bones) draw through XNA's SkinnedEffect,
    /// which is always lit and textured and has no vertex colors, second
    /// texture or alpha test; each of those is reported when the material
    /// asks for it. A palette on a mesh without BlendIndices/BlendWeights or
    /// normals is ignored (the bind pose is drawn) and reported; more than 72
    /// bones is reported and not drawn.
    class StockMaterialBinder final : public IMaterialBinder
    {
    public:
        explicit StockMaterialBinder(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        void BeginFrame(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Camera& camera,
                        const ForwardLighting& lighting) override;
        bool Apply(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const RenderItem& item,
                   RenderBucket bucket, const Camera& camera, const FallbackSink& fallback) override;

        /// The BasicEffect used for non-alpha-tested items (escape hatch for
        /// custom light setups; per-item parameters are overwritten).
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::BasicEffect& LitEffect() noexcept { return *basic_; }

    private:
        bool ApplySkinned(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const RenderItem& item,
                          RenderBucket bucket, const Camera& camera, Microsoft::Xna::Framework::Graphics::Texture2D* texture,
                          bool vertexColors, const FallbackSink& fallback);

        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> basic_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::AlphaTestEffect> alphaTest_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::DualTextureEffect> dual_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SkinnedEffect> skinned_;
        /// 1x1 white texture for untextured alpha-tested materials.
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> white_;
        bool defaultLighting_ = true;
        bool fillEnabled_[2] = {false, false};
    };
}
