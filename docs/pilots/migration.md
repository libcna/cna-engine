# Migrating the pilots to cna-engine

Status: **both pilots run on the engine.** The owner allowed adoption on
2026-10-20 on new branches of both pilots (one revertible commit per step);
the same day the branches were pushed and, at the owner's request, merged
fast-forward into car-simulator's `main` and street-simulator's `develop`
(each also gained a commit naming cna-engine as a sibling checkout, and
car-simulator's CI checks it out). Each pilot's branch
`engine-adoption` carries its steps
below except the camera, which both keep (see "car-simulator adoption" and
"street-simulator adoption"). This document is the migration plan the read-only
probes support; it covers PILOT-003's requirement that migration notes state
reversibility and known differences.

## Principles

- **One facility per step**, each a separate commit in the pilot. Facilities
  do not depend on each other beyond the header graph (an `ActionMap` needs no
  renderer), so steps can be taken in any order and reverted alone.
- **Evidence first.** Every step below names the engine probe that already
  compares the engine with the pilot's own code. Before the pilot's code is
  deleted, the probe and the pilot's own tests run green against the commit.
- **Reversal is `git revert` of that step's commit.** No step changes data
  formats on disk except where noted (binding files), and none needs a flag:
  where the probe shows identical behaviour there is nothing to switch between.
- **Linking.** car-simulator links `CnaEngine::Core` and `CnaEngine::Graphics`
  only and adds the ADR-001 allowance (`CnaEngine/<module>/` includes) to its
  `scripts/check_xna_only.py`; the pilot's checker already passes the strict
  engine surface with that allowance. street-simulator additionally links
  `CnaEngine::EnhancedGraphics`, and must link CNA's static module targets
  rather than the `CNA` umbrella (two CNA runtimes otherwise, UP-006).

## car-simulator (strict)

| Step | Pilot code replaced | Engine facility | Evidence | Known differences |
| --- | --- | --- | --- | --- |
| 1 | `src/Input/InputMapper.cpp` (tables, overrides, pad snapshot) | `Input::ActionMap`, `InputNames`, `BindingCapture` | `CarSimulatorControlsTests`: `Held` and `DriverControls` identical over 6,000 random frames | `Pressed` is action-level (a second key does not re-press a held action); analog-derived holds report presses; display names stay in the game; binding files use the engine's text format (keep the pilot's reader for one release to migrate saved overrides) |
| 2 | step loop in `Sim/Vehicle::Update` | `FixedStepClock` | `CarSimulatorClockProbe` | double instead of float accumulation: with jittery frame times 12 of 36,000 frames take a step one frame earlier or later; fixed rates and `--lockstep` captures identical |
| 3 | `src/Render/Camera.cpp` (camera state; chase/cockpit policy stays) | `Graphics::Camera` | `CarSimulatorRenderTests` (projection < 1e-6, view to float precision) | not taken in the adoption: the engine camera's view differs by float rounding, which would move every capture; car-simulator keeps `CameraPose` and hands its matrices to the engine (`Camera::SetMatrices`) |
| 4 | `src/Render/MeshData.cpp`, `GpuMesh.cpp` | `MeshData`, `MeshBuilder`, `VertexLayout`, `GpuMesh` | pixel-identical uploads | signed-volume sign convention; the conversion report names dropped attributes |
| 5 | `src/Render/Screenshot.cpp` | `ScreenCapture`, `ScreenshotQueue` | identical PNG pixels | failures are values, not stderr |
| 6 | texture caches (e.g. `TrafficRenderer::PlateTexture`) | `AssetRegistry` (`SetNormalizeKeys(false)`) | `CarSimulatorAssetsTests` | none observed |
| 7 | `src/Render/QualityTier.cpp` | `QualitySchema` | `CarSimulatorQualityTests` (every tier, Reach and HiDef) | none observed |
| 8 | world passes of `WorldRenderer::Draw` (terrain, roads and markings, objects, trees) | `RenderItem`, `Visibility`, `LodSelector`, `RenderQueue`, `ForwardRenderer` with the stock binder; `ForwardLighting` from `LightingRig`; materials with samplers and depth bias | `carsim_scene_comparison` on `lipova`: every visibility count equal, image 0.1/255 | the engine culls with the frustum plane test; with CNA's GJK `Intersects` (CNA `5ab1c7729`) car-simulator draws 3 object and 1 tree batch more, all off screen; signs, lamp light, the headlight fill (nested renderers in a custom pass) and the mirrors (`Camera::SetMirrored`) are covered by the comparison; rain and snow stay game passes |

Stays in car-simulator: vehicle physics, traffic, map loading, procedural
content generators, the lighting rig's values, HUD and instrument cluster.

### car-simulator adoption (branch `engine-adoption`)

Worktree `libcnalab/car-simulator-engine`, branched from car-simulator's
`main` at `cd6d948`; pushed to `origin/engine-adoption`. Each commit was checked with car-simulator's
own suite (7/7 CTest programs, Release, software renderer), its unmodified
XNA-only checker (with the ADR-001 allowance added in the first commit) and a
deterministic headless capture against the commit before it (`--frames 3
--lockstep --time 10:30 --time-scale 0 --weather clear --no-save
--no-audio`).

| Commit | Step | Result |
| --- | --- | --- |
| `f816606` | build: `add_subdirectory` of cna-engine (strict options), CNA's static modules (UP-006), checker allowance | suite and checker green |
| `06444c5` | 1: `InputMapper` on `ActionMap` (held and pressed actions, pad snapshot) | suite green |
| `6ee2d10` | 2: `Vehicle` physics steps from `FixedStepClock` | suite green; `--lockstep` captures identical |
| `a90c3c6` | 5: screenshots through `CaptureBackBuffer` | identical PNG pixels |
| `8a9818c` | 6: plate textures in an `AssetRegistry` | capture pixel-identical |
| `6a927fb` | 7: quality tiers as a `QualitySchema` | capture pixel-identical |
| `c9f9937` | 4 (partial): car-simulator's `GpuMesh` exposes its buffers as a borrowed engine `GpuMesh` | additive |
| `dc83bb4` | 8: the world passes (terrain with LOD, roads and markings, objects, trees, signs, headlight fill, lamp light) as render items through `ForwardRenderer` and the stock binder; rain and snow stay game passes drawn between the engine's segments | captures by day, at night with headlights, in rain, snow and fog, from the cockpit with mirrors and at the low tier: largest difference 2/255, no pixel differs visibly (> 3/255) — inside the agreed tolerance |

The same seven captures on OpenGL ES 3 (Mesa llvmpipe under Xvfb, build
`build-probe/` in the worktree, through car-simulator's
`scripts/run_headless.sh`), world-renderer files of `6a927fb` against
`dc83bb4`: largest difference 16/255, at most one pixel per image differs
by more than 3/255 — inside the tolerance. On the machine's hardware GPU (AMD
Radeon 780M, radeonsi, reached headless through SDL's offscreen video driver
and EGL): largest difference 12/255, at most one pixel per image visibly
different.

On the hardware GPU the world pass is not slower either (best of three,
1280 × 720: 6.73 ms before, 4.71 ms after; `performance-budgets.md`).

CPU time of the world pass (car-simulator's `--benchmark`, Release, software
renderer, 320 × 180, day, best of three alternating runs at load average ~8):
1,313 ms before `dc83bb4`, 1,358 ms after (+3.4 %, inside the proposed 1.15 ×
budget of `performance-budgets.md`; the software rasterizer dominates). Draw
calls 1,285 / 1,286 and visible terrain chunks 203 / 204: the engine's plane
test keeps one chunk at a frustum corner that XNA's GJK `Intersects` drops.

Step 4 is partial on purpose: car-simulator's mesh data and upload code stay
(its vertex layouts and shared index buffers are the game's), and the engine
draws from the same buffers. The world renderer's frame is several engine
segments rather than one: the weather layers lie on the terrain, the roads
and the roofs with a depth bias, so each must be drawn after its surface and
before the next one, as before.

## street-simulator (enhanced)

| Step | Pilot code replaced | Engine facility | Evidence | Known differences |
| --- | --- | --- | --- | --- |
| 1 | `src/Render/Camera.cpp`, `CameraController.cpp` | `Graphics::Camera`, `FlyCameraController` | `StreetSimulatorTests`: fly trajectory within 1e-3 | not taken in the adoption: Street's camera has its own yaw convention and the engine's would move every capture by rounding; Street hands its matrices to the engine (`Camera::SetMatrices`) |
| 2 | `src/Geometry/MeshBuilder.cpp`, `src/Render/GpuMesh.cpp` (tangent frames) | `MeshBuilder`, `MeshData::ComputeTangents`, `GpuMesh` (`Borrow` for model parts) | pixel-identical uploads | none observed |
| 3 | `src/Render/InstancedMesh.cpp` | `InstanceSet`, instanced `RenderItem` | pixel-identical hardware path | the per-instance fallback is used on Reach |
| 4 | presets in `src/Render/RenderSettings.cpp` | `QualitySchema` (26 settings) | `StreetQualityTests` | Reach limits are reported as decisions |
| 5 | caches in `MaterialLibrary` / `ModelLibrary` | `AssetRegistry` | `StreetAssetsTests` (derived-material cache) | none observed |
| 6 | `SceneRenderer` culling, `drawOpaque`, `drawTransparent`, `drawSkinned`, `applyMaterial`, `applyLighting`, `applyEnvironment` | `Visibility`, `LodSelector`, `RenderQueue`, `ForwardRenderer`, `PbrMaterialBinder` (exposure, sky environment, `EnvironmentSelector` for probes), skinned render items; Street's sky as a `BeforeOpaque` pass; statistics through `FormatDiagnostics` | `street_scene_comparison`: visibility, figures and draw calls identical; image 0.01/255 (53 px); with probes 75 px; pedestrian close-up 2 of 10,412 figure pixels | the engine draws blended surfaces with depth reads only (Street writes depth: 49 of the 53 px); order of coplanar ground meshes can differ under tiny view-matrix differences; Street's per-family counters (vehicles, drivers, characters) become the game's own counts around the engine's |

### street-simulator adoption (branch `engine-adoption`)

Worktree `libcnalab/street-simulator-engine`, branched from Street's
`develop` at `86b4a2e`; pushed to `origin/engine-adoption`. Build `build/` there: Release, CNA's
software renderer, headless platform, null audio (the software renderer
runs no custom effects, so Street's atmospheric sky is black in every
capture, before and after). Each commit was checked with Street's own suite
(15/15 CTest programs). Step 1 (camera) is not taken, as above.

| Commit | Step | Result |
| --- | --- | --- |
| `75ee3bf` | build: `add_subdirectory` of cna-engine (strict and enhanced graphics, no cna-extended), CNA's static modules (UP-006) | suite green; captures deterministic run to run |
| `64d1c07` | 2 (partial): `GpuMesh` and `SkinnedGpuMesh` expose their buffers as borrowed engine meshes | additive |
| `b137025` | 6 (with 3): `SceneRenderer` culls with the engine's rules and draws the frame through `ForwardRenderer` and the `PbrMaterialBinder` — items, movers, instance groups as `InstanceSet`s, skinned figures, Street's lighting, exposure, sky environment and per-item probes, the sky as a `BeforeOpaque` pass | all 18 named viewpoints and a night view (640 × 360) inside the agreed tolerance: mean ≤ 0.066/255, at most 0.47 % of pixels visibly different, around glass |
| `fe36c1d` | 4: the presets as a `QualitySchema`, applied as overlays through `QualitySchema::PresetValues` | settings dumped after every preset and after preset/flag combinations byte-identical |
| `513dd93` | 5: `MaterialLibrary`'s derived and added materials cached in an `AssetRegistry` (`GetOrCreate`, first definition wins; a material whose content texture is missing still is not cached) | capture pixel-identical, start-up log identical |

The same comparison on OpenGL 3.3 (Street's default renderer; Mesa
llvmpipe under Xvfb, build `build-probe/` in the worktree), where the
atmospheric sky draws through the engine's custom pass: all 19 images inside
the tolerance as well (mean ≤ 0.138/255, at most 0.47 % of pixels visibly
different), and on the machine's hardware GPU (AMD Radeon 780M, radeonsi,
SDL's offscreen video driver): mean ≤ 0.133/255, at most 0.48 % of pixels
visibly different.

Benchmarks (Street's `--benchmark`, 320 × 180, no probes, Release, best of
two alternating runs at load average ~22): draw calls identical (baseline
1,571, crowd 1,294); best CPU frame 2,068 → 1,924 ms (baseline) and 1,547 →
1,486 ms (crowd). The engine's instanced-draw and triangle counts are
slightly higher because Street's own transparent pass never counted blended
instance groups (UP-012). On the hardware GPU (1280 × 720): `baseline`
11.54 → 10.82 ms, `crowd` 10.34 → 10.34 ms (best of three and of five).

Known differences, beyond the comparison's: Street's apply-versus-draw
timing split (`opaqueApplyMs`, `opaqueDrawMs`, `skinnedMs`) is not
measured by the engine and reads -1; Street's figures were drawn with
counter-clockwise culling regardless of `doubleSided`, the engine honours
it. The probe bake still draws through Street's own effect code (it stays
in Street, as below).

Stays in street-simulator: `SkySystem`, `AtmosphereModel`,
`EnvironmentBaker` (the probe bake itself), `CityScene` and its content,
`CharacterLibrary` and animation, and its debug overlay (the engine's
cna-extended `DebugOverlay` can replace it later, as a separate step).

## galaxy-eggbert (strict, easy-3d replaced; PILOT-004)

galaxy-eggbert (`openeggbert/galaxy-eggbert`) used the helper library
easy-3d (`openeggbert/easy-3d`, namespace `Easy3D`), whose only consumer it
was. The owner decided on 2026-10-20 to move what is useful into the engine
and to migrate the game. The engine took over easy-3d's camera, cameras,
atlas, mesh shapes, mesh renderers and debug lines (`Easy3DEquivalenceTests`
compiles easy-3d read-only and finds the same vertices, indices and matrices
bit for bit); the game links `CnaEngine::Graphics` only.

| Step | Game code replaced | Engine facility | Known differences |
| --- | --- | --- | --- |
| 1 build | `easy3d` beside CNA | `CnaEngine::Core`, `CnaEngine::Graphics`; CNA static (UP-006) | none |
| 2 camera | `Easy3D::Camera3D` | `Camera::SetLookAt`, `SetPerspective` | an eye on its target used to give a NaN view for that frame; the camera keeps the previous view |
| 3 geometry and drawing | vertex/index lists, `CubeBatch`/`BillboardBatch`, `Append*Mesh`, `Cube/BillboardMeshRenderer`, `UvRect`, `PlateAxis`, `CubeFace` | `MeshBuilder` (`AddBox` with `BoxFaces`, `AddPlate`, `AddCrossedPlates`, `AddBillboard`), `GpuMesh::Upload`/`Draw(device, effect)`, `UvRect`, `PlateAxis`, `BoxFace` | none; the pyramid tip under teleporters stays in the game (`AddPyramidTip`, same vertices), and the order of geometry in each buffer is kept |
| 4 removal | easy-3d in CMake, CI, docs | — | — |

Not used by the game, so validated by the probe only: the orbit and follow
controllers, `TextureAtlas` itself (the game computes its tile regions) and
`DebugDraw`.

### galaxy-eggbert adoption (branch `engine-adoption`)

Worktree `openeggbert/galaxy-eggbert-engine`, branched from galaxy-eggbert's
`develop` at `44d27bf`; not pushed. Every commit was checked in a Release
`OPENGLES3` build against `~/deps/cna-baseline` (`900d0db3b`) with
galaxy-eggbert's suite (93 CTest cases, `-E easy-gl-resource-smoke-tests`)
and its deterministic outputs — the three golden frames
(`--golden-capture`), the behavioral trace (`--golden-capture-trace`) and the
startup screenshots — under Xvfb (llvmpipe) and on the GPU
(`SDL_VIDEODRIVER=offscreen`, Radeon 780M), against the build before the
first step.

| Commit | Step | Result |
| --- | --- | --- |
| `1676150` | build | 93/93; all 12 captures byte-identical |
| `61600dd` | camera | 93/93; all 12 captures byte-identical |
| `01c6d8f` | geometry and drawing | 93/93; all 12 captures byte-identical |
| `1780a35` | remove easy-3d | 93/93; all 12 captures byte-identical |
| `b66ed2d` | docs site | — |
| `5c5b727` | records | 93/93 and 12/12 again against engine `d86c62d` |

Two runs of the pre-migration build were byte-identical, so the captures are
deterministic and any difference would have been real. An `OPENGL33` build of
the final state ran the menus and the editor under Xvfb (browser, new world,
palette, placing blocks, flying the camera); the 20 changed sources and the
engine's strict modules compile with Emscripten's `em++` against the `WEBGL2`
headers (the web build itself was not linked or run). Found on the way,
independent of the migration: galaxy-eggbert's committed golden references
are stale against its `develop` with this CNA, and its editor terminates on
OpenGL ES contexts without polygon mode (UP-014).

## backrooms-simulator (strict; PILOT-005)

backrooms-simulator (`libcnalab/backrooms-simulator`) streams a 5 by 5
neighborhood of 40-metre chunks into a pool of plain vertex buffers (one
per chunk and material, no indices) and draws them with one unlit,
vertex-colored, textured, fogged BasicEffect; creatures follow with a
depth-only pre-pass and alpha blending. The owner allowed the migration on
2026-10-21.

| Step | Game code replaced | Engine facility | Known differences |
| --- | --- | --- | --- |
| capture mode | — (new: `--capture`, `--view`, `--time`) | — | captures run under the HiDef profile, which can read the back buffer |
| build | shared CNA | `CnaEngine::Core`, `CnaEngine::Graphics`; CNA static (UP-006) | a copied package no longer needs `libcna.so` |
| capture writing | back-buffer read and PNG write | `CaptureBackBuffer` | a failed capture ends the run with the engine's error |
| chunk drawing | the chunk loop over the game's BasicEffect | `GpuMesh::Borrow` (non-indexed), `RenderItem`, `RenderQueue`, `ForwardRenderer`, stock binder | none; the game's order is kept as `RenderItem::order` (the engine's own sorting moved one or two edge pixels in 4 of 30 views) |
| keys and buttons | `KeyboardState` edges | `ActionMap` (two key-pair axes, four actions) | none; focus is not passed, as the game ignored it |
| title statistics | ring of draw intervals, triangle sum | `FrameStatistics`, `RenderStatistics` | none |
| camera | `CreateLookAt`, `CreatePerspectiveFieldOfView`, frustum | `Camera::SetLookAt`, `SetPerspective`, `Frustum` | none |

Stays in the game: creatures and contact spots (a depth-only pre-pass is
not a render item), streaming and the buffer pool, collision, the lighting
bake, audio, and mouse look (the engine's mouse axes difference positions,
while CNA's relative mode reports motion).

### backrooms-simulator adoption (branch `engine-adoption`)

Worktree `libcnalab/backrooms-simulator-engine`, branched from `main` at
`71d9e9e`; fast-forwarded into `main` on 2026-10-21, not pushed. Release build against `~/deps/cna-baseline`
(`900d0db3b`), `~/deps/sharp-runtime-baseline` and easy-gl `v0.1.1`. The
check after every step: the three CTest suites and 30 captures (12 standard
views and the same views looking back and down, five creature views at
three distances, one inside the creature fade) under Xvfb (llvmpipe) and on
the GPU (`SDL_VIDEODRIVER=offscreen`, Radeon 780M), compared byte for byte
with the step before. Two runs of the first commit gave identical bytes on
both renderers.

| Commit | Step | Result |
| --- | --- | --- |
| `7c6c909` | capture mode | 30 views twice, identical |
| `ea04b6d` | build | 3/3; 30/30 identical on both renderers |
| `c915022` | capture writing | 30/30 identical on both renderers |
| `3a4e342` | chunk drawing | 30/30 identical on both renderers; 1.6 km streaming sweep |
| `e098720` | keys and buttons | `tools/validate_controls.py` passes; 30/30 identical |
| `cc5850c` | title statistics | 30/30 identical; streaming sweep |
| `3b0a517` | camera | 30/30 identical on both renderers, same chunks drawn |
| `b1d1a64` | records | — |

Found on the way: the game's workaround for EasyGL buffer writes without a
current context (its `bugs.md`) is fixed in CNA `00b9c9898`, contained in
the baseline; the workaround stays until the game's pooled-buffer
regression walk is repeated without it.

## school-simulator (strict; PILOT-005)

school-simulator (`libcnalab/school-simulator`) streams the rooms of a
six-level school around the player (portal culling, corridor groups) as
static vertex-coloured batches drawn through one unlit BasicEffect, with a
facade built once, glass blended with straight alpha, CCTV views rendered to
targets, and people, sky, weather and screens as further passes. Its
geometry is wound counter-clockwise (it culls clockwise). The owner allowed
the migration on 2026-10-21.

| Step | Game code replaced | Engine facility | Known differences |
| --- | --- | --- | --- |
| deterministic pictures | — (fixed-step runs: room builds by count, light fades by the step) | — | none in normal runs; `tools/determinism.sh` now fails on different pictures |
| build | shared CNA | `CnaEngine::Core`, `CnaEngine::Graphics`; CNA static (UP-006) | none |
| screenshots | back-buffer read and the game's uncompressed PNG writer | `CaptureBackBuffer` with `CaptureOptions::opaque` | the files are compressed (a third of the size); same pixels |
| camera | `CreateLookAt`, `CreatePerspectiveFieldOfView` in `FirstPersonCamera` and the CCTV views | `Camera::SetLookAt`, `SetPerspective` | none |
| meshes | — (the draw call) | `GpuMesh::Borrow` of `StaticMesh`/`LitMesh` buffers | none |
| world geometry | `WorldRenderer::DrawBatches` and its state changes | `RenderItem`, `RenderQueue`, `ForwardRenderer`, stock binder, `Material::frontFace`, `Material::straightAlpha`; doors, cabs and lamps in the `AfterOpaque` pass | none; the game's culling and order are kept (`RenderItem::order`) |
| system keys | `InputState::Pressed` for 30 controls and "use" | `ActionMap` | a press counts per action (holding H and pressing F1 toggles help once) |
| texture cache | name-to-texture map with a failure marker | `AssetRegistry` (loader, fallback, `GetOrCreate` for run-time textures) | none |

Stays in the game: walking and looking (`PlayerController` reads analog
sticks and CNA's relative mouse motion), people, sky, weather, particles,
screens and room demonstrations, audio, and the F3 frame rate (a
half-second block average also shown on in-game monitors).

### school-simulator adoption (branch `engine-adoption`)

Worktree `libcnalab/school-simulator-engine`, branched from `main` at
`bfb5613`; fast-forwarded into `main` on 2026-10-21, not pushed. Release build (two jobs, the project's limit)
against `~/deps/cna-baseline` (`900d0db3b`), `~/deps/sharp-runtime-baseline`
and easy-gl `v0.1.1`. The check after every step: the five CTest entries
and 19 fixed-step captures (the game's 17 tour places without the HUD, the
entrance with it, the lobby at night in rain) under Xvfb (llvmpipe) and on
the GPU (Radeon 780M), compared with the step before — byte for byte, or
pixel for pixel across the change of PNG writer. Before the first step one
of the 19 differed between two runs (a night view with lights fading in);
after it two runs on each renderer were identical.

| Commit | Step | Result |
| --- | --- | --- |
| `52ca272` | deterministic pictures | 19 views twice, identical, on both renderers |
| `e85071c` | build | 5/5; 19/19 identical on both renderers |
| `405a90e` | screenshots | 19/19 pixel-identical (RGBA) on both renderers |
| `78b6d96` | camera | 19/19 identical on both renderers |
| `bb9e30c` | meshes | 19/19 identical on both renderers |
| `cf588c6` | world geometry | 19/19 identical on both renderers; every statistics line unchanged but memory |
| `a48bd82` | system keys | X11 input (F3, M, F12, Tab, F1, Escape, Q) does what it did; 19/19 identical |
| `3a1beef` | texture cache | 19/19 identical on both renderers; a removed texture shows the checker, reported once |
| `21ab2f0` | records | `tools/check_all.sh` (unit and simulation tests, validation, simulated days, the 21-stop tour): all pass |

Not checked: the browser build (`tools/build_web.sh`), which now also
compiles the engine.

## Order of work

Each pilot: input and fixed-step first (no rendering change), then camera,
meshes, screenshots, assets and quality, and the world/scene renderer last —
it is the largest step and the one whose images must be compared against the
agreed tolerances (MILESTONE-2).
