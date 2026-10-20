# Migrating the pilots to cna-engine

Status: **car-simulator adopted on a branch; street-simulator not started.**
The owner allowed adoption on 2026-10-20 on new branches of both pilots (one
revertible commit per step, nothing pushed). car-simulator's branch
`engine-adoption` carries every step below except the camera (see
"car-simulator adoption"). This document is the migration plan the read-only
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
`main` at `cd6d948`; not pushed. Each commit was checked with car-simulator's
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
| 1 | `src/Render/Camera.cpp`, `CameraController.cpp` | `Graphics::Camera`, `FlyCameraController` | `StreetSimulatorTests`: fly trajectory within 1e-3 | none observed |
| 2 | `src/Geometry/MeshBuilder.cpp`, `src/Render/GpuMesh.cpp` (tangent frames) | `MeshBuilder`, `MeshData::ComputeTangents`, `GpuMesh` (`Borrow` for model parts) | pixel-identical uploads | none observed |
| 3 | `src/Render/InstancedMesh.cpp` | `InstanceSet`, instanced `RenderItem` | pixel-identical hardware path | the per-instance fallback is used on Reach |
| 4 | presets in `src/Render/RenderSettings.cpp` | `QualitySchema` (26 settings) | `StreetQualityTests` | Reach limits are reported as decisions |
| 5 | caches in `MaterialLibrary` / `ModelLibrary` | `AssetRegistry` | `StreetAssetsTests` (derived-material cache) | none observed |
| 6 | `SceneRenderer` culling, `drawOpaque`, `drawTransparent`, `drawSkinned`, `applyMaterial`, `applyLighting`, `applyEnvironment` | `Visibility`, `LodSelector`, `RenderQueue`, `ForwardRenderer`, `PbrMaterialBinder` (exposure, sky environment, `EnvironmentSelector` for probes), skinned render items; Street's sky as a `BeforeOpaque` pass; statistics through `FormatDiagnostics` | `street_scene_comparison`: visibility, figures and draw calls identical; image 0.01/255 (53 px); with probes 75 px; pedestrian close-up 2 of 10,412 figure pixels | the engine draws blended surfaces with depth reads only (Street writes depth: 49 of the 53 px); order of coplanar ground meshes can differ under tiny view-matrix differences; Street's per-family counters (vehicles, drivers, characters) become the game's own counts around the engine's |

Stays in street-simulator: `SkySystem`, `AtmosphereModel`,
`EnvironmentBaker` (the probe bake itself), `CityScene` and its content,
`CharacterLibrary` and animation, and its debug overlay (the engine's
cna-extended `DebugOverlay` can replace it later, as a separate step).

## Order of work

Each pilot: input and fixed-step first (no rendering change), then camera,
meshes, screenshots, assets and quality, and the world/scene renderer last —
it is the largest step and the one whose images must be compared against the
agreed tolerances (MILESTONE-2).
