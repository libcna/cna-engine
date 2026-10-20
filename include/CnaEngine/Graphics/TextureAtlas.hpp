// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace CnaEngine::Graphics
{
    /// A region of an atlas texture in texels: left, top, width, height.
    struct AtlasRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    /// Texture coordinates of a region: (u0, v0) is its top-left corner and
    /// (u1, v1) its bottom-right, in XNA's convention (v grows downwards).
    struct UvRect
    {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;

        /// The whole texture, (0, 0)-(1, 1).
        [[nodiscard]] static constexpr UvRect Full() noexcept { return UvRect{0.0f, 0.0f, 1.0f, 1.0f}; }
    };

    /// Named regions of one texture -- tiles, sprite frames, glyphs -- and
    /// their texture coordinates.
    ///
    /// Regions are given in texels; Uv() divides by the atlas size, so it
    /// returns an empty UvRect until the size is known (SetSize or the
    /// constructor). The atlas holds no texture: what it describes is drawn
    /// with whatever texture the game binds.
    class TextureAtlas
    {
    public:
        TextureAtlas() = default;
        TextureAtlas(int width, int height) noexcept : width_(width), height_(height) {}

        void SetSize(int width, int height) noexcept
        {
            width_ = width;
            height_ = height;
        }
        [[nodiscard]] int Width() const noexcept { return width_; }
        [[nodiscard]] int Height() const noexcept { return height_; }

        /// Adds or replaces the region `name`.
        void Add(std::string name, const AtlasRect& rect);
        /// Adds a sprite sheet's frames as `prefix_0`, `prefix_1`, ... row by
        /// row: `columns` x `rows` frames of `frameWidth` x `frameHeight`
        /// texels starting at (`startX`, `startY`), `spacingX`/`spacingY`
        /// texels apart. Throws std::invalid_argument for an empty prefix, a
        /// non-positive frame size or count, or a negative start or spacing.
        void AddGrid(std::string_view prefix, int frameWidth, int frameHeight, int columns, int rows, int startX = 0,
                     int startY = 0, int spacingX = 0, int spacingY = 0);

        [[nodiscard]] bool Contains(std::string_view name) const;
        /// Throws std::out_of_range for an unknown region.
        [[nodiscard]] const AtlasRect& Rect(std::string_view name) const;
        /// Texture coordinates of `name`; throws std::out_of_range for an
        /// unknown region, returns an empty UvRect while the size is unknown.
        [[nodiscard]] UvRect Uv(std::string_view name) const;
        /// Like Uv(), but returns `fallback` for an unknown region.
        [[nodiscard]] UvRect UvOrDefault(std::string_view name, const UvRect& fallback = {}) const;

        [[nodiscard]] std::size_t Count() const noexcept { return regions_.size(); }
        [[nodiscard]] bool Empty() const noexcept { return regions_.empty(); }

    private:
        [[nodiscard]] UvRect ToUv(const AtlasRect& rect) const;

        int width_ = 0;
        int height_ = 0;
        std::map<std::string, AtlasRect, std::less<>> regions_;
    };
}
