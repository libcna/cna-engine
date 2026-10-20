// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/TextureAtlas.hpp"

#include <stdexcept>

namespace CnaEngine::Graphics
{
    void TextureAtlas::Add(std::string name, const AtlasRect& rect)
    {
        regions_.insert_or_assign(std::move(name), rect);
    }

    void TextureAtlas::AddGrid(const std::string_view prefix, const int frameWidth, const int frameHeight,
                               const int columns, const int rows, const int startX, const int startY,
                               const int spacingX, const int spacingY)
    {
        if (prefix.empty() || frameWidth <= 0 || frameHeight <= 0 || columns <= 0 || rows <= 0 || startX < 0
            || startY < 0 || spacingX < 0 || spacingY < 0) {
            throw std::invalid_argument("TextureAtlas::AddGrid: invalid grid parameters");
        }
        const std::string base(prefix);
        for (int i = 0; i < columns * rows; ++i) {
            const int column = i % columns;
            const int row = i / columns;
            Add(base + "_" + std::to_string(i), AtlasRect{startX + column * (frameWidth + spacingX),
                                                          startY + row * (frameHeight + spacingY), frameWidth,
                                                          frameHeight});
        }
    }

    bool TextureAtlas::Contains(const std::string_view name) const { return regions_.find(name) != regions_.end(); }

    const AtlasRect& TextureAtlas::Rect(const std::string_view name) const
    {
        const auto found = regions_.find(name);
        if (found == regions_.end()) {
            throw std::out_of_range("TextureAtlas: unknown region '" + std::string(name) + "'");
        }
        return found->second;
    }

    UvRect TextureAtlas::Uv(const std::string_view name) const { return ToUv(Rect(name)); }

    UvRect TextureAtlas::UvOrDefault(const std::string_view name, const UvRect& fallback) const
    {
        const auto found = regions_.find(name);
        return found == regions_.end() ? fallback : ToUv(found->second);
    }

    UvRect TextureAtlas::ToUv(const AtlasRect& rect) const
    {
        if (width_ <= 0 || height_ <= 0) {
            return UvRect{};
        }
        const auto w = static_cast<float>(width_);
        const auto h = static_cast<float>(height_);
        return UvRect{static_cast<float>(rect.x) / w, static_cast<float>(rect.y) / h,
                      static_cast<float>(rect.x + rect.width) / w, static_cast<float>(rect.y + rect.height) / h};
    }
}
