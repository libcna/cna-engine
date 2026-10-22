// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/ShadowReception.hpp"

#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Graphics/IShadowReceiverEXT.hpp"

namespace CnaEngine::Enhanced
{
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        void Receive(Xna::IShadowReceiverEXT& effect, Graphics::ShadowMap* map, const ShadowReceptionSettings& settings)
        {
            if (map == nullptr) {
                effect.setShadowsEnabledEXT(false);
                effect.setShadowMapEXT(nullptr);
                return;
            }
            effect.setShadowMapEXT(&map->Map());
            effect.setLightViewProjectionEXT(map->LightViewProjection());
            effect.setShadowDepthBiasEXT(map->SuggestedBias() + settings.extraBias);
            effect.setShadowFilterRadiusEXT(settings.filterRadius);
            effect.setShadowsEnabledEXT(true);
        }
    }

    void ApplyShadowReception(Graphics::ForwardRenderer& renderer, Graphics::ShadowMap* map,
                              const ShadowReceptionSettings settings)
    {
        auto& stock = *renderer.StockBinder();
        Receive(stock.LitEffect(), map, settings);
        Receive(stock.SkinnedLitEffect(), map, settings);
        if (auto* pbr = dynamic_cast<PbrMaterialBinder*>(&renderer.MaterialBinder())) {
            Receive(pbr->Effect(), map, settings);
            Receive(pbr->SkinnedEffect(), map, settings);
        }
    }

    bool EnableShadows(App::EngineGame& game, const App::ShadowSettings settings,
                       const ShadowReceptionSettings reception)
    {
        if (!game.getGraphicsDeviceProperty().SupportsShadowSamplingEXT()) {
            return false;
        }
        Graphics::ForwardRenderer* renderer = &game.Renderer();
        game.EnableShadowMap(settings, [renderer, reception](Graphics::ShadowMap& map) {
            ApplyShadowReception(*renderer, &map, reception);
        });
        return true;
    }
}
