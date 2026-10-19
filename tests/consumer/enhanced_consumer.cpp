// SPDX-License-Identifier: MIT
//
// Enhanced-profile consumer: links CnaEngine::Engine and runs a cna-extended
// World3D screen through the engine adapter.
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Extended/EngineWorldScreen.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "System/TimeSpan.hpp"

#include <cstdio>
#include <memory>

namespace
{
    class ProbeScreen final : public CnaEngine::Extended::EngineWorldScreen
    {
    protected:
        CnaEngine::Input::InputSnapshot CaptureInput() override { return {}; }
    };
}

int main()
{
    using Microsoft::Xna::Framework::GameTime;
    using Microsoft::Xna::Framework::Vector3;

    ProbeScreen screen;
    screen.Initialize();
    screen.Camera().SetPosition(Vector3(0.0f, 3.0f, 8.0f));
    screen.Camera().LookAt(Vector3::Zero);
    GameTime time(System::TimeSpan::FromSeconds(0.0), System::TimeSpan::FromSeconds(1.0 / 60.0));
    screen.Update(time);
    screen.Draw(time);

    // EnhancedGraphics: a PBR binder plugged into the strict renderer.
    Microsoft::Xna::Framework::Graphics::GraphicsDevice device;
    CnaEngine::Graphics::ForwardRenderer renderer(device);
    renderer.SetMaterialBinder(
        std::make_shared<CnaEngine::Enhanced::PbrMaterialBinder>(device, renderer.StockBinder()));

    const bool ok = screen.GetCamera3DEXT().getPositionProperty() == Vector3(0.0f, 3.0f, 8.0f)
                    && screen.FrameStats().FrameCount() == 1
                    && dynamic_cast<CnaEngine::Enhanced::PbrMaterialBinder*>(&renderer.MaterialBinder()) != nullptr;
    std::printf("enhanced consumer: World3D screen and PBR binder %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
