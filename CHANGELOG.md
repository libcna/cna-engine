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
became pilot-validated (their changes of that day are listed below).

### Pilot-validated API changes

- `Graphics/GpuMesh.hpp`: added `Borrow(const ModelMeshPart&, ...)`,
  `BaseVertex()` and `StartIndex()`; borrowed meshes now draw from a vertex
  offset and start index. Reason: Street's meshes share buffers through model
  parts (MILESTONE-2 scene comparison). Additive; existing calls unchanged.

### Other API additions (experimental headers)

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
