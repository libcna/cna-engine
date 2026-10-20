// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/GpuMemory.hpp"

#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"

#include <algorithm>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Graphics::IndexElementSize;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        // Bytes per pixel, or per 4x4 block for block-compressed formats (0
        // for a format outside XNA 4.0).
        int FormatSize(const SurfaceFormat format, bool& blocks)
        {
            blocks = false;
            switch (format) {
                case SurfaceFormat::Alpha8: return 1;
                case SurfaceFormat::Bgr565:
                case SurfaceFormat::Bgra5551:
                case SurfaceFormat::Bgra4444:
                case SurfaceFormat::NormalizedByte2:
                case SurfaceFormat::HalfSingle: return 2;
                case SurfaceFormat::Color:
                case SurfaceFormat::NormalizedByte4:
                case SurfaceFormat::Rgba1010102:
                case SurfaceFormat::Rg32:
                case SurfaceFormat::Single:
                case SurfaceFormat::HalfVector2: return 4;
                case SurfaceFormat::Rgba64:
                case SurfaceFormat::Vector2:
                case SurfaceFormat::HalfVector4:
                case SurfaceFormat::HdrBlendable: return 8;
                case SurfaceFormat::Vector4: return 16;
                case SurfaceFormat::Dxt1: blocks = true; return 8;
                case SurfaceFormat::Dxt3:
                case SurfaceFormat::Dxt5: blocks = true; return 16;
                default: return 0;
            }
        }
    }

    std::uint64_t TextureBytes(const Texture2D& texture, bool* exact)
    {
        bool blocks = false;
        int size = FormatSize(texture.getFormatProperty(), blocks);
        if (exact != nullptr) {
            *exact = size != 0;
        }
        if (size == 0) {
            size = 4;
        }
        std::uint64_t bytes = 0;
        const int levels = std::max(1, texture.getLevelCountProperty());
        for (int level = 0; level < levels; ++level) {
            const std::uint64_t width = static_cast<std::uint64_t>(std::max(1, texture.getWidthProperty() >> level));
            const std::uint64_t height = static_cast<std::uint64_t>(std::max(1, texture.getHeightProperty() >> level));
            bytes += blocks ? ((width + 3) / 4) * ((height + 3) / 4) * static_cast<std::uint64_t>(size)
                            : width * height * static_cast<std::uint64_t>(size);
        }
        return bytes;
    }

    void GpuMemoryEstimate::Add(const GpuMesh& mesh)
    {
        if (const auto* vertices = mesh.Vertices(); vertices != nullptr && seen_.insert(vertices).second) {
            vertexBytes_ += static_cast<std::uint64_t>(vertices->getVertexCountProperty())
                            * static_cast<std::uint64_t>(vertices->getVertexDeclarationProperty().getVertexStrideProperty());
            ++vertexBuffers_;
        }
        if (const auto* indices = mesh.Indices(); indices != nullptr && seen_.insert(indices).second) {
            const std::uint64_t element = indices->getIndexElementSizeProperty() == IndexElementSize::SixteenBits ? 2 : 4;
            indexBytes_ += static_cast<std::uint64_t>(indices->getIndexCountProperty()) * element;
            ++indexBuffers_;
        }
    }

    void GpuMemoryEstimate::Add(const Texture2D& texture)
    {
        if (!seen_.insert(&texture).second) {
            return;
        }
        bool exact = true;
        textureBytes_ += Graphics::TextureBytes(texture, &exact);
        ++textures_;
        approximated_ += exact ? 0u : 1u;
    }

    void GpuMemoryEstimate::Add(const Material& material)
    {
        for (const Texture2D* texture : {material.baseTexture, material.secondaryTexture, material.normalTexture,
                                         material.occlusionRoughnessMetallicTexture, material.emissiveTexture}) {
            if (texture != nullptr) {
                Add(*texture);
            }
        }
    }

    void GpuMemoryEstimate::Add(const RenderQueue& queue)
    {
        for (int bucket = 0; bucket < static_cast<int>(RenderBucket::Count); ++bucket) {
            for (const RenderItem& item : queue.Items(static_cast<RenderBucket>(bucket))) {
                if (item.mesh != nullptr) {
                    Add(*item.mesh);
                }
                if (item.material != nullptr) {
                    Add(*item.material);
                }
            }
        }
    }

    void GpuMemoryEstimate::Clear()
    {
        *this = GpuMemoryEstimate{};
    }
}
