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
   nearest instance through `LodSelector`), one instanced render item per
   group, `ForwardRenderer` with the `PbrMaterialBinder` carrying Street's
   lighting (sun, sky ambient and image-based environment under Street's
   exposure, fog matched to the horizon haze), and Street's sky drawn by a
   custom `BeforeOpaque` pass — the game's own CNA drawing inside the
   engine's frame.

Result (software renderer, Debug, CNA baseline `5a44d2c5`), with Street's
traffic and pedestrians on (since 2026-10-21; the whole program takes about
3 minutes and 2.7 GB):

| Measure | Street | Engine |
| --- | --- | --- |
| Scene | 1,361 static items, 201 instance groups (1,390 instances), moving vehicles, 193 skinned figures (pedestrians and drivers) | same objects |
| Visible items (static and vehicles) | 1,110 | **1,110** (the same rules) |
| Visible instances | 192 | **192** |
| Visible figures | 123 | **123** |
| Draw calls | 1,284 (51 instanced, 123 skinned) | **1,284** |
| CPU frame time | 10.2–11.8 s | 10.7–11.4 s (equal within run-to-run noise) |
| Image difference | — | mean **0.01 / 255** per channel; 68 of 129,600 pixels differ (largest 103 / 255) |

Without traffic (pedestrians only, 2026-10-20) the same view gave 652 / 192 /
117 visible, 820 / 820 draw calls and 53 differing pixels.

The program fails if the visibility sets differ. The images
(`docs/pilots/images/street-scene-street.png`, `…-engine.png`) are practically
identical. Street's transparent pass keeps depth writes on, the engine's
contract draws blended surfaces with depth reads only; switching depth writes
on for the engine's transparent items (a throwaway binder wrapper) leaves 4
differing pixels, so 49 of the 53 come from that difference. The remaining 4
were not investigated.

From this viewpoint the figures are far away and mostly hidden (539 pixels),
so the program draws a further view: a close-up 3.5 m in front of a
pedestrian (`docs/pilots/images/street-figure-*.png`). With traffic, 42
figures cover 4,112 pixels there and **none** of them differs from Street's
frame (without traffic: 57 figures, 10,412 pixels, 2 differ). The frame as a
whole differs in 781 pixels (mean 0.13 / 255): the engine draws exactly
Street's visible set, but this camera's view matrix differs from Street's by
1.7e-6 (its orientation goes through two implementations' yaw handling; for
the first viewpoint the matrices are identical), and the differing pixels lie
where Street's ground meshes overlap coplanar (kerb, paving, setts), whose
depth ties such a difference can flip. Neither draw order (forcing the engine
into Street's order: 764 pixels) nor depth writes (717) accounts for them.

The engine reports the same frame through its diagnostics:

    draws 820  triangles 471578  vertices 915084
    culling 1407 considered, 652 visible (layer 0, distance 186, frustum 569)
    lod 0: 8, 1: 6
    culling 661 considered, 192 visible (layer 0, distance 340, frustum 129)   (instances)
    culling 181 considered, 117 visible (layer 0, distance 0, frustum 64)      (figures)

Before the lighting was carried over (2026-10-19), the mean difference was
19/255; the engine had drawn Street's materials with a plain sun and ambient.

Generalizations made because of this comparison: `GpuMesh::Borrow` of model
parts with vertex offset and start index; optional box bounds, per-item cull
distance and minimum distance in visibility; `Material::keyLight` (Street's
`sunlit`); instanced render items (`RenderItem::instances`) drawn with one
hardware draw on HiDef — CNA's effects read the per-instance stream and
compose it with the effect's World, verified against the per-instance path;
binder-chosen blend states (`IMaterialBinder::BlendStateFor`: CNA's
`PbrEffect` writes straight alpha, XNA's stock effects premultiplied) and
`BlendMode::PremultipliedAlpha` for Street's glass (`premultipliedBlend`);
`PbrSettings::environment` and `exposure`; `CullStatistics::Record` and
`LodStatistics` for diagnostics; skinned render items (`RenderItem::bones`,
blend-index and blend-weight streams, skinned layouts, `SkinnedPbrEffect` in
the PBR binder) and `GpuMesh::Borrow` with offsets for Street's imported
skinned meshes.

### Engine input drives the camera

A third view is reached by the engine itself: from Street's viewpoint an
`ActionMap` (keys bound to move and look axes) and a `FlyCameraController`
walk the camera for 120 updates of scripted input — forward, then forward
while turning left — ending 5.95 m further and turned 0.40 rad. Street and the
engine then both render that pose:

| Measure | Street | Engine |
| --- | --- | --- |
| Visible items / instances / figures | 954 / 157 / 78 | **954 / 157 / 78** |
| Draw calls | 1,087 (55 instanced) | **1,087** |
| Image difference | — | mean 0.21 / 255; 884 of 129,600 pixels (14 of the 528 figure pixels) |

As in the close-up, the camera's orientation passes through both
implementations' yaw handling and the remaining pixels sit on coplanar
ground meshes (`docs/pilots/images/street-walk-*.png`).

### GPU resources

After the two views the program estimates the GPU memory of everything the
scene registers with the engine's `GpuMemoryEstimate` (vertex count × declared
stride, index count × element size, every texture level at its format; shared
buffers once) and sets it beside Street's own accounting:

| Measure | Street | Engine estimate |
| --- | --- | --- |
| Meshes Street uploads itself | 88,007,780 bytes | 76,677,228 bytes (vertex 67,983,312, index 8,693,916) |
| Everything the scene registers (incl. borrowed model parts, vehicles, skinned figures) | — | 424.7 MiB: vertex 66.7 MiB (1,758 buffers), index 8.4 MiB, textures 349.5 MiB (379) |
| Texture library | 367,877,828 bytes | — (Street counts every loaded texture at its top level only and generated ones with their mips; the engine counts the textures materials reference, every level) |

The mesh difference is Street's: it sizes vertices with `sizeof` of CNA's
vertex struct (56 bytes, it has a vtable) instead of the declared stride (48);
exactly 8 bytes per vertex over 1,416,319 vertices (upstream-issues.md UP-008;
1,287,299 vertices without traffic).

### With reflection probes (ENG-035 step 5)

Street's Medium and High presets light each item from its nearest reflection
probe. (Measured before traffic was switched on in the comparison.) `cna_engine_street_scene_comparison <dir> 0 probes` bakes Street's
probes (32-pixel faces every 32 m, as Medium does; about 5 minutes in all, so
the CTest run leaves it out) and gives the engine a
`PbrMaterialBinder::EnvironmentSelector` that maps each item's
`RenderItem::userData` (Street's `SceneItem::probe`) to that probe's
environment, as Street's `applyEnvironment` does. Measured on 2026-10-20 at the
CNA baseline:

| View | Street with vs without probes | Engine vs Street, both with probes |
| --- | --- | --- |
| First viewpoint | 106,533 of 129,600 pixels change (mean 11.1/255) | 75 pixels differ (mean 0.01/255) |
| Pedestrian close-up | — | 785 pixels differ (mean 0.36/255); 2 of 10,412 figure pixels |

Visible items, instances, figures and draw calls are identical in both views.
Images: `docs/pilots/images/street-probes-street.png`, `…-engine.png`.

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

Materials, the render queue, culling and instancing with Street's `PbrEffect`
are covered by the scene comparison above. What remains:

- Screenshot tolerances for MILESTONE-2, to be agreed with the owner (the
  measured difference above is the starting point).
- Adoption inside Street (PILOT-001) — requires the owner's permission to
  modify the pilot.
