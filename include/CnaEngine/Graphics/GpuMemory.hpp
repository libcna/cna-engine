// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <cstdint>
#include <unordered_set>

namespace CnaEngine::Graphics
{
    /// Bytes of a texture: every mip level at its surface format (DXT formats
    /// in 4x4 blocks). Sets `exact` to false for formats outside XNA 4.0, which
    /// are counted at 4 bytes per pixel.
    [[nodiscard]] std::uint64_t TextureBytes(const Microsoft::Xna::Framework::Graphics::Texture2D& texture,
                                             bool* exact = nullptr);

    /// GPU memory a set of meshes and textures occupies, estimated from the
    /// XNA resources themselves: vertex count times declared stride, index
    /// count times element size, TextureBytes() for textures. A buffer or
    /// texture shared by several meshes, materials or items is counted once,
    /// so the estimate describes what a scene or a frame keeps resident,
    /// whoever owns it (owned and borrowed meshes alike).
    class GpuMemoryEstimate
    {
    public:
        void Add(const GpuMesh& mesh);
        void Add(const Microsoft::Xna::Framework::Graphics::Texture2D& texture);
        /// Every texture the material references.
        void Add(const Material& material);
        /// The mesh and material of every item in every bucket.
        void Add(const RenderQueue& queue);

        [[nodiscard]] std::uint64_t VertexBytes() const noexcept { return vertexBytes_; }
        [[nodiscard]] std::uint64_t IndexBytes() const noexcept { return indexBytes_; }
        [[nodiscard]] std::uint64_t TextureBytes() const noexcept { return textureBytes_; }
        [[nodiscard]] std::uint64_t TotalBytes() const noexcept { return vertexBytes_ + indexBytes_ + textureBytes_; }
        [[nodiscard]] std::size_t VertexBuffers() const noexcept { return vertexBuffers_; }
        [[nodiscard]] std::size_t IndexBuffers() const noexcept { return indexBuffers_; }
        [[nodiscard]] std::size_t Textures() const noexcept { return textures_; }
        /// Textures in formats outside XNA 4.0 (counted at 4 bytes per pixel).
        [[nodiscard]] std::size_t ApproximatedTextures() const noexcept { return approximated_; }

        void Clear();

    private:
        std::unordered_set<const void*> seen_;
        std::uint64_t vertexBytes_ = 0;
        std::uint64_t indexBytes_ = 0;
        std::uint64_t textureBytes_ = 0;
        std::size_t vertexBuffers_ = 0;
        std::size_t indexBuffers_ = 0;
        std::size_t textures_ = 0;
        std::size_t approximated_ = 0;
    };
}
