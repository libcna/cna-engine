// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/App/MouseCapture.hpp"
#include "CnaEngine/Core/FixedStepClock.hpp"
#include "CnaEngine/Core/SettingsFile.hpp"
#include "CnaEngine/Diagnostics/FrameStatistics.hpp"
#include "CnaEngine/Graphics/AutomatedRun.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"
#include "CnaEngine/Graphics/ScreenCapture.hpp"
#include "CnaEngine/Graphics/ShadowMap.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"
#include "CnaEngine/Input/ActionMap.hpp"
#include "CnaEngine/Input/InputSnapshot.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::App
{
    /// What a game asks of its window and loop. Every field has a default; the
    /// player's preferences file and the command line may change the window's.
    struct GameOptions
    {
        /// The window's title (and the statistics line's prefix).
        std::string title = "CNA game";
        /// The folder of the preferences under the user's configuration
        /// directory (UserConfigPath); empty: no preferences are read or written.
        std::string application;
        int width = 1280;
        int height = 720;
        bool fullScreen = false;
        bool verticalSync = true;
        /// XNA's HiDef profile (hardware instancing, back-buffer capture);
        /// false selects Reach.
        bool hiDef = true;
        /// The length of one FixedUpdate step.
        double fixedStepSeconds = 1.0 / 60.0;
        Microsoft::Xna::Framework::Color clearColor = Microsoft::Xna::Framework::Color(135, 175, 215, 255);
        /// The name of the asset directory (FindAssetDirectory); the content
        /// manager's root is set to it when it is found.
        std::string assetDirectory = "assets";
        /// The main camera's projection; the aspect follows the window.
        float fieldOfViewDegrees = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 500.0f;
    };

    /// The items a game draws this frame; filled in EngineGame::Submit.
    ///
    /// Items are culled against the main camera with EngineGame::Visibility()
    /// and drawn by the forward renderer; meshes, materials and instance sets
    /// are borrowed and must outlive the frame.
    class Frame
    {
    public:
        /// A mesh drawn with `material` at `world`, bounded by the mesh's
        /// bounds transformed by `world`. The returned item may be adjusted
        /// (order, layers, cull distance) until the next Add.
        Graphics::RenderItem& Add(const Graphics::GpuMesh& mesh, const Graphics::Material& material,
                                  const Microsoft::Xna::Framework::Matrix& world);
        Graphics::RenderItem& Add(const Graphics::RenderItem& item);
        /// Every part of a model at `world`; skinned parts take `palette`
        /// (Animation::Animator::SkinPalette), which must outlive the frame.
        void Add(const Graphics::ModelView& model, const Microsoft::Xna::Framework::Matrix& world,
                 const std::vector<Microsoft::Xna::Framework::Matrix>* palette = nullptr);
        /// Every instance of `instances` with one mesh and material; the set
        /// culls its instances per camera, so it is not culled as a whole.
        void AddInstances(const Graphics::GpuMesh& mesh, const Graphics::Material& material,
                          Graphics::InstanceSet& instances);

        [[nodiscard]] std::span<const Graphics::RenderItem> Items() const noexcept { return items_; }
        [[nodiscard]] std::size_t InstancedCount() const noexcept { return instanced_.size(); }
        void Clear() noexcept;

    private:
        friend class EngineGame;
        struct Instanced
        {
            const Graphics::GpuMesh* mesh;
            const Graphics::Material* material;
            Graphics::InstanceSet* instances;
        };
        std::vector<Graphics::RenderItem> items_;
        std::vector<Instanced> instanced_;
    };

    /// The sun's shadow map of an EngineGame (EnableShadowMap).
    struct ShadowSettings
    {
        /// Texels along each side.
        int size = 2048;
        /// The shadowed area's radius around its centre, which sits
        /// `ahead * radius` in front of the camera.
        float radius = 30.0f;
        float ahead = 0.5f;
    };

    /// The actions every EngineGame has; their keys can be rebound like any other.
    struct BuiltInActions
    {
        /// Escape: releases a captured mouse; otherwise quits (in the browser,
        /// where a page has nothing to quit to, it only releases the mouse).
        Input::ActionId quit{};
        /// F12: saves a screenshot into the working directory.
        Input::ActionId screenshot{};
        /// F3: shows frame statistics in the window's title.
        Input::ActionId statistics{};
        /// F11: switches between a window and the full screen.
        Input::ActionId fullScreen{};
    };

    /// An XNA Game with the engine already wired: the device manager, an
    /// action map with the built-in actions, a fixed-step simulation clock, a
    /// main camera kept at the window's aspect, the forward renderer with
    /// culling, frame and render statistics, the screenshot queue, automated
    /// runs from the command line (AutomatedRun), the player's preferences
    /// and key bindings, the asset directory and mouse capture for mouse look.
    ///
    /// A game derives from it and overrides what it needs:
    ///
    ///     class MyGame final : public CnaEngine::App::EngineGame {
    ///         void LoadGame() override;                 // device and assets are ready
    ///         void FixedUpdate(double step) override;   // the simulation, fixed steps
    ///         void UpdateFrame(double seconds) override; // per frame: camera, animation
    ///         void Submit(CnaEngine::App::Frame& frame) override; // what to draw
    ///     };
    ///     int main(int argc, char** argv) { return CnaEngine::App::RunGame<MyGame>(argc, argv); }
    ///
    /// Everything stays reachable (the renderer, its lighting and custom
    /// passes, the camera, the device), and a game may still override XNA's
    /// Initialize, LoadContent, Update and Draw as long as it calls these.
    ///
    /// Update order: input snapshot, actions, built-in actions, mouse delta,
    /// then the automated run decides how much time passes, FixedUpdate runs
    /// that many fixed steps, and UpdateFrame runs once with the same time. In
    /// an automated --lockstep run each drawn frame is exactly one step.
    ///
    /// Draw order: automated-run check (a pending screenshot is read here,
    /// after the previous frame was presented), clear, Submit, culling,
    /// forward renderer, DrawOverlay, F12 screenshots.
    ///
    /// Preferences (not in automated runs, which must not depend on the
    /// player's files): `<config>/<application>/settings.ini` with
    /// `window.width`, `window.height`, `window.fullscreen`, `window.vsync`
    /// and whatever the game keeps there (Preferences()), and
    /// `<config>/<application>/bindings.ini` (ActionMap::SaveBindings), read
    /// after LoadGame so the game's own actions exist.
    class EngineGame : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit EngineGame(GameOptions options = {});
        ~EngineGame() override;

        EngineGame(const EngineGame&) = delete;
        EngineGame& operator=(const EngineGame&) = delete;

        /// Reads the command line: the automated-run options
        /// (AutomatedRunUsage), --settings <file> (preferences from that file
        /// instead of the user's), --width N, --height N, --fullscreen,
        /// --windowed, and whatever ParseArgument accepts. Throws
        /// std::invalid_argument for an unknown argument or a bad value. Call
        /// before Run().
        void ParseCommandLine(std::span<const std::string_view> arguments);
        /// The usage lines of ParseCommandLine's options.
        [[nodiscard]] static std::string Usage();

        [[nodiscard]] const GameOptions& Options() const noexcept { return options_; }
        [[nodiscard]] Microsoft::Xna::Framework::GraphicsDeviceManager& DeviceManager() noexcept { return graphics_; }
        [[nodiscard]] Input::ActionMap& Actions() noexcept { return actions_; }
        [[nodiscard]] const BuiltInActions& BuiltIns() const noexcept { return builtIns_; }
        [[nodiscard]] Graphics::Camera& MainCamera() noexcept { return camera_; }
        /// The forward renderer; throws std::logic_error before LoadContent.
        [[nodiscard]] Graphics::ForwardRenderer& Renderer();
        [[nodiscard]] Graphics::VisibilitySettings& Visibility() noexcept { return visibility_; }
        [[nodiscard]] const FixedStepClock& Clock() const noexcept { return clock_; }
        [[nodiscard]] const Graphics::AutomatedRun& Automation() const noexcept { return automation_; }
        [[nodiscard]] const Diagnostics::FrameStatistics& FrameTimes() const noexcept { return frames_; }
        /// The last frame's renderer and culling statistics.
        [[nodiscard]] const Graphics::RenderStatistics& RenderedLastFrame() const noexcept { return rendered_; }
        [[nodiscard]] const Graphics::CullStatistics& CulledLastFrame() const noexcept { return culled_; }
        [[nodiscard]] Graphics::ScreenshotQueue& Screenshots() noexcept { return screenshots_; }
        /// The player's preferences (empty without an application name).
        [[nodiscard]] SettingsFile& Preferences() noexcept { return preferences_; }
        /// Writes the window's state, Preferences() and the key bindings.
        /// Does nothing without an application name or in an automated run;
        /// returns false (with the reason in `error`) when a file cannot be written.
        bool SavePreferences(std::string* error = nullptr);
        /// The asset directory, absolute; empty when it was not found.
        [[nodiscard]] const std::filesystem::path& AssetDirectory() const noexcept { return assetDirectory_; }
        /// Updates since the start, counting only those that advanced time.
        [[nodiscard]] std::uint64_t UpdateCount() const noexcept { return updates_; }
        [[nodiscard]] bool StatisticsVisible() const noexcept { return statisticsVisible_; }

        /// Mouse look: hides and holds the mouse (RecenteringMouseCapture by
        /// default) and reports its movement in MouseDelta().
        void CaptureMouse(bool captured);
        [[nodiscard]] bool MouseCaptured() const noexcept;
        /// The mouse's movement in pixels during this update; zero while the
        /// mouse is not captured.
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 MouseDelta() const noexcept { return mouseDelta_; }
        /// Replaces how the mouse is captured (null restores the default).
        void SetMouseCapture(std::unique_ptr<IMouseCapture> capture);

        /// Renders a shadow map of each frame's opaque items, from the key
        /// light (ForwardLighting::lightDirection), before the frame is
        /// drawn; `receiver` is then called with it (Enhanced/ShadowReception
        /// makes the effects sample it). Call once the device exists (in
        /// LoadGame). A device without a render target format for it throws.
        void EnableShadowMap(ShadowSettings settings, std::function<void(Graphics::ShadowMap&)> receiver = {});
        void DisableShadowMap() noexcept;
        /// The shadow map, or null when shadows are off.
        [[nodiscard]] Graphics::ShadowMap* Shadows() noexcept { return shadows_.get(); }

    protected:
        /// The device, the renderer and the asset directory are ready; load
        /// the game's content here.
        virtual void LoadGame() {}
        /// One simulation step of Options().fixedStepSeconds.
        virtual void FixedUpdate(double stepSeconds) { (void)stepSeconds; }
        /// Once per update after the fixed steps, with the time they covered:
        /// camera, animation, anything that follows the frame rate.
        virtual void UpdateFrame(double frameSeconds) { (void)frameSeconds; }
        /// The items to draw this frame.
        virtual void Submit(Frame& frame) { (void)frame; }
        /// Drawing after the engine's frame (SpriteBatch text, a HUD). The
        /// renderer has restored the device states it changed.
        virtual void DrawOverlay(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) { (void)device; }
        /// Automated runs photograph a frame only once this holds: content
        /// still loading in the background must not be captured half-done.
        [[nodiscard]] virtual bool ReadyForCapture() const { return true; }
        /// The input of this update; the real devices by default. Examples and
        /// tests override it with a script.
        [[nodiscard]] virtual Input::InputSnapshot ReadInput();
        /// A game's own command-line option at `arguments[index]`: advance
        /// `index` past its value and return true, or return false.
        virtual bool ParseArgument(std::span<const std::string_view> arguments, std::size_t& index)
        {
            (void)arguments;
            (void)index;
            return false;
        }
        /// The built-in quit action; by default releases a captured mouse or
        /// quits (only releases in the browser).
        virtual void OnQuit();
        /// A screenshot of the queue (F12, or one the game requested) was
        /// taken; by default its path or error is printed.
        virtual void OnScreenshot(const Graphics::CaptureResult& result);

        void Initialize() override;
        void LoadContent() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

    private:
        void ApplyWindowOptions();
        void ApplyWindowPreferences();
        void ShowStatistics();

        GameOptions options_;
        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        Input::ActionMap actions_;
        BuiltInActions builtIns_;
        FixedStepClock clock_;
        Graphics::AutomatedRunOptions automationOptions_;
        Graphics::AutomatedRun automation_;
        Graphics::Camera camera_;
        Graphics::VisibilitySettings visibility_;
        std::unique_ptr<Graphics::ForwardRenderer> renderer_;
        Frame frame_;
        Graphics::RenderQueue queue_;
        Diagnostics::FrameStatistics frames_{120};
        Graphics::RenderStatistics rendered_;
        Graphics::CullStatistics culled_;
        Graphics::ScreenshotQueue screenshots_;
        SettingsFile preferences_;
        std::filesystem::path preferencesPath_;
        std::filesystem::path assetDirectory_;
        std::unique_ptr<IMouseCapture> mouse_;
        std::unique_ptr<Graphics::ShadowMap> shadows_;
        ShadowSettings shadowSettings_;
        std::function<void(Graphics::ShadowMap&)> shadowReceiver_;
        std::vector<Graphics::RenderItem> casters_;
        Microsoft::Xna::Framework::Vector2 mouseDelta_ = Microsoft::Xna::Framework::Vector2::Zero;
        std::uint64_t updates_ = 0;
        double statisticsAge_ = 0.0;
        bool statisticsVisible_ = false;
        bool usePreferences_ = true;
        bool widthGiven_ = false;
        bool heightGiven_ = false;
        bool fullScreenGiven_ = false;
    };

    /// Constructs `TGame`, reads the command line, runs it and returns the
    /// process's exit code: 0, or 1 when an automated run's screenshot could
    /// not be taken or the game threw, 2 for a bad command line (with the
    /// usage on stderr). `--help` prints the usage and returns 0.
    template <typename TGame>
    int RunGame(int argc, char** argv);

    namespace Detail
    {
        [[nodiscard]] std::vector<std::string_view> Arguments(int argc, char** argv);
        [[nodiscard]] bool WantsHelp(std::span<const std::string_view> arguments);
        void PrintUsage(std::string_view program);
        void PrintError(std::string_view what);
        [[nodiscard]] int ExitCodeAfterRun(const EngineGame& game);
    }

    template <typename TGame>
    int RunGame(const int argc, char** argv)
    {
        const auto arguments = Detail::Arguments(argc, argv);
        if (Detail::WantsHelp(arguments)) {
            Detail::PrintUsage(argc > 0 ? argv[0] : "game");
            return 0;
        }
        try {
            TGame game;
            try {
                game.ParseCommandLine(arguments);
            } catch (const std::invalid_argument& e) {
                Detail::PrintError(e.what());
                Detail::PrintUsage(argc > 0 ? argv[0] : "game");
                return 2;
            }
            game.Run();
            return Detail::ExitCodeAfterRun(game);
        } catch (const std::exception& e) {
            Detail::PrintError(e.what());
            return 1;
        }
    }
}
