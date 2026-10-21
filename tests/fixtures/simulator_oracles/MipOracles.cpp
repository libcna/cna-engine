// SPDX-License-Identifier: MIT
#include "MipOracles.hpp"

#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace simulator_oracles
{
    // ---------------------------------------------------------------- car-simulator

    Pixels CarDownsampled(const Pixels& image)
    {
        const int width_ = image.width;
        const int height_ = image.height;
        const auto At = [&](const int x, const int y) -> const Color& {
            return image.colors[static_cast<std::size_t>(y) * width_ + x];
        };
        const int w = std::max(1, width_ / 2);
        const int h = std::max(1, height_ / 2);
        Pixels out{w, h, std::vector<Color>(static_cast<std::size_t>(w) * h, Color(0, 0, 0, 255))};
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                for (int dy = 0; dy < 2; ++dy) {
                    for (int dx = 0; dx < 2; ++dx) {
                        const Color& c = At(std::min(width_ - 1, x * 2 + dx), std::min(height_ - 1, y * 2 + dy));
                        r += c.getRProperty();
                        g += c.getGProperty();
                        b += c.getBProperty();
                        a += c.getAProperty();
                    }
                }
                out.colors[static_cast<std::size_t>(y) * w + x] = Color(r / 4, g / 4, b / 4, a / 4);
            }
        }
        return out;
    }

    std::unique_ptr<Texture2D> CarUploadTexture(GraphicsDevice& device, const Pixels& image, const bool mipmaps)
    {
        auto texture = std::make_unique<Texture2D>(device, image.width, image.height, mipmaps, SurfaceFormat::Color);
        texture->SetData(0, nullptr, image.colors.data(), 0, static_cast<int>(image.colors.size()));
        if (texture->getLevelCountProperty() > 1) {
            Pixels level = image;
            int index = 1;
            while (level.width > 1 || level.height > 1) {
                level = CarDownsampled(level);
                texture->SetData(index, nullptr, level.colors.data(), 0, static_cast<int>(level.colors.size()));
                ++index;
            }
        }
        return texture;
    }

    // ---------------------------------------------------------------- school-simulator

    std::vector<std::vector<Color>> SchoolBuildMipChain(const std::vector<Color>& pixels, int width, int height)
    {
        std::vector<std::vector<Color>> chain;
        chain.push_back(pixels);
        int w = width;
        int h = height;
        while (w > 1 || h > 1) {
            const int nw = std::max(1, w / 2);
            const int nh = std::max(1, h / 2);
            const std::vector<Color>& src = chain.back();
            std::vector<Color> dst(static_cast<std::size_t>(nw) * static_cast<std::size_t>(nh));
            for (int y = 0; y < nh; ++y) {
                for (int x = 0; x < nw; ++x) {
                    int r = 0, g = 0, b = 0, a = 0, n = 0;
                    for (int dy = 0; dy < 2; ++dy) {
                        for (int dx = 0; dx < 2; ++dx) {
                            const int sx = std::min(w - 1, x * 2 + dx);
                            const int sy = std::min(h - 1, y * 2 + dy);
                            const Color& c = src[static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(sx)];
                            r += c.getRProperty(); g += c.getGProperty(); b += c.getBProperty(); a += c.getAProperty();
                            ++n;
                        }
                    }
                    dst[static_cast<std::size_t>(y) * static_cast<std::size_t>(nw) + static_cast<std::size_t>(x)] =
                        Color(static_cast<std::uint8_t>(r / n), static_cast<std::uint8_t>(g / n),
                              static_cast<std::uint8_t>(b / n), static_cast<std::uint8_t>(a / n));
                }
            }
            chain.push_back(std::move(dst));
            w = nw;
            h = nh;
        }
        return chain;
    }

    std::unique_ptr<Texture2D> SchoolCreateMipmapped(GraphicsDevice& device, const std::vector<Color>& pixels,
                                                     int width, int height)
    {
        const auto chain = SchoolBuildMipChain(pixels, width, height);
        auto texture = std::make_unique<Texture2D>(device, width, height, true, SurfaceFormat::Color);
        int w = width;
        int h = height;
        const int levels = std::min(static_cast<int>(chain.size()), texture->getLevelCountProperty());
        for (int level = 0; level < levels; ++level) {
            const Rectangle rect(0, 0, w, h);
            texture->SetData(level, &rect, chain[static_cast<std::size_t>(level)].data(), 0,
                             static_cast<int>(chain[static_cast<std::size_t>(level)].size()));
            w = std::max(1, w / 2);
            h = std::max(1, h / 2);
        }
        return texture;
    }

    // ---------------------------------------------------------------- train-simulator

    Pixels TrainDownsampled(const Pixels& image)
    {
        const int width = image.width;
        const int height = image.height;
        const auto& pixels = image.colors;
        if (width <= 0 || height <= 0
            || pixels.size() != static_cast<std::size_t>(width * height))
            throw std::invalid_argument("Cannot downsample an invalid procedural image");

        Pixels result;
        result.width = std::max(1,width/2);
        result.height = std::max(1,height/2);
        result.colors.resize(static_cast<std::size_t>(result.width*result.height));
        for (int y = 0; y < result.height; ++y)
            for (int x = 0; x < result.width; ++x)
            {
                int red = 0;
                int green = 0;
                int blue = 0;
                int alpha = 0;
                for (int offsetY = 0; offsetY < 2; ++offsetY)
                    for (int offsetX = 0; offsetX < 2; ++offsetX)
                    {
                        const int sourceX = std::min(width-1,x*2+offsetX);
                        const int sourceY = std::min(height-1,y*2+offsetY);
                        const Color& color = pixels[static_cast<std::size_t>(sourceY*width+sourceX)];
                        red += color.getRProperty();
                        green += color.getGProperty();
                        blue += color.getBProperty();
                        alpha += color.getAProperty();
                    }
                result.colors[static_cast<std::size_t>(y*result.width+x)] =
                    Color(red/4,green/4,blue/4,alpha/4);
            }
        return result;
    }

    Pixels TrainDownsampledForAlphaTest(const Pixels& image, int referenceAlpha, double coverage)
    {
        if (referenceAlpha<1 || referenceAlpha>254 || !std::isfinite(coverage)
            || coverage<0 || coverage>1)
            throw std::invalid_argument("Invalid alpha-test coverage target");
        auto result=TrainDownsampled(image);
        const auto covered=[&](double scale) {
            return static_cast<double>(std::count_if(result.colors.begin(),result.colors.end(),
                [&](const Color& pixel) { return pixel.getAProperty()*scale>=referenceAlpha+1.0; }))
                /static_cast<double>(result.colors.size());
        };
        double best=1, error=std::abs(covered(1)-coverage), low=0, high=8;
        for (int iteration=0;iteration<24;++iteration)
        {
            const double scale=(low+high)*0.5;
            const double current=covered(scale);
            const double difference=std::abs(current-coverage);
            if (difference<error) { best=scale; error=difference; }
            if (current<coverage) low=scale;
            else high=scale;
        }
        for (auto& pixel:result.colors)
            pixel.setAProperty(static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(pixel.getAProperty()*best),0,255)));
        return result;
    }

    std::unique_ptr<Texture2D> TrainUploadMipmappedTexture(GraphicsDevice& device, const Pixels& image,
                                                           int alphaTestReference)
    {
        if (image.width <= 0 || image.height <= 0
            || image.colors.size() != static_cast<std::size_t>(image.width*image.height))
            throw std::invalid_argument("Cannot upload an invalid procedural image");

        auto texture = std::make_unique<Texture2D>(
            device,image.width,image.height,true,SurfaceFormat::Color);
        Pixels level = image;
        const double coverage=static_cast<double>(std::count_if(image.colors.begin(),image.colors.end(),
            [&](const Color& pixel) { return pixel.getAProperty()>alphaTestReference; }))
            /static_cast<double>(image.colors.size());
        for (int mip = 0; mip < texture->getLevelCountProperty(); ++mip)
        {
            texture->SetData(mip,nullptr,level.colors.data(),0,
                             static_cast<int>(level.colors.size()));
            if (level.width == 1 && level.height == 1) break;
            level = alphaTestReference<0 ? TrainDownsampled(level)
                : TrainDownsampledForAlphaTest(level,alphaTestReference,coverage);
        }
        return texture;
    }
}
