// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaModeEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureTransformEXT.hpp"

#include <stdexcept>
#include <utility>

namespace CnaEngine::Enhanced
{
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::AlphaModeEXT;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::PbrEffect;
    using Microsoft::Xna::Framework::Graphics::Texture2D;
    using Microsoft::Xna::Framework::Graphics::TextureTransformEXT;

    PbrMaterialBinder::PbrMaterialBinder(GraphicsDevice& device, std::shared_ptr<Graphics::StockMaterialBinder> fallback,
                                         const PbrSettings settings)
        : effect_(std::make_unique<PbrEffect>(device)), fallback_(std::move(fallback)), settings_(settings)
    {
        if (!fallback_) {
            throw std::invalid_argument("PbrMaterialBinder: a stock binder is required for delegated materials");
        }
    }

    void PbrMaterialBinder::BeginFrame(GraphicsDevice& device, const Graphics::Camera& camera,
                                       const Graphics::ForwardLighting& lighting)
    {
        fallback_->BeginFrame(device, camera, lighting);
        PbrEffect& effect = *effect_;
        effect.setLightingEnabledProperty(true);
        auto& key = effect.getDirectionalLight0Property();
        key.setEnabledProperty(true);
        key.setDirectionProperty(Vector3::Normalize(lighting.lightDirection));
        key.setDiffuseColorProperty(lighting.lightColor * settings_.lightIntensity);
        key.setSpecularColorProperty(lighting.lightColor * settings_.lightIntensity);
        effect.getDirectionalLight1Property().setEnabledProperty(false);
        effect.getDirectionalLight2Property().setEnabledProperty(false);
        effect.setAmbientLightColorProperty(lighting.ambientColor);
        effect.setFogEnabledProperty(lighting.fogEnabled);
        effect.setFogColorProperty(lighting.fogColor);
        effect.setFogStartProperty(lighting.fogStart);
        effect.setFogEndProperty(lighting.fogEnd);
        effect.setEncodeOutputToSrgbEXTProperty(settings_.encodeOutputToSrgb);
    }

    bool PbrMaterialBinder::Apply(GraphicsDevice& device, const Graphics::RenderItem& item,
                                  const Graphics::RenderBucket bucket, const Graphics::Camera& camera,
                                  const Graphics::FallbackSink& fallback)
    {
        using Graphics::VertexAttribute;
        const Graphics::Material& material = *item.material;
        const Graphics::AttributeMask attributes = item.mesh->Attributes();

        if (material.shading == Graphics::ShadingModel::Unlit || material.vertexColors) {
            return fallback_->Apply(device, item, bucket, camera, fallback);
        }
        if (!attributes.Has(VertexAttribute::Normal)) {
            return fallback_->Apply(device, item, bucket, camera, fallback);
        }

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

        PbrEffect& effect = *effect_;
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

        switch (bucket) {
            case Graphics::RenderBucket::Opaque: effect.setAlphaModeEXTProperty(AlphaModeEXT::Opaque); break;
            case Graphics::RenderBucket::AlphaTested: effect.setAlphaModeEXTProperty(AlphaModeEXT::Mask); break;
            case Graphics::RenderBucket::Transparent:
            case Graphics::RenderBucket::Overlay:
            case Graphics::RenderBucket::Count: effect.setAlphaModeEXTProperty(AlphaModeEXT::Blend); break;
        }
        effect.setAlphaCutoffEXTProperty(material.alphaCutoff);
        effect.setDoubleSidedEXTProperty(material.doubleSided);

        TextureTransformEXT transform;
        transform.Scale = material.uvScale;
        transform.Offset = material.uvOffset;
        for (int slot = 0; slot < 5; ++slot) {
            effect.setTextureTransformEXTProperty(slot, transform);
        }

        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        return true;
    }
}
