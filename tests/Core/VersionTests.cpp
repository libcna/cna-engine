// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/Version.hpp"

#include <gtest/gtest.h>

#include <string>

namespace CnaEngine
{
    TEST(VersionTests, LibraryMatchesHeaders)
    {
        const std::string expected = std::to_string(VersionMajor) + "." + std::to_string(VersionMinor) + "."
                                     + std::to_string(VersionPatch);
        EXPECT_EQ(LibraryVersion(), expected);
    }
}
