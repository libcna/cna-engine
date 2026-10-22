// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/LocalLights.hpp"

#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <algorithm>
#include <cmath>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    float LightReach(const LocalLight& light, const BoundingSphere& bounds) noexcept
    {
        const Vector3 offset = bounds.Center - light.position;
        const float centre = offset.Length();
        const float distance = std::max(0.0f, centre - bounds.Radius);
        if (!(distance < light.range) || !(light.range > 0.0f)) {
            return 0.0f;
        }
        const float brightness = std::max({light.color.X, light.color.Y, light.color.Z});
        const float fade = 1.0f - distance / light.range;
        float reach = brightness * fade * fade;
        if (light.kind == LocalLight::Kind::Spot && centre > bounds.Radius) {
            // The cone's angle to the sphere's centre, widened by the sphere.
            const float length = light.direction.Length();
            const Vector3 axis = length > 1e-6f ? light.direction * (1.0f / length) : Vector3(0.0f, -1.0f, 0.0f);
            const float cosine = std::clamp(Vector3::Dot(offset * (1.0f / centre), axis), -1.0f, 1.0f);
            const float angle = std::acos(cosine) - std::asin(std::min(1.0f, bounds.Radius / centre));
            if (angle > light.outerAngle) {
                return 0.0f;
            }
            if (angle > light.innerAngle && light.outerAngle > light.innerAngle) {
                reach *= (light.outerAngle - angle) / (light.outerAngle - light.innerAngle);
            }
        }
        return reach;
    }

    std::optional<std::size_t> StrongestLight(const std::span<const LocalLight> lights, const BoundingSphere& bounds) noexcept
    {
        std::optional<std::size_t> best;
        float strongest = 0.0f;
        for (std::size_t i = 0; i < lights.size(); ++i) {
            const float reach = LightReach(lights[i], bounds);
            if (reach > strongest) {
                strongest = reach;
                best = i;
            }
        }
        return best;
    }

    LightPools::LightPools(Xna::GraphicsDevice& device)
    {
        constexpr int Size = 64;
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        for (int y = 0; y < Size; ++y) {
            for (int x = 0; x < Size; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) / Size * 2.0f - 1.0f;
                const float dy = (static_cast<float>(y) + 0.5f) / Size * 2.0f - 1.0f;
                const float t = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy));
                const int v = static_cast<int>(std::lround(t * t * 255.0f));
                pixels[static_cast<std::size_t>(y * Size + x)] = Color(v, v, v, 255);
            }
        }
        gradient_ = std::make_unique<Xna::Texture2D>(device, Size, Size);
        gradient_->SetData(pixels.data(), static_cast<int>(pixels.size()));
        MeshBuilder builder({VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord0});
        builder.AddGrid(Vector3::Zero, 2.0f, 2.0f, 1, 1, 1.0f);
        disc_ = GpuMesh::Upload(device, builder.Take(), VertexLayout::PositionNormalTexture());
    }

    void LightPools::Append(const std::span<const LocalLight> lights, const float groundY, std::vector<RenderItem>& items,
                            const float brightness)
    {
        materials_.resize(lights.size());
        for (std::size_t i = 0; i < lights.size(); ++i) {
            const LocalLight& light = lights[i];
            const float height = std::max(0.0f, light.position.Y - groundY);
            if (!(height < light.range)) {
                continue;
            }
            // Wider and fainter the higher the lamp hangs.
            const float radius = std::sqrt(std::max(0.0f, light.range * light.range - height * height)) * 0.6f;
            const float strength = brightness * (1.0f - height / light.range);
            Material& material = materials_[i];
            material = Material{};
            material.name = "light pool";
            material.shading = ShadingModel::Unlit;
            material.blend = BlendMode::Additive;
            material.baseTexture = gradient_.get();
            material.sampler = &Xna::SamplerState::LinearClamp;
            material.diffuseColor = light.color * strength;
            material.fog = false;
            material.depthBias = -0.0005f;
            RenderItem item;
            item.mesh = &disc_;
            item.material = &material;
            item.world = Matrix::CreateScale(radius, 1.0f, radius) *
                         Matrix::CreateTranslation(light.position.X, groundY + 0.01f, light.position.Z);
            item.bounds = WorldBounds(disc_, item.world);
            items.push_back(item);
        }
    }
}
