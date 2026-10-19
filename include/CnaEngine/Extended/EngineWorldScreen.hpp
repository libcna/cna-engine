// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001): this header depends on cna-extended and must
// never be included from CnaEngine::Core or CnaEngine::Graphics.
#pragma once

#include "CnaEngine/Diagnostics/FrameStatistics.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include "CNA/Extended/World3DEXT/World3DScreenEXT.hpp"

#include <memory>
#include <optional>

namespace CnaEngine::Extended
{
    /// A cna-extended World3D screen driven by engine input and camera policy.
    ///
    /// The screen keeps cna-extended's lifecycle and ECS untouched (it *is* a
    /// World3DScreenEXT, so ScreenManager, transitions and World3D systems work
    /// as before) and adds, per Update():
    ///
    ///  1. one InputSnapshot (CaptureInput(), overridable for replays/tests)
    ///     fed to the screen's ActionMap;
    ///  2. UpdateScreen() for game logic, with actions already up to date;
    ///  3. the camera controller, if one is set, from the configured axes;
    ///  4. the engine camera copied into GetCamera3DEXT();
    ///  5. the World3D systems (World3DScreenEXT::Update).
    ///
    /// Draw() runs the World3D systems and records the frame time in
    /// FrameStats(). Games that do not want this order use the free functions
    /// in WorldCameraSync.hpp with their own World3DScreenEXT instead.
    class EngineWorldScreen : public CNA::Extended::World3DEXT::World3DScreenEXT
    {
    public:
        EngineWorldScreen();

        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

        [[nodiscard]] Input::ActionMap& Actions() noexcept { return actions_; }
        [[nodiscard]] Graphics::Camera& Camera() noexcept { return camera_; }
        [[nodiscard]] const Diagnostics::FrameStatistics& FrameStats() const noexcept { return frameStats_; }

        /// Lets `controller` move the camera from `axes` each update; the
        /// controller adopts the camera's current heading. Pass nullptr to
        /// stop driving the camera.
        void SetCameraController(std::unique_ptr<Graphics::ICameraController> controller,
                                 const Graphics::CameraInputAxes& axes);

    protected:
        /// Default: InputSnapshot::Capture(getIsActiveProperty()).
        [[nodiscard]] virtual Input::InputSnapshot CaptureInput();
        /// Game logic hook, called after the actions were updated and before
        /// the camera moves. The default does nothing.
        virtual void UpdateScreen(Microsoft::Xna::Framework::GameTime& gameTime) { (void)gameTime; }

    private:
        Input::ActionMap actions_;
        Graphics::Camera camera_;
        std::unique_ptr<Graphics::ICameraController> controller_;
        Graphics::CameraInputAxes axes_;
        Diagnostics::FrameStatistics frameStats_;
    };
}
