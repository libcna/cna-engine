# Changelog

All public API changes are recorded here. While the version is 0.x, a minor
version may break the API (ADR-001); changes to pilot-validated headers
(tracked in `docs/api-snapshot.txt`, checked by `cna_engine.api_snapshot`) must
be listed below with the reason.

## Unreleased (0.1.0)

First development line. Highlights:

- Strict targets `CnaEngine::Core` and `CnaEngine::Graphics`: input actions and
  axes with rebinding and persistence; camera with fly/walk controllers; CPU
  mesh data, builders, vertex layouts; RAII GPU meshes; materials, material
  binders, render queues, visibility and LOD, a forward renderer; instancing;
  quality presets and game-declared quality schemas; screenshots; asset
  registry; diagnostics.
- Enhanced targets `CnaEngine::EnhancedGraphics` (PBR binder over CNA's
  `PbrEffect`) and `CnaEngine::ExtendedIntegration` (cna-extended World3D
  screen, camera sync, debug overlay).
- Pilot-validated against car-simulator and street-simulator through
  read-only equivalence probes (`docs/pilots/`), then adopted by both on
  their `engine-adoption` branches: car-simulator's input, physics clock,
  screenshots, caches, quality tiers and world renderer, street-simulator's
  scene renderer, presets and material cache run on the engine
  (`docs/pilots/migration.md`), merged into their main branches on
  2026-10-20. Stable since then (both pilots use them): `Graphics/Camera`,
  `GpuMesh`, `RenderStatistics`, `Material`, `RenderQueue`, `Visibility`,
  `ForwardRenderer`, `QualitySchema`, `Assets/AssetRegistry`.

API snapshot baseline taken 2026-10-19 (12 headers); extended 2026-10-20 to 20
headers as the render facilities, the fixed-step clock and the PBR binder
became pilot-validated (their changes of that day are listed below);
`Graphics/RenderStatistics.hpp` joined on 2026-10-20 (21 headers), and
`Graphics/MeshBuilder.hpp` and `Graphics/TextureAtlas.hpp` (its `UvRect`)
the same day, when galaxy-eggbert moved onto them (23 headers).

### Pilot-validated API changes

- `Graphics/QualitySchema.hpp`, additive: `PresetValues(name)` — the values
  a preset was defined with, nothing else. Reason: street-simulator's presets
  are overlays (a preset leaves the settings it does not list as they are),
  which `Resolve` cannot express.

- `Graphics/Camera.hpp`, additive: `SetMatrices(view, projection)` and
  `ProjectionKind::Custom` — a game's own matrices used verbatim, the pose
  (and, for perspective matrices, the projection parameters) derived from
  them; pose setters return to a derived view, projection setters to a
  derived projection. Reason: car-simulator keeps its own `CameraPose`
  (changing it would move every matrix by a rounding step) and renders its
  world through the engine with exactly its matrices (PILOT-002, migration
  step 8).

- `Graphics/Camera.hpp`, additive: `SetMirrored`/`Mirrored` — the projection
  flips clip-space X (the frustum does not), and `ForwardRenderer` culls
  clockwise for mirrored cameras. Reason: car-simulator's rear-view mirrors.

- `Graphics/GpuMesh.hpp`: added `Borrow(const ModelMeshPart&, ...)`,
  `BaseVertex()` and `StartIndex()`; borrowed meshes now draw from a vertex
  offset and start index. Reason: Street's meshes share buffers through model
  parts (MILESTONE-2 scene comparison). Additive; existing calls unchanged.

- Skinning (ENG-032), additive: `VertexAttribute::BlendIndices` and
  `BlendWeights` with `MeshData::blendIndices`/`blendWeights` (and
  `BuilderVertex` fields); `VertexLayout::PositionNormalTextureSkinned()` and
  `PositionNormalTangentTextureSkinned()` (CNA's skinned vertex layouts,
  `Byte4` element format); `RenderItem::bones` (borrowed palette, at most
  72); the stock binder draws skinned items with XNA's `SkinnedEffect`, the
  PBR binder with CNA's `SkinnedPbrEffect`. Reason: Street's pedestrians are
  skinned, and static, dynamic, skinned and instanced submissions are to share
  one render item.
- `Graphics/GpuMesh.hpp`: the buffer overload of `Borrow` takes an optional
  `baseVertex` and `startIndex` (trailing, defaulted: existing calls are
  unchanged). Reason: Street's imported skinned meshes share buffers.

- Per-item environments (ENG-035 step 5), additive: `RenderItem::userData`
  (the game's own data, never read by the engine) and
  `PbrMaterialBinder::SetEnvironmentSelector` (an item's reflection probe
  replaces `PbrSettings::environment`; rebinding only when it changes).
  Reason: Street lights items from their nearest reflection probe.

- `Input/InputNames.hpp`, additive: `KeyDisplayName`, `ButtonDisplayName`
  and `DisplayName(Binding / AxisBinding)` — English labels for settings
  screens and prompts, identical to car-simulator's own labels where it has
  them. Reason: rebinding screens need text for people, not identifiers.

- `Material::fog` (default true), additive: glows and light pools opt out
  of the scene's fog in both binders. Reason: car-simulator's street-lamp
  light is unfogged.

- `Material::discardBelowCutoff` (default false), additive: blended
  materials can also discard below `alphaCutoff` (stock binder:
  AlphaTestEffect; PBR binder: Mask mode). Reason: car-simulator relights its
  tree and sign cards additively under the headlights.

- Reflective materials, additive: `Material::environmentMap`,
  `environmentMapAmount`, `environmentMapSpecular`, `fresnelFactor`; the stock
  binder draws them with XNA's `EnvironmentMapEffect` (pixel-identical to the
  effect configured by hand), the PBR binder reports them as ignored. Reason:
  car-simulator paints its cars with `EnvironmentMapEffect`.

- `Graphics/Camera.hpp`, additive: `SetLookAt(position, target, up)` — the
  view is XNA's `Matrix::CreateLookAt` of exactly these arguments (the
  orientation follows it). Reason: galaxy-eggbert's camera was easy-3d's
  `Camera3D`, a position/target camera; its matrices must not move when it
  moves to the engine (PILOT-004, `Easy3DEquivalenceTests`).
- `Graphics/CameraControllers.hpp`, additive: `OrbitCameraController`
  (`OrbitSettings`) and `FollowCameraController` — easy-3d's `OrbitCamera`
  and `FollowCamera`, same positions bit for bit, now driving a `Camera`.
  Reason: PILOT-004. Later the same day, additive:
  `FollowCameraController::SetPosition` (a cut to any point) and `Apply`
  (place the camera without moving on) — easy-3d's `FollowCamera::SetPosition`
  and `ApplyTo`, the only easy-3d features the first pass had left out.
- `Graphics/GpuMesh.hpp`, additive: `Draw(device, effect, statistics)` — the
  mesh drawn once per pass of the effect's current technique, for games that
  configure their own effect (easy-3d's mesh renderers). Reason: PILOT-004.

- `Graphics/GpuMesh.hpp`, additive: `Borrow(vertices, primitiveCount,
  bounds, attributes, startVertex)` — a borrowed non-indexed triangle list
  drawn with `DrawPrimitives` — and `Indexed()`. `BaseVertex()` is the start
  vertex of such a list. XNA instances indexed meshes only:
  `InstanceSet::DrawHardware` rejects a non-indexed mesh, and
  `ForwardRenderer` draws one instanced on HiDef once per instance. Reason:
  backrooms-simulator streams plain vertex lists into pooled buffers.

- `Graphics/RenderQueue.hpp`: an unused private member removed (no API
  change; the header's fingerprint changes). Reason: Clang reports it
  (`-Wunused-private-field`), which failed the engine's `-Werror` build with
  Clang (Emscripten, Android NDK, macOS).

### Other API additions (experimental headers)

- `Graphics/TextureAtlas.hpp`: `TextureAtlas`, `AtlasRect`, `UvRect` (named
  regions in pixels, normalized coordinates, frame grids) — easy-3d's atlas.
- `Graphics/MeshBuilder.hpp`: `AddBox(min, max, BoxFaces)` (per-face
  visibility and texture region; `AddBox(min, max)` is now its full-texture
  case, same vertices as before), `AddDoubleSidedQuad`, `AddPlate`
  (`PlateAxis`), `AddCrossedPlates` and `AddBillboard` — easy-3d's cube,
  plate, triple-cross and billboard meshes, identical vertices and indices.
- `Graphics/DebugDraw.hpp`: `DebugDraw` (lines and box outlines drawn with
  a vertex-colored `BasicEffect`, device state restored) — easy-3d's
  `DebugDraw` queue, which had no renderer of its own.

- `Graphics/GpuMemory.hpp`: `GpuMemoryEstimate` (meshes, textures, materials,
  queues; shared resources once) and `TextureBytes`; `DiagnosticsSources::memoryEstimate`.

- `RenderItem::box`, `cullDistance`, `instances`; `VisibilitySettings::minDistance`;
  `Classify(const RenderItem&, ...)`, `DistanceToBox`.
- `Material::keyLight`; `InstanceSet::VisibleWorlds()`.
- `ForwardLighting::fillLights` (up to two, with specular) and
  `perPixelLighting`; `DirectionalLightDescription`; `Material::secondaryTexture`
  (XNA `DualTextureEffect` in the stock binder). `keyLight == false` now
  switches off only the key light; fill lights and ambient stay (Street's
  `sunlit`, car-simulator's three-light rig).
- `Material::depthBias` and `slopeScaleDepthBias` (decals such as road markings).
- `Core/FixedStepClock.hpp`: fixed-step simulation policy (car-simulator's
  vehicle step loop).
- `BlendMode::PremultipliedAlpha` (glass: `color + behind * (1 - alpha)`) and
  `IMaterialBinder::BlendStateFor`: the binder, which knows whether its effect
  writes premultiplied or straight color, chooses the blend state of
  Transparent and Overlay items. Fix: PBR transparent surfaces were blended
  with XNA's premultiplied `AlphaBlend` although CNA's `PbrEffect` writes
  straight alpha, so they came out too bright; they now use
  `NonPremultiplied`. Overlay items now honor `Additive` materials.
- `Material::sampler` and `secondarySampler` (borrowed XNA sampler states,
  LinearWrap when unset): binders set them on every texture slot they
  sample, and `ForwardRenderer::Render` restores samplers 0–4 afterwards.
  Before, draws inherited whatever sampler the previous drawer had left.
- `CullStatistics::Record` (for culling loops that call `Classify`
  themselves), `LodStatistics` and `DiagnosticsSources::lod` ("lod 0: …, 1: …").
- `PbrSettings::environment` (image-based lighting) and
  `PbrSettings::exposure`, which replaces `lightIntensity` and also scales
  the ambient color and the environment (Street's exposure).
