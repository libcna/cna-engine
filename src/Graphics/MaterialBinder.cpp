// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/MaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectFog.hpp"

#include <algorithm>
#include <cmath>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::AlphaTestEffect;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::CompareFunction;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::IEffectFog;
    using Microsoft::Xna::Framework::Graphics::SkinnedEffect;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    const Microsoft::Xna::Framework::Graphics::BlendState& IMaterialBinder::BlendStateFor(const RenderItem& item) const
    {
        using Microsoft::Xna::Framework::Graphics::BlendState;
        if (item.material->blend == BlendMode::Additive) {
            return BlendState::Additive;
        }
        // Stock effects pass texture and vertex colors through as they are.
        return item.material->straightAlpha ? BlendState::NonPremultiplied : BlendState::AlphaBlend;
    }

    StockMaterialBinder::StockMaterialBinder(GraphicsDevice& device)
        : basic_(std::make_unique<BasicEffect>(device)),
          alphaTest_(std::make_unique<AlphaTestEffect>(device)),
          dual_(std::make_unique<Microsoft::Xna::Framework::Graphics::DualTextureEffect>(device)),
          skinned_(std::make_unique<SkinnedEffect>(device)),
          environment_(std::make_unique<Microsoft::Xna::Framework::Graphics::EnvironmentMapEffect>(device)),
          white_(std::make_unique<Texture2D>(device, 1, 1))
    {
        const Color white(255, 255, 255, 255);
        white_->SetData(&white, 1);
    }

    void StockMaterialBinder::BeginFrame(GraphicsDevice&, const Camera&, const ForwardLighting& lighting)
    {
        defaultLighting_ = lighting.defaultLighting;
        fogEnabled_ = lighting.fogEnabled;
        // BasicEffect and SkinnedEffect light the same way.
        auto light = [&](auto& effect) {
            if (lighting.defaultLighting) {
                effect.EnableDefaultLighting();
                fillEnabled_[0] = fillEnabled_[1] = true;
            } else {
                auto& key = effect.getDirectionalLight0Property();
                key.setEnabledProperty(true);
                key.setDirectionProperty(Vector3::Normalize(lighting.lightDirection));
                key.setDiffuseColorProperty(lighting.lightColor);
                key.setSpecularColorProperty(lighting.lightColor);
                Microsoft::Xna::Framework::Graphics::DirectionalLight* fills[2] = {
                    &effect.getDirectionalLight1Property(), &effect.getDirectionalLight2Property()};
                for (std::size_t i = 0; i < 2; ++i) {
                    fillEnabled_[i] = i < lighting.fillLights.size();
                    fills[i]->setEnabledProperty(fillEnabled_[i]);
                    if (fillEnabled_[i]) {
                        fills[i]->setDirectionProperty(Vector3::Normalize(lighting.fillLights[i].direction));
                        fills[i]->setDiffuseColorProperty(lighting.fillLights[i].diffuse);
                        fills[i]->setSpecularColorProperty(lighting.fillLights[i].specular);
                    }
                }
            }
            if constexpr (requires { effect.setPreferPerPixelLightingProperty(true); }) {
                effect.setPreferPerPixelLightingProperty(lighting.perPixelLighting);
            }
            effect.setAmbientLightColorProperty(lighting.ambientColor);
        };
        light(*basic_);
        light(*skinned_);
        light(*environment_);
        keyDiffuse_ = basic_->getDirectionalLight0Property().getDiffuseColorProperty();
        keySpecular_ = basic_->getDirectionalLight0Property().getSpecularColorProperty();
        ambient_ = basic_->getAmbientLightColorProperty();
        bakedLighting_ = false;
        for (IEffectFog* fog : {static_cast<IEffectFog*>(basic_.get()), static_cast<IEffectFog*>(alphaTest_.get()),
                                static_cast<IEffectFog*>(dual_.get()), static_cast<IEffectFog*>(skinned_.get()),
                                static_cast<IEffectFog*>(environment_.get())}) {
            fog->setFogEnabledProperty(lighting.fogEnabled);
            fog->setFogColorProperty(lighting.fogColor);
            fog->setFogStartProperty(lighting.fogStart);
            fog->setFogEndProperty(lighting.fogEnd);
        }
    }

    bool StockMaterialBinder::Apply(GraphicsDevice& device, const RenderItem& item, const RenderBucket bucket,
                                    const Camera& camera, const FallbackSink& fallback)
    {
        const Material& material = *item.material;
        const AttributeMask attributes = item.mesh->Attributes();

        Texture2D* texture = material.baseTexture;
        if (texture != nullptr && !attributes.Has(VertexAttribute::TexCoord0)) {
            fallback(material, "texture ignored: mesh has no TexCoord0");
            texture = nullptr;
        }
        if (texture != nullptr && texture->getIsDisposedProperty()) {
            fallback(material, "texture ignored: it has been disposed");
            texture = nullptr;
        }
        bool vertexColors = material.vertexColors;
        if (vertexColors && !attributes.Has(VertexAttribute::Color)) {
            fallback(material, "vertex colors ignored: mesh has no Color attribute");
            vertexColors = false;
        }
        if (material.blend == BlendMode::PremultipliedAlpha && material.alpha < 1.0f) {
            fallback(material, "premultiplied blending drawn as AlphaBlend (stock effects scale the color by alpha)");
        }
        if (material.straightAlpha && material.alpha < 1.0f
            && (bucket == RenderBucket::Transparent || bucket == RenderBucket::Overlay)) {
            fallback(material, "straight alpha with alpha < 1: stock effects premultiply it, so it counts twice");
        }

        if (item.bones != nullptr) {
            if (!attributes.Has(VertexAttribute::BlendIndices) || !attributes.Has(VertexAttribute::BlendWeights)) {
                fallback(material, "bone palette ignored: mesh has no BlendIndices and BlendWeights");
            } else if (!attributes.Has(VertexAttribute::Normal)) {
                fallback(material, "bone palette ignored: XNA SkinnedEffect needs normals");
            } else if (item.bones->size() > static_cast<std::size_t>(SkinnedEffect::MaxBones)) {
                fallback(material, "not drawn: more bones than XNA SkinnedEffect's 72");
                return false;
            } else {
                return ApplySkinned(device, item, bucket, camera, texture, vertexColors, fallback);
            }
        }

        const bool blended = bucket == RenderBucket::Transparent || bucket == RenderBucket::Overlay;
        if (bucket == RenderBucket::AlphaTested || (blended && material.discardBelowCutoff)) {
            if (material.shading != ShadingModel::Unlit) {
                fallback(material, "alpha-tested surfaces draw unlit (XNA AlphaTestEffect)");
            }
            AlphaTestEffect& effect = *alphaTest_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(camera.View());
            effect.setProjectionProperty(camera.Projection());
            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setAlphaProperty(material.alpha);
            effect.setTextureProperty(texture != nullptr ? texture : white_.get());
            effect.setVertexColorEnabledProperty(vertexColors);
            effect.setAlphaFunctionProperty(CompareFunction::GreaterEqual);
            effect.setReferenceAlphaProperty(
                static_cast<int>(std::lround(std::clamp(material.alphaCutoff, 0.0f, 1.0f) * 255.0f)));
            device.getSamplerStatesProperty()[0] = SamplerFor(material);
            effect.setFogEnabledProperty(fogEnabled_ && material.fog);
            effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
            return true;
        }

        bool lit = material.shading != ShadingModel::Unlit;
        // Baked light keeps (1 - share) and gets the key light's share from
        // the lit effect, where a received shadow can take it away.
        float bakedShare = !lit && material.keyLight ? std::min(material.bakedKeyLight, 1.0f) : 0.0f;
        if ((lit || bakedShare > 0.0f) && !attributes.Has(VertexAttribute::Normal)) {
            fallback(material, "drawn unlit: mesh has no normals");
            lit = false;
            bakedShare = 0.0f;
        } else if (material.shading == ShadingModel::PhysicallyBased) {
            fallback(material, "PhysicallyBased drawn as Lit (stock effects have no PBR)");
        }
        Texture2D* secondary = material.secondaryTexture;
        if (secondary != nullptr && (!attributes.Has(VertexAttribute::TexCoord1) || texture == nullptr)) {
            fallback(material, texture == nullptr ? "secondary texture ignored: no base texture"
                                                  : "secondary texture ignored: mesh has no TexCoord1");
            secondary = nullptr;
        }
        if (secondary != nullptr && secondary->getIsDisposedProperty()) {
            fallback(material, "secondary texture ignored: it has been disposed");
            secondary = nullptr;
        }
        if (secondary != nullptr) {
            if (lit || bakedShare > 0.0f) {
                fallback(material, "two-texture surfaces draw unlit (XNA DualTextureEffect)");
            }
            if (material.environmentMap != nullptr) {
                fallback(material, "environment map ignored: two-texture surfaces draw through DualTextureEffect");
            }
            auto& effect = *dual_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(camera.View());
            effect.setProjectionProperty(camera.Projection());
            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setAlphaProperty(material.alpha);
            effect.setTextureProperty(texture);
            effect.setTexture2Property(secondary);
            device.getSamplerStatesProperty()[0] = SamplerFor(material);
            device.getSamplerStatesProperty()[1] = SecondarySamplerFor(material);
            effect.setVertexColorEnabledProperty(vertexColors);
            effect.setFogEnabledProperty(fogEnabled_ && material.fog);
            effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
            return true;
        }

        if (material.environmentMap != nullptr) {
            if (!lit) {
                fallback(material, "environment map ignored: the surface is drawn unlit");
            } else if (material.environmentMap->getIsDisposedProperty()) {
                fallback(material, "environment map ignored: it has been disposed");
            } else {
                if (vertexColors) {
                    fallback(material, "vertex colors ignored: XNA EnvironmentMapEffect has none");
                }
                auto& effect = *environment_;
                effect.getDirectionalLight0Property().setEnabledProperty(material.keyLight);
                effect.getDirectionalLight1Property().setEnabledProperty(fillEnabled_[0]);
                effect.getDirectionalLight2Property().setEnabledProperty(fillEnabled_[1]);
                effect.setWorldProperty(item.world);
                effect.setViewProperty(camera.View());
                effect.setProjectionProperty(camera.Projection());
                effect.setDiffuseColorProperty(material.diffuseColor);
                effect.setAlphaProperty(material.alpha);
                effect.setEmissiveColorProperty(material.emissiveColor);
                // EnvironmentMapEffect always samples its texture: white stands for "none".
                effect.setTextureProperty(texture != nullptr ? texture : white_.get());
                effect.setEnvironmentMapProperty(material.environmentMap);
                effect.setEnvironmentMapAmountProperty(material.environmentMapAmount);
                effect.setEnvironmentMapSpecularProperty(material.environmentMapSpecular);
                effect.setFresnelFactorProperty(material.fresnelFactor);
                device.getSamplerStatesProperty()[0] = SamplerFor(material);
                device.getSamplerStatesProperty()[1] = SecondarySamplerFor(material);
                effect.setFogEnabledProperty(fogEnabled_ && material.fog);
                effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
                return true;
            }
        }

        BasicEffect& effect = *basic_;
        const bool baked = bakedShare > 0.0f;
        // The key light can be switched off per material (interiors); fill
        // lights and ambient stay.
        auto& key = effect.getDirectionalLight0Property();
        key.setEnabledProperty(material.keyLight);
        effect.getDirectionalLight1Property().setEnabledProperty(fillEnabled_[0] && !baked);
        effect.getDirectionalLight2Property().setEnabledProperty(fillEnabled_[1] && !baked);
        if (baked) {
            key.setDiffuseColorProperty(Vector3(bakedShare, bakedShare, bakedShare));
            key.setSpecularColorProperty(Vector3::Zero);
            effect.setAmbientLightColorProperty(Vector3(1.0f - bakedShare, 1.0f - bakedShare, 1.0f - bakedShare));
            bakedLighting_ = true;
        } else if (bakedLighting_) {
            key.setDiffuseColorProperty(keyDiffuse_);
            key.setSpecularColorProperty(keySpecular_);
            effect.setAmbientLightColorProperty(ambient_);
            bakedLighting_ = false;
        }
        effect.setWorldProperty(item.world);
        effect.setViewProperty(camera.View());
        effect.setProjectionProperty(camera.Projection());
        effect.setLightingEnabledProperty(lit || baked);
        effect.setDiffuseColorProperty(material.diffuseColor);
        effect.setAlphaProperty(material.alpha);
        effect.setEmissiveColorProperty(material.emissiveColor);
        effect.setSpecularColorProperty(baked ? Vector3::Zero : material.specularColor);
        effect.setSpecularPowerProperty(material.specularPower);
        effect.setTextureEnabledProperty(texture != nullptr);
        effect.setTextureProperty(texture);
        if (texture != nullptr) {
            device.getSamplerStatesProperty()[0] = SamplerFor(material);
        }
        effect.setVertexColorEnabledProperty(vertexColors);
        effect.setFogEnabledProperty(fogEnabled_ && material.fog);
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        return true;
    }
    bool StockMaterialBinder::ApplySkinned(GraphicsDevice& device, const RenderItem& item, const RenderBucket bucket,
                                           const Camera& camera, Texture2D* texture, const bool vertexColors,
                                           const FallbackSink& fallback)
    {
        const Material& material = *item.material;
        if (bucket == RenderBucket::AlphaTested) {
            fallback(material, "alpha-tested skinned surfaces draw without the alpha test (XNA SkinnedEffect)");
        }
        if (material.shading == ShadingModel::Unlit) {
            fallback(material, "skinned surfaces draw lit (XNA SkinnedEffect)");
        } else if (material.shading == ShadingModel::PhysicallyBased) {
            fallback(material, "PhysicallyBased drawn as Lit (stock effects have no PBR)");
        }
        if (vertexColors) {
            fallback(material, "vertex colors ignored: XNA SkinnedEffect has none");
        }
        if (material.secondaryTexture != nullptr) {
            fallback(material, "secondary texture ignored: XNA SkinnedEffect has one texture");
        }
        SkinnedEffect& effect = *skinned_;
        effect.getDirectionalLight0Property().setEnabledProperty(material.keyLight);
        effect.getDirectionalLight1Property().setEnabledProperty(fillEnabled_[0]);
        effect.getDirectionalLight2Property().setEnabledProperty(fillEnabled_[1]);
        effect.setWorldProperty(item.world);
        effect.setViewProperty(camera.View());
        effect.setProjectionProperty(camera.Projection());
        effect.setDiffuseColorProperty(material.diffuseColor);
        effect.setAlphaProperty(material.alpha);
        effect.setEmissiveColorProperty(material.emissiveColor);
        effect.setSpecularColorProperty(material.specularColor);
        effect.setSpecularPowerProperty(material.specularPower);
        // SkinnedEffect always samples its texture: white stands for "none".
        effect.setTextureProperty(texture != nullptr ? texture : white_.get());
        effect.setWeightsPerVertexProperty(4);
        effect.SetBoneTransforms(*item.bones);
        device.getSamplerStatesProperty()[0] = SamplerFor(material);
        effect.setFogEnabledProperty(fogEnabled_ && material.fog);
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        return true;
    }
}
