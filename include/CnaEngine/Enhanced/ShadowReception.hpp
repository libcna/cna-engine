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

    /// Shadows for an EngineGame: the game renders a shadow map of each
    /// frame's opaque items (EngineGame::EnableShadowMap) and the renderer's
    /// effects receive it. Call once the renderer exists (in LoadGame).
    /// Devices that cannot sample shadow maps (WebGPU, headless) get no map:
    /// returns false then.
    bool EnableShadows(App::EngineGame& game, App::ShadowSettings settings = {},
                       ShadowReceptionSettings reception = {});
}
