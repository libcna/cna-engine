// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001 addendum): this header uses CNA graphics
// extensions (PbrEffect, SkinnedPbrEffect, SkinningData, Model's skins) and
// must never be included from the strict modules. It does not depend on
// cna-extended.
#pragma once

#include "CnaEngine/Animation/Skeleton.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Graphics/AnimationPlayer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"

#include <optional>
#include <vector>

namespace CnaEngine::Enhanced
{
    /// A PbrEffect or SkinnedPbrEffect part -- what CNA's glTF import gives
    /// -- as a physically based material: base colour, alpha and its mode,
    /// metallic, roughness, emissive, the four maps, normal scale, occlusion
    /// strength, IOR, specular, double-sidedness. Street- and room-simulator's
    /// translation; an EffectMaterialReader for Graphics::ModelView. Draw such
    /// parts with PbrMaterialBinder.
    [[nodiscard]] std::optional<Graphics::PartMaterial> PbrMaterialFromEffect(
        const Microsoft::Xna::Framework::Graphics::Effect& effect);

    /// The model's skin: the first of its skins (glTF import), else the
    /// SkinningData in its Tag (compiled .cnb models); null for a rigid model.
    [[nodiscard]] const Microsoft::Xna::Framework::Graphics::SkinningData* FindSkinningData(
        const Microsoft::Xna::Framework::Graphics::Model& model);

    /// CNA's skeleton as the engine's: parents, bind pose (decomposed), inverse
    /// binds, the root prefix. Bones are named "bone <index>" (SkinningData
    /// carries no names).
    [[nodiscard]] Animation::Skeleton SkeletonFromSkinningData(
        const Microsoft::Xna::Framework::Graphics::SkinningData& skin);

    /// CNA's clips as the engine's, ordered by name. Keyframe times become
    /// seconds.
    [[nodiscard]] std::vector<Animation::AnimationClip> ClipsFromSkinningData(
        const Microsoft::Xna::Framework::Graphics::SkinningData& skin);
}
