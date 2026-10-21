// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/AutomatedRun.hpp"

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace CnaEngine::Graphics
{
    namespace
    {
        template <typename Number>
        Number ParseNumber(const std::string_view option, const std::string_view text)
        {
            Number value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(static_cast<double>(value))
                || value < Number{}) {
                throw std::invalid_argument(std::string(option) + " needs a number of at least 0, not '"
                                            + std::string(text) + "'");
            }
            return value;
        }
    }

    bool ParseAutomatedRunOption(const std::span<const std::string_view> args, std::size_t& index,
                                 AutomatedRunOptions& options)
    {
        const std::string_view option = args[index];
        const auto value = [&]() {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument(std::string(option) + " needs a value");
            }
            return args[++index];
        };
        if (option == "--frames") {
            options.frames = ParseNumber<int>(option, value());
            return true;
        }
        if (option == "--run-seconds") {
            options.runSeconds = ParseNumber<double>(option, value());
            return true;
        }
        if (option == "--screenshot") {
            const std::string_view path = value();
            if (path.empty()) {
                throw std::invalid_argument("--screenshot needs a file name");
            }
            options.screenshotPath = std::string(path);
            return true;
        }
        if (option == "--lockstep") {
            options.lockstep = true;
            return true;
        }
        return false;
    }

    std::string_view AutomatedRunUsage() noexcept
    {
        return "  --frames N          stop after N drawn frames\n"
               "  --run-seconds S     stop after S simulated seconds\n"
               "  --screenshot FILE   save the last frame as a PNG before stopping\n"
               "  --lockstep          one fixed simulation step per drawn frame (repeatable captures)\n";
    }

    AutomatedRun::AutomatedRun(AutomatedRunOptions options) : options_(std::move(options))
    {
    }

    std::optional<double> AutomatedRun::BeginUpdate(const double measuredSeconds, const double fixedStepSeconds)
    {
        if (limitReached_ || (options_.lockstep && updatesSinceDraw_ > 0)) {
            return std::nullopt;
        }
        ++updatesSinceDraw_;
        const double step = options_.lockstep ? fixedStepSeconds : measuredSeconds;
        simulatedSeconds_ += step;
        return step;
    }

    bool AutomatedRun::BeginDraw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device)
    {
        updatesSinceDraw_ = 0;
        if (!finished_) {
            return false;
        }
        if (!options_.screenshotPath.empty() && !screenshot_) {
            CaptureOptions capture;
            capture.path = options_.screenshotPath;
            screenshot_ = CaptureBackBuffer(device, capture);
        }
        return true;
    }

    void AutomatedRun::EndDraw(const bool ready)
    {
        ++framesDrawn_;
        if (!limitReached_) {
            limitReached_ = (options_.frames > 0 && framesDrawn_ >= options_.frames)
                            || (options_.runSeconds > 0.0 && simulatedSeconds_ >= options_.runSeconds);
        }
        if (limitReached_ && ready) {
            finished_ = true;
        }
    }
}
