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

## Still open for this pilot

- Materials and render queue with Street's PBR effect (needs CNA's CNAEXT
  `PbrEffect` in an enhanced renderer; CNA is built with `CNA_CNAEXT=OFF` here).
- Instancing against Street's `InstancedMesh`, culling/LOD counts, quality
  tiers.
- Asset registry: Street's caches (`CharacterLibrary`, `ModelLibrary`) are
  tied to its `MaterialLibrary` and glTF import; a probe needs those pieces
  compiled read-only first.
- One complete Street vertical slice (MILESTONE-2).
