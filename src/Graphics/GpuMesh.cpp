// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/GpuMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::IndexBuffer;
    using Microsoft::Xna::Framework::Graphics::IndexElementSize;
    using Microsoft::Xna::Framework::Graphics::PrimitiveType;
    using Microsoft::Xna::Framework::Graphics::VertexBuffer;

    GpuMesh::~GpuMesh() { Reset(); }

    GpuMesh::GpuMesh(GpuMesh&& other) noexcept { MoveFrom(other); }

    GpuMesh& GpuMesh::operator=(GpuMesh&& other) noexcept
    {
        if (this != &other) {
            Reset();
            MoveFrom(other);
        }
        return *this;
    }

    void GpuMesh::MoveFrom(GpuMesh& other) noexcept
    {
        ownedVertices_ = std::move(other.ownedVertices_);
        ownedIndices_ = std::move(other.ownedIndices_);
        vertices_ = std::exchange(other.vertices_, nullptr);
        indices_ = std::exchange(other.indices_, nullptr);
        owned_ = std::exchange(other.owned_, false);
        attributes_ = std::exchange(other.attributes_, AttributeMask{});
        vertexCount_ = std::exchange(other.vertexCount_, 0);
        primitiveCount_ = std::exchange(other.primitiveCount_, 0);
        baseVertex_ = std::exchange(other.baseVertex_, 0);
        startIndex_ = std::exchange(other.startIndex_, 0);
        indexSize_ = other.indexSize_;
        vertexBytes_ = std::exchange(other.vertexBytes_, 0);
        indexBytes_ = std::exchange(other.indexBytes_, 0);
        bounds_ = other.bounds_;
        sphere_ = other.sphere_;
        report_ = std::move(other.report_);
    }

    void GpuMesh::Reset() noexcept
    {
        if (owned_) {
            Detail::AdjustGpuMemory(-1, -vertexBytes_, -indexBytes_);
        }
        ownedVertices_.reset();
        ownedIndices_.reset();
        vertices_ = nullptr;
        indices_ = nullptr;
        owned_ = false;
        attributes_ = AttributeMask{};
        vertexCount_ = 0;
        primitiveCount_ = 0;
        baseVertex_ = 0;
        startIndex_ = 0;
        vertexBytes_ = 0;
        indexBytes_ = 0;
        report_ = InterleavedVertices{};
    }

    GpuMesh GpuMesh::Upload(GraphicsDevice& device, const MeshData& mesh, const VertexLayout& layout,
                            const GpuMeshOptions& options)
    {
        if (mesh.Empty()) {
            throw std::invalid_argument("GpuMesh::Upload: mesh has no triangles");
        }
        if (mesh.VertexCount() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            || mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument("GpuMesh::Upload: mesh exceeds the buffer size limits");
        }
        InterleavedVertices interleaved = Interleave(mesh, layout, options.missingAttributes);

        // Decide the index size before allocating anything, so an impossible
        // request fails without leaving half a mesh behind.
        const std::uint32_t maxIndex = *std::max_element(mesh.indices.begin(), mesh.indices.end());
        const bool sixteenBit = options.preferSixteenBitIndices && maxIndex <= std::numeric_limits<std::uint16_t>::max();
        if (!sixteenBit && device.getGraphicsProfileProperty() == GraphicsProfile::Reach) {
            throw std::invalid_argument(
                maxIndex > std::numeric_limits<std::uint16_t>::max()
                    ? "GpuMesh::Upload: mesh needs 32-bit indices, which the Reach profile does not support; "
                      "split it into meshes of at most 65536 vertices or use the HiDef profile"
                    : "GpuMesh::Upload: 32-bit indices were requested but the Reach profile does not support them");
        }

        GpuMesh gpu;
        gpu.attributes_ = layout.Attributes();
        gpu.vertexCount_ = interleaved.vertexCount;
        gpu.primitiveCount_ = static_cast<int>(mesh.TriangleCount());
        gpu.bounds_ = mesh.Bounds();
        gpu.sphere_ = mesh.Sphere();

        gpu.ownedVertices_ = std::make_unique<VertexBuffer>(device, layout.Declaration(), interleaved.vertexCount,
                                                            options.usage);
        gpu.ownedVertices_->SetData(interleaved.bytes.data(), static_cast<int>(interleaved.bytes.size()));
        gpu.vertexBytes_ = static_cast<std::int64_t>(interleaved.bytes.size());

        const auto indexCount = static_cast<int>(mesh.indices.size());
        if (sixteenBit) {
            std::vector<std::uint16_t> narrow(mesh.indices.begin(), mesh.indices.end());
            gpu.indexSize_ = IndexElementSize::SixteenBits;
            gpu.ownedIndices_ = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits, indexCount,
                                                              options.usage);
            gpu.ownedIndices_->SetData(narrow.data(), indexCount);
            gpu.indexBytes_ = static_cast<std::int64_t>(indexCount) * 2;
        } else {
            gpu.indexSize_ = IndexElementSize::ThirtyTwoBits;
            gpu.ownedIndices_ = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits, indexCount,
                                                              options.usage);
            gpu.ownedIndices_->SetData(mesh.indices.data(), indexCount);
            gpu.indexBytes_ = static_cast<std::int64_t>(indexCount) * 4;
        }

        gpu.vertices_ = gpu.ownedVertices_.get();
        gpu.indices_ = gpu.ownedIndices_.get();
        gpu.owned_ = true;
        interleaved.bytes.clear();
        interleaved.bytes.shrink_to_fit();
        gpu.report_ = std::move(interleaved);
        Detail::AdjustGpuMemory(1, gpu.vertexBytes_, gpu.indexBytes_);
        return gpu;
    }

    GpuMesh GpuMesh::Borrow(VertexBuffer& vertices, IndexBuffer& indices, const int vertexCount,
                            const int primitiveCount, const BoundingBox& bounds, const AttributeMask attributes,
                            const int baseVertex, const int startIndex)
    {
        if (vertexCount <= 0 || primitiveCount <= 0) {
            throw std::invalid_argument("GpuMesh::Borrow: counts must be positive");
        }
        if (baseVertex < 0 || startIndex < 0) {
            throw std::invalid_argument("GpuMesh::Borrow: offsets must not be negative");
        }
        GpuMesh gpu;
        gpu.vertices_ = &vertices;
        gpu.indices_ = &indices;
        gpu.attributes_ = attributes;
        gpu.attributes_.Add(VertexAttribute::Position);
        gpu.vertexCount_ = vertexCount;
        gpu.primitiveCount_ = primitiveCount;
        gpu.indexSize_ = indices.getIndexElementSizeProperty();
        gpu.bounds_ = bounds;
        gpu.sphere_ = BoundingSphere::CreateFromBoundingBox(bounds);
        gpu.baseVertex_ = baseVertex;
        gpu.startIndex_ = startIndex;
        return gpu;
    }

    GpuMesh GpuMesh::Borrow(const Microsoft::Xna::Framework::Graphics::ModelMeshPart& part, const BoundingBox& bounds,
                            const AttributeMask attributes)
    {
        if (part.getVertexBufferProperty() == nullptr || part.getIndexBufferProperty() == nullptr) {
            throw std::invalid_argument("GpuMesh::Borrow: the mesh part needs a vertex and an index buffer");
        }
        return Borrow(*part.getVertexBufferProperty(), *part.getIndexBufferProperty(), part.getNumVerticesProperty(),
                      part.getPrimitiveCountProperty(), bounds, attributes, part.getVertexOffsetProperty(),
                      part.getStartIndexProperty());
    }

    bool GpuMesh::IsDrawable() const noexcept
    {
        return vertices_ != nullptr && indices_ != nullptr && primitiveCount_ > 0
               && !vertices_->getIsDisposedProperty() && !indices_->getIsDisposedProperty();
    }

    bool GpuMesh::Draw(GraphicsDevice& device, RenderStatistics* statistics) const
    {
        if (!IsDrawable()) {
            return false;
        }
        device.SetVertexBuffer(vertices_);
        device.setIndicesProperty(indices_);
        device.DrawIndexedPrimitives(PrimitiveType::TriangleList, baseVertex_, 0, vertexCount_, startIndex_,
                                     primitiveCount_);
        if (statistics != nullptr) {
            statistics->Record(static_cast<std::uint64_t>(primitiveCount_), static_cast<std::uint64_t>(vertexCount_));
        }
        return true;
    }
}
