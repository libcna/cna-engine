// SPDX-License-Identifier: MIT
//
// hello_engine: the smallest complete game on EngineGame (plan.md GAME-003).
// A textured crate turns on a lit field while the camera circles it; F3 shows
// statistics, F12 saves a screenshot, Escape quits. Built with
// cna_engine_add_game(), which copies assets/ beside the executable (or
// preloads it in a browser build); the crate's texture is assets/crate.png.
//
// Automated: cna_engine_hello --frames 120 --lockstep --screenshot hello.png
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <cmath>
#include <memory>

using namespace CnaEngine;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;
using Microsoft::Xna::Framework::Graphics::Texture2D;

class HelloGame final : public App::EngineGame
{
public:
    HelloGame() : EngineGame(MakeOptions()) {}

private:
    static App::GameOptions MakeOptions()
    {
        App::GameOptions options;
        options.title = "Hello, engine";
        options.width = 640;
        options.height = 360;
        return options;
    }

    void LoadGame() override
    {
        auto& device = getGraphicsDeviceProperty();
        const auto& layout = Graphics::VertexLayout::PositionNormalTexture();
        Graphics::MeshBuilder builder;
        builder.AddGrid(Vector3::Zero, 40.0f, 40.0f, 8, 8, 10.0f);
        ground_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
        builder.AddBox(Vector3(-1.0f, 0.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f));
        crate_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);

        crateTexture_ = std::make_unique<Texture2D>(getContentProperty().Load<Texture2D>("crate"));
        crateMaterial_.baseTexture = crateTexture_.get();
        crateMaterial_.sampler = &Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp;
        groundMaterial_.diffuseColor = Vector3(0.35f, 0.55f, 0.3f);
    }

    void FixedUpdate(const double step) override { time_ += step; }

    void UpdateFrame(double) override
    {
        const auto angle = static_cast<float>(time_ * 0.4);
        MainCamera().SetLookAt(Vector3(8.0f * std::cos(angle), 4.0f, 8.0f * std::sin(angle)), Vector3(0.0f, 1.0f, 0.0f));
    }

    void Submit(App::Frame& frame) override
    {
        frame.Add(ground_, groundMaterial_, Matrix::getIdentityProperty());
        frame.Add(crate_, crateMaterial_, Matrix::CreateRotationY(static_cast<float>(time_)));
    }

    Graphics::GpuMesh ground_;
    Graphics::GpuMesh crate_;
    std::unique_ptr<Texture2D> crateTexture_;
    Graphics::Material groundMaterial_;
    Graphics::Material crateMaterial_;
    double time_ = 0.0;
};

int main(int argc, char** argv)
{
    return App::RunGame<HelloGame>(argc, argv);
}
