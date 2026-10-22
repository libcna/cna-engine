// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/ShadowReception.hpp"

#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/IShadowReceiverEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <vector>

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

    bool CalibrateShadowMap(Xna::GraphicsDevice& device, Graphics::ShadowMap& map)
    {
        using Microsoft::Xna::Framework::Matrix;
        using Microsoft::Xna::Framework::Vector3;
        if (!device.SupportsShadowSamplingEXT()) {
            return false;
        }
        // A light straight down over a 2 m floor, and a plate 0.5 m up over
        // the floor's -Z half: its shadow belongs on the -Z half.
        constexpr int Size = 16;
        Graphics::ShadowMap probe(device, Size);
        probe.SetDrawnUpsideDown(false);
        probe.Fit(Vector3(0.0f, 1.0f, 0.0f), Vector3::Zero, 1.0f);
        const auto& layout = Graphics::VertexLayout::PositionNormalTexture();
        Graphics::MeshBuilder builder;
        builder.AddBox(Vector3(-1.0f, -0.1f, -1.0f), Vector3(1.0f, 0.0f, 1.0f));
        const Graphics::GpuMesh floor = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
        builder.AddBox(Vector3(-1.0f, 0.5f, -1.0f), Vector3(1.0f, 0.55f, 0.0f));
        const Graphics::GpuMesh plate = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
        const Graphics::Material solid;
        Graphics::RenderItem caster;
        caster.mesh = &plate;
        caster.material = &solid;
        caster.bounds = Graphics::WorldBounds(plate, caster.world);
        probe.Render(device, std::span<const Graphics::RenderItem>(&caster, 1));

        // The floor seen from above with -Z at the top of the picture, lit
        // from above, receiving the map.
        Xna::RenderTarget2D target(device, Size, Size, false, Xna::SurfaceFormat::Color, Xna::DepthFormat::Depth24);
        Xna::BasicEffect receiver(device);
        receiver.setLightingEnabledProperty(true);
        receiver.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
        receiver.setAmbientLightColorProperty(Vector3::Zero);
        receiver.getDirectionalLight0Property().setEnabledProperty(true);
        receiver.getDirectionalLight0Property().setDirectionProperty(Vector3(0.0f, -1.0f, 0.0f));
        receiver.getDirectionalLight0Property().setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
        receiver.getDirectionalLight1Property().setEnabledProperty(false);
        receiver.getDirectionalLight2Property().setEnabledProperty(false);
        receiver.setWorldProperty(Matrix::getIdentityProperty());
        receiver.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 5.0f, 0.0f), Vector3::Zero, Vector3::Forward));
        receiver.setProjectionProperty(Matrix::CreateOrthographic(2.0f, 2.0f, 0.1f, 10.0f));
        receiver.setShadowMapEXT(&probe.Map());
        receiver.setLightViewProjectionEXT(probe.LightViewProjection());
        receiver.setShadowDepthBiasEXT(0.01f);
        receiver.setShadowFilterRadiusEXT(0);
        receiver.setShadowsEnabledEXT(true);

        const auto blend = device.getBlendStateProperty();
        const auto depth = device.getDepthStencilStateProperty();
        const auto rasterizer = device.getRasterizerStateProperty();
        device.SetRenderTarget(&target);
        device.Clear(Microsoft::Xna::Framework::Color(0, 0, 0, 255));
        device.setBlendStateProperty(Xna::BlendState::Opaque);
        device.setDepthStencilStateProperty(Xna::DepthStencilState::Default);
        device.setRasterizerStateProperty(Xna::RasterizerState::CullNone);
        auto& passes = receiver.getCurrentTechniqueProperty()->getPassesProperty();
        for (int i = 0; i < passes.getCountProperty(); ++i) {
            passes[i]->Apply();
            (void)floor.Draw(device);
        }
        device.SetRenderTarget(nullptr);
        device.setBlendStateProperty(blend);
        device.setDepthStencilStateProperty(depth);
        device.setRasterizerStateProperty(rasterizer);

        std::vector<Microsoft::Xna::Framework::Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        long top = 0;    // -Z: where the shadow belongs
        long bottom = 0; // +Z
        for (int y = 0; y < Size; ++y) {
            for (int x = 0; x < Size; ++x) {
                (y < Size / 2 ? top : bottom) += pixels[static_cast<std::size_t>(y * Size + x)].getRProperty();
            }
        }
        const bool upsideDown = top > bottom;
        map.SetDrawnUpsideDown(upsideDown);
        return upsideDown;
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
        CalibrateShadowMap(game.getGraphicsDeviceProperty(), *game.Shadows());
        return true;
    }
}
