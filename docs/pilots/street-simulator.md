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
- Asset registry: Street's caches (`CharacterLibrary`, `ModelLibrary`) are
  tied to its `MaterialLibrary` and glTF import; a probe needs those pieces
  compiled read-only first.
- One complete Street vertical slice (MILESTONE-2).
