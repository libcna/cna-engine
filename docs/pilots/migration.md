# Migrating the pilots to cna-engine

Status: **not started.** Adoption (PILOT-001, PILOT-002) changes the pilots,
which needs the owner's permission. This document is the migration plan the
read-only probes support, so the work can start as soon as it is allowed. It
covers PILOT-003's requirement that migration notes state reversibility and
known differences.

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
| 3 | `src/Render/Camera.cpp` (camera state; chase/cockpit policy stays) | `Graphics::Camera` | `CarSimulatorRenderTests` (projection < 1e-6, view to float precision) | none observed |
| 4 | `src/Render/MeshData.cpp`, `GpuMesh.cpp` | `MeshData`, `MeshBuilder`, `VertexLayout`, `GpuMesh` | pixel-identical uploads | signed-volume sign convention; the conversion report names dropped attributes |
| 5 | `src/Render/Screenshot.cpp` | `ScreenCapture`, `ScreenshotQueue` | identical PNG pixels | failures are values, not stderr |
| 6 | texture caches (e.g. `TrafficRenderer::PlateTexture`) | `AssetRegistry` (`SetNormalizeKeys(false)`) | `CarSimulatorAssetsTests` | none observed |
| 7 | `src/Render/QualityTier.cpp` | `QualitySchema` | `CarSimulatorQualityTests` (every tier, Reach and HiDef) | none observed |
| 8 | world passes of `WorldRenderer::Draw` (terrain, roads and markings, objects, trees) | `RenderItem`, `Visibility`, `LodSelector`, `RenderQueue`, `ForwardRenderer` with the stock binder; `ForwardLighting` from `LightingRig`; materials with samplers and depth bias | `carsim_scene_comparison` on `lipova`: every visibility count equal, image 0.1/255 | the engine culls with the frustum plane test; with CNA's GJK `Intersects` (in progress upstream) car-simulator draws 3 object and 1 tree batch more, all off screen; skirt, signs, lamp glows, headlight fill, rain and mirrors stay game passes (custom `ForwardRenderer` passes or direct CNA drawing) |

Stays in car-simulator: vehicle physics, traffic, map loading, procedural
content generators, the lighting rig's values, HUD and instrument cluster.

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
