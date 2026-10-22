// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// What a part's effect says about its surface.
    struct PartMaterial
    {
        Material material;
        /// The part is skinned (drawn with a bone palette).
        bool skinned = false;
    };

    /// Reads a model part's effect into a material; nothing for an effect it
    /// does not know (the next reader is asked).
    using EffectMaterialReader = std::function<std::optional<PartMaterial>(const Microsoft::Xna::Framework::Graphics::Effect&)>;

    /// XNA's stock effects as materials: BasicEffect and SkinnedEffect
    /// (diffuse, alpha, emissive, specular, texture, vertex colours, unlit
    /// when lighting is off) and AlphaTestEffect (alpha-tested).
    [[nodiscard]] std::optional<PartMaterial> StockMaterialFromEffect(const Microsoft::Xna::Framework::Graphics::Effect& effect);

    /// One drawable part of a model.
    struct ModelPart
    {
        GpuMesh mesh;
        PartMaterial material;
        /// The model bone the part's mesh hangs from.
        int bone = -1;
        /// The mesh's bounds in its bone's space.
        Microsoft::Xna::Framework::BoundingSphere bounds;
        std::string meshName;
    };

    /// An XNA Model (from the content manager: XNB, or a .gltf/.glb CNA
    /// imports at run time) as render items, so a model is drawn by the
    /// forward renderer with culling and the engine's materials like any
    /// other item. Every part's buffers are borrowed (GpuMesh::Borrow); its
    /// material is read from its effect.
    ///
    /// Rigid parts are placed by their bone chain (the model's rest pose, or
    /// SetBoneTransforms for node animation); skinned parts are placed by the
    /// palette given to AppendItems (Animation::Animator::SkinPalette).
    ///
    /// Street-, room- and museum-simulator each wrote this walk over meshes,
    /// parts, bones and effects (docs/game-facilities-survey.md).
    class ModelView
    {
    public:
        /// Borrows `model`, which must outlive the view. Each part's effect
        /// goes to `reader` first, then to StockMaterialFromEffect; a part
        /// neither reads gets a lit grey material, listed in Fallbacks().
        explicit ModelView(const Microsoft::Xna::Framework::Graphics::Model& model, EffectMaterialReader reader = {});

        [[nodiscard]] std::span<ModelPart> Parts() noexcept { return parts_; }
        [[nodiscard]] std::span<const ModelPart> Parts() const noexcept { return parts_; }
        [[nodiscard]] bool Skinned() const noexcept;
        /// Parts whose effect no reader knew: "mesh: effect type".
        [[nodiscard]] const std::vector<std::string>& Fallbacks() const noexcept { return fallbacks_; }

        /// Local transforms of the model's bones (one per bone, relative to
        /// the parent); the rest pose is the model's own.
        void SetBoneTransforms(std::span<const Microsoft::Xna::Framework::Matrix> local);
        void ResetBoneTransforms();
        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Matrix>& AbsoluteBoneTransforms() const noexcept
        {
            return absolute_;
        }

        /// The model's render items at `world`. Skinned parts take `palette`
        /// (they are skipped without one).
        void AppendItems(std::vector<RenderItem>& out, const Microsoft::Xna::Framework::Matrix& world,
                         const std::vector<Microsoft::Xna::Framework::Matrix>* palette = nullptr) const;
        /// The bounds of every rigid part at `world` (skinned parts at their rest place).
        [[nodiscard]] Microsoft::Xna::Framework::BoundingSphere Bounds(const Microsoft::Xna::Framework::Matrix& world) const;

    private:
        const Microsoft::Xna::Framework::Graphics::Model* model_;
        std::vector<ModelPart> parts_;
        std::vector<int> boneParents_;
        std::vector<Microsoft::Xna::Framework::Matrix> rest_;
        std::vector<Microsoft::Xna::Framework::Matrix> absolute_;
        std::vector<std::string> fallbacks_;
    };
}
