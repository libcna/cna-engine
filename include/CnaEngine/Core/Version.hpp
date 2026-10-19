// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace CnaEngine
{
    /// Compile-time version of the cna-engine headers in use (ADR-001 versioning).
    /// Every public API is experimental while the major version is 0.
    inline constexpr int VersionMajor = 0;
    inline constexpr int VersionMinor = 1;
    inline constexpr int VersionPatch = 0;

    /// "major.minor.patch" of the compiled library. Comparing it with the header
    /// constants detects a consumer built against different headers.
    [[nodiscard]] std::string_view LibraryVersion() noexcept;
}
