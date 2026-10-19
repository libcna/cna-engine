// SPDX-License-Identifier: MIT
//
// Deliberate strict-boundary violations. The test `cna_engine.strict_boundary_detects_leaks`
// runs tools/check_strict_boundary.py on this file and requires it to FAIL;
// it proves the checker still catches each class of leak. Never include this file.
#pragma once

#include "CNA/Extended/World3DEXT/Camera3DEXT.hpp"
#include "CnaEngine/Extended/WorldCameraSync.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTangentTexture.hpp"

namespace LeakyHeader
{
    using LeakedCamera = CNA::Extended::World3DEXT::Camera3DEXT;
    inline bool LeakedQuery() { return SupportsCapability(); }
}
