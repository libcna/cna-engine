// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001): this header depends on cna-extended and must
// never be included from CnaEngine::Core or CnaEngine::Graphics.
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"

#include "CNA/Extended/World3DEXT/Camera3DEXT.hpp"

namespace CnaEngine::Extended
{
    /// Copies an engine camera into cna-extended's World3D camera, so World3D
    /// render systems draw from the engine camera's point of view.
    ///
    /// Camera3DEXT has no orthographic mode; for an orthographic engine camera
    /// only the pose, aspect ratio and depth range are copied and the function
    /// returns false so the caller can decide how to render.
    bool SyncToWorldCamera(const Graphics::Camera& camera, CNA::Extended::World3DEXT::Camera3DEXT& target);

    /// Reads pose and perspective parameters back from a World3D camera.
    void SyncFromWorldCamera(const CNA::Extended::World3DEXT::Camera3DEXT& source, Graphics::Camera& camera);
}
