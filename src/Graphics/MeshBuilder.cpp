// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::MathHelper;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;

    MeshBuilder::MeshBuilder(const AttributeMask attributes) : attributes_(attributes)
    {
        attributes_.Add(VertexAttribute::Position);
    }

    MeshData MeshBuilder::Take()
    {
        MeshData result = std::move(mesh_);
        mesh_ = MeshData{};
        return result;
    }

    std::uint32_t MeshBuilder::AddVertex(const BuilderVertex& vertex)
    {
        if (mesh_.positions.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("MeshBuilder: 32-bit vertex index range exhausted");
        }
        mesh_.positions.push_back(vertex.position);
        if (attributes_.Has(VertexAttribute::Normal)) {
            mesh_.normals.push_back(vertex.normal);
        }
        if (attributes_.Has(VertexAttribute::Tangent)) {
            mesh_.tangents.push_back(vertex.tangent);
        }
        if (attributes_.Has(VertexAttribute::Color)) {
            mesh_.colors.push_back(vertex.color);
        }
        if (attributes_.Has(VertexAttribute::TexCoord0)) {
            mesh_.texCoords0.push_back(vertex.texCoord0);
        }
        if (attributes_.Has(VertexAttribute::TexCoord1)) {
            mesh_.texCoords1.push_back(vertex.texCoord1);
        }
        if (attributes_.Has(VertexAttribute::BlendIndices)) {
            mesh_.blendIndices.push_back(vertex.blendIndices);
        }
        if (attributes_.Has(VertexAttribute::BlendWeights)) {
            mesh_.blendWeights.push_back(vertex.blendWeights);
        }
        return static_cast<std::uint32_t>(mesh_.positions.size() - 1);
    }

    void MeshBuilder::AddTriangle(const std::uint32_t a, const std::uint32_t b, const std::uint32_t c)
    {
        const std::size_t count = mesh_.positions.size();
        if (a >= count || b >= count || c >= count) {
            throw std::out_of_range("MeshBuilder::AddTriangle: index out of range");
        }
        mesh_.indices.insert(mesh_.indices.end(), {a, b, c});
    }

    void MeshBuilder::AddQuad(const std::uint32_t a, const std::uint32_t b, const std::uint32_t c, const std::uint32_t d)
    {
        AddTriangle(a, b, c);
        AddTriangle(a, c, d);
    }

    BuilderVertex MeshBuilder::Vertex(const Vector3& position, const Vector3& normal, const Vector2& uv) const
    {
        BuilderVertex vertex;
        vertex.position = position;
        vertex.normal = normal;
        vertex.texCoord0 = uv;
        vertex.color = color_;
        return vertex;
    }

    void MeshBuilder::AddBox(const Vector3& min, const Vector3& max)
    {
        const Vector3 center = (min + max) * 0.5f;
        const Vector3 half = (max - min) * 0.5f;
        struct Face
        {
            Vector3 normal;
            Vector3 right; // screen right seen from the front; cross(right, up) == normal
            Vector3 up;
        };
        const Face faces[] = {
            {Vector3::UnitZ, Vector3::UnitX, Vector3::UnitY},
            {-Vector3::UnitZ, -Vector3::UnitX, Vector3::UnitY},
            {Vector3::UnitX, -Vector3::UnitZ, Vector3::UnitY},
            {-Vector3::UnitX, Vector3::UnitZ, Vector3::UnitY},
            {Vector3::UnitY, Vector3::UnitX, -Vector3::UnitZ},
            {-Vector3::UnitY, Vector3::UnitX, Vector3::UnitZ},
        };
        auto scaled = [&](const Vector3& axis) { return axis * std::fabs(Vector3::Dot(axis, half)); };
        for (const auto& face : faces) {
            const Vector3 c = center + scaled(face.normal);
            const Vector3 r = scaled(face.right);
            const Vector3 u = scaled(face.up);
            const auto tl = AddVertex(Vertex(c - r + u, face.normal, Vector2(0.0f, 0.0f)));
            const auto tr = AddVertex(Vertex(c + r + u, face.normal, Vector2(1.0f, 0.0f)));
            const auto br = AddVertex(Vertex(c + r - u, face.normal, Vector2(1.0f, 1.0f)));
            const auto bl = AddVertex(Vertex(c - r - u, face.normal, Vector2(0.0f, 1.0f)));
            AddQuad(tl, tr, br, bl);
        }
    }

    void MeshBuilder::AddGrid(const Vector3& center, const float sizeX, const float sizeZ, const int segmentsX,
                              const int segmentsZ, const float uvScale)
    {
        if (segmentsX < 1 || segmentsZ < 1) {
            throw std::invalid_argument("MeshBuilder::AddGrid: at least one segment per axis");
        }
        const auto first = static_cast<std::uint32_t>(mesh_.positions.size());
        const int columns = segmentsX + 1;
        for (int j = 0; j <= segmentsZ; ++j) {
            for (int i = 0; i <= segmentsX; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(segmentsX);
                const float v = static_cast<float>(j) / static_cast<float>(segmentsZ);
                const Vector3 position(center.X + (u - 0.5f) * sizeX, center.Y, center.Z + (v - 0.5f) * sizeZ);
                AddVertex(Vertex(position, Vector3::UnitY, Vector2(u * uvScale, v * uvScale)));
            }
        }
        // Seen from above, +X is right and -Z is up: row j is above row j + 1.
        for (int j = 0; j < segmentsZ; ++j) {
            for (int i = 0; i < segmentsX; ++i) {
                const auto tl = first + static_cast<std::uint32_t>(j * columns + i);
                const auto bl = first + static_cast<std::uint32_t>((j + 1) * columns + i);
                AddQuad(tl, tl + 1, bl + 1, bl);
            }
        }
    }

    void MeshBuilder::AddSphere(const Vector3& center, const float radius, const int slices, const int stacks)
    {
        if (slices < 3 || stacks < 2) {
            throw std::invalid_argument("MeshBuilder::AddSphere: needs >= 3 slices and >= 2 stacks");
        }
        const auto first = static_cast<std::uint32_t>(mesh_.positions.size());
        const int columns = slices + 1; // duplicated seam column for continuous UVs
        for (int s = 0; s <= stacks; ++s) {
            const float phi = MathHelper::Pi * static_cast<float>(s) / static_cast<float>(stacks);
            for (int k = 0; k <= slices; ++k) {
                const float theta = MathHelper::TwoPi * static_cast<float>(k) / static_cast<float>(slices);
                // theta = 0 faces +Z and increases towards +X: "right" seen from outside.
                const Vector3 normal(std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta));
                AddVertex(Vertex(center + normal * radius, normal,
                                 Vector2(static_cast<float>(k) / static_cast<float>(slices),
                                         static_cast<float>(s) / static_cast<float>(stacks))));
            }
        }
        for (int s = 0; s < stacks; ++s) {
            for (int k = 0; k < slices; ++k) {
                const auto tl = first + static_cast<std::uint32_t>(s * columns + k);
                const auto bl = first + static_cast<std::uint32_t>((s + 1) * columns + k);
                // The triangle touching a pole would be degenerate; skip it.
                if (s != 0) {
                    AddTriangle(tl, tl + 1, bl + 1);
                }
                if (s != stacks - 1) {
                    AddTriangle(tl, bl + 1, bl);
                }
            }
        }
    }

    void MeshBuilder::AddCylinder(const Vector3& baseCenter, const float radius, const float height, const int slices,
                                  const bool caps)
    {
        if (slices < 3) {
            throw std::invalid_argument("MeshBuilder::AddCylinder: needs >= 3 slices");
        }
        const Vector3 top = baseCenter + Vector3::UnitY * height;
        auto ring = [&](const int k) {
            const float theta = MathHelper::TwoPi * static_cast<float>(k) / static_cast<float>(slices);
            return Vector3(std::sin(theta), 0.0f, std::cos(theta));
        };

        const auto first = static_cast<std::uint32_t>(mesh_.positions.size());
        for (int k = 0; k <= slices; ++k) {
            const Vector3 n = ring(k);
            const float u = static_cast<float>(k) / static_cast<float>(slices);
            AddVertex(Vertex(top + n * radius, n, Vector2(u, 0.0f)));
            AddVertex(Vertex(baseCenter + n * radius, n, Vector2(u, 1.0f)));
        }
        for (int k = 0; k < slices; ++k) {
            const auto tl = first + static_cast<std::uint32_t>(2 * k);
            AddQuad(tl, tl + 2, tl + 3, tl + 1);
        }

        if (!caps) {
            return;
        }
        auto cap = [&](const Vector3& c, const Vector3& normal, const bool upward) {
            const auto hub = AddVertex(Vertex(c, normal, Vector2(0.5f, 0.5f)));
            const auto rim = static_cast<std::uint32_t>(mesh_.positions.size());
            for (int k = 0; k <= slices; ++k) {
                const Vector3 n = ring(k);
                AddVertex(Vertex(c + n * radius, normal, Vector2(0.5f + 0.5f * n.X, 0.5f - 0.5f * n.Z)));
            }
            for (int k = 0; k < slices; ++k) {
                const auto a = rim + static_cast<std::uint32_t>(k);
                if (upward) {
                    AddTriangle(hub, a + 1, a);
                } else {
                    AddTriangle(hub, a, a + 1);
                }
            }
        };
        cap(top, Vector3::UnitY, true);
        cap(baseCenter, -Vector3::UnitY, false);
    }
}
