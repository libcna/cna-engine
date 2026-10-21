// SPDX-License-Identifier: MIT
//
// Graphics/Image.hpp and Core/Noise.hpp against the games they were taken
// from, compiled read-only: every mip level, alpha-test level, uploaded texel
// and noise value must be identical. A game not checked out beside this
// repository leaves out its tests.
#include "CnaEngine/Core/Noise.hpp"
#include "CnaEngine/Graphics/Image.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#if defined(CNA_ENGINE_PROBE_CAR_SIMULATOR)
#include "CarSim/Render/Image.hpp"
#endif
#if defined(CNA_ENGINE_PROBE_SCHOOL_SIMULATOR)
#include "Rendering/ImageUtil.hpp"
#endif
#if defined(CNA_ENGINE_PROBE_TRAIN_SIMULATOR)
#include "trainsim/render/ProceduralMaterials.hpp"
#endif
#if defined(CNA_ENGINE_PROBE_ROOM_SIMULATOR)
#include "CnaRoom/Assets/Image.hpp"
#endif
#if defined(CNA_ENGINE_PROBE_STREET_SIMULATOR)
#include "CnaStreet/Assets/Noise.hpp"
#endif

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace
{
    using CnaEngine::Graphics::Image;
    using CnaEngine::Graphics::MipOptions;
    using CnaEngine::Graphics::MipRounding;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    // Square, wide, tall, odd and power-of-two sizes.
    constexpr std::array<std::pair<int, int>, 8> Sizes{
        {{1, 1}, {2, 2}, {3, 5}, {7, 2}, {16, 16}, {17, 9}, {64, 31}, {128, 128}}};

    Image Random(const int width, const int height, const std::uint32_t seed)
    {
        std::mt19937 random(seed);
        Image image(width, height);
        for (auto& byte : image.Bytes()) {
            byte = static_cast<std::uint8_t>(random() & 0xFFU);
        }
        return image;
    }

    // The games run HiDef: Reach refuses mipmapped non-power-of-two textures.
    struct HiDefDevice
    {
        HiDefDevice() { device.SetGraphicsProfileEXT(GraphicsProfile::HiDef); }
        GraphicsDevice device;
    };

    std::vector<std::vector<std::uint8_t>> Levels(const Texture2D& texture)
    {
        std::vector<std::vector<std::uint8_t>> levels;
        int width = texture.getWidthProperty();
        int height = texture.getHeightProperty();
        for (int level = 0; level < texture.getLevelCountProperty(); ++level) {
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
            texture.GetData(level, nullptr, bytes.data(), 0, static_cast<int>(bytes.size()));
            levels.push_back(std::move(bytes));
            width = std::max(1, width / 2);
            height = std::max(1, height / 2);
        }
        return levels;
    }
}

#if defined(CNA_ENGINE_PROBE_CAR_SIMULATOR)
TEST(ImageEquivalenceTests, CarSimulatorLevelsAndUploads)
{
    HiDefDevice hiDef;
    GraphicsDevice& device = hiDef.device;
    std::uint32_t seed = 1;
    for (const auto& [width, height] : Sizes) {
        const Image image = Random(width, height, seed++);
        CarSim::Render::Image car(width, height);
        car.Pixels() = image.ToColors();
        EXPECT_EQ(CnaEngine::Graphics::Downsample(image).ToColors(), car.Downsampled().Pixels())
            << width << "x" << height;
        const auto engine = CnaEngine::Graphics::UploadTexture(device, image);
        const auto theirs = CarSim::Render::UploadTexture(device, car);
        EXPECT_EQ(Levels(*engine), Levels(*theirs)) << width << "x" << height;
    }
}
#endif

#if defined(CNA_ENGINE_PROBE_SCHOOL_SIMULATOR)
TEST(ImageEquivalenceTests, SchoolSimulatorChainsAndUploads)
{
    HiDefDevice hiDef;
    GraphicsDevice& device = hiDef.device;
    std::uint32_t seed = 100;
    for (const auto& [width, height] : Sizes) {
        const Image image = Random(width, height, seed++);
        const auto theirs = ImageUtil::BuildMipChain(image.ToColors(), width, height);
        const auto engine = CnaEngine::Graphics::BuildMipChain(image);
        ASSERT_EQ(engine.size(), theirs.size()) << width << "x" << height;
        for (std::size_t level = 0; level < engine.size(); ++level) {
            EXPECT_EQ(engine[level].ToColors(), theirs[level]) << width << "x" << height << " level " << level;
        }
        EXPECT_EQ(Levels(*CnaEngine::Graphics::UploadTexture(device, image)),
                  Levels(*ImageUtil::CreateMipmapped(device, image.ToColors(), width, height)))
            << width << "x" << height;
    }
}
#endif

#if defined(CNA_ENGINE_PROBE_TRAIN_SIMULATOR)
TEST(ImageEquivalenceTests, TrainSimulatorLevelsAlphaTestAndUploads)
{
    HiDefDevice hiDef;
    GraphicsDevice& device = hiDef.device;
    std::uint32_t seed = 200;
    for (const auto& [width, height] : Sizes) {
        const Image image = Random(width, height, seed++);
        TrainSim::Render::ProceduralImage train{width, height, image.ToColors()};
        EXPECT_EQ(CnaEngine::Graphics::Downsample(image).ToColors(), train.downsampled().pixels)
            << width << "x" << height;
        for (const int reference : {1, 96, 128, 254}) {
            for (const double coverage : {0.0, 0.3, 0.62, 1.0}) {
                EXPECT_EQ(CnaEngine::Graphics::DownsampleForAlphaTest(image, reference, coverage).ToColors(),
                          train.downsampledForAlphaTest(reference, coverage).pixels)
                    << width << "x" << height << " reference " << reference << " coverage " << coverage;
            }
            const MipOptions alphaTest{.alphaTestReference = reference};
            EXPECT_EQ(Levels(*CnaEngine::Graphics::UploadTexture(device, image, alphaTest)),
                      Levels(*TrainSim::Render::uploadMipmappedTexture(device, train, reference)))
                << width << "x" << height << " reference " << reference;
        }
        EXPECT_EQ(Levels(*CnaEngine::Graphics::UploadTexture(device, image)),
                  Levels(*TrainSim::Render::uploadMipmappedTexture(device, train)))
            << width << "x" << height;
    }
    // The game's own foliage card, uploaded as it uploads it.
    const TrainSim::Render::ProceduralImage leaves = TrainSim::Render::makeLeafSprayImage();
    const Image engineLeaves = Image::FromColors(leaves.width, leaves.height, leaves.pixels);
    EXPECT_EQ(Levels(*CnaEngine::Graphics::UploadTexture(device, engineLeaves, {.alphaTestReference = 96})),
              Levels(*TrainSim::Render::uploadMipmappedTexture(device, leaves, 96)));
}
#endif

#if defined(CNA_ENGINE_PROBE_ROOM_SIMULATOR)
TEST(ImageEquivalenceTests, RoomSimulatorRoundedAndSrgbLevels)
{
    std::uint32_t seed = 300;
    for (const auto& [width, height] : Sizes) {
        const Image image = Random(width, height, seed++);
        CnaRoom::Assets::Image room(width, height);
        room.rgba = image.Bytes();
        EXPECT_EQ(CnaEngine::Graphics::Downsample(image, {.rounding = MipRounding::Nearest}).Bytes(),
                  room.halved(false).rgba)
            << width << "x" << height;
        EXPECT_EQ(CnaEngine::Graphics::Downsample(image, {.rounding = MipRounding::Nearest, .srgb = true}).Bytes(),
                  room.halved(true).rgba)
            << width << "x" << height << " sRGB";
    }
}
#endif

#if defined(CNA_ENGINE_PROBE_STREET_SIMULATOR)
TEST(ImageEquivalenceTests, StreetSimulatorNoise)
{
    namespace Engine = CnaEngine::Noise;
    namespace Street = CnaStreet::Noise;
    std::mt19937 random(400);
    std::uniform_real_distribution<float> coordinate(-300.0f, 300.0f);
    std::uniform_int_distribution<int> lattice(-100000, 100000);
    for (int sample = 0; sample < 4000; ++sample) {
        const float x = coordinate(random);
        const float y = coordinate(random);
        const int ix = lattice(random);
        const int iy = lattice(random);
        const int iz = lattice(random);
        const auto seed = static_cast<std::uint32_t>(random());
        const int period = sample % 7 == 0 ? 0 : 1 + sample % 300;
        const int octaves = 1 + sample % 6;
        const float lacunarity = sample % 3 == 0 ? 2.0f : 1.7f;
        const float gain = 0.35f + static_cast<float>(sample % 5) * 0.1f;
        ASSERT_EQ(Engine::Hash(static_cast<std::uint32_t>(ix)), Street::hashInt(static_cast<std::uint32_t>(ix)));
        ASSERT_EQ(Engine::Hash2(ix, iy, seed), Street::hash2(ix, iy, seed));
        ASSERT_EQ(Engine::Hash3(ix, iy, iz, seed), Street::hash3(ix, iy, iz, seed));
        ASSERT_EQ(Engine::Random2(ix, iy, seed), Street::random2(ix, iy, seed));
        ASSERT_EQ(Engine::Random3(ix, iy, iz, seed), Street::random3(ix, iy, iz, seed));
        ASSERT_EQ(Engine::Wrap(ix, period), Street::wrap(ix, period));
        ASSERT_EQ(Engine::Value2(x, y, period, seed), Street::value2(x, y, period, seed)) << x << "," << y;
        ASSERT_EQ(Engine::Gradient2(x, y, period, seed), Street::gradient2(x, y, period, seed)) << x << "," << y;
        ASSERT_EQ(Engine::Fbm(x, y, period, octaves, lacunarity, gain, seed),
                  Street::fbm(x, y, period, octaves, lacunarity, gain, seed));
        ASSERT_EQ(Engine::Ridged(x, y, period, octaves, seed), Street::ridged(x, y, period, octaves, seed));
        float engineSecond = 0.0f;
        float streetSecond = 0.0f;
        ASSERT_EQ(Engine::Worley2(x, y, period, seed, &engineSecond), Street::worley2(x, y, period, seed, &streetSecond));
        ASSERT_EQ(engineSecond, streetSecond);
        const float edge = x / 300.0f;
        ASSERT_EQ(Engine::SmoothStep(edge, -edge, y / 300.0f), Street::smoothstep(edge, -edge, y / 300.0f));
        ASSERT_EQ(Engine::Fade(x), Street::fade(x));
    }
    ASSERT_EQ(Engine::SmoothStep(0.5f, 0.5f, 0.4f), Street::smoothstep(0.5f, 0.5f, 0.4f));
}
#endif
