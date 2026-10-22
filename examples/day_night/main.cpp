// SPDX-License-Identifier: MIT
//
// day_night: one scene at four hours, on the enhanced profile (plan.md
// GAME-005): the sky dome and the light follow the clock, the sun casts
// shadows, and at night a lamp lights the ground around it (CNA's punctual
// light through LocalLightBinder, plus a light pool for the ground's glow).
// Writes day_night_<hour>.png into the output directory (default: the
// working directory) and checks that noon is the brightest and night the
// darkest, and that the lamp lights the night.
//
// Usage: cna_engine_day_night [output-directory]
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Enhanced/LocalLightBinder.hpp"
#include "CnaEngine/Enhanced/ShadowReception.hpp"
#include "CnaEngine/Graphics/LocalLights.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/Sky.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace CnaEngine;
using Microsoft::Xna::Framework::Color;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;

namespace
{
    constexpr std::array<double, 4> Hours = {6.5, 12.0, 19.5, 23.5};
    constexpr int FramesPerHour = 4;
}

class DayNight final : public App::EngineGame
{
public:
    explicit DayNight(std::filesystem::path output) : EngineGame(Options()), output_(std::move(output)) {}

    std::array<double, 4> brightness{};
    double lampSpot = 0.0;
    double darkSpot = 0.0;

protected:
    static App::GameOptions Options()
    {
        App::GameOptions options;
        options.title = "Day and night";
        options.width = 320;
        options.height = 180;
        options.assetDirectory.clear();
        return options;
    }

    void LoadGame() override
    {
        auto& device = getGraphicsDeviceProperty();
        Graphics::MeshBuilder builder;
        builder.AddGrid(Vector3::Zero, 30.0f, 30.0f, 30, 30, 1.0f);
        ground_ = Graphics::GpuMesh::Upload(device, builder.Take(), Graphics::VertexLayout::PositionNormalTexture());
        builder.AddBox(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.0f, 0.5f));
        box_ = Graphics::GpuMesh::Upload(device, builder.Take(), Graphics::VertexLayout::PositionNormalTexture());
        grass_.diffuseColor = Vector3(0.4f, 0.55f, 0.35f);
        stone_.diffuseColor = Vector3(0.7f, 0.68f, 0.64f);

        sky_ = std::make_unique<Graphics::SkyDome>(device);
        sky_->AttachTo(Renderer());
        (void)Enhanced::EnableShadows(*this, {.size = 512, .radius = 12.0f, .ahead = 0.6f});
        lamps_ = &Enhanced::InstallLocalLights(Renderer());
        pools_ = std::make_unique<Graphics::LightPools>(device);
        time_.latitudeDegrees = 50.0;
        time_.dayOfYear = 172.0;
        MainCamera().SetLookAt(Vector3(0.0f, 4.0f, 9.0f), Vector3(0.0f, 0.5f, 0.0f));
    }

    void UpdateFrame(double) override
    {
        const auto frame = static_cast<int>(UpdateCount()) - 1;
        hour_ = std::min(static_cast<std::size_t>(frame / FramesPerHour), Hours.size() - 1);
        time_.hours = Hours[hour_];
        const Vector3 sun = time_.DirectionToSun();
        const auto colors = palette_.At(static_cast<float>(time_.Sun().elevationDegrees));
        sky_->SetColors(colors, sun);
        Graphics::ApplySkyLighting(Renderer().Lighting(), colors, sun, 15.0f, 40.0f);

        // A lamp that is lit after dark.
        lamps_->lights.clear();
        if (time_.Sun().elevationDegrees < -4.0) {
            Graphics::LocalLight lamp;
            lamp.position = Vector3(-3.0f, 2.5f, 2.0f);
            lamp.range = 6.0f;
            lamp.color = Vector3(1.0f, 0.8f, 0.5f);
            lamps_->lights.push_back(lamp);
        }
        // The last frame of each hour is photographed.
        if (frame % FramesPerHour == FramesPerHour - 1) {
            char name[64];
            std::snprintf(name, sizeof name, "day_night_%04.1f.png", Hours[hour_]);
            (void)Screenshots().Request({.path = (output_ / name).string(), .keepPixels = true, .opaque = true});
        }
    }

    void Submit(App::Frame& frame) override
    {
        frame.Add(ground_, grass_, Matrix::getIdentityProperty());
        frame.Add(box_, stone_, Matrix::CreateTranslation(1.5f, 0.0f, 0.0f));
        frame.Add(box_, stone_, Matrix::CreateScale(1.0f, 2.0f, 1.0f) * Matrix::CreateTranslation(-1.0f, 0.0f, -2.0f));
        std::vector<Graphics::RenderItem> pools;
        pools_->Append(lamps_->lights, 0.0f, pools, 0.35f);
        for (const auto& item : pools) {
            frame.Add(item);
        }
    }

    void OnScreenshot(const Graphics::CaptureResult& result) override
    {
        EngineGame::OnScreenshot(result);
        if (!result.success || result.pixels.empty()) {
            return;
        }
        double sum = 0.0;
        for (const Color& c : result.pixels) {
            sum += c.getRProperty() + c.getGProperty() + c.getBProperty();
        }
        brightness[hour_] = sum / (3.0 * static_cast<double>(result.pixels.size()));
        if (hour_ == Hours.size() - 1) {
            // Under the lamp and the same distance from the camera on the other side.
            lampSpot = Brightness(result, Vector3(-3.0f, 0.0f, 2.0f));
            darkSpot = Brightness(result, Vector3(3.5f, 0.0f, 2.0f));
        }
    }

private:
    double Brightness(const Graphics::CaptureResult& shot, const Vector3& point)
    {
        const auto clip = Microsoft::Xna::Framework::Vector4::Transform(Microsoft::Xna::Framework::Vector4(point, 1.0f),
                                                                        MainCamera().ViewProjection());
        const int x = static_cast<int>((clip.X / clip.W * 0.5f + 0.5f) * static_cast<float>(shot.width));
        const int y = static_cast<int>((-clip.Y / clip.W * 0.5f + 0.5f) * static_cast<float>(shot.height));
        const Color c = shot.pixels[static_cast<std::size_t>(y * shot.width + x)];
        return (c.getRProperty() + c.getGProperty() + c.getBProperty()) / 3.0;
    }

    std::filesystem::path output_;
    std::size_t hour_ = 0;
    Graphics::TimeOfDay time_;
    Graphics::DayNightPalette palette_ = Graphics::DayNightPalette::Default();
    std::unique_ptr<Graphics::SkyDome> sky_;
    Enhanced::LocalLightBinder* lamps_ = nullptr;
    std::unique_ptr<Graphics::LightPools> pools_;
    Graphics::GpuMesh ground_;
    Graphics::GpuMesh box_;
    Graphics::Material grass_;
    Graphics::Material stone_;
};

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path output = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path();
        std::filesystem::create_directories(output);
        DayNight game(output);
        const std::string frames = std::to_string(Hours.size() * FramesPerHour + 1);
        game.ParseCommandLine(std::vector<std::string_view>{"--frames", frames, "--lockstep"});
        game.Run();
        std::printf("day_night: mean brightness at 06:30 %.1f, 12:00 %.1f, 19:30 %.1f, 23:30 %.1f; "
                    "at night %.1f under the lamp, %.1f away from it\n",
                    game.brightness[0], game.brightness[1], game.brightness[2], game.brightness[3], game.lampSpot,
                    game.darkSpot);
        const bool ordered = game.brightness[1] > game.brightness[0] && game.brightness[1] > game.brightness[2] &&
                             game.brightness[3] < game.brightness[0] && game.brightness[3] < game.brightness[2];
        if (!ordered || game.lampSpot < game.darkSpot + 20.0) {
            std::fprintf(stderr, "day_night: the light does not follow the clock\n");
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "day_night: %s\n", e.what());
        return 1;
    }
}
