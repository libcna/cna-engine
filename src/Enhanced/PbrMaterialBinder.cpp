// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaModeEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureTransformEXT.hpp"

#include <stdexcept>
#include <utility>

namespace CnaEngine::Enhanced
{
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::AlphaModeEXT;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT;
    using Microsoft::Xna::Framework::Graphics::PbrEffect;
    using Microsoft::Xna::Framework::Graphics::SkinnedPbrEffect;
    using Microsoft::Xna::Framework::Graphics::Texture2D;
    using Microsoft::Xna::Framework::Graphics::TextureTransformEXT;

    namespace
    {
        // PbrEffect and SkinnedPbrEffect share their parameters; the frame's
        // lighting and each draw's material are set the same way on both.
        template <typename Effect>
        void SetFrame(Effect& effect, const Graphics::ForwardLighting& lighting, const PbrSettings& settings,
                      const Vector3& keyColor)
        {
            effect.setLightingEnabledProperty(true);
            auto& key = effect.getDirectionalLight0Property();
            key.setEnabledProperty(true);
            key.setDirectionProperty(Vector3::Normalize(lighting.lightDirection));
            key.setDiffuseColorProperty(keyColor);
            key.setSpecularColorProperty(keyColor);
            Microsoft::Xna::Framework::Graphics::DirectionalLight* fills[2] = {&effect.getDirectionalLight1Property(),
                                                                                &effect.getDirectionalLight2Property()};
            for (std::size_t i = 0; i < 2; ++i) {
                const bool enabled = !lighting.defaultLighting && i < lighting.fillLights.size();
                fills[i]->setEnabledProperty(enabled);
                if (enabled) {
                    fills[i]->setDirectionProperty(Vector3::Normalize(lighting.fillLights[i].direction));
                    fills[i]->setDiffuseColorProperty(lighting.fillLights[i].diffuse * settings.exposure);
                    fills[i]->setSpecularColorProperty(lighting.fillLights[i].specular * settings.exposure);
                }
            }
            effect.setAmbientLightColorProperty(lighting.ambientColor * settings.exposure);
            auto environment = settings.environment;
            environment.Intensity *= settings.exposure;
            effect.setImageBasedLightEXT(environment);
            effect.setFogEnabledProperty(lighting.fogEnabled);
            effect.setFogColorProperty(lighting.fogColor);
            effect.setFogStartProperty(lighting.fogStart);
            effect.setFogEndProperty(lighting.fogEnd);
            effect.setEncodeOutputToSrgbEXTProperty(settings.encodeOutputToSrgb);
        }

        // Binds `wanted` (null: the frame's environment) unless the effect
        // holds it already: a cube upload per draw would dominate the pass.
        template <typename Effect>
        void BindEnvironment(Effect& effect, const ImageBasedLightEXT* wanted, const ImageBasedLightEXT*& bound,
                             const PbrSettings& settings)
        {
            if (wanted == bound) {
                return;
            }
            auto environment = wanted != nullptr ? *wanted : settings.environment;
            environment.Intensity *= settings.exposure;
            effect.setImageBasedLightEXT(environment);
            bound = wanted;
        }

        template <typename Effect>
        void SetDraw(Effect& effect, GraphicsDevice& device, const Graphics::RenderItem& item,
                     const Graphics::RenderBucket bucket, const Graphics::Camera& camera, const Vector3& keyColor,
                     const bool fog, const Graphics::FallbackSink& fallback)
        {
            using Graphics::VertexAttribute;
            const Graphics::Material& material = *item.material;
            const Graphics::AttributeMask attributes = item.mesh->Attributes();
            const bool hasUv = attributes.Has(VertexAttribute::TexCoord0);
            auto usable = [&](Texture2D* texture, const char* what) -> Texture2D* {
                if (texture == nullptr) {
                    return nullptr;
                }
                if (!hasUv) {
                    fallback(material, std::string(what) + " ignored: mesh has no TexCoord0");
                    return nullptr;
                }
                if (texture->getIsDisposedProperty()) {
                    fallback(material, std::string(what) + " ignored: it has been disposed");
                    return nullptr;
                }
                return texture;
            };
            Texture2D* normalMap = usable(material.normalTexture, "normal map");
            if (normalMap != nullptr && !attributes.Has(VertexAttribute::Tangent)) {
                fallback(material, "normal map ignored: mesh has no Tangent attribute");
                normalMap = nullptr;
            }
            Texture2D* orm = usable(material.occlusionRoughnessMetallicTexture, "occlusion-roughness-metallic map");
            if (material.environmentMap != nullptr) {
                fallback(material, "environment map ignored: PBR surfaces reflect PbrSettings::environment");
            }

            const Vector3 key = material.keyLight ? keyColor : Vector3::Zero;
            effect.getDirectionalLight0Property().setDiffuseColorProperty(key);
            effect.getDirectionalLight0Property().setSpecularColorProperty(key);
            effect.setWorldProperty(item.world);
            effect.setViewProperty(camera.View());
            effect.setProjectionProperty(camera.Projection());

            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setAlphaProperty(material.alpha);
            effect.setMetallicFactorProperty(material.metallic);
            effect.setRoughnessFactorProperty(material.roughness);
            effect.setEmissiveFactorProperty(material.emissiveColor);

            effect.setTextureProperty(usable(material.baseTexture, "base color texture"));
            effect.setNormalMapProperty(normalMap);
            effect.setMetallicRoughnessMapProperty(orm);
            effect.setOcclusionMapProperty(orm);
            effect.setEmissiveMapProperty(usable(material.emissiveTexture, "emissive texture"));
            effect.setBaseColorTextureIsSrgbEXTProperty(material.colorTexturesAreSrgb);
            effect.setEmissiveTextureIsSrgbEXTProperty(material.colorTexturesAreSrgb);
            effect.setNormalScaleEXTProperty(material.normalScale);
            effect.setOcclusionStrengthEXTProperty(material.occlusionStrength);
            effect.setIorEXTProperty(material.ior);
            effect.setSpecularFactorEXTProperty(material.specularFactor);

            // The renderer passes a sequenced item's material bucket; a direct caller may not.
            switch (bucket == Graphics::RenderBucket::Sequenced ? Graphics::BucketFor(material) : bucket) {
                case Graphics::RenderBucket::Sequenced:
                case Graphics::RenderBucket::Opaque: effect.setAlphaModeEXTProperty(AlphaModeEXT::Opaque); break;
                case Graphics::RenderBucket::AlphaTested: effect.setAlphaModeEXTProperty(AlphaModeEXT::Mask); break;
                case Graphics::RenderBucket::Transparent:
                case Graphics::RenderBucket::Overlay:
                case Graphics::RenderBucket::Count:
                    effect.setAlphaModeEXTProperty(material.discardBelowCutoff ? AlphaModeEXT::Mask : AlphaModeEXT::Blend);
                    break;
            }
            effect.setAlphaCutoffEXTProperty(material.alphaCutoff);
            effect.setDoubleSidedEXTProperty(material.doubleSided);
            effect.setFogEnabledProperty(fog && material.fog);

            TextureTransformEXT transform;
            transform.Scale = material.uvScale;
            transform.Offset = material.uvOffset;
            for (int slot = 0; slot < 5; ++slot) {
                effect.setTextureTransformEXTProperty(slot, transform);
                // Base color, normal, metallic-roughness, occlusion, emissive.
                device.getSamplerStatesProperty()[slot] = Graphics::SamplerFor(material);
            }
        }
    }

    PbrMaterialBinder::PbrMaterialBinder(GraphicsDevice& device, std::shared_ptr<Graphics::StockMaterialBinder> fallback,
                                         const PbrSettings settings)
        : effect_(std::make_unique<PbrEffect>(device)),
          skinned_(std::make_unique<SkinnedPbrEffect>(device)),
          fallback_(std::move(fallback)),
          settings_(settings)
    {
        if (!fallback_) {
            throw std::invalid_argument("PbrMaterialBinder: a stock binder is required for delegated materials");
        }
    }

    void PbrMaterialBinder::BeginFrame(GraphicsDevice& device, const Graphics::Camera& camera,
                                       const Graphics::ForwardLighting& lighting)
    {
        fallback_->BeginFrame(device, camera, lighting);
        keyColor_ = lighting.lightColor * settings_.exposure;
        fogEnabled_ = lighting.fogEnabled;
        SetFrame(*effect_, lighting, settings_, keyColor_);
        SetFrame(*skinned_, lighting, settings_, keyColor_);
        boundEnvironment_ = nullptr;
        boundSkinnedEnvironment_ = nullptr;
    }

    bool PbrMaterialBinder::Apply(GraphicsDevice& device, const Graphics::RenderItem& item,
                                  const Graphics::RenderBucket bucket, const Graphics::Camera& camera,
                                  const Graphics::FallbackSink& fallback)
    {
        using Graphics::VertexAttribute;
        if (Delegated(item)) {
            return fallback_->Apply(device, item, bucket, camera, fallback);
        }
        const ImageBasedLightEXT* environment = selector_ ? selector_(item) : nullptr;
        if (item.bones != nullptr) {
            const Graphics::Material& material = *item.material;
            const Graphics::AttributeMask attributes = item.mesh->Attributes();
            if (!attributes.Has(VertexAttribute::BlendIndices) || !attributes.Has(VertexAttribute::BlendWeights)) {
                fallback(material, "bone palette ignored: mesh has no BlendIndices and BlendWeights");
            } else if (item.bones->size() > static_cast<std::size_t>(SkinnedPbrEffect::MaxBones)) {
                fallback(material, "not drawn: more bones than SkinnedPbrEffect's 72");
                return false;
            } else {
                SkinnedPbrEffect& effect = *skinned_;
                BindEnvironment(effect, environment, boundSkinnedEnvironment_, settings_);
                SetDraw(effect, device, item, bucket, camera, keyColor_, fogEnabled_, fallback);
                effect.setWeightsPerVertexProperty(4);
                effect.SetBoneTransforms(*item.bones);
                effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
                return true;
            }
        }
        PbrEffect& effect = *effect_;
        BindEnvironment(effect, environment, boundEnvironment_, settings_);
        SetDraw(effect, device, item, bucket, camera, keyColor_, fogEnabled_, fallback);
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        return true;
    }

    const BlendState& PbrMaterialBinder::BlendStateFor(const Graphics::RenderItem& item) const
    {
        if (Delegated(item)) {
            return fallback_->BlendStateFor(item);
        }
        switch (item.material->blend) {
            case Graphics::BlendMode::Additive: return BlendState::Additive;
            case Graphics::BlendMode::PremultipliedAlpha: return BlendState::AlphaBlend;
            case Graphics::BlendMode::Opaque:
            case Graphics::BlendMode::AlphaTest:
            case Graphics::BlendMode::AlphaBlend: break;
        }
        return BlendState::NonPremultiplied;
    }

    bool PbrMaterialBinder::Delegated(const Graphics::RenderItem& item)
    {
        const Graphics::Material& material = *item.material;
        return material.shading == Graphics::ShadingModel::Unlit || material.vertexColors
               || !item.mesh->Attributes().Has(Graphics::VertexAttribute::Normal);
    }
}
