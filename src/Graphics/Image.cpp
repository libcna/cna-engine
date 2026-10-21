// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Image.hpp"

#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        // sRGB transfer through tables, as room-simulator builds them: a mip
        // chain of a large texture converts tens of millions of values.
        struct SrgbTables
        {
            float decode[256];
            std::uint8_t encode[4097]; // linear 0..1 in 1/4096 steps

            SrgbTables()
            {
                for (int i = 0; i < 256; ++i) {
                    const float c = static_cast<float>(i) / 255.0f;
                    decode[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
                }
                for (int i = 0; i <= 4096; ++i) {
                    const float linear = static_cast<float>(i) / 4096.0f;
                    const float encoded = linear <= 0.0031308f ? linear * 12.92f
                                                               : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
                    encode[i] = static_cast<std::uint8_t>(std::clamp(encoded * 255.0f + 0.5f, 0.0f, 255.0f));
                }
            }

            [[nodiscard]] std::uint8_t Encode(const float linear) const
            {
                return encode[static_cast<int>(std::clamp(linear, 0.0f, 1.0f) * 4096.0f + 0.5f)];
            }
        };

        const SrgbTables& Tables()
        {
            static const SrgbTables tables;
            return tables;
        }

        void RequireImage(const Image& image, const char* what)
        {
            if (image.Empty()) {
                throw std::invalid_argument(std::string(what) + ": the image is empty");
            }
        }
    }

    Image::Image(const int width, const int height, const Color& fill)
        : width_(width), height_(height)
    {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("Image: width and height must be positive");
        }
        const std::size_t texels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        rgba_.resize(texels * 4U);
        for (std::size_t texel = 0; texel < texels; ++texel) {
            rgba_[texel * 4U] = fill.getRProperty();
            rgba_[texel * 4U + 1U] = fill.getGProperty();
            rgba_[texel * 4U + 2U] = fill.getBProperty();
            rgba_[texel * 4U + 3U] = fill.getAProperty();
        }
    }

    Image Image::FromColors(const int width, const int height, const std::vector<Color>& colors)
    {
        Image image(width, height);
        if (colors.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            throw std::invalid_argument("Image::FromColors: the colour count does not match the size");
        }
        for (std::size_t texel = 0; texel < colors.size(); ++texel) {
            image.rgba_[texel * 4U] = colors[texel].getRProperty();
            image.rgba_[texel * 4U + 1U] = colors[texel].getGProperty();
            image.rgba_[texel * 4U + 2U] = colors[texel].getBProperty();
            image.rgba_[texel * 4U + 3U] = colors[texel].getAProperty();
        }
        return image;
    }

    Color Image::At(const int x, const int y) const
    {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            throw std::out_of_range("Image::At: texel outside the image");
        }
        const std::uint8_t* texel = rgba_.data() + Offset(x, y);
        return Color(static_cast<int>(texel[0]), static_cast<int>(texel[1]), static_cast<int>(texel[2]),
                     static_cast<int>(texel[3]));
    }

    Color Image::AtWrapped(const int x, const int y) const
    {
        RequireImage(*this, "Image::AtWrapped");
        return At(((x % width_) + width_) % width_, ((y % height_) + height_) % height_);
    }

    void Image::Set(const int x, const int y, const Color& color)
    {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            throw std::out_of_range("Image::Set: texel outside the image");
        }
        std::uint8_t* texel = rgba_.data() + Offset(x, y);
        texel[0] = color.getRProperty();
        texel[1] = color.getGProperty();
        texel[2] = color.getBProperty();
        texel[3] = color.getAProperty();
    }

    std::vector<Color> Image::ToColors() const
    {
        std::vector<Color> colors;
        colors.reserve(rgba_.size() / 4U);
        for (std::size_t texel = 0; texel < rgba_.size(); texel += 4U) {
            colors.emplace_back(static_cast<int>(rgba_[texel]), static_cast<int>(rgba_[texel + 1U]),
                                static_cast<int>(rgba_[texel + 2U]), static_cast<int>(rgba_[texel + 3U]));
        }
        return colors;
    }

    Image Downsample(const Image& image, const MipOptions& options)
    {
        RequireImage(image, "Downsample");
        const int width = image.Width();
        const int height = image.Height();
        Image result(std::max(1, width / 2), std::max(1, height / 2));
        // Halving rounds down, so a block reaches past the edge only of a
        // dimension of 1, which it reads twice.
        const auto column = [&](const int x) { return std::min(width - 1, x); };
        const auto row = [&](const int y) { return std::min(height - 1, y); };
        const int bias = options.rounding == MipRounding::Nearest ? 2 : 0;
        const SrgbTables* tables = options.srgb ? &Tables() : nullptr;
        const std::uint8_t* source = image.Bytes().data();
        std::uint8_t* target = result.Bytes().data();
        const auto texel = [&](const int x, const int y) {
            return source + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
        };
        for (int y = 0; y < result.Height(); ++y) {
            const int y0 = row(y * 2);
            const int y1 = row(y * 2 + 1);
            for (int x = 0; x < result.Width(); ++x, target += 4) {
                const int x0 = column(x * 2);
                const int x1 = column(x * 2 + 1);
                const std::uint8_t* p0 = texel(x0, y0);
                const std::uint8_t* p1 = texel(x1, y0);
                const std::uint8_t* p2 = texel(x0, y1);
                const std::uint8_t* p3 = texel(x1, y1);
                for (int channel = 0; channel < 4; ++channel) {
                    if (tables != nullptr && channel < 3) {
                        target[channel] = tables->Encode((tables->decode[p0[channel]] + tables->decode[p1[channel]]
                                                          + tables->decode[p2[channel]] + tables->decode[p3[channel]])
                                                         * 0.25f);
                    } else {
                        target[channel] =
                            static_cast<std::uint8_t>((p0[channel] + p1[channel] + p2[channel] + p3[channel] + bias) / 4);
                    }
                }
            }
        }
        return result;
    }

    double AlphaTestCoverage(const Image& image, const int reference)
    {
        RequireImage(image, "AlphaTestCoverage");
        const auto& bytes = image.Bytes();
        std::size_t passing = 0;
        for (std::size_t alpha = 3; alpha < bytes.size(); alpha += 4U) {
            passing += bytes[alpha] > reference ? 1U : 0U;
        }
        return static_cast<double>(passing) / static_cast<double>(bytes.size() / 4U);
    }

    Image DownsampleForAlphaTest(const Image& image, const int reference, const double coverage,
                                 const MipOptions& options)
    {
        if (reference < 1 || reference > 254 || !std::isfinite(coverage) || coverage < 0.0 || coverage > 1.0) {
            throw std::invalid_argument("DownsampleForAlphaTest: reference must be 1..254 and coverage 0..1");
        }
        Image result = Downsample(image, options);
        auto& bytes = result.Bytes();
        const double texels = static_cast<double>(bytes.size() / 4U);
        const auto covered = [&](const double scale) {
            std::size_t passing = 0;
            for (std::size_t alpha = 3; alpha < bytes.size(); alpha += 4U) {
                passing += static_cast<double>(bytes[alpha]) * scale >= reference + 1.0 ? 1U : 0U;
            }
            return static_cast<double>(passing) / texels;
        };
        // Coverage is a step function of the scale: keep the closest result
        // seen rather than whichever end the bisection stops at (tiny levels).
        double best = 1.0;
        double error = std::abs(covered(1.0) - coverage);
        double low = 0.0;
        double high = 8.0;
        for (int iteration = 0; iteration < 24; ++iteration) {
            const double scale = (low + high) * 0.5;
            const double current = covered(scale);
            const double difference = std::abs(current - coverage);
            if (difference < error) {
                best = scale;
                error = difference;
            }
            if (current < coverage) {
                low = scale;
            } else {
                high = scale;
            }
        }
        for (std::size_t alpha = 3; alpha < bytes.size(); alpha += 4U) {
            bytes[alpha] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(bytes[alpha] * best), 0, 255));
        }
        return result;
    }

    namespace
    {
        // Steps through a mip chain level by level without keeping it.
        class MipLevels
        {
        public:
            MipLevels(const Image& image, const MipOptions& options)
                : level_(image), options_(options),
                  coverage_(options.alphaTestReference ? AlphaTestCoverage(image, *options.alphaTestReference) : 0.0)
            {
            }

            [[nodiscard]] const Image& Current() const noexcept { return level_; }
            [[nodiscard]] bool Last() const noexcept { return level_.Width() == 1 && level_.Height() == 1; }

            void Next()
            {
                level_ = options_.alphaTestReference
                             ? DownsampleForAlphaTest(level_, *options_.alphaTestReference, coverage_, options_)
                             : Downsample(level_, options_);
            }

        private:
            Image level_;
            MipOptions options_;
            double coverage_;
        };
    }

    std::vector<Image> BuildMipChain(const Image& image, const MipOptions& options)
    {
        RequireImage(image, "BuildMipChain");
        MipLevels levels(image, options);
        std::vector<Image> chain{levels.Current()};
        while (!levels.Last()) {
            levels.Next();
            chain.push_back(levels.Current());
        }
        return chain;
    }

    std::unique_ptr<Texture2D> UploadTexture(GraphicsDevice& device, const Image& image, const MipOptions& options,
                                             const bool mipmaps)
    {
        RequireImage(image, "UploadTexture");
        auto texture = std::make_unique<Texture2D>(device, image.Width(), image.Height(), mipmaps, SurfaceFormat::Color);
        UpdateTexture(*texture, image, options);
        return texture;
    }

    void UpdateTexture(Texture2D& texture, const Image& image, const MipOptions& options)
    {
        RequireImage(image, "UpdateTexture");
        if (texture.getWidthProperty() != image.Width() || texture.getHeightProperty() != image.Height()) {
            throw std::invalid_argument("UpdateTexture: the texture and the image differ in size");
        }
        MipLevels levels(image, options);
        for (int level = 0; level < texture.getLevelCountProperty(); ++level) {
            const auto& bytes = levels.Current().Bytes();
            texture.SetData(level, nullptr, bytes.data(), 0, static_cast<int>(bytes.size()));
            if (levels.Last()) {
                break;
            }
            levels.Next();
        }
    }
}
