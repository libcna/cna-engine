// SPDX-License-Identifier: MIT
#pragma once

// Frozen test-only oracle extracted from openeggbert/easy-3d at
// ad63bf9c176af5a798ad05e75c21e8802dda0f2f. This is deliberately not a
// compatibility API and must never be installed or linked by production code.

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CnaEngine::Pilot::Easy3DLegacy
{
    struct AtlasRect
    {
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;
    };

    struct UvRect
    {
        float U0 = 0.0f;
        float V0 = 0.0f;
        float U1 = 0.0f;
        float V1 = 0.0f;
    };

    class TextureAtlas
    {
    public:
        TextureAtlas() = default;
        TextureAtlas(const int width, const int height) noexcept : width_(width), height_(height) {}

        void Add(std::string name, const AtlasRect& rect);
        void AddGrid(std::string_view prefix, int frameWidth, int frameHeight, int columns, int rows,
                     int startX = 0, int startY = 0, int spacingX = 0, int spacingY = 0);
        [[nodiscard]] const AtlasRect& GetRect(std::string_view name) const;
        [[nodiscard]] UvRect GetUv(std::string_view name) const;
        [[nodiscard]] UvRect GetUvOrDefault(std::string_view name, const UvRect& fallback = {}) const;
        [[nodiscard]] std::size_t Count() const noexcept { return regions_.size(); }

    private:
        int width_ = 0;
        int height_ = 0;
        std::unordered_map<std::string, AtlasRect> regions_;
    };

    struct CubeVertex
    {
        Microsoft::Xna::Framework::Vector3 Position;
        Microsoft::Xna::Framework::Vector2 Uv;
    };

    struct CubeItem
    {
        Microsoft::Xna::Framework::Vector3 Center;
        Microsoft::Xna::Framework::Vector3 Size;
        UvRect Uv{0.0f, 0.0f, 1.0f, 1.0f};
    };

    struct DirectionalCubeFace
    {
        bool Visible = true;
        UvRect Uv{0.0f, 0.0f, 1.0f, 1.0f};
    };

    struct DirectionalCubeItem
    {
        Microsoft::Xna::Framework::Vector3 Center;
        Microsoft::Xna::Framework::Vector3 Size;
        DirectionalCubeFace Faces[6];
    };

    enum class PlateAxis
    {
        Z,
        X,
        Y,
    };

    struct PlateItem
    {
        Microsoft::Xna::Framework::Vector3 Center;
        float Width = 1.0f;
        float Height = 1.0f;
        UvRect Uv{0.0f, 0.0f, 1.0f, 1.0f};
        PlateAxis Axis = PlateAxis::Z;
    };

    struct TripleCrossItem
    {
        Microsoft::Xna::Framework::Vector3 Center;
        float Width = 1.0f;
        float Height = 1.0f;
        UvRect Uv{0.0f, 0.0f, 1.0f, 1.0f};
    };

    void AppendCubeMesh(const CubeItem& item, std::vector<CubeVertex>& vertices,
                        std::vector<std::uint32_t>& indices);
    void AppendDirectionalCubeMesh(const DirectionalCubeItem& item, std::vector<CubeVertex>& vertices,
                                   std::vector<std::uint32_t>& indices);
    void AppendPlateMesh(const PlateItem& item, std::vector<CubeVertex>& vertices,
                         std::vector<std::uint32_t>& indices);
    void AppendTripleCrossMesh(const TripleCrossItem& item, std::vector<CubeVertex>& vertices,
                               std::vector<std::uint32_t>& indices);

    struct BillboardVertex
    {
        Microsoft::Xna::Framework::Vector3 Position;
        Microsoft::Xna::Framework::Vector2 Uv;
    };

    struct BillboardItem
    {
        Microsoft::Xna::Framework::Vector3 Position;
        Microsoft::Xna::Framework::Vector2 Size;
        UvRect Uv{0.0f, 0.0f, 1.0f, 1.0f};
        Microsoft::Xna::Framework::Vector2 Origin{0.5f, 0.5f};
        float RotationRadians = 0.0f;
    };

    void AppendBillboardMesh(const BillboardItem& item,
                             const Microsoft::Xna::Framework::Vector3& cameraRight,
                             const Microsoft::Xna::Framework::Vector3& cameraUp,
                             std::vector<BillboardVertex>& vertices, std::vector<std::uint32_t>& indices);

    class Camera3D
    {
    public:
        using Vector3 = Microsoft::Xna::Framework::Vector3;
        using Matrix = Microsoft::Xna::Framework::Matrix;

        Camera3D();
        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        void SetPosition(const Vector3& value) noexcept { position_ = value; }
        [[nodiscard]] const Vector3& GetTarget() const noexcept { return target_; }
        void SetTarget(const Vector3& value) noexcept { target_ = value; }
        [[nodiscard]] const Vector3& GetUp() const noexcept { return up_; }
        [[nodiscard]] float GetFieldOfView() const noexcept { return fieldOfView_; }
        void SetFieldOfView(const float value) noexcept { fieldOfView_ = value; }
        [[nodiscard]] float GetAspectRatio() const noexcept { return aspectRatio_; }
        void SetAspectRatio(const float value) noexcept { aspectRatio_ = value; }
        [[nodiscard]] float GetNearPlane() const noexcept { return nearPlane_; }
        void SetNearPlane(const float value) noexcept { nearPlane_ = value; }
        [[nodiscard]] float GetFarPlane() const noexcept { return farPlane_; }
        void SetFarPlane(const float value) noexcept { farPlane_ = value; }
        [[nodiscard]] Matrix GetViewMatrix() const;
        [[nodiscard]] Matrix GetProjectionMatrix() const;

    private:
        Vector3 position_;
        Vector3 target_;
        Vector3 up_;
        float fieldOfView_;
        float aspectRatio_;
        float nearPlane_;
        float farPlane_;
    };

    class OrbitCamera
    {
    public:
        using Vector3 = Microsoft::Xna::Framework::Vector3;

        OrbitCamera();
        void SetTarget(const Vector3& value) noexcept { target_ = value; }
        void SetYaw(const float value) noexcept { yaw_ = value; }
        void SetPitch(const float value) noexcept { pitch_ = value; }
        void SetDistance(const float value) noexcept { distance_ = value; }
        [[nodiscard]] Vector3 ComputePosition() const;

    private:
        Vector3 target_;
        float yaw_;
        float pitch_;
        float distance_;
    };

    class FollowCamera
    {
    public:
        using Vector3 = Microsoft::Xna::Framework::Vector3;

        FollowCamera();
        [[nodiscard]] const Vector3& GetOffset() const noexcept { return offset_; }
        [[nodiscard]] float GetSmoothing() const noexcept { return smoothing_; }
        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        void SetPosition(const Vector3& value) noexcept { position_ = value; }
        void Update(const Vector3& targetPosition, float deltaSeconds);
        void ApplyTo(Camera3D& camera, const Vector3& targetPosition) const;

    private:
        Vector3 position_;
        Vector3 offset_;
        float smoothing_;
    };
}
