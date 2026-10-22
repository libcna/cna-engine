// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/LocalLightBinder.hpp"

#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Graphics/IShadowReceiverEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/PunctualLightEXT.hpp"

namespace CnaEngine::Enhanced
{
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    LocalLightBinder::LocalLightBinder(Graphics::ForwardRenderer& renderer)
        : renderer_(renderer), inner_(renderer.SharedMaterialBinder())
    {
    }

    void LocalLightBinder::BeginFrame(Xna::GraphicsDevice& device, const Graphics::Camera& camera,
                                      const Graphics::ForwardLighting& lighting)
    {
        inner_->BeginFrame(device, camera, lighting);
    }

    bool LocalLightBinder::Apply(Xna::GraphicsDevice& device, const Graphics::RenderItem& item,
                                 const Graphics::RenderBucket bucket, const Graphics::Camera& camera,
                                 const Graphics::FallbackSink& fallback)
    {
        Xna::PunctualLightEXT punctual;
        const auto chosen = Graphics::StrongestLight(lights, item.bounds);
        last_ = chosen ? static_cast<int>(*chosen) : -1;
        if (chosen) {
            const Graphics::LocalLight& light = lights[*chosen];
            punctual.Kind = light.kind == Graphics::LocalLight::Kind::Spot ? Xna::PunctualLightKindEXT::Spot
                                                                           : Xna::PunctualLightKindEXT::Point;
            punctual.Position = light.position;
            punctual.Direction = light.direction;
            punctual.DiffuseColor = light.color;
            punctual.Range = light.range;
            punctual.InnerAngle = light.innerAngle;
            punctual.OuterAngle = light.outerAngle;
        }
        auto& stock = *renderer_.StockBinder();
        stock.LitEffect().setPunctualLightEXT(punctual);
        stock.SkinnedLitEffect().setPunctualLightEXT(punctual);
        if (auto* pbr = dynamic_cast<PbrMaterialBinder*>(inner_.get())) {
            pbr->Effect().setPunctualLightEXT(punctual);
            pbr->SkinnedEffect().setPunctualLightEXT(punctual);
        }
        return inner_->Apply(device, item, bucket, camera, fallback);
    }

    const Xna::BlendState& LocalLightBinder::BlendStateFor(const Graphics::RenderItem& item) const
    {
        return inner_->BlendStateFor(item);
    }

    LocalLightBinder& InstallLocalLights(Graphics::ForwardRenderer& renderer)
    {
        auto binder = std::make_shared<LocalLightBinder>(renderer);
        LocalLightBinder& result = *binder;
        renderer.SetMaterialBinder(std::move(binder));
        return result;
    }
}
