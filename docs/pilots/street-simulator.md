# Pilot: street-simulator (enhanced)

Plan item: `PILOT-001`. The pilot is `libcnalab/street-simulator` (MIT),
formerly `libcna/cna-street`. It is never modified from this repository;
`tests/pilots/StreetSimulatorTests.cpp` compiles Street's `Render/Camera.cpp`,
`Render/CameraController.cpp`, `Geometry/MeshBuilder.cpp` and
`Render/GpuMesh.cpp` read-only and compares them with the engine.

## Camera data (ENG-021) — validated

Street's `CameraController` flies for 300 frames under scripted keyboard and
relative-mouse input. Each frame its camera is mirrored into an engine
`Camera` (`SetPosition`, `SetYawPitchRoll(-yaw, pitch)`, `SetPerspective`).
Projections are identical (< 1e-6), forward and right vectors agree to 1e-4,
and view matrices to < 1e-3.

Convention difference: Street measures yaw clockwise seen from above; the
engine follows XNA's `CreateFromYawPitchRoll` (counter-clockwise). The adapter
negates yaw; pitch agrees.

Street's fly/walk/cinematic controller (collision probes, wheel speed,
viewpoints) stays in the game: it is policy around the camera, and the engine's
`ICameraController` lets the game keep it while using the engine camera.

## Controls (ENG-020) — validated

Street reads WASD/QE, arrows, Shift, Ctrl and relative mouse directly. The
probe expresses them as engine axes and actions (mouse axes for look deltas,
key axes for look rates, `Fast`/`Slow` actions) and drives an engine
`FlyCameraController`; over 600 frames of random input its camera position
stays within 1e-3 and its view direction within 1e-4 of Street's own
controller.

Generalizations made because of this pilot: the fly controller normalizes the
movement direction (diagonals are not faster) and scales speed by the input
magnitude (analog sticks keep partial speed), climbs along world +Y by
default, has a slow multiplier, and `CameraInputAxes` separates per-update
look deltas (mouse) from look rates (keys, sticks; radians per second).

Street's mouse-wheel speed and Walk/Cinematic modes (collision, gravity,
viewpoints) stay game policy.

## Tangent-frame geometry and upload (ENG-022/023) — validated

Street's `MeshBuilder` box, cylinder and sphere (the `VertexPositionNormalTangentTexture`
format its `PbrEffect` requires) convert to engine `MeshData` with
position/normal/tangent/UV streams, identical bounds and outward winding.
Street's authored tangent frames agree with the engine's
`ComputeTangents()` for more than 90 % of the vertices (the rest are at UV
seams and poles where any tangent is valid).

Uploaded with the engine's 48-byte `PositionNormalTangentTexture` layout and
with Street's `GpuMesh`, both render a **pixel-identical** image with
BasicEffect lighting; the conversion report shows nothing dropped or
narrowed.

## Asset registry (ENG-024) — validated

`StreetAssetsTests` reproduces `MaterialLibrary::derive`'s cache (one derived
material per name, first definition wins, shared name space with
`deriveTinted`) with `AssetRegistry::GetOrCreate` over Street's own base
materials; identity and content match Street's results.

Generalization made because of this pilot: `GetOrCreate(key, factory)` for
assets whose recipe is not derivable from the key.

## Instancing (ENG-034) — validated on the hardware path

`StreetSimulatorTests.InstancingMatchesStreetInstancedMesh` wraps the same
engine-uploaded buffers in a `ModelMeshPart` for Street's `InstancedMesh` and
draws seven instances with both. Street's capability check selects the
hardware path on the software renderer; the engine's `DrawHardware` issues the
same single draw with the same 64-byte per-instance declaration
(TextureCoordinate 1–4) and produces a pixel-identical image.

Generalizations made because of this pilot:

- `DrawHardware` restores the device's previous vertex and index bindings,
  so the next frame's instance upload never writes into a bound buffer
  (XNA refuses that);
- the per-instance fallback places instances relative to the effect's current
  World (a shared pivot) and restores it afterwards, as Street does. This path
  is not equivalence-tested against Street, because Street's capability check
  cannot be forced to its fallback without modifying it.

Street selects the path with CNA capability queries
(`SupportsCapability(Instancing)`, `MultiStreamVertexInput`); the strict
engine API uses the XNA profile (`ChoosePath`). An enhanced capability-based
choice belongs in `EnhancedGraphics` when a second consumer needs it.

## A real Street scene through the engine (MILESTONE-2)

`cna_engine.pilot.street_scene_comparison` (built with
`-DCNA_ENGINE_CNA_CNAEXT=ON`, because Street's sky uses CNA's CNAEXT shader
packages; run in `build-probe/`) compiles all of Street's core read-only,
builds Street's procedural `CityScene` (Low preset, no reflection probes, no
traffic or pedestrians), and draws the first viewpoint twice into the same
480×270 target:

1. with Street's own `SceneRenderer`;
2. with the engine: Street's registered items read from the renderer, meshes
   borrowed (`GpuMesh::Borrow(ModelMeshPart&)`), materials converted field by
   field, engine culling with Street's rules (box distance and per-item limits
   for items, sphere distance with min/max for instances, batch LOD from the
   nearest instance), one instanced render item per group, `ForwardRenderer`
   with the `PbrMaterialBinder`.

Result (software renderer, Debug, CNA baseline `5a44d2c5`):

| Measure | Street | Engine |
| --- | --- | --- |
| Scene | 1,361 static items, 93 instance groups (661 instances) | same objects |
| Visible items | 652 | **652** |
| Visible instances | 192 | **192** |
| Draw calls | 703 (51 instanced) | **703** (instanced render items) |
| CPU frame time | 6.2–9.7 s | 4.8–8.2 s (engine faster in every run) |
| Mean image difference | — | 19 / 255 per channel |

The program fails if the visibility sets differ. The images
(`docs/pilots/images/street-scene-street.png`, `…-engine.png`) show the same
geometry, materials and composition; the remaining difference is Street's
lighting model (image-based sky lighting, exposure, its own sun intensity),
which is game art direction and stays in the game — a Street binder
deriving from the engine's would carry it.

Generalizations made because of this comparison: `GpuMesh::Borrow` of model
parts with vertex offset and start index; optional box bounds, per-item cull
distance and minimum distance in visibility; `Material::keyLight` (Street's
`sunlit`); instanced render items (`RenderItem::instances`) drawn with one
hardware draw on HiDef — CNA's effects read the per-instance stream and
compose it with the effect's World, verified against the per-instance path.

## Shadows (ENG-035 step 3) — not required at present

Street still carries shadow settings and statistics, but CNA's
`CascadedShadowMap` was removed in CNA's graphics-ext scope reduction and
Street's renderer never sets `drewShadows` or counts a shadow draw. Shadow
passes therefore stay deferred under the plan's rule ("only when a pilot
requires them"). CNA keeps the receiver side (`IShadowReceiverEXT` on
BasicEffect/PbrEffect: a light-space distance map plus matrices), which an
engine shadow pass would feed.

## Quality presets (ENG-036) — validated

`StreetQualityTests` declares the 26 settings Street's `applyPreset()` writes
(read through member pointers into Street's own `RenderSettings`) and defines
the four presets from it. On HiDef every preset resolves to Street's values
with no decisions; on Reach only the declared multisampling limit changes a
value and it is recorded. Player overrides persist and resolve.

Generalization made because of the pilots: the fixed-field `QualityIntent` was
not enough for either game; `QualitySchema` lets a game declare its own
settings while the engine keeps presets, overrides, limits, the decision log
and persistence.

## Still open for this pilot

- Materials and render queue with Street's PBR effect (needs CNA's CNAEXT
  `PbrEffect` in an enhanced renderer; CNA is built with `CNA_CNAEXT=OFF` here).
- Culling/LOD counts against Street's own scene.
- One complete Street vertical slice (MILESTONE-2).
