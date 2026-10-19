// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/MeshData.hpp"

#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// One interleaved element: which MeshData stream feeds it and how it is
    /// declared to the GPU.
    struct VertexLayoutElement
    {
        VertexAttribute attribute = VertexAttribute::Position;
        /// Vector2, Vector3, Vector4, Color or Byte4 (bone indices).
        Microsoft::Xna::Framework::Graphics::VertexElementFormat format =
            Microsoft::Xna::Framework::Graphics::VertexElementFormat::Vector3;
        Microsoft::Xna::Framework::Graphics::VertexElementUsage usage =
            Microsoft::Xna::Framework::Graphics::VertexElementUsage::Position;
        int usageIndex = 0;
    };

    /// An interleaved GPU vertex format built from MeshData attributes.
    ///
    /// The predefined layouts have the GPU layout of XNA's stock vertex types
    /// (same elements, offsets and stride; the CNA renderers bind elements by
    /// usage). Note that CNA's C++ vertex structs are not byte images of that
    /// layout (Color carries a vtable), which is why interleaving writes the
    /// bytes explicitly instead of copying structs. Games may define further
    /// layouts; only the five element formats listed above are supported, so a
    /// layout can always be produced from MeshData without a lossy cast other
    /// than the documented ones (Vector4 tangent -> Vector3 drops handedness).
    class VertexLayout
    {
    public:
        /// Offsets are assigned in order; throws std::invalid_argument for an
        /// unsupported format, a missing Position element or a format that
        /// cannot represent its attribute.
        VertexLayout(std::string name, std::vector<VertexLayoutElement> elements);

        [[nodiscard]] const std::string& Name() const noexcept { return name_; }
        [[nodiscard]] const std::vector<VertexLayoutElement>& Elements() const noexcept { return elements_; }
        [[nodiscard]] const std::vector<int>& Offsets() const noexcept { return offsets_; }
        [[nodiscard]] int Stride() const noexcept { return stride_; }
        [[nodiscard]] AttributeMask Attributes() const noexcept { return attributes_; }
        [[nodiscard]] const Microsoft::Xna::Framework::Graphics::VertexDeclaration& Declaration() const noexcept
        {
            return *declaration_;
        }

        // XNA stock layouts.
        [[nodiscard]] static const VertexLayout& PositionColor();          ///< 16 bytes
        [[nodiscard]] static const VertexLayout& PositionTexture();        ///< 20 bytes
        [[nodiscard]] static const VertexLayout& PositionColorTexture();   ///< 24 bytes
        [[nodiscard]] static const VertexLayout& PositionNormalTexture();  ///< 32 bytes
        // Engine layouts built from XNA vertex elements.
        /// 36 bytes: lit and vertex-coloured (position, normal, color, uv).
        [[nodiscard]] static const VertexLayout& PositionNormalColorTexture();
        /// 40 bytes: DualTextureEffect (albedo + lightmap UV).
        [[nodiscard]] static const VertexLayout& PositionNormalDualTexture();
        /// 48 bytes: tangent-frame layout for normal mapping
        /// (position, normal, tangent xyzw, uv), tangent bound by usage.
        [[nodiscard]] static const VertexLayout& PositionNormalTangentTexture();
        /// 52 bytes: skinned (position, normal, uv, bone weights Vector4,
        /// bone indices Byte4), the layout of CNA's
        /// VertexPositionNormalTextureSkinned.
        [[nodiscard]] static const VertexLayout& PositionNormalTextureSkinned();
        /// 68 bytes: skinned with a tangent frame, the layout of CNA's
        /// VertexPositionNormalTangentTextureSkinned.
        [[nodiscard]] static const VertexLayout& PositionNormalTangentTextureSkinned();

    private:
        std::string name_;
        std::vector<VertexLayoutElement> elements_;
        std::vector<int> offsets_;
        int stride_ = 0;
        AttributeMask attributes_;
        std::shared_ptr<const Microsoft::Xna::Framework::Graphics::VertexDeclaration> declaration_;
    };

    /// What to do when the layout needs an attribute the mesh does not have.
    enum class MissingAttributePolicy : std::uint8_t
    {
        /// Throw std::invalid_argument naming the attributes.
        Fail,
        /// Fill with the neutral values of MeshData::AddAttribute and report them.
        UseDefaults,
    };

    /// Interleaved vertex bytes plus a report of everything that was not a
    /// one-to-one copy.
    struct InterleavedVertices
    {
        std::vector<std::uint8_t> bytes;
        int vertexCount = 0;
        int stride = 0;
        /// Required by the layout, absent from the mesh (filled with defaults).
        AttributeMask missing;
        /// Present in the mesh, not represented by the layout.
        AttributeMask dropped;
        /// Represented with loss (e.g. tangent handedness in a Vector3 element).
        AttributeMask narrowed;
    };

    /// Converts `mesh` (which must Validate() cleanly) to `layout`.
    [[nodiscard]] InterleavedVertices Interleave(const MeshData& mesh, const VertexLayout& layout,
                                                 MissingAttributePolicy policy = MissingAttributePolicy::Fail);

    /// Human-readable list such as "Normal, TexCoord1"; empty for an empty mask.
    [[nodiscard]] std::string Describe(AttributeMask attributes);
}
