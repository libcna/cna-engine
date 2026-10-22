// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/ParticleBillboards.hpp"

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace CnaEngine::Extended
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    ParticleBillboards::ParticleBillboards(Xna::GraphicsDevice& device) : device_(device)
    {
        // A soft round dot: white, its alpha and brightness falling to the edge.
        constexpr int Size = 32;
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        for (int y = 0; y < Size; ++y) {
            for (int x = 0; x < Size; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) / Size * 2.0f - 1.0f;
                const float dy = (static_cast<float>(y) + 0.5f) / Size * 2.0f - 1.0f;
                const float t = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                const int v = static_cast<int>(std::lround(t * t * 255.0f));
                pixels[static_cast<std::size_t>(y * Size + x)] = Color(v, v, v, v);
            }
        }
        soft_ = std::make_unique<Xna::Texture2D>(device, Size, Size);
        soft_->SetData(pixels.data(), static_cast<int>(pixels.size()));

        material_.name = "particles";
        material_.shading = Graphics::ShadingModel::Unlit;
        material_.blend = Graphics::BlendMode::Additive;
        material_.vertexColors = true;
        material_.doubleSided = true;
        material_.fog = false;
        material_.baseTexture = soft_.get();
        material_.sampler = &Xna::SamplerState::LinearClamp;
    }

    void ParticleBillboards::Reserve(const std::size_t particles)
    {
        if (particles <= capacity_) {
            return;
        }
        capacity_ = std::max<std::size_t>(particles, capacity_ * 2);
        vertices_ = std::make_unique<Xna::DynamicVertexBuffer>(
            device_, Xna::VertexPositionColorTexture::getVertexDeclarationStatic(), static_cast<int>(capacity_ * 4),
            Xna::BufferUsage::WriteOnly);
        // Quads never change their corners' order: the indices are made once per size.
        const bool wide = capacity_ * 4 > 65535;
        if (wide) {
            std::vector<std::uint32_t> indices;
            indices.reserve(capacity_ * 6);
            for (std::uint32_t q = 0; q < capacity_; ++q) {
                indices.insert(indices.end(), {q * 4, q * 4 + 1, q * 4 + 2, q * 4 + 2, q * 4 + 1, q * 4 + 3});
            }
            indices_ = std::make_unique<Xna::IndexBuffer>(device_, Xna::IndexElementSize::ThirtyTwoBits,
                                                          static_cast<int>(indices.size()), Xna::BufferUsage::WriteOnly);
            indices_->SetData(indices.data(), static_cast<int>(indices.size()));
        } else {
            std::vector<std::uint16_t> indices;
            indices.reserve(capacity_ * 6);
            for (std::uint32_t q = 0; q < capacity_; ++q) {
                const auto b = static_cast<std::uint16_t>(q * 4);
                indices.insert(indices.end(), {b, static_cast<std::uint16_t>(b + 1), static_cast<std::uint16_t>(b + 2),
                                               static_cast<std::uint16_t>(b + 2), static_cast<std::uint16_t>(b + 1),
                                               static_cast<std::uint16_t>(b + 3)});
            }
            indices_ = std::make_unique<Xna::IndexBuffer>(device_, Xna::IndexElementSize::SixteenBits,
                                                          static_cast<int>(indices.size()), Xna::BufferUsage::WriteOnly);
            indices_->SetData(indices.data(), static_cast<int>(indices.size()));
        }
    }

    void ParticleBillboards::Upload()
    {
        // Packed by the declaration's own offsets and stride (the C++ vertex
        // struct need not match the GPU layout byte for byte), and written
        // whole -- CNA takes no partial SetData here: the unused quads stay
        // zero, points that draw nothing.
        const auto& declaration = Xna::VertexPositionColorTexture::getVertexDeclarationStatic();
        const int stride = declaration.getVertexStrideProperty();
        int position = 0;
        int color = 12;
        int uv = 16;
        for (const auto& element : declaration.GetVertexElements()) {
            switch (element.getVertexElementUsageProperty()) {
            case Xna::VertexElementUsage::Position: position = element.getOffsetProperty(); break;
            case Xna::VertexElementUsage::Color: color = element.getOffsetProperty(); break;
            case Xna::VertexElementUsage::TextureCoordinate: uv = element.getOffsetProperty(); break;
            default: break;
            }
        }
        bytes_.assign(capacity_ * 4 * static_cast<std::size_t>(stride), 0);
        for (std::size_t i = 0; i < staging_.size(); ++i) {
            std::uint8_t* vertex = bytes_.data() + i * static_cast<std::size_t>(stride);
            const auto& v = staging_[i];
            const float xyz[3] = {v.Position.X, v.Position.Y, v.Position.Z};
            std::memcpy(vertex + position, xyz, sizeof xyz);
            const std::uint8_t rgba[4] = {v.Color.getRProperty(), v.Color.getGProperty(), v.Color.getBProperty(),
                                          v.Color.getAProperty()};
            std::memcpy(vertex + color, rgba, sizeof rgba);
            const float st[2] = {v.TextureCoordinate.X, v.TextureCoordinate.Y};
            std::memcpy(vertex + uv, st, sizeof st);
        }
        vertices_->SetData(bytes_.data(), static_cast<int>(bytes_.size()));
    }

    std::optional<Graphics::RenderItem> ParticleBillboards::Build(
        const CNA::Extended::World3DEXT::ParticleEffect3DEXT& effect, const Graphics::Camera& camera)
    {
        count_ = 0;
        for (const auto& emitter : effect.getEmittersProperty()) {
            for (const auto& particle : emitter->ParticlesEXT) {
                count_ += particle.IsExpiredEXT() ? 0 : 1;
            }
        }
        if (count_ == 0) {
            return std::nullopt;
        }
        Reserve(count_);
        staging_.clear();
        const Vector3 right = camera.Right();
        const Vector3 up = camera.Up();
        Vector3 low(1e30f, 1e30f, 1e30f);
        Vector3 high(-1e30f, -1e30f, -1e30f);
        for (const auto& emitter : effect.getEmittersProperty()) {
            for (const auto& particle : emitter->ParticlesEXT) {
                if (particle.IsExpiredEXT()) {
                    continue;
                }
                const float c = std::cos(particle.RotationEXT);
                const float s = std::sin(particle.RotationEXT);
                const Vector3 across = (right * c + up * s) * (0.5f * particle.ScaleEXT.X);
                const Vector3 along = (up * c - right * s) * (0.5f * particle.ScaleEXT.Y);
                const float opacity = std::clamp(particle.OpacityEXT, 0.0f, 1.0f);
                // Additive light fades by darkening; alpha blending by its alpha.
                const Color color(static_cast<int>(std::lround(particle.ColorEXT.getRProperty() * opacity)),
                                  static_cast<int>(std::lround(particle.ColorEXT.getGProperty() * opacity)),
                                  static_cast<int>(std::lround(particle.ColorEXT.getBProperty() * opacity)),
                                  static_cast<int>(std::lround(particle.ColorEXT.getAProperty() * opacity)));
                const Vector3& p = particle.PositionEXT;
                staging_.emplace_back(p - across + along, color, Vector2(0.0f, 0.0f));
                staging_.emplace_back(p + across + along, color, Vector2(1.0f, 0.0f));
                staging_.emplace_back(p - across - along, color, Vector2(0.0f, 1.0f));
                staging_.emplace_back(p + across - along, color, Vector2(1.0f, 1.0f));
                const float extent = std::max(particle.ScaleEXT.X, particle.ScaleEXT.Y);
                low = Vector3::Min(low, p - Vector3(extent, extent, extent));
                high = Vector3::Max(high, p + Vector3(extent, extent, extent));
            }
        }
        Upload();
        const BoundingBox bounds(low, high);
        mesh_ = Graphics::GpuMesh::Borrow(*vertices_, *indices_, static_cast<int>(staging_.size()), static_cast<int>(count_ * 2),
                                          bounds,
                                          {Graphics::VertexAttribute::Position, Graphics::VertexAttribute::Color,
                                           Graphics::VertexAttribute::TexCoord0});
        Graphics::RenderItem item;
        item.mesh = &mesh_;
        item.material = &material_;
        item.bounds = Microsoft::Xna::Framework::BoundingSphere::CreateFromBoundingBox(bounds);
        return item;
    }
}
