// SPDX-License-Identifier: MIT
//
// PILOT-002 / ENG-024 probe: car-simulator's licence-plate texture cache
// (TrafficRenderer::PlateTexture: generate once per plate text, then reuse)
// expressed with the engine AssetRegistry over the pilot's own generator
// (PlateRenderer::Render + UploadTexture, compiled read-only).
#include "CnaEngine/Assets/AssetRegistry.hpp"

#include "CarSim/Render/BitmapFont.hpp"
#include "CarSim/Render/Image.hpp"
#include "CarSim/Render/PlateRenderer.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace CnaEngine::Pilot
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        std::vector<Color> Pixels(Texture2D& texture)
        {
            std::vector<Color> pixels(
                static_cast<std::size_t>(texture.getWidthProperty()) * static_cast<std::size_t>(texture.getHeightProperty()));
            texture.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return pixels;
        }
    }

    TEST(CarSimulatorAssetsTests, PlateCacheExpressedWithAssetRegistry)
    {
        GraphicsDevice device;
        // The pilot runs HiDef: plates are mipmapped 512x108 textures, which
        // Reach rejects (the registry would report that as a load failure).
        device.SetGraphicsProfileEXT(Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef);
        const auto font = CarSim::Render::BitmapFont::CreateBuiltin(device);
        const CarSim::Render::Image atlas = font->AtlasImage();

        int generated = 0;
        Assets::AssetRegistry plates;
        // Plate numbers are text, not paths: no key normalization.
        plates.SetNormalizeKeys(false);
        plates.RegisterLoader<Texture2D>([&](const std::string& text) {
            ++generated;
            return std::shared_ptr<Texture2D>(
                CarSim::Render::UploadTexture(device, CarSim::Render::PlateRenderer::Render(text, *font, atlas), true));
        });

        const std::vector<std::string> traffic = {"1AB 2345", "7XY 0001", "1AB 2345", "./1AB", "1AB 2345", "7XY 0001"};
        std::vector<std::shared_ptr<Texture2D>> used;
        for (const auto& plate : traffic) {
            used.push_back(plates.Get<Texture2D>(plate));
        }
        EXPECT_EQ(generated, 3) << "one generation per distinct plate, as in TrafficRenderer";
        EXPECT_EQ(used[0], used[2]);
        EXPECT_EQ(used[0], used[4]);
        EXPECT_EQ(used[1], used[5]);
        EXPECT_NE(used[0], used[3]) << "text keys are not treated as paths";
        EXPECT_EQ(plates.Statistics().hits, 3u);

        // The cached texture is exactly what the pilot uploads itself.
        const auto direct =
            CarSim::Render::UploadTexture(device, CarSim::Render::PlateRenderer::Render("1AB 2345", *font, atlas), true);
        EXPECT_EQ(used[0]->getWidthProperty(), CarSim::Render::PlateRenderer::kWidth);
        EXPECT_EQ(used[0]->getHeightProperty(), CarSim::Render::PlateRenderer::kHeight);
        EXPECT_EQ(Pixels(*used[0]), Pixels(*direct));
    }
}
