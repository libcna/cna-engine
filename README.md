# cna-engine

A C++23 game-engine layer above [CNA](../cna) (the XNA 4.0-compatible runtime)
that makes ordinary game code shorter without hiding CNA. It reuses
[cna-extended](../cna-extended) for screens, ECS, World3D, particles, fonts and
tilemaps instead of duplicating them.

Status: **0.1.0, experimental**. Every API may change until the pilot games
have adopted it (see `plan.md` and `docs/api-status.md`); facilities already
checked against both pilots' own code are marked `pilot-validated`, and both
pilots' real scenes render through the engine practically identically
(`docs/pilots/`). Both pilots run on the engine (merged into their main branches on
2026-10-20); the facilities both use are `stable`.

## Modules

| Target | Profile | What it offers |
| --- | --- | --- |
| `CnaEngine::Core` | strict | named input actions and axes, rebinding, binding capture and display names, fixed-step clock, frame statistics, CPU profiler, settings files, save slots; static collision world, character controller; skeletons, clips and an animator |
| `CnaEngine::Graphics` | strict | camera state and fly/walk/orbit/follow controllers, CPU mesh data and builders, vertex layouts (skinned included), RAII GPU meshes, materials, render queues, culling and LOD, instancing, skinned items, a forward renderer over XNA's stock effects, models as render items, sky dome and time of day, a sun shadow map, local lights and light pools, post-processing, render statistics and GPU memory estimates, screenshots, typed asset registry, quality schemas |
| `CnaEngine::App` | strict | `EngineGame`: an XNA Game with the engine wired (input, fixed steps, camera, renderer, statistics, screenshots, automated runs, preferences, asset directory, mouse capture, shadows, post-processing) |
| `CnaEngine::Audio` | strict | positional voices, mix categories, a voice budget, sound banks; silent without a device |
| `CnaEngine::UI` | strict | a built-in font, a HUD painter on a virtual screen, toasts, menus and a menu stack, settings and rebinding rows |
| `CnaEngine::EnhancedGraphics` | enhanced | PBR material binder over CNA's `PbrEffect` and `SkinnedPbrEffect` (image-based lighting, exposure, reflection probes); glTF model materials and skins; shadow reception; punctual lights per item; the browser's pointer lock (no cna-extended) |
| `CnaEngine::ExtendedIntegration` | enhanced | adapters to cna-extended (`EngineWorldScreen`, World3D camera sync, 3D particles as billboards) |
| `CnaEngine::Engine` | enhanced | everything above |

*Strict* code uses only XNA 4.0 types of CNA, Sharp Runtime and the standard
library, so strict-XNA games (car-simulator) can use it; the rule is enforced
by `tools/check_strict_boundary.py` (ADR-001, `docs/adr/`).

## Start a game

```sh
git clone https://github.com/libcna/cna-engine.git
cna-engine/tools/fetch_dependencies.sh            # CNA, Sharp Runtime, easy-gl, meta-gl beside it
cna-engine/tools/new_game.sh my-game "My Game"    # a first-person game to grow from
cd my-game && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel 8
./build/my-game                                   # tools/build_web.sh --serve: the same in a browser
```

`docs/tutorial.md` goes on from there: the game class, drawing, input,
walking and collisions, the sky, shadows and lamps, glTF models and
animation, sound, HUD and menus, post-processing, saving, particles.
`examples/first_person`, `examples/third_person` and `examples/day_night`
are complete games that run headless as tests.

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
materials, culling, an instanced item, the forward renderer, a rear-view
mirror rendered into a target and composited with `SpriteBatch`, diagnostics
and a screenshot) on the strict profile. `examples/orbit_scene/main.cpp` (strict,
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
| `CNA_ENGINE_ENFORCE_BASELINE` | `OFF` | fail configuration unless CNA, Sharp Runtime and cna-extended are exactly the pinned revisions (CI) |
| `CNA_ENGINE_REQUIRE_BASELINE_DESCENDANT` | `OFF` | fail configuration when a dependency checkout does not contain its pinned revision (older or diverged) |
| `CNA_ENGINE_PILOT_SUITES` | `OFF` | also build and run car-simulator's and street-simulator's own test suites (read-only) against this build's dependencies — ADR-002's consumer checks; slow |
| `CNA_ENGINE_SANITIZE` | empty | `-fsanitize=` list (e.g. `address,undefined`) for the engine's own targets only; dependencies stay uninstrumented, so a sanitizer tree reuses their objects from ccache |
| `CNA_ENGINE_BUILD_TESTS` / `_EXAMPLES` | top level | tests and examples |
| `CNA_ENGINE_CNA_RENDERER` / `_PLATFORM` / `_AUDIO_PLATFORM` | `SOFTWARE` / `HEADLESS` / `NULL` | CNA axes for a standalone build |

The default configuration needs no window, GPU or audio device; rendering
tests read back pixels from CNA's software renderer.

## Using an installed engine

`cmake --install build --prefix <dir>` installs headers, libraries and
`CnaEngineConfig.cmake`. CNA installs a relocatable C++ SDK since
`58ebc46fd` (`CNA_INSTALL_CXX`, UP-001), and cna-extended installs a
`CnaExtended` package since 0.2.0. Put all three installation prefixes on
`CMAKE_PREFIX_PATH`; the engine package finds its dependencies, so the
consumer only needs:

```cmake
find_package(CnaEngine 0.1 REQUIRED COMPONENTS Core Graphics)
target_link_libraries(my_game PRIVATE CnaEngine::Core CnaEngine::Graphics)
```

`tools/run_consumer_probes.sh` builds and tests this installed-only route.

## Documents

- `docs/tutorial.md` — a 3D game from an empty directory
- `plan.md` — task plan and status; `handoff.md` — context for the next agent
- `docs/adr/` — architecture decisions (boundary, baseline, material/binder contract)
- `docs/guide-strict.md`, `docs/guide-enhanced.md` — how to use each profile
- `docs/conventions.md` — API, error, ownership and stability rules
- `docs/verification-matrix.md` — what is checked where
- `docs/pilots/` — equivalence probes against the two pilot games; `docs/pilots/migration.md` — how each pilot adopts the engine, step by step, with the adoption branches' evidence
- `CHANGELOG.md`, `NOTICE.md`, `LICENSE`
- `docs/api-status.md` — status of every public facility
- `docs/verification.md` — what was built and tested, with which revisions
- `upstream-issues.md` — defects found in sibling repositories (recorded, not fixed here)
- `analysis.md`, `gap-audit.md` — the feasibility study and audit that led here
- `docs/game-facilities-survey.md` — how the simulators did what Phase 7 made into facilities
