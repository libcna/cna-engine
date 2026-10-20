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
  read-only equivalence probes (`docs/pilots/`).

API snapshot baseline taken 2026-10-19 (12 headers); extended 2026-10-20 to 20
headers as the render facilities, the fixed-step clock and the PBR binder
became pilot-validated (their changes of that day are listed below);
`Graphics/RenderStatistics.hpp` joined on 2026-10-20 (21 headers).

### Pilot-validated API changes

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

### Other API additions (experimental headers)

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
