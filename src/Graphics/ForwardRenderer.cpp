// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectFog.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::AlphaTestEffect;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::CompareFunction;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        // Restores the states a renderer changes, whatever happens in between.
        class StateScope
        {
        public:
            explicit StateScope(GraphicsDevice& device)
                : device_(device),
                  blend_(device.getBlendStateProperty()),
                  depth_(device.getDepthStencilStateProperty()),
                  rasterizer_(device.getRasterizerStateProperty())
            {
            }
            ~StateScope()
            {
                device_.setBlendStateProperty(blend_);
                device_.setDepthStencilStateProperty(depth_);
                device_.setRasterizerStateProperty(rasterizer_);
            }
            StateScope(const StateScope&) = delete;
            StateScope& operator=(const StateScope&) = delete;

        private:
            GraphicsDevice& device_;
            BlendState blend_;
            DepthStencilState depth_;
            RasterizerState rasterizer_;
        };
    }

    ForwardRenderer::ForwardRenderer(GraphicsDevice& device)
        : device_(device),
          basic_(std::make_unique<BasicEffect>(device)),
          alphaTest_(std::make_unique<AlphaTestEffect>(device)),
          white_(std::make_unique<Texture2D>(device, 1, 1))
    {
        const Color white(255, 255, 255, 255);
        white_->SetData(&white, 1);
    }

    void ForwardRenderer::AddPass(const PassPoint point, std::string name, PassCallback callback)
    {
        passes_.push_back(Pass{point, std::move(name), std::move(callback)});
    }

    std::size_t ForwardRenderer::RemovePass(const std::string& name)
    {
        return std::erase_if(passes_, [&](const Pass& pass) { return pass.name == name; });
    }

    void ForwardRenderer::Fallback(const Material& material, const std::string& what)
    {
        std::string message = "material '" + material.name + "': " + what;
        if (std::find(fallbacks_.begin(), fallbacks_.end(), message) == fallbacks_.end()) {
            fallbacks_.push_back(std::move(message));
        }
    }

    void ForwardRenderer::RunPasses(const PassPoint point, const Camera& camera, RenderStatistics& statistics)
    {
        for (const auto& pass : passes_) {
            if (pass.point == point && pass.callback) {
                pass.callback(device_, camera, statistics);
            }
        }
    }

    void ForwardRenderer::Render(const RenderQueue& queue, const Camera& camera, RenderStatistics* statistics)
    {
        RenderStatistics local;
        RenderStatistics& stats = statistics != nullptr ? *statistics : local;
        fallbacks_.clear();
        const StateScope restore(device_);

        if (lighting_.defaultLighting) {
            basic_->EnableDefaultLighting();
        }
        basic_->setAmbientLightColorProperty(lighting_.ambientColor);
        for (auto* fog : {static_cast<Microsoft::Xna::Framework::Graphics::IEffectFog*>(basic_.get()),
                          static_cast<Microsoft::Xna::Framework::Graphics::IEffectFog*>(alphaTest_.get())}) {
            fog->setFogEnabledProperty(lighting_.fogEnabled);
            fog->setFogColorProperty(lighting_.fogColor);
            fog->setFogStartProperty(lighting_.fogStart);
            fog->setFogEndProperty(lighting_.fogEnd);
        }

        RunPasses(PassPoint::BeforeOpaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Opaque), RenderBucket::Opaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::AlphaTested), RenderBucket::AlphaTested, camera, stats);
        RunPasses(PassPoint::AfterOpaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Transparent), RenderBucket::Transparent, camera, stats);
        RunPasses(PassPoint::AfterTransparent, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Overlay), RenderBucket::Overlay, camera, stats);
        RunPasses(PassPoint::AfterOverlay, camera, stats);
    }

    void ForwardRenderer::DrawBucket(const std::span<const RenderItem> items, const RenderBucket bucket,
                                     const Camera& camera, RenderStatistics& statistics)
    {
        switch (bucket) {
            case RenderBucket::Opaque:
            case RenderBucket::AlphaTested:
                device_.setBlendStateProperty(BlendState::Opaque);
                device_.setDepthStencilStateProperty(DepthStencilState::Default);
                break;
            case RenderBucket::Transparent:
                device_.setBlendStateProperty(BlendState::AlphaBlend);
                device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
                break;
            case RenderBucket::Overlay:
            case RenderBucket::Count:
                device_.setBlendStateProperty(BlendState::AlphaBlend);
                device_.setDepthStencilStateProperty(DepthStencilState::None);
                break;
        }
        device_.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
        for (const auto& item : items) {
            DrawItem(item, bucket, camera, statistics);
        }
    }

    void ForwardRenderer::DrawItem(const RenderItem& item, const RenderBucket bucket, const Camera& camera,
                                   RenderStatistics& statistics)
    {
        const Material& material = *item.material;
        const GpuMesh& mesh = *item.mesh;
        const AttributeMask attributes = mesh.Attributes();

        Texture2D* texture = material.baseTexture;
        if (texture != nullptr && !attributes.Has(VertexAttribute::TexCoord0)) {
            Fallback(material, "texture ignored: mesh has no TexCoord0");
            texture = nullptr;
        }
        if (texture != nullptr && texture->getIsDisposedProperty()) {
            Fallback(material, "texture ignored: it has been disposed");
            texture = nullptr;
        }
        bool vertexColors = material.vertexColors;
        if (vertexColors && !attributes.Has(VertexAttribute::Color)) {
            Fallback(material, "vertex colors ignored: mesh has no Color attribute");
            vertexColors = false;
        }

        device_.setRasterizerStateProperty(material.doubleSided ? RasterizerState::CullNone
                                                                : RasterizerState::CullCounterClockwise);
        if (bucket == RenderBucket::Transparent) {
            device_.setBlendStateProperty(material.blend == BlendMode::Additive ? BlendState::Additive
                                                                                : BlendState::AlphaBlend);
        }

        if (bucket == RenderBucket::AlphaTested) {
            if (material.shading != ShadingModel::Unlit) {
                Fallback(material, "alpha-tested surfaces draw unlit (XNA AlphaTestEffect)");
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
        } else {
            bool lit = material.shading != ShadingModel::Unlit;
            if (lit && !attributes.Has(VertexAttribute::Normal)) {
                Fallback(material, "drawn unlit: mesh has no normals");
                lit = false;
            } else if (material.shading == ShadingModel::PhysicallyBased) {
                Fallback(material, "PhysicallyBased drawn as Lit (stock effects have no PBR)");
            }
            BasicEffect& effect = *basic_;
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
        }
        mesh.Draw(device_, &statistics);
    }
}
