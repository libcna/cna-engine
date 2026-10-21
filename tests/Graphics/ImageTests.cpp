// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Image.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        // A deterministic image whose texels all differ.
        Image Pattern(const int width, const int height)
        {
            Image image(width, height);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    image.Set(x, y, Color((x * 37 + y * 11) % 256, (x * 5 + y * 91) % 256, (x * y * 13 + 7) % 256,
                                          (x * 53 + y * 29) % 256));
                }
            }
            return image;
        }

        std::vector<std::uint8_t> Level(const Texture2D& texture, const int level, const int width, const int height)
        {
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
            texture.GetData(level, nullptr, bytes.data(), 0, static_cast<int>(bytes.size()));
            return bytes;
        }
    }

    TEST(ImageTests, StoresTexelsAsBytes)
    {
        Image image(3, 2, Color(1, 2, 3, 4));
        EXPECT_EQ(image.Width(), 3);
        EXPECT_EQ(image.Height(), 2);
        EXPECT_EQ(image.Bytes().size(), 24u);
        EXPECT_EQ(image.At(2, 1), Color(1, 2, 3, 4));
        image.Set(1, 0, Color(200, 100, 50, 25));
        EXPECT_EQ(image.Bytes()[4], 200);
        EXPECT_EQ(image.Bytes()[7], 25);
        EXPECT_EQ(image.AtWrapped(-2, 2), Color(200, 100, 50, 25));
        EXPECT_EQ(Image::FromColors(3, 2, image.ToColors()), image);
        EXPECT_THROW(Image(0, 1), std::invalid_argument);
        EXPECT_THROW((void)Image::FromColors(2, 2, image.ToColors()), std::invalid_argument);
        EXPECT_THROW((void)image.At(3, 0), std::out_of_range);
    }

    TEST(ImageTests, DownsampleAveragesBlocksAndRounds)
    {
        Image image(2, 2);
        image.Set(0, 0, Color(1, 0, 0, 255));
        image.Set(1, 0, Color(2, 0, 0, 255));
        image.Set(0, 1, Color(3, 0, 0, 255));
        image.Set(1, 1, Color(4, 255, 0, 0));
        const Image truncated = Downsample(image);
        ASSERT_EQ(truncated.Width(), 1);
        EXPECT_EQ(truncated.At(0, 0), Color(2, 63, 0, 191)) << "10 / 4, 255 / 4, 765 / 4 rounded down";
        EXPECT_EQ(Downsample(image, {.rounding = MipRounding::Nearest}).At(0, 0), Color(3, 64, 0, 191));

        // A dimension of 1 is read twice; an odd last column is left out.
        Image column(1, 3, Color(10, 10, 10, 10));
        EXPECT_EQ(Downsample(column).Height(), 1);
        EXPECT_EQ(Downsample(column).At(0, 0), Color(10, 10, 10, 10));
        const Image odd = Pattern(5, 4);
        EXPECT_EQ(Downsample(odd).Width(), 2);
        EXPECT_THROW((void)Downsample(Image()), std::invalid_argument);
    }

    TEST(ImageTests, SrgbLevelsAverageInLinearLight)
    {
        Image image(2, 1);
        image.Set(0, 0, Color(0, 0, 0, 0));
        image.Set(1, 0, Color(255, 255, 255, 255));
        const Color linear = Downsample(image, {.rounding = MipRounding::Nearest}).At(0, 0);
        const Color srgb = Downsample(image, {.rounding = MipRounding::Nearest, .srgb = true}).At(0, 0);
        EXPECT_EQ(linear, Color(128, 128, 128, 128));
        EXPECT_EQ(srgb, Color(188, 188, 188, 128)) << "half the light is sRGB 188; alpha is averaged as it is";
    }

    TEST(ImageTests, AlphaTestedLevelsKeepTheirCoverage)
    {
        const Image image = Pattern(32, 32);
        constexpr int reference = 96;
        const double target = AlphaTestCoverage(image, reference);
        const MipOptions alphaTest{.alphaTestReference = reference};
        const auto plain = BuildMipChain(image);
        const auto kept = BuildMipChain(image, alphaTest);
        ASSERT_EQ(plain.size(), kept.size());
        ASSERT_EQ(kept.size(), 6u);
        // The first level starts from the same image either way (scale 1 is
        // the plain level). Deeper levels hold few distinct alphas, so they
        // reach only as close as those steps allow (the pilot probe compares
        // them with train-simulator's).
        EXPECT_LE(std::abs(AlphaTestCoverage(kept[1], reference) - target),
                  std::abs(AlphaTestCoverage(plain[1], reference) - target));
        EXPECT_NEAR(AlphaTestCoverage(kept[1], reference), target, 0.02);
        EXPECT_GT(std::abs(AlphaTestCoverage(plain[1], reference) - target), 0.05) << "the plain level fades";
        EXPECT_THROW((void)DownsampleForAlphaTest(image, 0, 0.5), std::invalid_argument);
        EXPECT_THROW((void)DownsampleForAlphaTest(image, 96, 1.5), std::invalid_argument);
    }

    TEST(ImageTests, ChainsRunToOneTexel)
    {
        const auto chain = BuildMipChain(Pattern(5, 3));
        ASSERT_EQ(chain.size(), 3u);
        EXPECT_EQ(chain[1].Width(), 2);
        EXPECT_EQ(chain[1].Height(), 1);
        EXPECT_EQ(chain[2].Width(), 1);
        EXPECT_EQ(chain[1], Downsample(chain[0]));
        EXPECT_EQ(BuildMipChain(Image(1, 1)).size(), 1u);
    }

    TEST(ImageTests, UploadWritesEveryLevel)
    {
        GraphicsDevice device;
        const Image image = Pattern(8, 4);
        const auto chain = BuildMipChain(image);
        auto texture = UploadTexture(device, image);
        ASSERT_EQ(texture->getLevelCountProperty(), 4);
        for (int level = 0; level < 4; ++level) {
            const Image& expected = chain[static_cast<std::size_t>(level)];
            EXPECT_EQ(Level(*texture, level, expected.Width(), expected.Height()), expected.Bytes()) << "level " << level;
        }

        const Image other = Pattern(8, 4);
        Image changed = other;
        changed.Set(0, 0, Color(255, 0, 0, 255));
        UpdateTexture(*texture, changed);
        EXPECT_EQ(Level(*texture, 1, 4, 2), BuildMipChain(changed)[1].Bytes());
        EXPECT_THROW(UpdateTexture(*texture, Pattern(4, 4)), std::invalid_argument);

        auto single = UploadTexture(device, image, {}, false);
        EXPECT_EQ(single->getLevelCountProperty(), 1);
        EXPECT_EQ(Level(*single, 0, 8, 4), image.Bytes());
    }
}
