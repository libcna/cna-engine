# Pilot: car-simulator (strict-XNA)

Plan item: `PILOT-002`. The pilot is `libcnalab/car-simulator` (MIT). It is
never modified from this repository; probes compile selected pilot sources
read-only next to engine code (`tests/pilots/`) and compare behaviour on
identical inputs.

## Input actions (ENG-020) — validated

Probe: `tests/pilots/CarSimulatorControlsTests.cpp`, compiled with
`simulator/src/Input/InputMapper.cpp` from the pilot checkout.

The engine `ActionMap` is built from the pilot's own default tables
(`InputMapper::DefaultBindings()`, `DefaultPadBindings()`): 51 actions, every
keyboard and pad binding, plus the analog-derived holds (triggers past half
travel, curved stick past half). On 4 × 1500 frames of deterministic random
input (overlapping keys, pad buttons, analog levels, pad disconnects):

| Behaviour | Result |
| --- | --- |
| `Held` for every action | identical on every frame |
| `Pressed` | identical except one documented difference (below), checked as an exact relation |
| Throttle, brake, steering (`DriverControls`) | identical to `BuildDriverControls()` on every frame |
| Binding text round trip | the full binding set saves and reloads unchanged |
| Shared keys (Shift: `ShiftUp` / `ToggleRun`) | reported by `FindConflicts()`, not rejected |

### Generalizations made because of this pilot

- **Response curves**: `AxisBinding::ForGamePad(..., exponent)` — the pilot
  steers with `|x|^1.6`.
- **Axis composition**: key pairs form one digital composite (opposite keys
  cancel, same-direction keys do not add) and compete with analog inputs by
  largest magnitude, keys winning ties. This is the pilot's
  `DriverControls` rule and matches common engine practice.

### Intentional difference

`Pressed`: the pilot reports a press whenever *any binding* goes down, even
while the action is already held through another binding (holding W, then
pressing Up, presses Throttle again). The engine reports *action-level*
edges: an action is pressed only when it goes from released to held. The probe
asserts `engine.Pressed == pilot.Pressed && !heldLastFrame` for digital
inputs, and confirms the difference occurs in the random histories. For
analog-derived holds the pilot never reports a press; the engine does when the
threshold is crossed.

Adopting the engine in the pilot would change toggles (e.g. `ToggleHelp`
bound to one key only) in no observable way; it would only suppress repeated
toggles from a second key while the first is held.

### Not covered

- Steering-wheel profile (`wheel == true`: linear steering). A game expresses
  it by swapping the stick binding's exponent to 1 when it detects a wheel;
  device-type detection stays in the game.
- Display names are covered since 2026-10-20: `KeyDisplayName` and
  `ButtonDisplayName` return the pilot's labels for every key and button it
  labels (probe `DisplayNamesMatchTheGamesLabels`); the pilot's text doubles
  as its binding-file format, the engine keeps the two apart
  (`FormatBinding` for files).

## Camera data (ENG-021) — validated

Probe: `tests/pilots/CarSimulatorRenderTests.cpp` with the pilot's
`Render/Camera.cpp`. The pilot's `ChaseCamera` (vehicle-specific policy that
stays in the game) runs over 600 frames of a curving, accelerating vehicle;
each `CameraPose` is applied to an engine `Camera` with a four-line adapter
(`SetPosition`, `LookAt`, `SetPerspective`). Projections are identical
(< 1e-6), forward vectors agree to 1e-4, and view matrices agree to float
precision (< 2e-3 at world coordinates of ~200 m). The engine camera can carry
the pilot's chase and cockpit cameras without owning their behaviour.

## Mesh data and GPU upload (ENG-022/023) — validated

Same probe with the pilot's `Render/MeshData.cpp` and `Render/GpuMesh.cpp`.
The pilot's procedural box, cylinder and torus convert to engine `MeshData`
(array-of-structs to attribute streams) with identical bounds, triangle
counts and winding (the pilot reports signed volume with the opposite sign
convention). Uploaded through the engine (`PositionNormalTexture`, 16-bit
indices) and through the pilot's `GpuMesh` (32-bit indices), both draw a
**pixel-identical** image with BasicEffect default lighting. The engine's
conversion report names the attributes the layout drops (color, second UV set).

## Strict checker contract (ADR-001) — validated

`cna_engine.pilot.car_simulator_checker_allowance` runs the pilot's own
`scripts/check_xna_only.py` over all strict engine headers and sources (44
files at the time of writing). The only findings are the 48 includes of
strict `CnaEngine/<module>/` headers — exactly the allowance ADR-001 asks the
pilot to add. No identifier, XNA-type or extension rule fires.

## Screenshots (ENG-025) — validated

`CarSimulatorRenderTests.ScreenshotsMatchThePilotCapture` captures the same
HiDef back buffer with the pilot's `SaveBackBufferPng` and the engine's
`CaptureBackBuffer`: both PNG files decode to identical pixels, and the
engine's in-memory copy matches them. On a Reach device both refuse; the
engine reports why as a value instead of printing to stderr.

## Asset registry (ENG-024) — validated

`CarSimulatorAssetsTests` replaces `TrafficRenderer::PlateTexture`'s map
(generate a plate texture once per plate text, then reuse) with an engine
`AssetRegistry` over the pilot's own `PlateRenderer::Render` and
`UploadTexture`: one generation per distinct plate, shared textures for
repeats, pixels identical to a direct upload.

Generalization made because of this pilot: `SetNormalizeKeys(false)` for
registries keyed by text rather than asset paths.

## Quality tiers (ENG-036) — validated

`CarSimulatorQualityTests` declares the pilot's four tier settings in an
engine `QualitySchema` and defines the presets from the pilot's own
`SettingsFor()`; every tier resolves to exactly the pilot's values on Reach
and HiDef, with no decisions, and the preset names parse with the pilot's
`QualityFromName()`.

## Fixed-step clock — validated

`CarSimulatorClockProbe` (in `cna_engine_pilot_carsim_sim_tests`) drives
car-simulator's own `Vehicle::Update` (reference vehicle on flat ground) and
an engine `FixedStepClock` with car-simulator's constants using the same frame
times, and recovers car-simulator's per-frame step count from its private
accumulator.

| Frame sequence | Frames | Result |
| --- | --- | --- |
| Fixed 30, 60, 120, 144 and 240 Hz | 36,000 each | identical step count in every frame |
| Hitches (5 s, 0.3 s, 0.25 s), zero and negative frames | 10 | identical (same clamp, same 30-step cap) |
| 60 Hz with ±4 ms jitter | 36,000 (71,922 steps) | 12 frames differ by one step; running totals never more than one step apart |

### Intentional difference

car-simulator accumulates in `float`, the engine in `double`. A frame that
ends within a few ULPs of a step boundary can be rounded to the other side,
so that step is taken one frame earlier or later. car-simulator's
deterministic captures (`--lockstep`, exactly 1/60 s per frame) are in the
identical case.

## Its real map through the engine (strict ForwardRenderer)

`cna_engine.pilot.carsim_scene_comparison` compiles car-simulator's libraries
read-only, loads its real map (`content/maps/lipova`) and draws the driver's
view from the player spawn twice into the same 480×270 target: with
car-simulator's own `WorldRenderer`, and with the engine — terrain chunks,
roads and markings, static objects and trees as render items over the same
buffers and textures, engine culling with car-simulator's rules, the strict
`ForwardRenderer` with the stock binder and car-simulator's lighting rig.

| Measure | car-simulator | Engine |
| --- | --- | --- |
| Terrain chunks drawn | 207 | **207** |
| Road and marking batches | 77 | **77** |
| Object batches | 503 | **503** |
| Tree batches | 33 | **33** |
| Sign batches (since 2026-10-20) | 46 (not counted by car-simulator; its rule) | **46** |
| Draw calls | 867 (every world pass) | **867** |
| Image difference | — | mean **0.05 / 255** per channel; 14,030 of 129,600 pixels differ, 13,891 of them by one level, 39 by more than three |
| CPU frame time | 5.2–6.6 s | 6.6–8.1 s (software renderer, Debug; varies with machine load) |

The engine's image is practically identical to car-simulator's
(`docs/pilots/images/carsim-scene-*.png`); the one-level differences are
rounding in lit facades and road noise. Signs draw with the tree material,
double-sided. car-simulator's headlight-fill and lamp-light passes draw
nothing at this daytime view, so the draw calls match in full.

The engine sets every sampler from the materials (car-simulator's anisotropic
wrap for terrain, roads and objects, linear clamp for the terrain's macro layer
and the trees); the comparison leaves point sampling on the device before the
engine draws, so nothing is inherited from car-simulator's pass.

After dark. The program then sets car-simulator's lighting rig to 22:00
(`LightingRig::SetTimeOfDay`, `ApplyLighting`) and compares again: lit windows
(night emissive) and the street lamps' light, which car-simulator adds on top
of the scene without fog. The engine draws the lamps as additive,
double-sided, unfogged (`Material::fog = false`) items with the stock binder:

| Measure (22:00) | car-simulator | Engine |
| --- | --- | --- |
| Terrain / roads / objects / trees / signs | 207 / 77 / 295 / 33 / 46 | **the same** |
| Lamp lights | 15 (its rule) | **15** |
| Draw calls | 674 | **674** |
| Image difference | — | mean 0.00 / 255; 800 pixels differ, at most 20 / 255 |

Images: `docs/pilots/images/carsim-night-*.png`.

With the headlights on. A third view switches car-simulator's low beams on
at the driver's position (`WorldRenderer::SetHeadlights`). car-simulator then
draws a headlight fill: objects, trees and signs inside the beam once more,
lit by the headlights alone, faded to black with distance and added on top.
The engine expresses it as a second and third `ForwardRenderer` (the
headlights' own lighting and fog) run from a custom `AfterOpaque` pass of the
main one; the cards use `Material::discardBelowCutoff` (alpha-tested and
additive at once).

| Measure (22:00, low beams) | car-simulator | Engine |
| --- | --- | --- |
| Headlight fill draws | 124 (its rule) | **124** |
| Draw calls | 798 | **798** |
| Image difference | — | mean 0.01 / 255; 809 pixels differ, 47 visibly |

Images: `docs/pilots/images/carsim-headlights-*.png`.

Frustum test. car-simulator culls with `BoundingFrustum::Intersects(sphere)`,
the engine with the plane test (`Contains(sphere) != Disjoint`). At the pinned
CNA the two agree. CNA `5ab1c7729` (2026-10-20, after the pin) implements
`Intersects` with XNA's GJK query and its
scale-relative tolerance, which also accepts some spheres just outside a
plane: car-simulator then draws 506 object and 34 tree batches, the engine 503
and 33, with the same image (0.1/255). The comparison therefore counts both:
the engine's rules with car-simulator's frustum test must reproduce
car-simulator's counts exactly (they do, at both CNA states), and the engine's
own counts are reported next to them. The program fails if the reproduction
differs.

Generalizations made because of this comparison: fill lights and per-pixel
lighting in `ForwardLighting` (car-simulator's sun / sky fill / ground bounce
rig), `Material::secondaryTexture` (`DualTextureEffect` terrain),
`Material::depthBias` for road markings, `Material::sampler` /
`secondarySampler`, `Material::fog` for the lamp light, and
`Material::discardBelowCutoff` for the headlit cards.

## Still open for this pilot
- The strict checker allowance (ADR-001) applied in an integration branch of
  the pilot — requires the owner's permission to touch car-simulator.
