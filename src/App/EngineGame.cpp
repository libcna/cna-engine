// SPDX-License-Identifier: MIT
#include "CnaEngine/App/EngineGame.hpp"

#include "CnaEngine/App/AssetDirectory.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/GameWindow.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <charconv>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace CnaEngine::App
{
    namespace
    {
        using Microsoft::Xna::Framework::Input::Keys;

        int ParseInt(const std::string_view option, const std::string_view text, const int minimum, const int maximum)
        {
            int value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end != text.data() + text.size() || value < minimum || value > maximum) {
                throw std::invalid_argument(std::string(option) + " needs a whole number from " +
                                            std::to_string(minimum) + " to " + std::to_string(maximum) + ", not '" +
                                            std::string(text) + "'");
            }
            return value;
        }

        std::string_view Value(const std::span<const std::string_view> arguments, std::size_t& index)
        {
            if (index + 1 >= arguments.size()) {
                throw std::invalid_argument(std::string(arguments[index]) + " needs a value");
            }
            return arguments[++index];
        }

        // Writes `text` beside `path` and renames it over it, as SettingsFile::Save does.
        void WriteAtomically(const std::filesystem::path& path, const std::string& text)
        {
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }
            auto temporary = path;
            temporary += ".tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                stream << text;
                if (!stream) {
                    throw std::runtime_error("cannot write " + temporary.string());
                }
            }
            std::filesystem::rename(temporary, path);
        }
    }

    Graphics::RenderItem& Frame::Add(const Graphics::GpuMesh& mesh, const Graphics::Material& material,
                                     const Microsoft::Xna::Framework::Matrix& world)
    {
        Graphics::RenderItem item;
        item.mesh = &mesh;
        item.material = &material;
        item.world = world;
        item.bounds = Graphics::WorldBounds(mesh, world);
        return Add(item);
    }

    Graphics::RenderItem& Frame::Add(const Graphics::RenderItem& item)
    {
        if (item.mesh == nullptr || item.material == nullptr) {
            throw std::invalid_argument("Frame::Add: a render item needs a mesh and a material");
        }
        return items_.emplace_back(item);
    }

    void Frame::Add(const Graphics::ModelView& model, const Microsoft::Xna::Framework::Matrix& world,
                    const std::vector<Microsoft::Xna::Framework::Matrix>* palette)
    {
        model.AppendItems(items_, world, palette);
    }

    void Frame::AddInstances(const Graphics::GpuMesh& mesh, const Graphics::Material& material,
                             Graphics::InstanceSet& instances)
    {
        instanced_.push_back({&mesh, &material, &instances});
    }

    void Frame::Clear() noexcept
    {
        items_.clear();
        instanced_.clear();
    }

    EngineGame::EngineGame(GameOptions options)
        : options_(std::move(options)),
          graphics_(this),
          clock_(options_.fixedStepSeconds),
          automation_(automationOptions_),
          mouse_(std::make_unique<RecenteringMouseCapture>())
    {
        builtIns_.quit = actions_.AddAction("Quit");
        actions_.Bind(builtIns_.quit, Input::Binding::ForKey(Keys::Escape));
        builtIns_.screenshot = actions_.AddAction("Screenshot");
        actions_.Bind(builtIns_.screenshot, Input::Binding::ForKey(Keys::F12));
        builtIns_.statistics = actions_.AddAction("Statistics");
        actions_.Bind(builtIns_.statistics, Input::Binding::ForKey(Keys::F3));
        builtIns_.fullScreen = actions_.AddAction("FullScreen");
        actions_.Bind(builtIns_.fullScreen, Input::Binding::ForKey(Keys::F11));

        // The device is created when Run() starts, before Initialize: the
        // window's settings must be in place by then.
        ApplyWindowOptions();
        camera_.SetPerspective(Microsoft::Xna::Framework::MathHelper::ToRadians(options_.fieldOfViewDegrees),
                               static_cast<float>(options_.width) / static_cast<float>(options_.height),
                               options_.nearPlane, options_.farPlane);
        // The fixed-step clock paces the simulation; XNA's own fixed step
        // would sleep between frames and repeat Update instead.
        setIsFixedTimeStepProperty(false);
        setIsMouseVisibleProperty(true);
    }

    EngineGame::~EngineGame() = default;

    void EngineGame::ParseCommandLine(const std::span<const std::string_view> arguments)
    {
        Graphics::AutomatedRunOptions automation;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            const std::string_view argument = arguments[i];
            if (Graphics::ParseAutomatedRunOption(arguments, i, automation)) {
                continue;
            }
            if (argument == "--settings") {
                const auto value = Value(arguments, i);
                if (value.empty()) {
                    throw std::invalid_argument("--settings needs a file");
                }
                preferencesPath_ = std::filesystem::path(value);
            } else if (argument == "--width") {
                options_.width = ParseInt(argument, Value(arguments, i), 64, 16384);
                widthGiven_ = true;
            } else if (argument == "--height") {
                options_.height = ParseInt(argument, Value(arguments, i), 64, 16384);
                heightGiven_ = true;
            } else if (argument == "--fullscreen" || argument == "--windowed") {
                options_.fullScreen = argument == "--fullscreen";
                fullScreenGiven_ = true;
            } else if (!ParseArgument(arguments, i)) {
                throw std::invalid_argument("unknown argument '" + std::string(argument) + "'");
            }
        }
        automationOptions_ = automation;
        automation_ = Graphics::AutomatedRun(automationOptions_);
        ApplyWindowOptions();
    }

    std::string EngineGame::Usage()
    {
        std::string usage(Graphics::AutomatedRunUsage());
        if (!usage.empty() && usage.back() != '\n') {
            usage += '\n';
        }
        usage += "  --settings FILE     preferences from FILE instead of the user's\n"
                 "  --width N, --height N\n"
                 "                      the window's size\n"
                 "  --fullscreen, --windowed\n"
                 "  --help              this text\n";
        return usage;
    }

    Graphics::ForwardRenderer& EngineGame::Renderer()
    {
        if (!renderer_) {
            throw std::logic_error("EngineGame::Renderer: the renderer is created in LoadContent");
        }
        return *renderer_;
    }

    void EngineGame::CaptureMouse(const bool captured)
    {
        mouse_->SetCaptured(*this, captured);
        mouseDelta_ = Microsoft::Xna::Framework::Vector2::Zero;
    }

    bool EngineGame::MouseCaptured() const noexcept
    {
        return mouse_->Captured();
    }

    void EngineGame::SetMouseCapture(std::unique_ptr<IMouseCapture> capture)
    {
        const bool captured = mouse_->Captured();
        mouse_->SetCaptured(*this, false);
        mouse_ = capture ? std::move(capture) : std::make_unique<RecenteringMouseCapture>();
        mouse_->SetCaptured(*this, captured);
    }

    void EngineGame::EnableShadowMap(const ShadowSettings settings, std::function<void(Graphics::ShadowMap&)> receiver)
    {
        if (!(settings.radius > 0.0f)) {
            throw std::invalid_argument("EngineGame::EnableShadowMap: the radius must be positive");
        }
        shadows_ = std::make_unique<Graphics::ShadowMap>(getGraphicsDeviceProperty(), settings.size);
        shadowSettings_ = settings;
        shadowReceiver_ = std::move(receiver);
    }

    Graphics::PostProcessChain& EngineGame::EnablePostProcessing()
    {
        if (!post_) {
            post_ = std::make_unique<Graphics::PostProcessChain>(getGraphicsDeviceProperty());
        }
        return *post_;
    }

    void EngineGame::DisableShadowMap() noexcept
    {
        shadows_.reset();
        shadowReceiver_ = {};
    }

    Input::InputSnapshot EngineGame::ReadInput()
    {
        return Input::InputSnapshot::Capture(getIsActiveProperty());
    }

    void EngineGame::OnQuit()
    {
        if (MouseCaptured()) {
            CaptureMouse(false);
            return;
        }
#ifndef __EMSCRIPTEN__
        // A page in a browser has nothing to quit to.
        Exit();
#endif
    }

    void EngineGame::OnScreenshot(const Graphics::CaptureResult& result)
    {
        if (!result.success) {
            std::fprintf(stderr, "screenshot failed: %s\n", result.error.c_str());
        } else if (!result.path.empty()) {
            std::printf("screenshot: %s\n", result.path.c_str());
        }
    }

    void EngineGame::ApplyWindowOptions()
    {
        graphics_.setPreferredBackBufferWidthProperty(options_.width);
        graphics_.setPreferredBackBufferHeightProperty(options_.height);
        graphics_.setIsFullScreenProperty(options_.fullScreen);
        graphics_.setSynchronizeWithVerticalRetraceProperty(options_.verticalSync);
        graphics_.setGraphicsProfileProperty(options_.hiDef ? Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef
                                                            : Microsoft::Xna::Framework::Graphics::GraphicsProfile::Reach);
    }

    void EngineGame::ApplyWindowPreferences()
    {
        // The command line wins over the file; the file over the game's defaults.
        if (!widthGiven_) {
            options_.width = preferences_.GetInt("window.width", options_.width, 64, 16384);
        }
        if (!heightGiven_) {
            options_.height = preferences_.GetInt("window.height", options_.height, 64, 16384);
        }
        if (!fullScreenGiven_) {
            options_.fullScreen = preferences_.GetBool("window.fullscreen", options_.fullScreen);
        }
        options_.verticalSync = preferences_.GetBool("window.vsync", options_.verticalSync);
    }

    void EngineGame::Initialize()
    {
        // Automated runs (captures, smoke tests) must not depend on the player's files.
        usePreferences_ = !automationOptions_.Automated();
        if (usePreferences_ && preferencesPath_.empty() && !options_.application.empty()) {
            preferencesPath_ = UserConfigPath(options_.application, "settings.ini");
        }
        if (usePreferences_ && !preferencesPath_.empty()) {
            preferences_ = SettingsFile::Load(preferencesPath_);
            for (const auto& problem : preferences_.Problems()) {
                std::fprintf(stderr, "%s:%d: %s\n", preferencesPath_.string().c_str(), problem.line,
                             problem.message.c_str());
            }
            const GameOptions before = options_;
            ApplyWindowPreferences();
            if (before.width != options_.width || before.height != options_.height ||
                before.fullScreen != options_.fullScreen || before.verticalSync != options_.verticalSync) {
                ApplyWindowOptions();
                graphics_.ApplyChanges();
            }
        }
        getWindowProperty().setTitleProperty(options_.title);
        Game::Initialize(); // creates the device, then calls LoadContent
    }

    void EngineGame::LoadContent()
    {
        renderer_ = std::make_unique<Graphics::ForwardRenderer>(getGraphicsDeviceProperty());
        if (!options_.assetDirectory.empty()) {
            if (const auto found = FindAssetDirectory(options_.assetDirectory)) {
                assetDirectory_ = *found;
                getContentProperty().setRootDirectoryProperty(assetDirectory_.string());
            }
        }
        LoadGame();

        // The bindings after LoadGame: the game's own actions exist now.
        if (usePreferences_ && !preferencesPath_.empty()) {
            const auto path = preferencesPath_.parent_path() / "bindings.ini";
            std::ifstream stream(path, std::ios::binary);
            if (stream) {
                const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
                for (const auto& warning : actions_.LoadBindings(text).warnings) {
                    std::fprintf(stderr, "%s: %s\n", path.string().c_str(), warning.c_str());
                }
            }
        }
    }

    bool EngineGame::SavePreferences(std::string* error)
    {
        if (!usePreferences_ || preferencesPath_.empty()) {
            return true;
        }
        try {
            preferences_.SetInt("window.width", graphics_.getPreferredBackBufferWidthProperty());
            preferences_.SetInt("window.height", graphics_.getPreferredBackBufferHeightProperty());
            preferences_.SetBool("window.fullscreen", graphics_.getIsFullScreenProperty());
            preferences_.SetBool("window.vsync", graphics_.getSynchronizeWithVerticalRetraceProperty());
            preferences_.Save(preferencesPath_);
            WriteAtomically(preferencesPath_.parent_path() / "bindings.ini", actions_.SaveBindings());
            return true;
        } catch (const std::exception& e) {
            if (error != nullptr) {
                *error = e.what();
            }
            return false;
        }
    }

    void EngineGame::Update(Microsoft::Xna::Framework::GameTime& gameTime)
    {
        input_ = ReadInput();
        actions_.Update(input_);
        mouseDelta_ = mouse_->Delta(*this, input_.mouse);

        if (actions_.Pressed(builtIns_.quit)) {
            OnQuit();
        }
        if (actions_.Pressed(builtIns_.screenshot)) {
            (void)screenshots_.RequestNext();
        }
        if (actions_.Pressed(builtIns_.statistics)) {
            statisticsVisible_ = !statisticsVisible_;
            statisticsAge_ = 1.0; // show (or restore the title) right away
        }
        if (actions_.Pressed(builtIns_.fullScreen)) {
            graphics_.ToggleFullScreen();
        }

        const double measured = gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty();
        if (const auto seconds = automation_.BeginUpdate(measured, clock_.StepSeconds())) {
            ++updates_;
            clock_.Run(*seconds, [this](const double step) { FixedUpdate(step); });
            UpdateFrame(*seconds);
        }

        statisticsAge_ += measured;
        if (statisticsAge_ >= 0.5) {
            statisticsAge_ = 0.0;
            ShowStatistics();
        }
        Game::Update(gameTime);
    }

    void EngineGame::ShowStatistics()
    {
        if (!statisticsVisible_) {
            if (getWindowProperty().getTitleProperty() != options_.title) {
                getWindowProperty().setTitleProperty(options_.title);
            }
            return;
        }
        const double average = frames_.AverageSeconds();
        char line[256];
        std::snprintf(line, sizeof line, " | %.1f fps (%.2f ms) | %llu draws | %llu triangles | %llu of %llu visible",
                      average > 0.0 ? 1.0 / average : 0.0, average * 1000.0,
                      static_cast<unsigned long long>(rendered_.drawCalls),
                      static_cast<unsigned long long>(rendered_.primitives),
                      static_cast<unsigned long long>(culled_.visible),
                      static_cast<unsigned long long>(culled_.considered));
        getWindowProperty().setTitleProperty(options_.title + line);
    }

    void EngineGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime)
    {
        auto& device = getGraphicsDeviceProperty();
        if (automation_.BeginDraw(device)) {
            Exit();
            return;
        }
        frames_.AddFrame(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());

        const auto& viewport = device.getViewportProperty();
        if (viewport.getHeightProperty() > 0) {
            const float aspect =
                static_cast<float>(viewport.getWidthProperty()) / static_cast<float>(viewport.getHeightProperty());
            if (camera_.Kind() == Graphics::ProjectionKind::Perspective && aspect != camera_.AspectRatio()) {
                camera_.SetAspectRatio(aspect);
            }
        }

        rendered_ = {};
        frame_.Clear();
        Submit(frame_);

        // The shadow map first: switching back from its target discards the back buffer.
        if (shadows_ && renderer_) {
            const auto& lighting = renderer_->Lighting();
            const Microsoft::Xna::Framework::Vector3 towardsLight = -lighting.lightDirection;
            shadows_->Fit(towardsLight,
                          camera_.Position() + camera_.Forward() * (shadowSettings_.radius * shadowSettings_.ahead),
                          shadowSettings_.radius);
            casters_.assign(frame_.Items().begin(), frame_.Items().end());
            for (const auto& entry : frame_.instanced_) {
                Graphics::RenderItem item;
                item.mesh = entry.mesh;
                item.material = entry.material;
                item.instances = entry.instances;
                casters_.push_back(item);
            }
            shadows_->Render(device, casters_, &rendered_);
            if (shadowReceiver_) {
                shadowReceiver_(*shadows_);
            }
        }

        if (post_) {
            post_->BeginScene(); // the world goes into the scene target
        }
        device.Clear(options_.clearColor);

        queue_.Clear();
        culled_ = {};
        Graphics::CullInto(frame_.Items(), camera_, visibility_, queue_, culled_);
        for (const auto& entry : frame_.instanced_) {
            entry.instances->Cull(camera_, visibility_, culled_);
            if (entry.instances->VisibleCount() == 0) {
                continue;
            }
            Graphics::RenderItem item;
            item.mesh = entry.mesh;
            item.material = entry.material;
            item.instances = entry.instances;
            bool first = true;
            for (const auto& world : entry.instances->VisibleWorlds()) {
                const auto bounds = Graphics::WorldBounds(*entry.mesh, world);
                item.bounds = first ? bounds : Microsoft::Xna::Framework::BoundingSphere::CreateMerged(item.bounds, bounds);
                first = false;
            }
            queue_.Submit(item);
        }
        queue_.Sort(camera_.Position());
        renderer_->Render(queue_, camera_, &rendered_);
        if (post_) {
            post_->EndScene();
        }

        DrawOverlay(device);
        for (const auto& shot : screenshots_.Process(device)) {
            OnScreenshot(shot);
        }
        automation_.EndDraw(ReadyForCapture());
        Game::Draw(gameTime);
    }

    namespace Detail
    {
        std::vector<std::string_view> Arguments(const int argc, char** argv)
        {
            std::vector<std::string_view> arguments;
            for (int i = 1; i < argc; ++i) {
                arguments.emplace_back(argv[i]);
            }
            return arguments;
        }

        bool WantsHelp(const std::span<const std::string_view> arguments)
        {
            for (const auto argument : arguments) {
                if (argument == "--help" || argument == "-h") {
                    return true;
                }
            }
            return false;
        }

        void PrintUsage(const std::string_view program)
        {
            std::fprintf(stderr, "usage: %.*s [options]\n%s", static_cast<int>(program.size()), program.data(),
                         EngineGame::Usage().c_str());
        }

        void PrintError(const std::string_view what)
        {
            std::fprintf(stderr, "error: %.*s\n", static_cast<int>(what.size()), what.data());
        }

        int ExitCodeAfterRun(const EngineGame& game)
        {
            const auto& options = game.Automation().Options();
            if (options.screenshotPath.empty()) {
                return 0;
            }
            const auto& shot = game.Automation().Screenshot();
            if (!shot || !shot->success) {
                PrintError("no screenshot: " + (shot ? shot->error : std::string("the run ended before it")));
                return 1;
            }
            std::printf("screenshot: %s (%dx%d)\n", shot->path.c_str(), shot->width, shot->height);
            return 0;
        }
    }
}
