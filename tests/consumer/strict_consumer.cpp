// SPDX-License-Identifier: MIT
//
// Strict-profile consumer: XNA 4.0 types, Sharp Runtime, std and strict
// cna-engine headers only. tools/check_strict_boundary.py also scans this file.
#include "CnaEngine/Core/Version.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Input/ActionMap.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <cstdio>
#include <string>

int main()
{
    using namespace CnaEngine;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    Input::ActionMap actions;
    const Input::ActionId fire = actions.AddAction("Fire");
    actions.Bind(fire, Input::Binding::ForKey(Keys::Space));
    Input::InputSnapshot snapshot;
    snapshot.keyboard = KeyboardState({Keys::Space});
    actions.Update(snapshot);

    Graphics::Camera camera;
    camera.SetPosition(Vector3(0.0f, 0.0f, 5.0f));
    camera.LookAt(Vector3::Zero);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice device;
    Graphics::MeshBuilder builder;
    builder.AddBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
    const Graphics::GpuMesh mesh =
        Graphics::GpuMesh::Upload(device, builder.Mesh(), Graphics::VertexLayout::PositionNormalTexture());

    const bool ok = actions.Pressed(fire) && mesh.IsDrawable() && mesh.PrimitiveCount() == 12;
    std::printf("strict consumer: cna-engine %s, %s\n", std::string(LibraryVersion()).c_str(), ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
