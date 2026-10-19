// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/MeshData.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Vector4;

    namespace
    {
        Vector3 SafeNormalize(const Vector3& v, const Vector3& fallback)
        {
            const float lengthSquared = v.LengthSquared();
            if (!(lengthSquared > 1e-20f) || !std::isfinite(lengthSquared)) {
                return fallback;
            }
            return v * (1.0f / std::sqrt(lengthSquared));
        }

        // Any unit vector perpendicular to `n`.
        Vector3 Perpendicular(const Vector3& n)
        {
            const Vector3 axis = std::fabs(n.X) < 0.9f ? Vector3::UnitX : Vector3::UnitY;
            return SafeNormalize(Vector3::Cross(axis, n), Vector3::UnitX);
        }

        template <typename T>
        void AppendStream(std::vector<T>& target, const std::vector<T>& source)
        {
            target.insert(target.end(), source.begin(), source.end());
        }
    }

    const char* ToString(const VertexAttribute attribute) noexcept
    {
        switch (attribute) {
            case VertexAttribute::Position: return "Position";
            case VertexAttribute::Normal: return "Normal";
            case VertexAttribute::Tangent: return "Tangent";
            case VertexAttribute::Color: return "Color";
            case VertexAttribute::TexCoord0: return "TexCoord0";
            case VertexAttribute::TexCoord1: return "TexCoord1";
            case VertexAttribute::BlendIndices: return "BlendIndices";
            case VertexAttribute::BlendWeights: return "BlendWeights";
        }
        return "?";
    }

    void MeshData::Clear()
    {
        positions.clear();
        normals.clear();
        tangents.clear();
        colors.clear();
        texCoords0.clear();
        texCoords1.clear();
        blendIndices.clear();
        blendWeights.clear();
        indices.clear();
    }

    AttributeMask MeshData::Attributes() const noexcept
    {
        AttributeMask mask;
        if (!positions.empty()) {
            mask.Add(VertexAttribute::Position);
        }
        if (!normals.empty()) {
            mask.Add(VertexAttribute::Normal);
        }
        if (!tangents.empty()) {
            mask.Add(VertexAttribute::Tangent);
        }
        if (!colors.empty()) {
            mask.Add(VertexAttribute::Color);
        }
        if (!texCoords0.empty()) {
            mask.Add(VertexAttribute::TexCoord0);
        }
        if (!texCoords1.empty()) {
            mask.Add(VertexAttribute::TexCoord1);
        }
        if (!blendIndices.empty()) {
            mask.Add(VertexAttribute::BlendIndices);
        }
        if (!blendWeights.empty()) {
            mask.Add(VertexAttribute::BlendWeights);
        }
        return mask;
    }

    void MeshData::AddAttribute(const VertexAttribute attribute)
    {
        const std::size_t n = VertexCount();
        switch (attribute) {
            case VertexAttribute::Position: break;
            case VertexAttribute::Normal:
                if (normals.empty()) {
                    normals.assign(n, Vector3::UnitY);
                }
                break;
            case VertexAttribute::Tangent:
                if (tangents.empty()) {
                    tangents.assign(n, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                }
                break;
            case VertexAttribute::Color:
                if (colors.empty()) {
                    colors.assign(n, Color(255, 255, 255, 255));
                }
                break;
            case VertexAttribute::TexCoord0:
                if (texCoords0.empty()) {
                    texCoords0.assign(n, Vector2::Zero);
                }
                break;
            case VertexAttribute::TexCoord1:
                if (texCoords1.empty()) {
                    texCoords1.assign(n, Vector2::Zero);
                }
                break;
            case VertexAttribute::BlendIndices:
                if (blendIndices.empty()) {
                    blendIndices.assign(n, std::array<std::uint8_t, 4>{0, 0, 0, 0});
                }
                break;
            case VertexAttribute::BlendWeights:
                if (blendWeights.empty()) {
                    blendWeights.assign(n, Vector4(1.0f, 0.0f, 0.0f, 0.0f));
                }
                break;
        }
    }

    void MeshData::SetUniformColor(const Color& fill) { colors.assign(VertexCount(), fill); }

    void MeshData::RemoveAttribute(const VertexAttribute attribute)
    {
        switch (attribute) {
            case VertexAttribute::Position: Clear(); break;
            case VertexAttribute::Normal: normals.clear(); break;
            case VertexAttribute::Tangent: tangents.clear(); break;
            case VertexAttribute::Color: colors.clear(); break;
            case VertexAttribute::TexCoord0: texCoords0.clear(); break;
            case VertexAttribute::TexCoord1: texCoords1.clear(); break;
            case VertexAttribute::BlendIndices: blendIndices.clear(); break;
            case VertexAttribute::BlendWeights: blendWeights.clear(); break;
        }
    }

    std::vector<std::string> MeshData::Validate() const
    {
        std::vector<std::string> problems;
        const std::size_t n = VertexCount();
        auto checkStream = [&](const std::size_t size, const VertexAttribute attribute) {
            if (size != 0 && size != n) {
                problems.push_back(std::string(ToString(attribute)) + " stream has " + std::to_string(size)
                                   + " elements for " + std::to_string(n) + " vertices");
            }
        };
        checkStream(normals.size(), VertexAttribute::Normal);
        checkStream(tangents.size(), VertexAttribute::Tangent);
        checkStream(colors.size(), VertexAttribute::Color);
        checkStream(texCoords0.size(), VertexAttribute::TexCoord0);
        checkStream(texCoords1.size(), VertexAttribute::TexCoord1);
        checkStream(blendIndices.size(), VertexAttribute::BlendIndices);
        checkStream(blendWeights.size(), VertexAttribute::BlendWeights);
        if (n > std::numeric_limits<std::uint32_t>::max()) {
            problems.push_back("more vertices than 32-bit indices can address");
        }
        if (indices.size() % 3 != 0) {
            problems.push_back("index count " + std::to_string(indices.size()) + " is not a multiple of 3");
        }
        for (std::size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] >= n) {
                problems.push_back("index " + std::to_string(i) + " (" + std::to_string(indices[i])
                                   + ") is out of range");
                break;
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            const auto& p = positions[i];
            if (!std::isfinite(p.X) || !std::isfinite(p.Y) || !std::isfinite(p.Z)) {
                problems.push_back("position " + std::to_string(i) + " is not finite");
                break;
            }
        }
        for (std::size_t i = 0; i < blendWeights.size(); ++i) {
            const auto& w = blendWeights[i];
            if (!std::isfinite(w.X) || !std::isfinite(w.Y) || !std::isfinite(w.Z) || !std::isfinite(w.W)) {
                problems.push_back("bone weight " + std::to_string(i) + " is not finite");
                break;
            }
        }
        return problems;
    }

    BoundingBox MeshData::Bounds() const
    {
        if (positions.empty()) {
            return BoundingBox(Vector3::Zero, Vector3::Zero);
        }
        Vector3 min = positions.front();
        Vector3 max = positions.front();
        for (const auto& p : positions) {
            min = Vector3::Min(min, p);
            max = Vector3::Max(max, p);
        }
        return BoundingBox(min, max);
    }

    BoundingSphere MeshData::Sphere() const
    {
        if (positions.empty()) {
            return BoundingSphere(Vector3::Zero, 0.0f);
        }
        // Centre of the bounds, radius to the farthest vertex: conservative and
        // deterministic (unlike Ritter-style approximations).
        const BoundingBox box = Bounds();
        const Vector3 center = (box.Min + box.Max) * 0.5f;
        float radiusSquared = 0.0f;
        for (const auto& p : positions) {
            radiusSquared = std::max(radiusSquared, Vector3::DistanceSquared(center, p));
        }
        return BoundingSphere(center, std::sqrt(radiusSquared));
    }

    void MeshData::Append(const MeshData& other)
    {
        if (other.positions.empty()) {
            return;
        }
        if (!positions.empty() && Attributes() != other.Attributes()) {
            throw std::invalid_argument("MeshData::Append: attribute sets differ; convert explicitly first");
        }
        const std::size_t base = positions.size();
        if (base + other.positions.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("MeshData::Append: result exceeds 32-bit index range");
        }
        AppendStream(positions, other.positions);
        AppendStream(normals, other.normals);
        AppendStream(tangents, other.tangents);
        AppendStream(colors, other.colors);
        AppendStream(texCoords0, other.texCoords0);
        AppendStream(texCoords1, other.texCoords1);
        AppendStream(blendIndices, other.blendIndices);
        AppendStream(blendWeights, other.blendWeights);
        indices.reserve(indices.size() + other.indices.size());
        for (const auto index : other.indices) {
            indices.push_back(static_cast<std::uint32_t>(base + index));
        }
    }

    void MeshData::Append(const MeshData& other, const Matrix& transform)
    {
        MeshData copy = other;
        copy.Transform(transform);
        Append(copy);
    }

    void MeshData::Transform(const Matrix& transform)
    {
        for (auto& p : positions) {
            p = Vector3::Transform(p, transform);
        }
        if (!normals.empty() || !tangents.empty()) {
            const Matrix normalMatrix = Matrix::Transpose(Matrix::Invert(transform));
            for (auto& n : normals) {
                n = SafeNormalize(Vector3::TransformNormal(n, normalMatrix), Vector3::UnitY);
            }
            for (auto& t : tangents) {
                // Tangents follow the surface, so they transform like directions
                // in the surface (by the matrix itself), then are renormalized.
                const Vector3 xyz = SafeNormalize(Vector3::TransformNormal(Vector3(t.X, t.Y, t.Z), transform), Vector3::UnitX);
                t = Vector4(xyz.X, xyz.Y, xyz.Z, t.W);
            }
        }
        if (transform.Determinant() < 0.0f) {
            FlipWinding();
            for (auto& t : tangents) {
                t.W = -t.W;
            }
        }
    }

    void MeshData::FlipWinding()
    {
        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            std::swap(indices[i + 1], indices[i + 2]);
        }
    }

    void MeshData::FlipNormals()
    {
        for (auto& n : normals) {
            n = -n;
        }
        for (auto& t : tangents) {
            t.W = -t.W;
        }
    }

    Vector3 MeshData::FaceNormal(const std::size_t triangle) const
    {
        const Vector3& a = positions.at(indices.at(triangle * 3));
        const Vector3& b = positions.at(indices.at(triangle * 3 + 1));
        const Vector3& c = positions.at(indices.at(triangle * 3 + 2));
        return Vector3::Cross(c - a, b - a);
    }

    void MeshData::ComputeNormals()
    {
        // Angle-weighted (Thuermer & Wuethrich): each face contributes its unit
        // normal times the corner angle, so the result does not depend on how
        // a polygon was split into triangles.
        std::vector<Vector3> accumulated(positions.size(), Vector3::Zero);
        for (std::size_t t = 0; t < TriangleCount(); ++t) {
            const Vector3 face = FaceNormal(t);
            const float faceLength = face.Length();
            if (!(faceLength > 0.0f)) {
                continue; // degenerate triangle
            }
            const Vector3 unit = face * (1.0f / faceLength);
            for (int corner = 0; corner < 3; ++corner) {
                const std::uint32_t i = indices[t * 3 + corner];
                const Vector3& p = positions[i];
                const Vector3 e1 = positions[indices[t * 3 + (corner + 1) % 3]] - p;
                const Vector3 e2 = positions[indices[t * 3 + (corner + 2) % 3]] - p;
                const float lengths = e1.Length() * e2.Length();
                if (!(lengths > 0.0f)) {
                    continue;
                }
                const float angle = std::acos(std::clamp(Vector3::Dot(e1, e2) / lengths, -1.0f, 1.0f));
                accumulated[i] += unit * angle;
            }
        }
        normals.resize(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i) {
            normals[i] = SafeNormalize(accumulated[i], Vector3::UnitY);
        }
    }

    void MeshData::ComputeTangents()
    {
        if (normals.size() != positions.size() || texCoords0.size() != positions.size() || positions.empty()) {
            throw std::logic_error("MeshData::ComputeTangents requires normals and texCoords0");
        }
        std::vector<Vector3> tan(positions.size(), Vector3::Zero);
        std::vector<Vector3> bitan(positions.size(), Vector3::Zero);
        for (std::size_t t = 0; t < TriangleCount(); ++t) {
            const std::uint32_t i0 = indices[t * 3];
            const std::uint32_t i1 = indices[t * 3 + 1];
            const std::uint32_t i2 = indices[t * 3 + 2];
            const Vector3 e1 = positions[i1] - positions[i0];
            const Vector3 e2 = positions[i2] - positions[i0];
            const Vector2 d1 = texCoords0[i1] - texCoords0[i0];
            const Vector2 d2 = texCoords0[i2] - texCoords0[i0];
            const float det = d1.X * d2.Y - d2.X * d1.Y;
            if (std::fabs(det) < 1e-12f) {
                continue;
            }
            const float r = 1.0f / det;
            const Vector3 sdir = (e1 * d2.Y - e2 * d1.Y) * r;
            const Vector3 tdir = (e2 * d1.X - e1 * d2.X) * r;
            for (const auto i : {i0, i1, i2}) {
                tan[i] += sdir;
                bitan[i] += tdir;
            }
        }
        tangents.resize(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const Vector3& n = normals[i];
            // Gram-Schmidt orthogonalize against the normal.
            Vector3 tangent = tan[i] - n * Vector3::Dot(n, tan[i]);
            tangent = SafeNormalize(tangent, Perpendicular(n));
            const float handedness = Vector3::Dot(Vector3::Cross(n, tangent), bitan[i]) < 0.0f ? -1.0f : 1.0f;
            tangents[i] = Vector4(tangent.X, tangent.Y, tangent.Z, handedness);
        }
    }

    float MeshData::SignedVolume() const
    {
        double volume = 0.0;
        for (std::size_t t = 0; t < TriangleCount(); ++t) {
            const Vector3& a = positions[indices[t * 3]];
            const Vector3& b = positions[indices[t * 3 + 1]];
            const Vector3& c = positions[indices[t * 3 + 2]];
            volume += static_cast<double>(Vector3::Dot(a, Vector3::Cross(c, b)));
        }
        return static_cast<float>(volume / 6.0);
    }
}
