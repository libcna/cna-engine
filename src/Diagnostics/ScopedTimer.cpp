// SPDX-License-Identifier: MIT
#include "CnaEngine/Diagnostics/ScopedTimer.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Diagnostics
{
    CpuProfiler::CpuProfiler()
        : CpuProfiler([] {
              return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                   std::chrono::steady_clock::now().time_since_epoch())
                                                   .count());
          })
    {
    }

    CpuProfiler::CpuProfiler(Clock clock) : clock_(std::move(clock))
    {
        if (!clock_) {
            throw std::invalid_argument("CpuProfiler: clock must be callable");
        }
    }

    void CpuProfiler::Record(const std::string_view name, const std::int64_t nanoseconds)
    {
        for (auto& section : current_) {
            if (section.name == name) {
                section.nanoseconds += nanoseconds;
                ++section.calls;
                return;
            }
        }
        current_.push_back(SectionTiming{std::string(name), nanoseconds, 1});
    }

    void CpuProfiler::EndFrame()
    {
        last_ = std::move(current_);
        current_.clear();
    }
}
