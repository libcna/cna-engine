// SPDX-License-Identifier: MIT
#include "CnaEngine/Enhanced/ModelImport.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaModeEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedPbrEffect.hpp"

#include <algorithm>

namespace CnaEngine::Enhanced
{
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        template <typename TEffect>
        Graphics::PartMaterial FromPbr(const TEffect& pbr, const bool skinned)
        {
            Graphics::PartMaterial result;
            result.skinned = skinned;
            Graphics::Material& m = result.material;
            m.shading = Graphics::ShadingModel::PhysicallyBased;
            switch (pbr.getAlphaModeEXTProperty()) {
            case Xna::AlphaModeEXT::Opaque: m.blend = Graphics::BlendMode::Opaque; break;
            case Xna::AlphaModeEXT::Mask: m.blend = Graphics::BlendMode::AlphaTest; break;
            case Xna::AlphaModeEXT::Blend: m.blend = Graphics::BlendMode::AlphaBlend; break;
            }
            m.alpha = pbr.getAlphaProperty();
            m.alphaCutoff = pbr.getAlphaCutoffEXTProperty();
            m.doubleSided = pbr.getDoubleSidedEXTProperty();
            m.diffuseColor = pbr.getDiffuseColorProperty();
            m.emissiveColor = pbr.getEmissiveFactorProperty();
            m.metallic = pbr.getMetallicFactorProperty();
            m.roughness = pbr.getRoughnessFactorProperty();
            m.normalScale = pbr.getNormalScaleEXTProperty();
            m.occlusionStrength = pbr.getOcclusionStrengthEXTProperty();
            m.ior = pbr.getIorEXTProperty();
            m.specularFactor = pbr.getSpecularFactorEXTProperty();
            m.baseTexture = pbr.getTextureProperty();
            m.normalTexture = pbr.getNormalMapProperty();
            m.occlusionRoughnessMetallicTexture = pbr.getMetallicRoughnessMapProperty();
            m.emissiveTexture = pbr.getEmissiveMapProperty();
            return result;
        }
    }

    std::optional<Graphics::PartMaterial> PbrMaterialFromEffect(const Xna::Effect& effect)
    {
        if (const auto* skinned = dynamic_cast<const Xna::SkinnedPbrEffect*>(&effect)) {
            return FromPbr(*skinned, true);
        }
        if (const auto* pbr = dynamic_cast<const Xna::PbrEffect*>(&effect)) {
            return FromPbr(*pbr, false);
        }
        return std::nullopt;
    }

    const Xna::SkinningData* FindSkinningData(const Xna::Model& model)
    {
        for (const auto& skin : model.getSkinsEXTProperty()) {
            if (skin.Data != nullptr) {
                return skin.Data;
            }
        }
        return dynamic_cast<const Xna::SkinningData*>(model.getTagProperty());
    }

    Animation::Skeleton SkeletonFromSkinningData(const Xna::SkinningData& skin)
    {
        Animation::Skeleton skeleton;
        skeleton.parents = skin.SkeletonHierarchy;
        skeleton.inverseBind = skin.InverseBindPose;
        skeleton.rootPrefix = skin.SkeletonRootPrefix;
        skeleton.bindPose.reserve(skin.BindPose.size());
        for (const auto& matrix : skin.BindPose) {
            skeleton.bindPose.push_back(Animation::BonePose::FromMatrix(matrix));
        }
        for (std::size_t i = 0; i < skeleton.parents.size(); ++i) {
            skeleton.names.push_back("bone " + std::to_string(i));
        }
        skeleton.Validate();
        return skeleton;
    }

    std::vector<Animation::AnimationClip> ClipsFromSkinningData(const Xna::SkinningData& skin)
    {
        std::vector<Animation::AnimationClip> clips;
        for (const auto& [name, source] : skin.AnimationClips) {
            Animation::AnimationClip clip;
            clip.name = name;
            clip.duration = static_cast<float>(source.Duration.getTotalSecondsProperty());
            for (const auto& track : source.Tracks) {
                Animation::BoneTrack converted;
                converted.bone = track.BoneIndex;
                for (const auto& key : track.Keys) {
                    Animation::BoneKey k;
                    k.time = static_cast<float>(key.Time.getTotalSecondsProperty());
                    k.pose.translation = key.Translation;
                    k.pose.rotation = key.Rotation;
                    k.pose.scale = key.Scale;
                    converted.keys.push_back(k);
                }
                clip.tracks.push_back(std::move(converted));
            }
            clips.push_back(std::move(clip));
        }
        std::sort(clips.begin(), clips.end(),
                  [](const Animation::AnimationClip& a, const Animation::AnimationClip& b) { return a.name < b.name; });
        return clips;
    }
}
