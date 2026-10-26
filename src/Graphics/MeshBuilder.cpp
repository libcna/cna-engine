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

    void MeshBuilder::AddBox(const Vector3& min, const Vector3& max) { AddBox(min, max, BoxFaces{}); }

    void MeshBuilder::AddBox(const Vector3& min, const Vector3& max, const BoxFaces& faces)
    {
        // Corners of each face top-left, top-right, bottom-right, bottom-left
        // as seen from outside -- clockwise from the front, MeshData's
        // winding -- in BoxFace order, with the face's outward normal.
        struct Face
        {
            Vector3 corners[4];
            Vector3 normal;
        };
        const Face table[6] = {
            {{{min.X, max.Y, max.Z}, {max.X, max.Y, max.Z}, {max.X, min.Y, max.Z}, {min.X, min.Y, max.Z}},
             Vector3::UnitZ},
            {{{max.X, max.Y, min.Z}, {min.X, max.Y, min.Z}, {min.X, min.Y, min.Z}, {max.X, min.Y, min.Z}},
             -Vector3::UnitZ},
            {{{max.X, max.Y, max.Z}, {max.X, max.Y, min.Z}, {max.X, min.Y, min.Z}, {max.X, min.Y, max.Z}},
             Vector3::UnitX},
            {{{min.X, max.Y, min.Z}, {min.X, max.Y, max.Z}, {min.X, min.Y, max.Z}, {min.X, min.Y, min.Z}},
             -Vector3::UnitX},
            {{{min.X, max.Y, min.Z}, {max.X, max.Y, min.Z}, {max.X, max.Y, max.Z}, {min.X, max.Y, max.Z}},
             Vector3::UnitY},
            {{{min.X, min.Y, max.Z}, {max.X, min.Y, max.Z}, {max.X, min.Y, min.Z}, {min.X, min.Y, min.Z}},
             -Vector3::UnitY},
        };
        for (std::size_t f = 0; f < 6; ++f) {
            const BoxFaceStyle& style = faces[f];
            if (!style.visible) {
                continue;
            }
            const Face& face = table[f];
            const UvRect& uv = style.uv;
            const auto tl = AddVertex(Vertex(face.corners[0], face.normal, Vector2(uv.u0, uv.v0)));
            const auto tr = AddVertex(Vertex(face.corners[1], face.normal, Vector2(uv.u1, uv.v0)));
            const auto br = AddVertex(Vertex(face.corners[2], face.normal, Vector2(uv.u1, uv.v1)));
            const auto bl = AddVertex(Vertex(face.corners[3], face.normal, Vector2(uv.u0, uv.v1)));
            AddQuad(tl, tr, br, bl);
        }
    }

    void MeshBuilder::AddDoubleSidedQuad(const std::array<Vector3, 4>& corners,
                                         const std::array<Vector2, 4>& texCoords)
    {
        // The side that sees corners 0-1-2 clockwise is the one the normal
        // of cross(c - a, b - a) points at. The corners run counterclockwise
        // from the other side, which takes `texCoords` as given; this side
        // takes them mirrored back across the 0-1 edge's perpendicular
        // (0 <-> 1, 2 <-> 3), so neither side shows the texture mirrored.
        const Vector3 n = Vector3::Normalize(Vector3::Cross(corners[2] - corners[0], corners[1] - corners[0]));
        const std::array<Vector2, 4> mirrored = {texCoords[1], texCoords[0], texCoords[3], texCoords[2]};
        for (const bool reversed : {false, true}) {
            const Vector3 normal = reversed ? -n : n;
            const std::array<Vector2, 4>& uvs = reversed ? texCoords : mirrored;
            std::uint32_t index[4];
            for (std::size_t i = 0; i < 4; ++i) {
                index[i] = AddVertex(Vertex(corners[i], normal, uvs[i]));
            }
            if (!reversed) {
                AddTriangle(index[0], index[1], index[2]);
                AddTriangle(index[0], index[2], index[3]);
            } else {
                AddTriangle(index[0], index[2], index[1]);
                AddTriangle(index[0], index[3], index[2]);
            }
        }
    }

    void MeshBuilder::AddPlate(const Vector3& center, const float width, const float height, const PlateAxis axis,
                               const UvRect& uv)
    {
        const float halfW = width * 0.5f;
        const float halfH = height * 0.5f;
        std::array<Vector3, 4> corners;
        switch (axis) {
            case PlateAxis::Z:
                corners = {Vector3(center.X - halfW, center.Y - halfH, center.Z),
                           Vector3(center.X + halfW, center.Y - halfH, center.Z),
                           Vector3(center.X + halfW, center.Y + halfH, center.Z),
                           Vector3(center.X - halfW, center.Y + halfH, center.Z)};
                break;
            case PlateAxis::X:
                corners = {Vector3(center.X, center.Y - halfH, center.Z - halfW),
                           Vector3(center.X, center.Y - halfH, center.Z + halfW),
                           Vector3(center.X, center.Y + halfH, center.Z + halfW),
                           Vector3(center.X, center.Y + halfH, center.Z - halfW)};
                break;
            case PlateAxis::Y:
                // Seen from above, the region's top edge towards -Z (as a
                // box's top face shows it).
                corners = {Vector3(center.X - halfW, center.Y, center.Z + halfH),
                           Vector3(center.X + halfW, center.Y, center.Z + halfH),
                           Vector3(center.X + halfW, center.Y, center.Z - halfH),
                           Vector3(center.X - halfW, center.Y, center.Z - halfH)};
                break;
        }
        // Bottom-left, bottom-right, top-right, top-left: v grows downwards
        // in a texture, so the top corners take v0.
        AddDoubleSidedQuad(corners, {Vector2(uv.u0, uv.v1), Vector2(uv.u1, uv.v1), Vector2(uv.u1, uv.v0),
                                     Vector2(uv.u0, uv.v0)});
    }

    void MeshBuilder::AddCrossedPlates(const Vector3& center, const float width, const float height,
                                       const int planes, const UvRect& uv)
    {
        if (planes < 1) {
            throw std::invalid_argument("MeshBuilder::AddCrossedPlates: planes must be at least 1");
        }
        constexpr float pi = 3.14159265358979323846f;
        const float halfW = width * 0.5f;
        const float halfH = height * 0.5f;
        const std::array<Vector2, 4> texCoords = {Vector2(uv.u0, uv.v1), Vector2(uv.u1, uv.v1),
                                                  Vector2(uv.u1, uv.v0), Vector2(uv.u0, uv.v0)};
        const float step = 180.0f / static_cast<float>(planes);
        for (int i = 0; i < planes; ++i) {
            const float radians = static_cast<float>(i) * step * pi / 180.0f;
            const float dx = std::cos(radians) * halfW;
            const float dz = std::sin(radians) * halfW;
            AddDoubleSidedQuad({Vector3(center.X - dx, center.Y - halfH, center.Z - dz),
                                Vector3(center.X + dx, center.Y - halfH, center.Z + dz),
                                Vector3(center.X + dx, center.Y + halfH, center.Z + dz),
                                Vector3(center.X - dx, center.Y + halfH, center.Z - dz)},
                               texCoords);
        }
    }

    void MeshBuilder::AddBillboard(const Vector3& position, const Vector2& size, const Vector3& cameraRight,
                                   const Vector3& cameraUp, const UvRect& uv, const Vector2& origin,
                                   const float rotation)
    {
        // Bottom-left, bottom-right, top-right, top-left of the quad (u, v
        // across it, v up), with the region's texture coordinates.
        const float corners[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        const Vector2 texCoords[4] = {Vector2(uv.u0, uv.v1), Vector2(uv.u1, uv.v1), Vector2(uv.u1, uv.v0),
                                      Vector2(uv.u0, uv.v0)};
        const float cosR = std::cos(rotation);
        const float sinR = std::sin(rotation);
        const Vector3 normal = Vector3::Normalize(Vector3::Cross(cameraRight, cameraUp));
        std::uint32_t index[4];
        for (std::size_t i = 0; i < 4; ++i) {
            const float du = (corners[i][0] - origin.X) * size.X;
            const float dv = (corners[i][1] - origin.Y) * size.Y;
            const float rdu = du * cosR - dv * sinR;
            const float rdv = du * sinR + dv * cosR;
            index[i] = AddVertex(Vertex(position + cameraRight * rdu + cameraUp * rdv, normal, texCoords[i]));
        }
        // bl-tr-br and bl-tl-tr: clockwise as the camera sees them.
        AddTriangle(index[0], index[2], index[1]);
        AddTriangle(index[0], index[3], index[2]);
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
