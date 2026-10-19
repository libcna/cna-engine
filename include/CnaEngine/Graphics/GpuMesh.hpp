// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/MeshData.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"
#include "CnaEngine/Graphics/VertexLayouts.hpp"

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

#include <memory>

namespace CnaEngine::Graphics
{
    /// Options for GpuMesh::Upload.
    struct GpuMeshOptions
    {
        Microsoft::Xna::Framework::Graphics::BufferUsage usage =
            Microsoft::Xna::Framework::Graphics::BufferUsage::WriteOnly;
        MissingAttributePolicy missingAttributes = MissingAttributePolicy::Fail;
        /// Use 16-bit indices when every index fits (half the memory). 32-bit
        /// indices are used only when needed or when this is false; the Reach
        /// profile has no 32-bit indices, so Upload() rejects such a mesh on a
        /// Reach device instead of failing inside the driver.
        bool preferSixteenBitIndices = true;
    };

    /// An indexed triangle list on the GPU: one vertex buffer, one index
    /// buffer and the draw parameters.
    ///
    /// Ownership: a GpuMesh made by Upload() owns its buffers and releases
    /// them in its destructor (move-only). A GpuMesh made by Borrow() refers to
    /// buffers owned elsewhere; the owner must keep them alive while the mesh
    /// is drawn, and the GpuMesh never releases them.
    ///
    /// Device lifetime: buffers belong to the GraphicsDevice they were created
    /// on. XNA write-only buffers keep their contents across device reset and
    /// lost/restored contexts; when the device is disposed the buffers are
    /// disposed with it and IsDrawable() becomes false. Draw() on such a mesh
    /// draws nothing and returns false rather than touching a dead resource.
    class GpuMesh
    {
    public:
        GpuMesh() = default;
        ~GpuMesh();
        GpuMesh(GpuMesh&& other) noexcept;
        GpuMesh& operator=(GpuMesh&& other) noexcept;
        GpuMesh(const GpuMesh&) = delete;
        GpuMesh& operator=(const GpuMesh&) = delete;

        /// Converts `mesh` to `layout` and uploads it. Throws
        /// std::invalid_argument for an invalid or empty mesh and for missing
        /// attributes under MissingAttributePolicy::Fail.
        [[nodiscard]] static GpuMesh Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                            const MeshData& mesh, const VertexLayout& layout,
                                            const GpuMeshOptions& options = {});

        /// Wraps buffers owned elsewhere (e.g. a Model's mesh part).
        /// `attributes` describes the vertex buffer's elements for renderers
        /// that adapt to them.
        [[nodiscard]] static GpuMesh Borrow(Microsoft::Xna::Framework::Graphics::VertexBuffer& vertices,
                                            Microsoft::Xna::Framework::Graphics::IndexBuffer& indices,
                                            int vertexCount, int primitiveCount,
                                            const Microsoft::Xna::Framework::BoundingBox& bounds,
                                            AttributeMask attributes = {VertexAttribute::Position});

        /// Wraps a model mesh part (shared buffers with a vertex offset and a
        /// start index, as content-pipeline and glTF models use). The part
        /// and its buffers must outlive the GpuMesh.
        [[nodiscard]] static GpuMesh Borrow(const Microsoft::Xna::Framework::Graphics::ModelMeshPart& part,
                                            const Microsoft::Xna::Framework::BoundingBox& bounds,
                                            AttributeMask attributes = {VertexAttribute::Position});

        /// Releases owned buffers (no-op for borrowed ones) and empties the mesh.
        void Reset() noexcept;

        /// Binds the buffers and issues DrawIndexedPrimitives for the whole
        /// triangle list. The caller has applied an effect pass. Adds one draw
        /// to `statistics` when given. Returns false (and draws nothing) when
        /// the mesh is empty or its buffers have been disposed.
        bool Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                  RenderStatistics* statistics = nullptr) const;

        [[nodiscard]] bool IsDrawable() const noexcept;
        [[nodiscard]] bool OwnsBuffers() const noexcept { return owned_; }
        /// Attributes present in the vertex buffer (the layout's elements).
        [[nodiscard]] AttributeMask Attributes() const noexcept { return attributes_; }
        [[nodiscard]] int VertexCount() const noexcept { return vertexCount_; }
        /// Added to every index (DrawIndexedPrimitives' baseVertex).
        [[nodiscard]] int BaseVertex() const noexcept { return baseVertex_; }
        /// First index read from the index buffer.
        [[nodiscard]] int StartIndex() const noexcept { return startIndex_; }
        [[nodiscard]] int PrimitiveCount() const noexcept { return primitiveCount_; }
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::IndexElementSize IndexSize() const noexcept
        {
            return indexSize_;
        }
        [[nodiscard]] std::int64_t VertexBytes() const noexcept { return vertexBytes_; }
        [[nodiscard]] std::int64_t IndexBytes() const noexcept { return indexBytes_; }
        [[nodiscard]] const Microsoft::Xna::Framework::BoundingBox& Bounds() const noexcept { return bounds_; }
        [[nodiscard]] const Microsoft::Xna::Framework::BoundingSphere& Sphere() const noexcept { return sphere_; }
        /// Attributes filled with defaults / dropped / narrowed during Upload().
        [[nodiscard]] const InterleavedVertices& ConversionReport() const noexcept { return report_; }

        [[nodiscard]] Microsoft::Xna::Framework::Graphics::VertexBuffer* Vertices() const noexcept { return vertices_; }
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::IndexBuffer* Indices() const noexcept { return indices_; }

    private:
        void MoveFrom(GpuMesh& other) noexcept;

        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> ownedVertices_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ownedIndices_;
        Microsoft::Xna::Framework::Graphics::VertexBuffer* vertices_ = nullptr;
        Microsoft::Xna::Framework::Graphics::IndexBuffer* indices_ = nullptr;
        bool owned_ = false;
        AttributeMask attributes_;
        int vertexCount_ = 0;
        int primitiveCount_ = 0;
        int baseVertex_ = 0;
        int startIndex_ = 0;
        Microsoft::Xna::Framework::Graphics::IndexElementSize indexSize_ =
            Microsoft::Xna::Framework::Graphics::IndexElementSize::SixteenBits;
        std::int64_t vertexBytes_ = 0;
        std::int64_t indexBytes_ = 0;
        Microsoft::Xna::Framework::BoundingBox bounds_;
        Microsoft::Xna::Framework::BoundingSphere sphere_;
        InterleavedVertices report_;
    };
}
