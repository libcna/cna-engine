// SPDX-License-Identifier: MIT
#include "Easy3DLegacy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Pilot::Easy3DLegacy
{
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;

    void TextureAtlas::Add(std::string name, const AtlasRect& rect) { regions_[std::move(name)] = rect; }

    void TextureAtlas::AddGrid(const std::string_view prefix, const int frameWidth, const int frameHeight,
                               const int columns, const int rows, const int startX, const int startY,
                               const int spacingX, const int spacingY)
    {
        if (prefix.empty() || frameWidth <= 0 || frameHeight <= 0 || columns <= 0 || rows <= 0 || startX < 0 ||
            startY < 0 || spacingX < 0 || spacingY < 0) {
            throw std::invalid_argument("TextureAtlas::AddGrid: invalid grid parameters");
        }
        const std::string prefixString(prefix);
        for (int i = 0; i < columns * rows; ++i) {
            const int column = i % columns;
            const int row = i / columns;
            Add(prefixString + "_" + std::to_string(i),
                AtlasRect{startX + column * (frameWidth + spacingX), startY + row * (frameHeight + spacingY),
                          frameWidth, frameHeight});
        }
    }

    const AtlasRect& TextureAtlas::GetRect(const std::string_view name) const
    {
        const auto it = regions_.find(std::string(name));
        if (it == regions_.end()) {
            throw std::out_of_range("TextureAtlas: unknown region '" + std::string(name) + "'");
        }
        return it->second;
    }

    UvRect TextureAtlas::GetUv(const std::string_view name) const
    {
        const AtlasRect& rect = GetRect(name);
        if (width_ <= 0 || height_ <= 0) {
            return {};
        }
        const float width = static_cast<float>(width_);
        const float height = static_cast<float>(height_);
        return {static_cast<float>(rect.X) / width, static_cast<float>(rect.Y) / height,
                static_cast<float>(rect.X + rect.Width) / width, static_cast<float>(rect.Y + rect.Height) / height};
    }

    UvRect TextureAtlas::GetUvOrDefault(const std::string_view name, const UvRect& fallback) const
    {
        const auto it = regions_.find(std::string(name));
        if (it == regions_.end()) {
            return fallback;
        }
        if (width_ <= 0 || height_ <= 0) {
            return {};
        }
        const AtlasRect& rect = it->second;
        const float width = static_cast<float>(width_);
        const float height = static_cast<float>(height_);
        return {static_cast<float>(rect.X) / width, static_cast<float>(rect.Y) / height,
                static_cast<float>(rect.X + rect.Width) / width, static_cast<float>(rect.Y + rect.Height) / height};
    }

    namespace
    {
        void ComputeFaceCorners(const Vector3& min, const Vector3& max, Vector3 out[6][4])
        {
            const Vector3 corners[6][4] = {
                {{min.X, max.Y, max.Z}, {max.X, max.Y, max.Z}, {max.X, min.Y, max.Z}, {min.X, min.Y, max.Z}},
                {{max.X, max.Y, min.Z}, {min.X, max.Y, min.Z}, {min.X, min.Y, min.Z}, {max.X, min.Y, min.Z}},
                {{max.X, max.Y, max.Z}, {max.X, max.Y, min.Z}, {max.X, min.Y, min.Z}, {max.X, min.Y, max.Z}},
                {{min.X, max.Y, min.Z}, {min.X, max.Y, max.Z}, {min.X, min.Y, max.Z}, {min.X, min.Y, min.Z}},
                {{min.X, max.Y, min.Z}, {max.X, max.Y, min.Z}, {max.X, max.Y, max.Z}, {min.X, max.Y, max.Z}},
                {{min.X, min.Y, max.Z}, {max.X, min.Y, max.Z}, {max.X, min.Y, min.Z}, {min.X, min.Y, min.Z}},
            };
            for (int face = 0; face < 6; ++face) {
                for (int corner = 0; corner < 4; ++corner) {
                    out[face][corner] = corners[face][corner];
                }
            }
        }

        void AppendFace(const Vector3 corners[4], const UvRect& uv, std::vector<CubeVertex>& vertices,
                        std::vector<std::uint32_t>& indices)
        {
            const Vector2 faceUvs[4] = {{uv.U0, uv.V0}, {uv.U1, uv.V0}, {uv.U1, uv.V1}, {uv.U0, uv.V1}};
            const auto base = static_cast<std::uint32_t>(vertices.size());
            for (int i = 0; i < 4; ++i) {
                vertices.push_back({corners[i], faceUvs[i]});
            }
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }

        void AppendDoubleSidedQuad(const Vector3 corners[4], const Vector2 uvs[4],
                                   std::vector<CubeVertex>& vertices, std::vector<std::uint32_t>& indices)
        {
            auto base = static_cast<std::uint32_t>(vertices.size());
            for (int i = 0; i < 4; ++i) {
                vertices.push_back({corners[i], uvs[i]});
            }
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
            base = static_cast<std::uint32_t>(vertices.size());
            for (int i = 0; i < 4; ++i) {
                vertices.push_back({corners[i], uvs[i]});
            }
            indices.insert(indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
        }
    }

    void AppendCubeMesh(const CubeItem& item, std::vector<CubeVertex>& vertices,
                        std::vector<std::uint32_t>& indices)
    {
        const Vector3 half = item.Size * 0.5f;
        Vector3 corners[6][4];
        ComputeFaceCorners(item.Center - half, item.Center + half, corners);
        for (const auto& face : corners) {
            AppendFace(face, item.Uv, vertices, indices);
        }
    }

    void AppendDirectionalCubeMesh(const DirectionalCubeItem& item, std::vector<CubeVertex>& vertices,
                                   std::vector<std::uint32_t>& indices)
    {
        const Vector3 half = item.Size * 0.5f;
        Vector3 corners[6][4];
        ComputeFaceCorners(item.Center - half, item.Center + half, corners);
        for (int face = 0; face < 6; ++face) {
            if (item.Faces[face].Visible) {
                AppendFace(corners[face], item.Faces[face].Uv, vertices, indices);
            }
        }
    }

    void AppendPlateMesh(const PlateItem& item, std::vector<CubeVertex>& vertices,
                         std::vector<std::uint32_t>& indices)
    {
        const float halfWidth = item.Width * 0.5f;
        const float halfHeight = item.Height * 0.5f;
        Vector3 corners[4];
        if (item.Axis == PlateAxis::Z) {
            corners[0] = {item.Center.X - halfWidth, item.Center.Y - halfHeight, item.Center.Z};
            corners[1] = {item.Center.X + halfWidth, item.Center.Y - halfHeight, item.Center.Z};
            corners[2] = {item.Center.X + halfWidth, item.Center.Y + halfHeight, item.Center.Z};
            corners[3] = {item.Center.X - halfWidth, item.Center.Y + halfHeight, item.Center.Z};
        } else if (item.Axis == PlateAxis::X) {
            corners[0] = {item.Center.X, item.Center.Y - halfHeight, item.Center.Z - halfWidth};
            corners[1] = {item.Center.X, item.Center.Y - halfHeight, item.Center.Z + halfWidth};
            corners[2] = {item.Center.X, item.Center.Y + halfHeight, item.Center.Z + halfWidth};
            corners[3] = {item.Center.X, item.Center.Y + halfHeight, item.Center.Z - halfWidth};
        } else {
            corners[0] = {item.Center.X - halfWidth, item.Center.Y, item.Center.Z - halfHeight};
            corners[1] = {item.Center.X + halfWidth, item.Center.Y, item.Center.Z - halfHeight};
            corners[2] = {item.Center.X + halfWidth, item.Center.Y, item.Center.Z + halfHeight};
            corners[3] = {item.Center.X - halfWidth, item.Center.Y, item.Center.Z + halfHeight};
        }
        const Vector2 uvs[4] = {{item.Uv.U0, item.Uv.V0}, {item.Uv.U1, item.Uv.V0},
                                {item.Uv.U1, item.Uv.V1}, {item.Uv.U0, item.Uv.V1}};
        AppendDoubleSidedQuad(corners, uvs, vertices, indices);
    }

    void AppendTripleCrossMesh(const TripleCrossItem& item, std::vector<CubeVertex>& vertices,
                               std::vector<std::uint32_t>& indices)
    {
        constexpr float angles[3] = {0.0f, 60.0f, 120.0f};
        constexpr float pi = 3.14159265358979323846f;
        const float halfWidth = item.Width * 0.5f;
        const float halfHeight = item.Height * 0.5f;
        const Vector2 uvs[4] = {{item.Uv.U0, item.Uv.V0}, {item.Uv.U1, item.Uv.V0},
                                {item.Uv.U1, item.Uv.V1}, {item.Uv.U0, item.Uv.V1}};
        for (const float degrees : angles) {
            const float radians = degrees * pi / 180.0f;
            const float dx = std::cos(radians) * halfWidth;
            const float dz = std::sin(radians) * halfWidth;
            const Vector3 corners[4] = {
                {item.Center.X - dx, item.Center.Y - halfHeight, item.Center.Z - dz},
                {item.Center.X + dx, item.Center.Y - halfHeight, item.Center.Z + dz},
                {item.Center.X + dx, item.Center.Y + halfHeight, item.Center.Z + dz},
                {item.Center.X - dx, item.Center.Y + halfHeight, item.Center.Z - dz},
            };
            AppendDoubleSidedQuad(corners, uvs, vertices, indices);
        }
    }

    void AppendBillboardMesh(const BillboardItem& item, const Vector3& cameraRight, const Vector3& cameraUp,
                             std::vector<BillboardVertex>& vertices, std::vector<std::uint32_t>& indices)
    {
        constexpr float corners[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        const float cosine = std::cos(item.RotationRadians);
        const float sine = std::sin(item.RotationRadians);
        const Vector2 uvs[4] = {{item.Uv.U0, item.Uv.V1}, {item.Uv.U1, item.Uv.V1},
                                {item.Uv.U1, item.Uv.V0}, {item.Uv.U0, item.Uv.V0}};
        const auto base = static_cast<std::uint32_t>(vertices.size());
        for (int i = 0; i < 4; ++i) {
            const float du = (corners[i][0] - item.Origin.X) * item.Size.X;
            const float dv = (corners[i][1] - item.Origin.Y) * item.Size.Y;
            const float rotatedDu = du * cosine - dv * sine;
            const float rotatedDv = du * sine + dv * cosine;
            vertices.push_back({item.Position + cameraRight * rotatedDu + cameraUp * rotatedDv, uvs[i]});
        }
        indices.insert(indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }

    Camera3D::Camera3D()
        : position_(0.0f, 2.0f, 5.0f), target_(Vector3::Zero), up_(Vector3::Up), fieldOfView_(0.785398163f),
          aspectRatio_(16.0f / 9.0f), nearPlane_(0.1f), farPlane_(1000.0f)
    {
    }

    Camera3D::Matrix Camera3D::GetViewMatrix() const { return Matrix::CreateLookAt(position_, target_, up_); }

    Camera3D::Matrix Camera3D::GetProjectionMatrix() const
    {
        return Matrix::CreatePerspectiveFieldOfView(fieldOfView_, aspectRatio_, nearPlane_, farPlane_);
    }

    OrbitCamera::OrbitCamera() : target_(Vector3::Zero), yaw_(0.0f), pitch_(0.3f), distance_(10.0f) {}

    OrbitCamera::Vector3 OrbitCamera::ComputePosition() const
    {
        const float cosinePitch = std::cos(pitch_);
        return {target_.X + distance_ * cosinePitch * std::sin(yaw_), target_.Y + distance_ * std::sin(pitch_),
                target_.Z + distance_ * cosinePitch * std::cos(yaw_)};
    }

    FollowCamera::FollowCamera() : position_(Vector3::Zero), offset_(0.0f, 5.0f, 10.0f), smoothing_(0.15f) {}

    void FollowCamera::Update(const Vector3& targetPosition, const float deltaSeconds)
    {
        const Vector3 desired(targetPosition.X + offset_.X, targetPosition.Y + offset_.Y,
                              targetPosition.Z + offset_.Z);
        float amount;
        if (smoothing_ >= 1.0f) {
            amount = 1.0f;
        } else if (smoothing_ <= 0.0f) {
            amount = 0.0f;
        } else {
            amount = 1.0f - std::pow(1.0f - smoothing_, deltaSeconds * 60.0f);
        }
        position_ = Vector3::Lerp(position_, desired, amount);
    }

    void FollowCamera::ApplyTo(Camera3D& camera, const Vector3& targetPosition) const
    {
        camera.SetPosition(position_);
        camera.SetTarget(targetPosition);
    }
}
