// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/MaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
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
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    StockMaterialBinder::StockMaterialBinder(GraphicsDevice& device)
        : basic_(std::make_unique<BasicEffect>(device)),
          alphaTest_(std::make_unique<AlphaTestEffect>(device)),
          dual_(std::make_unique<Microsoft::Xna::Framework::Graphics::DualTextureEffect>(device)),
          white_(std::make_unique<Texture2D>(device, 1, 1))
    {
        const Color white(255, 255, 255, 255);
        white_->SetData(&white, 1);
    }

    void StockMaterialBinder::BeginFrame(GraphicsDevice&, const Camera&, const ForwardLighting& lighting)
    {
        defaultLighting_ = lighting.defaultLighting;
        if (lighting.defaultLighting) {
            basic_->EnableDefaultLighting();
            fillEnabled_[0] = fillEnabled_[1] = true;
        } else {
            auto& key = basic_->getDirectionalLight0Property();
            key.setEnabledProperty(true);
            key.setDirectionProperty(Vector3::Normalize(lighting.lightDirection));
            key.setDiffuseColorProperty(lighting.lightColor);
            key.setSpecularColorProperty(lighting.lightColor);
            Microsoft::Xna::Framework::Graphics::DirectionalLight* fills[2] = {&basic_->getDirectionalLight1Property(),
                                                                                &basic_->getDirectionalLight2Property()};
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
        basic_->setPreferPerPixelLightingProperty(lighting.perPixelLighting);
        basic_->setAmbientLightColorProperty(lighting.ambientColor);
        for (IEffectFog* fog : {static_cast<IEffectFog*>(basic_.get()), static_cast<IEffectFog*>(alphaTest_.get()),
                                static_cast<IEffectFog*>(dual_.get())}) {
            fog->setFogEnabledProperty(lighting.fogEnabled);
            fog->setFogColorProperty(lighting.fogColor);
            fog->setFogStartProperty(lighting.fogStart);
            fog->setFogEndProperty(lighting.fogEnd);
        }
    }

    bool StockMaterialBinder::Apply(GraphicsDevice&, const RenderItem& item, const RenderBucket bucket,
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

        if (bucket == RenderBucket::AlphaTested) {
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
            effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
            return true;
        }

        bool lit = material.shading != ShadingModel::Unlit;
        if (lit && !attributes.Has(VertexAttribute::Normal)) {
            fallback(material, "drawn unlit: mesh has no normals");
            lit = false;
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
            if (lit) {
                fallback(material, "two-texture surfaces draw unlit (XNA DualTextureEffect)");
            }
            auto& effect = *dual_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(camera.View());
            effect.setProjectionProperty(camera.Projection());
            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setAlphaProperty(material.alpha);
            effect.setTextureProperty(texture);
            effect.setTexture2Property(secondary);
            effect.setVertexColorEnabledProperty(vertexColors);
            effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
            return true;
        }

        BasicEffect& effect = *basic_;
        // The key light can be switched off per material (interiors); fill
        // lights and ambient stay.
        effect.getDirectionalLight0Property().setEnabledProperty(material.keyLight);
        effect.getDirectionalLight1Property().setEnabledProperty(fillEnabled_[0]);
        effect.getDirectionalLight2Property().setEnabledProperty(fillEnabled_[1]);
        effect.setWorldProperty(item.world);
        effect.setViewProperty(camera.View());
        effect.setProjectionProperty(camera.Projection());
        effect.setLightingEnabledProperty(lit);
        effect.setDiffuseColorProperty(material.diffuseColor);
        effect.setAlphaProperty(material.alpha);
        effect.setEmissiveColorProperty(material.emissiveColor);
        effect.setSpecularColorProperty(material.specularColor);
        effect.setSpecularPowerProperty(material.specularPower);
        effect.setTextureEnabledProperty(texture != nullptr);
        effect.setTextureProperty(texture);
        effect.setVertexColorEnabledProperty(vertexColors);
        effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
        return true;
    }
}
