// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// Per-vertex attribute streams a MeshData may carry.
    enum class VertexAttribute : std::uint8_t
    {
        Position,
        Normal,
        /// xyz: unit tangent; w: bitangent handedness (+1/-1, glTF convention:
        /// bitangent = cross(normal, tangent.xyz) * w).
        Tangent,
        Color,
        TexCoord0,
        TexCoord1,
        /// Four bone indices per vertex (skinning); into the bone palette of
        /// the render item that draws the mesh.
        BlendIndices,
        /// Four bone weights per vertex, matching BlendIndices (normally
        /// summing to 1).
        BlendWeights,
    };

    /// Bit set of VertexAttribute values.
    class AttributeMask
    {
    public:
        constexpr AttributeMask() noexcept = default;
        constexpr AttributeMask(std::initializer_list<VertexAttribute> attributes) noexcept
        {
            for (const auto attribute : attributes) {
                bits_ |= Bit(attribute);
            }
        }

        [[nodiscard]] constexpr bool Has(VertexAttribute attribute) const noexcept { return (bits_ & Bit(attribute)) != 0; }
        [[nodiscard]] constexpr bool Contains(AttributeMask other) const noexcept { return (bits_ & other.bits_) == other.bits_; }
        constexpr void Add(VertexAttribute attribute) noexcept { bits_ |= Bit(attribute); }
        constexpr void Remove(VertexAttribute attribute) noexcept { bits_ &= static_cast<std::uint8_t>(~Bit(attribute)); }
        [[nodiscard]] constexpr AttributeMask Without(AttributeMask other) const noexcept
        {
            AttributeMask result;
            result.bits_ = static_cast<std::uint8_t>(bits_ & ~other.bits_);
            return result;
        }
        [[nodiscard]] constexpr bool Empty() const noexcept { return bits_ == 0; }
        [[nodiscard]] constexpr std::uint8_t Bits() const noexcept { return bits_; }

        friend constexpr bool operator==(AttributeMask, AttributeMask) noexcept = default;

    private:
        [[nodiscard]] static constexpr std::uint8_t Bit(VertexAttribute attribute) noexcept
        {
            return static_cast<std::uint8_t>(1u << static_cast<unsigned>(attribute));
        }
        std::uint8_t bits_ = 0;
    };

    [[nodiscard]] const char* ToString(VertexAttribute attribute) noexcept;

    /// Renderer-independent indexed triangle list.
    ///
    /// Attributes are stored as separate streams (structure of arrays): a mesh
    /// pays only for the attributes it has. Every stream other than `positions`
    /// is either empty (attribute absent) or exactly VertexCount() long;
    /// Validate() reports a violation. Indices are 32-bit throughout.
    ///
    /// Winding convention: XNA's default, front faces are clockwise as seen by
    /// the viewer (RasterizerState::CullCounterClockwise). For a front-facing
    /// triangle (a, b, c) the outward normal is normalize(cross(c - a, b - a)).
    ///
    /// Conversion to GPU vertex formats is explicit (VertexLayouts.hpp) and
    /// reports missing and dropped attributes instead of guessing.
    class MeshData
    {
    public:
        std::vector<Microsoft::Xna::Framework::Vector3> positions;
        std::vector<Microsoft::Xna::Framework::Vector3> normals;
        std::vector<Microsoft::Xna::Framework::Vector4> tangents;
        std::vector<Microsoft::Xna::Framework::Color> colors;
        std::vector<Microsoft::Xna::Framework::Vector2> texCoords0;
        std::vector<Microsoft::Xna::Framework::Vector2> texCoords1;
        std::vector<std::array<std::uint8_t, 4>> blendIndices;
        std::vector<Microsoft::Xna::Framework::Vector4> blendWeights;
        std::vector<std::uint32_t> indices;

        [[nodiscard]] std::size_t VertexCount() const noexcept { return positions.size(); }
        [[nodiscard]] std::size_t TriangleCount() const noexcept { return indices.size() / 3; }
        [[nodiscard]] bool Empty() const noexcept { return indices.empty(); }
        void Clear();

        /// Attributes whose stream is non-empty (Position whenever there are vertices).
        [[nodiscard]] AttributeMask Attributes() const noexcept;
        [[nodiscard]] bool Has(VertexAttribute attribute) const noexcept { return Attributes().Has(attribute); }

        /// Adds an absent attribute stream filled with a neutral value: normal
        /// +Y, tangent (+X, +1), white, texture coordinate (0, 0), bone
        /// indices 0 with weights (1, 0, 0, 0) (rigidly bound to bone 0).
        /// Present streams and Position are left unchanged.
        void AddAttribute(VertexAttribute attribute);
        /// Adds (or overwrites) the color stream with `fill`.
        void SetUniformColor(const Microsoft::Xna::Framework::Color& fill);
        /// Drops an attribute stream. Removing Position clears the mesh.
        void RemoveAttribute(VertexAttribute attribute);

        /// Empty when the mesh is consistent; otherwise one message per problem
        /// (stream length mismatch, index out of range, index count not a
        /// multiple of three, non-finite position or bone weight).
        [[nodiscard]] std::vector<std::string> Validate() const;

        /// Axis-aligned bounds of all vertices; a zero box at the origin when
        /// there are none.
        [[nodiscard]] Microsoft::Xna::Framework::BoundingBox Bounds() const;
        [[nodiscard]] Microsoft::Xna::Framework::BoundingSphere Sphere() const;

        /// Appends `other`. Both meshes must have identical attribute sets
        /// (convert explicitly with AddAttribute/RemoveAttribute first);
        /// throws std::invalid_argument otherwise or when the result would
        /// exceed 2^32 vertices.
        void Append(const MeshData& other);
        void Append(const MeshData& other, const Microsoft::Xna::Framework::Matrix& transform);

        /// Transforms positions by `transform`, normals and tangents by its
        /// inverse transpose (renormalized). A mirroring transform (negative
        /// determinant) also flips the winding and the tangent handedness so
        /// front faces stay front faces.
        void Transform(const Microsoft::Xna::Framework::Matrix& transform);

        /// Reverses every triangle's winding; normals are left unchanged.
        void FlipWinding();
        /// Negates normals and the tangent handedness; winding is unchanged.
        void FlipNormals();

        /// Smooth normals from the triangle faces, weighted by the corner angle
        /// so the result is independent of triangulation (adds the stream when
        /// absent). Vertices used by no triangle get +Y.
        void ComputeNormals();
        /// Per-vertex tangent frames from positions, normals and texCoords0
        /// (adds the stream). Throws std::logic_error when either input stream
        /// is missing. Degenerate UV mappings fall back to a tangent
        /// perpendicular to the normal.
        void ComputeTangents();

        /// Outward geometric normal of triangle `triangle` under the winding
        /// convention above (not normalized; zero for degenerate triangles).
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 FaceNormal(std::size_t triangle) const;
        /// Signed volume enclosed by a closed mesh: positive when front faces
        /// point outward, negative when it is inside out.
        [[nodiscard]] float SignedVolume() const;
    };
}
