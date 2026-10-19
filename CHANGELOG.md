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

API snapshot baseline taken 2026-10-19 (12 headers).

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
