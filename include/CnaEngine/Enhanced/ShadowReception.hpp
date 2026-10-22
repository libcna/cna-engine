// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001 addendum): this header uses CNA's shadow
// extension (IShadowReceiverEXT) and must never be included from the strict
// modules. It does not depend on cna-extended.
#pragma once

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/ShadowMap.hpp"

namespace CnaEngine::Enhanced
{
    struct ShadowReceptionSettings
    {
        /// Percentage-closer filtering: 0 one sample, 1 a 3 x 3, 2 a 5 x 5 neighbourhood.
        int filterRadius = 1;
        /// Added to ShadowMap::SuggestedBias (acne on steep surfaces wants more).
        float extraBias = 0.0f;
    };

    /// Makes every effect the renderer's binders draw lit materials with --
    /// the stock binder's BasicEffect and SkinnedEffect, and a
    /// PbrMaterialBinder's PbrEffect and SkinnedPbrEffect when it is the
    /// renderer's binder -- sample `map` (null switches reception off). Call
    /// after the map was rendered, before the frame is drawn.
    void ApplyShadowReception(Graphics::ForwardRenderer& renderer, Graphics::ShadowMap* map,
                              ShadowReceptionSettings settings = {});

    /// Finds which way up this device's shadow receivers read a map
    /// (upstream-issues.md UP-019): draws a small shadow through a receiving
    /// BasicEffect into a scratch target and looks at where it fell. Sets
    /// `map`'s orientation and returns whether it is drawn upside down; does
    /// nothing (false) on a device that cannot sample shadow maps.
    bool CalibrateShadowMap(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Graphics::ShadowMap& map);

    /// Shadows for an EngineGame: the game renders a shadow map of each
    /// frame's opaque items (EngineGame::EnableShadowMap) and the renderer's
    /// effects receive it, calibrated for the device (CalibrateShadowMap).
    /// Call once the renderer exists (in LoadGame).
    /// Devices that cannot sample shadow maps (WebGPU, headless) get no map:
    /// returns false then.
    bool EnableShadows(App::EngineGame& game, App::ShadowSettings settings = {},
                       ShadowReceptionSettings reception = {});
}
