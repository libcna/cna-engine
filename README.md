# cna-engine

A C++23 game-engine layer above [CNA](../cna) (the XNA 4.0-compatible runtime)
that makes ordinary game code shorter without hiding CNA. It reuses
[cna-extended](../cna-extended) for screens, ECS, World3D, particles, fonts and
tilemaps instead of duplicating them.

Status: **0.1.0, experimental**. Every API may change until the pilot games
have adopted it (see `plan.md` and `docs/api-status.md`); facilities already
checked against both pilots' own code are marked `pilot-validated`, and both
pilots' real scenes render through the engine practically identically
(`docs/pilots/`).

## Modules

| Target | Profile | What it offers |
| --- | --- | --- |
| `CnaEngine::Core` | strict | named input actions and axes, rebinding, binding capture and display names, fixed-step clock, frame statistics, CPU profiler |
| `CnaEngine::Graphics` | strict | camera state and fly/walk controllers, CPU mesh data and builders, vertex layouts (skinned included), RAII GPU meshes, materials, render queues, culling and LOD, instancing, skinned items, a forward renderer over XNA's stock effects, render statistics and GPU memory estimates, screenshots, typed asset registry, quality schemas |
| `CnaEngine::EnhancedGraphics` | enhanced | PBR material binder over CNA's `PbrEffect` and `SkinnedPbrEffect`: image-based lighting, exposure, per-item reflection probes (no cna-extended) |
| `CnaEngine::ExtendedIntegration` | enhanced | adapters to cna-extended (`EngineWorldScreen`, World3D camera sync) |
| `CnaEngine::Engine` | enhanced | everything above |

*Strict* code uses only XNA 4.0 types of CNA, Sharp Runtime and the standard
library, so strict-XNA games (car-simulator) can use it; the rule is enforced
by `tools/check_strict_boundary.py` (ADR-001, `docs/adr/`).

## Quick look

```cpp
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Input/ActionMap.hpp"

using namespace CnaEngine;
using Microsoft::Xna::Framework::Input::Keys;

// Input: actions and axes are named; bindings are data.
Input::ActionMap actions;
Graphics::CameraInputAxes axes;
axes.moveForward = actions.AddAxis("MoveForward");
actions.BindAxis(axes.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
// ... other axes; actions.LoadBindings(settingsText) applies user overrides.

// Geometry: build once on the CPU, upload in the layout the effect needs.
Graphics::MeshBuilder builder;
builder.AddSphere(Vector3::Zero, 1.0f, 24, 16);
Graphics::GpuMesh sphere = Graphics::GpuMesh::Upload(device, builder.Mesh(),
                                                     Graphics::VertexLayout::PositionNormalTexture());

// Per update:
actions.Update(Input::InputSnapshot::Capture(IsActive()));
controller.Update(camera, Graphics::ReadCameraInput(actions, axes), elapsedSeconds);

// Per draw (any XNA effect):
effect.setViewProperty(camera.View());
effect.setProjectionProperty(camera.Projection());
pass->Apply();
sphere.Draw(device, &frameStats);
```

Start with `examples/quick_start/main.cpp`: a small complete game written
as an ordinary XNA `Game` (actions and axes, a fixed-step simulation,
materials, culling, an instanced item, the forward renderer, diagnostics and
a screenshot) on the strict profile. `examples/orbit_scene/main.cpp` (strict,
engine meshes with your own effect) and `examples/street_slice/main.cpp`
(enhanced profile: PBR, culling, LOD, instancing, diagnostics) complete the
set. All three are runnable headless programs and run as tests.

## Building

The engine consumes CNA, Sharp Runtime and (optionally) cna-extended as source
trees next to it (`../cna`, `../sharp-runtime`, `../cna-extended`); see
ADR-001 for why. The pinned revisions are in `cmake/CnaEngineBaseline.cmake`
(ADR-002).

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv   # this workspace's shared cache
export CCACHE_NOHASHDIR=1   # share Debug objects between build trees (upstream-issues.md UP-003)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build --parallel 3
ctest --test-dir build --parallel 3
```

Options:

| Option | Default | Meaning |
| --- | --- | --- |
| `CNA_ENGINE_WITH_EXTENDED` | `ON` | build the cna-extended integration |
| `CNA_ENGINE_WITH_ENHANCED_GRAPHICS` | `ON` | build the CNA-extension (PBR) graphics target |
| `CNA_ENGINE_PILOT_PROBES` | `ON` | build probes comparing engine code with the pilots (read-only) |
| `CNA_ENGINE_SANITIZE` | empty | `-fsanitize=` list (e.g. `address,undefined`) for the engine's own targets only; dependencies stay uninstrumented, so a sanitizer tree reuses their objects from ccache |
| `CNA_ENGINE_BUILD_TESTS` / `_EXAMPLES` | top level | tests and examples |
| `CNA_ENGINE_ENFORCE_BASELINE` | `OFF` | fail configuration off the pinned revisions |
| `CNA_ENGINE_CNA_RENDERER` / `_PLATFORM` / `_AUDIO_PLATFORM` | `SOFTWARE` / `HEADLESS` / `NULL` | CNA axes for a standalone build |

The default configuration needs no window, GPU or audio device; rendering
tests read back pixels from CNA's software renderer.

## Using an installed engine

`cmake --install build --prefix <dir>` installs headers, libraries and
`CnaEngineConfig.cmake`. CNA itself installs no package (`upstream-issues.md`
UP-001), so a consumer adds CNA (and cna-extended) as source trees first and
then calls `find_package(CnaEngine 0.1 REQUIRED COMPONENTS Core Graphics)`.

## Documents

- `plan.md` — task plan and status; `handoff.md` — context for the next agent
- `docs/adr/` — architecture decisions (boundary, baseline)
- `docs/guide-strict.md`, `docs/guide-enhanced.md` — how to use each profile
- `docs/conventions.md` — API, error, ownership and stability rules
- `docs/verification-matrix.md` — what is checked where
- `docs/pilots/` — equivalence probes against the two pilot games; `docs/pilots/migration.md` — how each pilot would adopt the engine
- `CHANGELOG.md`, `NOTICE.md`, `LICENSE`
- `docs/api-status.md` — status of every public facility
- `docs/verification.md` — what was built and tested, with which revisions
- `upstream-issues.md` — defects found in sibling repositories (recorded, not fixed here)
- `analysis.md`, `gap-audit.md` — the feasibility study and audit that led here
