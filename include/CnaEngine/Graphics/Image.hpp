// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace CnaEngine::Graphics
{
    /// How Downsample averages a 2x2 block into one texel.
    enum class MipRounding : std::uint8_t
    {
        /// sum / 4, rounding down (car-, train-, school-, museum- and
        /// island-simulator).
        Truncate,
        /// (sum + 2) / 4, rounding to nearest (backrooms- and room-simulator).
        Nearest,
    };

    struct MipOptions
    {
        MipRounding rounding = MipRounding::Truncate;
        /// The colour channels hold sRGB-encoded values: they are averaged in
        /// linear light and encoded again through a 4097-entry table
        /// (room-simulator's); alpha is averaged as it is.
        bool srgb = false;
        /// For textures drawn with an alpha test (AlphaTestEffect's
        /// ReferenceAlpha, 1..254): every level's alpha is rescaled so that the
        /// share of texels passing the test (alpha > reference) stays that of
        /// the full-size image, instead of shrinking as the averages fade
        /// (train-simulator's foliage cards).
        std::optional<int> alphaTestReference;
    };

    /// A CPU RGBA8 image: four bytes per texel (red, green, blue, alpha),
    /// row-major, the layout of SurfaceFormat::Color. Bytes rather than
    /// Color values: CNA's Color is a polymorphic type four times their size,
    /// and procedural textures are generated and filtered texel by texel.
    class Image
    {
    public:
        Image() = default;
        /// Throws std::invalid_argument unless width and height are positive.
        Image(int width, int height,
              const Microsoft::Xna::Framework::Color& fill = Microsoft::Xna::Framework::Color(0, 0, 0, 255));
        /// Throws std::invalid_argument when `colors` does not hold width * height texels.
        [[nodiscard]] static Image FromColors(int width, int height,
                                              const std::vector<Microsoft::Xna::Framework::Color>& colors);

        [[nodiscard]] int Width() const noexcept { return width_; }
        [[nodiscard]] int Height() const noexcept { return height_; }
        [[nodiscard]] bool Empty() const noexcept { return rgba_.empty(); }

        [[nodiscard]] Microsoft::Xna::Framework::Color At(int x, int y) const;
        /// Coordinates wrap around both edges (tileable textures).
        [[nodiscard]] Microsoft::Xna::Framework::Color AtWrapped(int x, int y) const;
        void Set(int x, int y, const Microsoft::Xna::Framework::Color& color);

        [[nodiscard]] std::vector<Microsoft::Xna::Framework::Color> ToColors() const;
        [[nodiscard]] const std::vector<std::uint8_t>& Bytes() const noexcept { return rgba_; }
        [[nodiscard]] std::vector<std::uint8_t>& Bytes() noexcept { return rgba_; }

        bool operator==(const Image&) const = default;

    private:
        [[nodiscard]] std::size_t Offset(int x, int y) const noexcept
        {
            return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)) * 4U;
        }

        int width_ = 0;
        int height_ = 0;
        std::vector<std::uint8_t> rgba_;
    };

    /// The next mip level: half the size, rounded down and at least 1, each
    /// texel the box average of a 2x2 block (an odd last row or column is
    /// left out; a dimension of 1 is read twice). Ignores
    /// MipOptions::alphaTestReference (see DownsampleForAlphaTest). Throws
    /// std::invalid_argument for an empty image.
    [[nodiscard]] Image Downsample(const Image& image, const MipOptions& options = {});

    /// The share of texels an alpha test with `reference` passes (alpha > reference).
    [[nodiscard]] double AlphaTestCoverage(const Image& image, int reference);

    /// Downsample, then alpha multiplied by the scale whose coverage
    /// (alpha * scale >= reference + 1) comes closest to `coverage`: 24
    /// bisection steps over 0..8, keeping the closest result seen, alpha
    /// truncated and clamped. Throws std::invalid_argument unless reference is
    /// 1..254 and coverage 0..1.
    [[nodiscard]] Image DownsampleForAlphaTest(const Image& image, int reference, double coverage,
                                               const MipOptions& options = {});

    /// The image followed by every level down to 1x1 (with an alpha-test
    /// reference, each level keeps the full-size image's coverage).
    [[nodiscard]] std::vector<Image> BuildMipChain(const Image& image, const MipOptions& options = {});

    /// A SurfaceFormat::Color texture of the image's size; with `mipmaps`,
    /// every level is built on the CPU (BuildMipChain) and uploaded.
    [[nodiscard]] std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>
    UploadTexture(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Image& image,
                  const MipOptions& options = {}, bool mipmaps = true);

    /// Replaces the contents of every level of `texture` (the image's size;
    /// throws std::invalid_argument otherwise) with the image and its mip chain.
    void UpdateTexture(Microsoft::Xna::Framework::Graphics::Texture2D& texture, const Image& image,
                       const MipOptions& options = {});
}
