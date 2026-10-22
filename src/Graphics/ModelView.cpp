// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBoneCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPartCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"

#include <stdexcept>
#include <typeinfo>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        template <typename TEffect>
        PartMaterial FromLitEffect(const TEffect& effect, const bool skinned)
        {
            PartMaterial result;
            Material& m = result.material;
            m.diffuseColor = effect.getDiffuseColorProperty();
            m.alpha = effect.getAlphaProperty();
            m.emissiveColor = effect.getEmissiveColorProperty();
            m.specularColor = effect.getSpecularColorProperty();
            m.specularPower = effect.getSpecularPowerProperty();
            m.baseTexture = effect.getTextureProperty();
            m.blend = m.alpha < 1.0f ? BlendMode::AlphaBlend : BlendMode::Opaque;
            result.skinned = skinned;
            return result;
        }

        AttributeMask AttributesOf(const Xna::VertexDeclaration& declaration)
        {
            AttributeMask mask;
            for (const auto& element : declaration.GetVertexElements()) {
                switch (element.getVertexElementUsageProperty()) {
                case Xna::VertexElementUsage::Position: mask.Add(VertexAttribute::Position); break;
                case Xna::VertexElementUsage::Normal: mask.Add(VertexAttribute::Normal); break;
                case Xna::VertexElementUsage::Tangent: mask.Add(VertexAttribute::Tangent); break;
                case Xna::VertexElementUsage::Color: mask.Add(VertexAttribute::Color); break;
                case Xna::VertexElementUsage::BlendIndices: mask.Add(VertexAttribute::BlendIndices); break;
                case Xna::VertexElementUsage::BlendWeight: mask.Add(VertexAttribute::BlendWeights); break;
                case Xna::VertexElementUsage::TextureCoordinate:
                    if (element.getUsageIndexProperty() == 0) {
                        mask.Add(VertexAttribute::TexCoord0);
                    } else if (element.getUsageIndexProperty() == 1) {
                        mask.Add(VertexAttribute::TexCoord1);
                    }
                    break;
                default: break;
                }
            }
            return mask;
        }

        BoundingSphere Transformed(const BoundingSphere& sphere, const Matrix& world)
        {
            return sphere.Transform(world);
        }
    }

    std::optional<PartMaterial> StockMaterialFromEffect(const Xna::Effect& effect)
    {
        if (const auto* skinned = dynamic_cast<const Xna::SkinnedEffect*>(&effect)) {
            return FromLitEffect(*skinned, true);
        }
        if (const auto* basic = dynamic_cast<const Xna::BasicEffect*>(&effect)) {
            PartMaterial result = FromLitEffect(*basic, false);
            if (!basic->getLightingEnabledProperty()) {
                result.material.shading = ShadingModel::Unlit;
            }
            result.material.vertexColors = basic->getVertexColorEnabledProperty();
            return result;
        }
        if (const auto* tested = dynamic_cast<const Xna::AlphaTestEffect*>(&effect)) {
            PartMaterial result;
            result.material.shading = ShadingModel::Unlit;
            result.material.diffuseColor = tested->getDiffuseColorProperty();
            result.material.alpha = tested->getAlphaProperty();
            result.material.baseTexture = tested->getTextureProperty();
            result.material.vertexColors = tested->getVertexColorEnabledProperty();
            result.material.blend = BlendMode::AlphaTest;
            result.material.alphaCutoff = static_cast<float>(tested->getReferenceAlphaProperty()) / 255.0f;
            return result;
        }
        return std::nullopt;
    }

    ModelView::ModelView(const Xna::Model& model, EffectMaterialReader reader) : model_(&model)
    {
        const auto& bones = model.getBonesProperty();
        const int boneCount = bones.getCountProperty();
        boneParents_.assign(static_cast<std::size_t>(boneCount), -1);
        rest_.assign(static_cast<std::size_t>(boneCount), Matrix::getIdentityProperty());
        for (int i = 0; i < boneCount; ++i) {
            const Xna::ModelBone* bone = bones[i];
            if (bone == nullptr) {
                continue;
            }
            rest_[static_cast<std::size_t>(i)] = bone->getTransformProperty();
            if (const Xna::ModelBone* parent = bone->getParentProperty()) {
                boneParents_[static_cast<std::size_t>(i)] = parent->getIndexProperty();
            }
        }
        ResetBoneTransforms();

        const auto& meshes = model.getMeshesProperty();
        for (int m = 0; m < meshes.getCountProperty(); ++m) {
            const Xna::ModelMesh* mesh = meshes[m];
            if (mesh == nullptr) {
                continue;
            }
            const BoundingSphere sphere = mesh->getBoundingSphereProperty();
            const Vector3 extent(sphere.Radius, sphere.Radius, sphere.Radius);
            const BoundingBox box(sphere.Center - extent, sphere.Center + extent);
            const auto& parts = mesh->getMeshPartsProperty();
            for (int p = 0; p < parts.getCountProperty(); ++p) {
                const Xna::ModelMeshPart* part = parts[p];
                if (part == nullptr || part->getPrimitiveCountProperty() <= 0 || part->getVertexBufferProperty() == nullptr) {
                    continue;
                }
                ModelPart entry;
                entry.meshName = mesh->getNameProperty();
                entry.bone = mesh->getParentBoneProperty() != nullptr ? mesh->getParentBoneProperty()->getIndexProperty() : -1;
                entry.bounds = sphere;
                entry.mesh = GpuMesh::Borrow(*part, box, AttributesOf(part->getVertexBufferProperty()->getVertexDeclarationProperty()));
                std::optional<PartMaterial> material;
                if (const Xna::Effect* effect = part->getEffectProperty()) {
                    if (reader) {
                        material = reader(*effect);
                    }
                    if (!material) {
                        material = StockMaterialFromEffect(*effect);
                    }
                    if (!material) {
                        fallbacks_.push_back(entry.meshName + ": " + typeid(*effect).name());
                    }
                } else {
                    fallbacks_.push_back(entry.meshName + ": no effect");
                }
                if (!material) {
                    material = PartMaterial{};
                    material->material.diffuseColor = Vector3(0.6f, 0.6f, 0.6f);
                }
                material->material.name = entry.meshName;
                entry.material = *material;
                parts_.push_back(std::move(entry));
            }
        }
    }

    bool ModelView::Skinned() const noexcept
    {
        for (const auto& part : parts_) {
            if (part.material.skinned) {
                return true;
            }
        }
        return false;
    }

    void ModelView::SetBoneTransforms(const std::span<const Matrix> local)
    {
        if (local.size() != rest_.size()) {
            throw std::invalid_argument("ModelView::SetBoneTransforms: one transform per model bone is needed");
        }
        absolute_.resize(local.size());
        for (std::size_t i = 0; i < local.size(); ++i) {
            const int parent = boneParents_[i];
            absolute_[i] = parent >= 0 && static_cast<std::size_t>(parent) < i ? local[i] * absolute_[static_cast<std::size_t>(parent)]
                                                                               : local[i];
        }
    }

    void ModelView::ResetBoneTransforms()
    {
        SetBoneTransforms(rest_);
    }

    void ModelView::AppendItems(std::vector<RenderItem>& out, const Matrix& world, const std::vector<Matrix>* palette) const
    {
        for (const auto& part : parts_) {
            RenderItem item;
            item.mesh = &part.mesh;
            item.material = &part.material.material;
            if (part.material.skinned) {
                if (palette == nullptr) {
                    continue;
                }
                // A skinned part's vertices are in model space; the palette poses them.
                item.world = world;
                item.bones = palette;
            } else {
                const Matrix bone = part.bone >= 0 && static_cast<std::size_t>(part.bone) < absolute_.size()
                                        ? absolute_[static_cast<std::size_t>(part.bone)]
                                        : Matrix::getIdentityProperty();
                item.world = bone * world;
            }
            item.bounds = Transformed(part.bounds, item.world);
            if (part.material.skinned) {
                // A pose can reach past the rest bounds: be generous rather than cull a limb.
                item.bounds.Radius *= 2.0f;
            }
            out.push_back(item);
        }
    }

    BoundingSphere ModelView::Bounds(const Matrix& world) const
    {
        bool first = true;
        BoundingSphere bounds;
        for (const auto& part : parts_) {
            const Matrix bone = !part.material.skinned && part.bone >= 0 && static_cast<std::size_t>(part.bone) < absolute_.size()
                                    ? absolute_[static_cast<std::size_t>(part.bone)]
                                    : Matrix::getIdentityProperty();
            const BoundingSphere sphere = Transformed(part.bounds, bone * world);
            bounds = first ? sphere : BoundingSphere::CreateMerged(bounds, sphere);
            first = false;
        }
        return bounds;
    }
}
