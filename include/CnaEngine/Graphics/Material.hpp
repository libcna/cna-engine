// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <string>

namespace CnaEngine::Graphics
{
    enum class BlendMode : std::uint8_t
    {
        Opaque,
        /// Pixels with alpha below Material::alphaCutoff are discarded.
        AlphaTest,
        /// Straight alpha blending, drawn back to front after opaque geometry.
        AlphaBlend,
        Additive,
    };

    enum class ShadingModel : std::uint8_t
    {
        Unlit,
        /// XNA BasicEffect lighting (three directional lights).
        Lit,
        /// Metallic/roughness description for enhanced renderers. Renderers
        /// without PBR support draw it as Lit and report the fallback.
        PhysicallyBased,
    };

    /// Device-independent description of a surface.
    ///
    /// A Material holds no GPU state: renderers translate it into effects and
    /// render states per device. Textures are borrowed (owned by assets or the
    /// ContentManager) and must outlive the draw that uses them.
    struct Material
    {
        std::string name;
        ShadingModel shading = ShadingModel::Lit;
        BlendMode blend = BlendMode::Opaque;

        Microsoft::Xna::Framework::Vector3 diffuseColor = Microsoft::Xna::Framework::Vector3(1.0f, 1.0f, 1.0f);
        float alpha = 1.0f;
        Microsoft::Xna::Framework::Vector3 emissiveColor = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 specularColor = Microsoft::Xna::Framework::Vector3::Zero;
        float specularPower = 16.0f;
        /// BlendMode::AlphaTest only, 0..1.
        float alphaCutoff = 0.5f;

        /// Multiply by the mesh's vertex colors (needs a Color attribute).
        bool vertexColors = false;
        /// Draw both faces (no back-face culling).
        bool doubleSided = false;

        /// Base color texture (needs TexCoord0); null means untextured.
        Microsoft::Xna::Framework::Graphics::Texture2D* baseTexture = nullptr;

        /// Base-color and emissive textures hold sRGB-encoded color (normal and
        /// occlusion-roughness-metallic maps are always linear).
        bool colorTexturesAreSrgb = true;
        /// Texture-coordinate transform applied to every texture of the
        /// material (atlas addressing; the mesh UVs carry the tiling).
        Microsoft::Xna::Framework::Vector2 uvScale = Microsoft::Xna::Framework::Vector2(1.0f, 1.0f);
        Microsoft::Xna::Framework::Vector2 uvOffset = Microsoft::Xna::Framework::Vector2(0.0f, 0.0f);

        // PhysicallyBased parameters (glTF metallic-roughness), ignored by the
        // stock-effect binder.
        float metallic = 0.0f;
        float roughness = 1.0f;
        /// Tangent-space normal map (needs Tangent and TexCoord0 attributes).
        Microsoft::Xna::Framework::Graphics::Texture2D* normalTexture = nullptr;
        float normalScale = 1.0f;
        /// glTF packing: R occlusion, G roughness, B metallic.
        Microsoft::Xna::Framework::Graphics::Texture2D* occlusionRoughnessMetallicTexture = nullptr;
        float occlusionStrength = 1.0f;
        Microsoft::Xna::Framework::Graphics::Texture2D* emissiveTexture = nullptr;
        /// KHR_materials_ior and KHR_materials_specular.
        float ior = 1.5f;
        float specularFactor = 1.0f;
    };
}
