// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::Diagnostics
{
    /// Accumulated CPU time of one named section during one frame.
    struct SectionTiming
    {
        std::string name;
        std::int64_t nanoseconds = 0;
        std::uint32_t calls = 0;
    };

    /// Per-frame CPU section timings.
    ///
    /// Disabled (the default), a ScopedTimer costs one branch: it reads no
    /// clock and records nothing. Enabled, sections are accumulated by name
    /// for the current frame; EndFrame() publishes them as LastFrame().
    /// Not thread-safe: use one profiler per thread.
    class CpuProfiler
    {
    public:
        /// Monotonic time source in nanoseconds; tests inject a fake clock.
        using Clock = std::function<std::int64_t()>;

        CpuProfiler();
        explicit CpuProfiler(Clock clock);

        void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }
        [[nodiscard]] bool Enabled() const noexcept { return enabled_; }

        /// Publishes the sections recorded since the previous EndFrame().
        void EndFrame();
        /// Sections of the last completed frame, in first-use order.
        [[nodiscard]] const std::vector<SectionTiming>& LastFrame() const noexcept { return last_; }
        [[nodiscard]] std::int64_t Now() const { return clock_(); }

        /// Adds `nanoseconds` to section `name` of the current frame.
        void Record(std::string_view name, std::int64_t nanoseconds);

    private:
        Clock clock_;
        bool enabled_ = false;
        std::vector<SectionTiming> current_;
        std::vector<SectionTiming> last_;
    };

    /// Times the enclosing scope into a CpuProfiler section.
    /// `name` must outlive the timer (string literals are typical).
    class ScopedTimer
    {
    public:
        ScopedTimer(CpuProfiler* profiler, std::string_view name)
            : profiler_(profiler != nullptr && profiler->Enabled() ? profiler : nullptr), name_(name)
        {
            if (profiler_ != nullptr) {
                start_ = profiler_->Now();
            }
        }
        ~ScopedTimer()
        {
            if (profiler_ != nullptr) {
                profiler_->Record(name_, profiler_->Now() - start_);
            }
        }
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

    private:
        CpuProfiler* profiler_;
        std::string_view name_;
        std::int64_t start_ = 0;
    };
}
