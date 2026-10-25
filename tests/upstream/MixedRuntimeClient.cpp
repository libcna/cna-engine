// SPDX-License-Identifier: MIT
//
// UP-006 regression: an engine library consumes CNA's public module targets,
// while the application also links CNA's umbrella target. With a shared CNA
// both paths must use the same libcna.so runtime.
#include "CnaEngine/Graphics/Camera.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsAdapter.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

int CnaEngineMixedRuntimeClient()
{
    using namespace Microsoft::Xna::Framework;

    CnaEngine::Graphics::Camera camera;
    camera.SetPosition(Vector3(0.0f, 0.0f, 5.0f));
    camera.LookAt(Vector3::Zero);

    // Retain CNA translation units with process-lifetime singleton state. A
    // second static copy used to crash while those objects were destroyed.
    auto* volatile adapters = &Graphics::GraphicsAdapter::getAdaptersProperty;
    const Input::KeyboardState keyboard;
    const bool cameraOk = camera.Position() == Vector3(0.0f, 0.0f, 5.0f)
                          && Vector3::Distance(camera.Position(), Vector3::Zero) == 5.0f;
    return adapters != nullptr && cameraOk && !keyboard.IsKeyDown(Input::Keys::Space)
                   && !Graphics::SamplerState::PointClamp.getIsDisposedProperty()
               ? 0
               : 1;
}
