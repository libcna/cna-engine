// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Instancing.hpp"

#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBufferBinding.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Graphics::BufferUsage;
    using Microsoft::Xna::Framework::Graphics::DynamicVertexBuffer;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::IEffectMatrices;
    using Microsoft::Xna::Framework::Graphics::IndexBuffer;
    using Microsoft::Xna::Framework::Graphics::PrimitiveType;
    using Microsoft::Xna::Framework::Graphics::VertexBufferBinding;
    using Microsoft::Xna::Framework::Graphics::VertexDeclaration;
    using Microsoft::Xna::Framework::Graphics::VertexElement;
    using Microsoft::Xna::Framework::Graphics::VertexElementFormat;
    using Microsoft::Xna::Framework::Graphics::VertexElementUsage;

    const VertexDeclaration& InstanceSet::InstanceDeclaration()
    {
        static const VertexDeclaration declaration(
            64, std::vector<VertexElement>{
                    VertexElement(0, VertexElementFormat::Vector4, VertexElementUsage::TextureCoordinate, 1),
                    VertexElement(16, VertexElementFormat::Vector4, VertexElementUsage::TextureCoordinate, 2),
                    VertexElement(32, VertexElementFormat::Vector4, VertexElementUsage::TextureCoordinate, 3),
                    VertexElement(48, VertexElementFormat::Vector4, VertexElementUsage::TextureCoordinate, 4),
                });
        return declaration;
    }

    void InstanceSet::Clear() noexcept
    {
        worlds_.clear();
        visible_.clear();
        cullActive_ = false;
        dirty_ = true;
    }

    void InstanceSet::Add(const Matrix& world)
    {
        worlds_.push_back(world);
        cullActive_ = false;
        dirty_ = true;
    }

    void InstanceSet::Set(const std::size_t index, const Matrix& world)
    {
        worlds_.at(index) = world;
        dirty_ = true;
    }

    void InstanceSet::UseAll()
    {
        cullActive_ = false;
        dirty_ = true;
    }

    void InstanceSet::Cull(const Camera& camera, const VisibilitySettings& settings, CullStatistics& stats)
    {
        if (mesh_ == nullptr) {
            throw std::logic_error("InstanceSet::Cull: no mesh");
        }
        visible_.clear();
        for (std::size_t i = 0; i < worlds_.size(); ++i) {
            ++stats.considered;
            switch (Classify(WorldBounds(*mesh_, worlds_[i]), 0xFFFFFFFFu, camera, settings)) {
                case Visibility::Visible:
                    ++stats.visible;
                    visible_.push_back(static_cast<std::uint32_t>(i));
                    break;
                case Visibility::RejectedByLayer: ++stats.byLayer; break;
                case Visibility::RejectedByDistance: ++stats.byDistance; break;
                case Visibility::RejectedByFrustum: ++stats.byFrustum; break;
            }
        }
        cullActive_ = true;
        dirty_ = true;
    }

    std::size_t InstanceSet::VisibleCount() const noexcept { return cullActive_ ? visible_.size() : worlds_.size(); }

    std::vector<Matrix> InstanceSet::VisibleWorlds() const
    {
        if (!cullActive_) {
            return worlds_;
        }
        std::vector<Matrix> visible;
        visible.reserve(visible_.size());
        for (const auto index : visible_) {
            visible.push_back(worlds_[index]);
        }
        return visible;
    }

    InstancingPath InstanceSet::ChoosePath(const GraphicsDevice& device, const bool effectReadsInstanceStream)
    {
        return effectReadsInstanceStream && device.getGraphicsProfileProperty() == GraphicsProfile::HiDef
                   ? InstancingPath::Hardware
                   : InstancingPath::PerInstance;
    }

    void InstanceSet::Upload(GraphicsDevice& device)
    {
        const std::size_t count = VisibleCount();
        staging_.resize(count * 16);
        for (std::size_t n = 0; n < count; ++n) {
            const Matrix& m = worlds_[cullActive_ ? visible_[n] : n];
            const float row[16] = {m.M11, m.M12, m.M13, m.M14, m.M21, m.M22, m.M23, m.M24,
                                   m.M31, m.M32, m.M33, m.M34, m.M41, m.M42, m.M43, m.M44};
            std::memcpy(staging_.data() + n * 16, row, sizeof(row));
        }
        if (!instances_ || instances_->getIsDisposedProperty()
            || instances_->getVertexCountProperty() < static_cast<int>(count)) {
            // Grow geometrically so a slowly growing set does not reallocate every frame.
            const int capacity = static_cast<int>(std::max<std::size_t>(count, instances_ ? count * 2 : count));
            instances_ = std::make_unique<DynamicVertexBuffer>(device, InstanceDeclaration(), capacity, BufferUsage::WriteOnly);
        }
        instances_->SetData(reinterpret_cast<const std::uint8_t*>(staging_.data()),
                            static_cast<int>(staging_.size() * sizeof(float)));
        dirty_ = false;
    }

    bool InstanceSet::DrawHardware(GraphicsDevice& device, RenderStatistics* statistics)
    {
        if (device.getGraphicsProfileProperty() != GraphicsProfile::HiDef) {
            throw std::logic_error("InstanceSet::DrawHardware: hardware instancing needs the HiDef profile");
        }
        const std::size_t count = VisibleCount();
        if (mesh_ == nullptr || !mesh_->IsDrawable() || count == 0) {
            return false;
        }
        if (!mesh_->Indexed()) {
            throw std::logic_error("InstanceSet::DrawHardware: XNA instances indexed meshes only");
        }
        if (dirty_ || !instances_ || instances_->getIsContentLostProperty()) {
            Upload(device);
        }
        // Borrow the device's bindings and give them back: leaving the
        // instance stream bound would make the next upload a write into a
        // buffer still set on the device, which XNA refuses.
        struct BindingRestore
        {
            GraphicsDevice& device;
            std::vector<VertexBufferBinding> vertices;
            const IndexBuffer* indices;
            ~BindingRestore()
            {
                try {
                    device.SetVertexBuffers(vertices);
                    device.setIndicesProperty(const_cast<IndexBuffer*>(indices));
                } catch (...) { // NOLINT(bugprone-empty-catch)
                    // Restoring must not throw out of a destructor.
                }
            }
        } restore{device, device.GetVertexBuffers(), device.getIndicesProperty()};

        device.SetVertexBuffers({VertexBufferBinding(mesh_->Vertices(), 0, 0),
                                 VertexBufferBinding(instances_.get(), 0, 1)});
        device.setIndicesProperty(mesh_->Indices());
        device.DrawInstancedPrimitives(PrimitiveType::TriangleList, mesh_->BaseVertex(), 0, mesh_->VertexCount(),
                                       mesh_->StartIndex(), mesh_->PrimitiveCount(), static_cast<int>(count));
        if (statistics != nullptr) {
            statistics->Record(static_cast<std::uint64_t>(mesh_->PrimitiveCount()) * count,
                               static_cast<std::uint64_t>(mesh_->VertexCount()) * count);
        }
        return true;
    }

    std::size_t InstanceSet::DrawPerInstance(GraphicsDevice& device, IEffectMatrices& matrices,
                                             const std::function<void()>& applyPass, RenderStatistics* statistics)
    {
        if (mesh_ == nullptr || !mesh_->IsDrawable()) {
            return 0;
        }
        const std::size_t count = VisibleCount();
        // Instances are placed relative to the effect's current World (a
        // shared pivot), which is restored afterwards.
        const Matrix base = matrices.getWorldProperty();
        for (std::size_t n = 0; n < count; ++n) {
            matrices.setWorldProperty(worlds_[cullActive_ ? visible_[n] : n] * base);
            if (applyPass) {
                applyPass();
            }
            mesh_->Draw(device, statistics);
        }
        matrices.setWorldProperty(base);
        return count;
    }
}
