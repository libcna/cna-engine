// SPDX-License-Identifier: MIT
#pragma once

// The CPU mip chains car-, school- and train-simulator built before they moved
// onto CnaEngine::Graphics::Image (README.md beside this file names the
// commits). Their arithmetic is kept exactly; only the containers are the
// fixture's own.

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <memory>
#include <vector>

namespace simulator_oracles
{
    /// Row-major colours with their size (car's Image, train's ProceduralImage).
    struct Pixels
    {
        int width = 0;
        int height = 0;
        std::vector<Microsoft::Xna::Framework::Color> colors;
    };

    // car-simulator: Image::Downsampled, UploadTexture and UpdateTexture.
    [[nodiscard]] Pixels CarDownsampled(const Pixels& image);
    [[nodiscard]] std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>
    CarUploadTexture(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Pixels& image, bool mipmaps = true);

    // school-simulator (from museum-simulator): ImageUtil::BuildMipChain and CreateMipmapped.
    [[nodiscard]] std::vector<std::vector<Microsoft::Xna::Framework::Color>>
    SchoolBuildMipChain(const std::vector<Microsoft::Xna::Framework::Color>& pixels, int width, int height);
    [[nodiscard]] std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>
    SchoolCreateMipmapped(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                          const std::vector<Microsoft::Xna::Framework::Color>& pixels, int width, int height);

    // train-simulator: ProceduralImage::downsampled, downsampledForAlphaTest and uploadMipmappedTexture.
    [[nodiscard]] Pixels TrainDownsampled(const Pixels& image);
    [[nodiscard]] Pixels TrainDownsampledForAlphaTest(const Pixels& image, int referenceAlpha, double coverage);
    [[nodiscard]] std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>
    TrainUploadMipmappedTexture(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Pixels& image,
                                int alphaTestReference = -1);
}
