# Public API status

All facilities are `experimental` until both pilots exercise them (see
`conventions.md`). `pilot-validated` means equivalence probes against the
pilots' own code pass (`docs/pilots/`); `stable` additionally requires
adoption in the pilots themselves (PILOT-003). "Evidence" names the existing implementations that shaped
the API; "Consumers" lists code that uses the engine facility today.

Both pilots use the engine in their own code since 2026-10-20: their
`engine-adoption` branches were merged into car-simulator's `main` and
street-simulator's `develop` (`docs/pilots/migration.md`). "car-simulator
branch" and "Street branch" in the Consumers column mark that use. The nine
facilities both pilots use — camera (`SetMatrices`), GPU mesh (`Borrow`),
render statistics, materials, render queue, visibility and LOD, forward
renderer, quality schema and asset registry — are `stable`; their headers are
in the API snapshot, so a change to them fails `cna_engine.api_snapshot` until
it is recorded in `CHANGELOG.md`.

| Facility | Header | Target | Status | Evidence | Consumers |
| --- | --- | --- | --- | --- | --- |
| Version constants | `Core/Version.hpp` | Core | experimental | — | tests |
| Fixed-step clock | `Core/FixedStepClock.hpp` | Core | pilot-validated (car-simulator) | car-simulator `Vehicle::Update` step loop | tests, car-simulator equivalence probe, car-simulator branch |
| Input snapshot | `Input/InputSnapshot.hpp` | Core | experimental | car-simulator `InputMapper::Update` | tests |
| Action map (actions, axes, rebinding, conflicts, focus loss, persistence) | `Input/ActionMap.hpp` | Core | pilot-validated (both) | car-simulator `InputMapper` (key names, overrides, pad snapshot) | tests, example, car-simulator equivalence probe, car-simulator branch |
| Binding names and text format | `Input/InputNames.hpp` | Core | experimental | car-simulator `KeyName`/`KeyFromName` | tests |
| Binding capture (rebinding prompt) | `Input/BindingCapture.hpp` | Core | experimental | — | tests |
| Frame statistics | `Diagnostics/FrameStatistics.hpp` | Core | experimental | Street/car-simulator benchmark overlays | tests, example |
| CPU profiler, scoped timers | `Diagnostics/ScopedTimer.hpp` | Core | experimental | — | tests |
| Camera state | `Graphics/Camera.hpp` | Graphics | stable | Street `Render/Camera`, car-simulator `Render/Camera`, cna-extended `Camera3DEXT`, easy-3d `Camera3D` (`SetLookAt`) | tests, example, car-simulator branch, Street branch, easy-3d equivalence probe, galaxy-eggbert branch |
| Fly/walk controllers | `Graphics/CameraControllers.hpp` | Graphics | pilot-validated (fly: Street) | Street `CameraController`, car-simulator walk mode | tests, example |
| Orbit/follow controllers | `Graphics/CameraControllers.hpp` | Graphics | experimental | easy-3d `OrbitCamera`, `FollowCamera` | tests, easy-3d equivalence probe |
| CPU mesh data | `Graphics/MeshData.hpp` | Graphics | pilot-validated (both) | Street `Geometry/MeshData`, car-simulator `Render/MeshData` | tests, example |
| Mesh builder and primitives | `Graphics/MeshBuilder.hpp` | Graphics | pilot-validated (galaxy-eggbert) | Street `MeshBuilder`, car-simulator `MeshData::Add*`, easy-3d `CubeMesh`/`BillboardMesh` (faced boxes, plates, crossed plates, billboards) | tests, example, easy-3d equivalence probe, galaxy-eggbert branch |
| Texture atlas regions | `Graphics/TextureAtlas.hpp` | Graphics | experimental (galaxy-eggbert uses `UvRect` only) | easy-3d `TextureAtlas` | tests, easy-3d equivalence probe |
| Vertex layouts and interleaving | `Graphics/VertexLayouts.hpp` | Graphics | pilot-validated (both) | car-simulator `VertexLayout`, Street tangent vertices | tests |
| GPU mesh | `Graphics/GpuMesh.hpp` | Graphics | stable | Street `Render/GpuMesh`, car-simulator `Render/GpuMesh`, easy-3d mesh renderers (`Draw` with an effect), backrooms-simulator pooled vertex lists (non-indexed `Borrow`) | tests, example, car-simulator branch, Street branch, galaxy-eggbert branch (`Draw` with an effect) |
| GPU memory estimate (meshes, textures, materials, queues) | `Graphics/GpuMemory.hpp` | Graphics | experimental | Street `GpuMesh::gpuBytes`, `MaterialLibrary::textureBytes` | tests, Street scene comparison |
| Render and GPU memory statistics | `Graphics/RenderStatistics.hpp` | Graphics | stable | car-simulator `GpuMesh::Submissions` | tests, example, both scene comparisons (draw calls equal the pilots' own counters), car-simulator branch, Street branch |
| Debug lines and box outlines | `Graphics/DebugDraw.hpp` | Graphics | experimental | easy-3d `DebugDraw` (queue only) | tests |
| Screen capture and screenshot queue | `Graphics/ScreenCapture.hpp` | Graphics | pilot-validated (car-simulator) | car-simulator `Render/Screenshot` | tests, example, car-simulator branch |
| Materials | `Graphics/Material.hpp` | Graphics | stable | Street material descriptions, car-simulator effect setup | tests, both scene comparisons, car-simulator branch, Street branch |
| Render items and queue (buckets, deterministic sorting) | `Graphics/RenderQueue.hpp` | Graphics | stable | Street render submission model | tests, both scene comparisons, car-simulator branch, Street branch |
| Visibility policies and LOD selection | `Graphics/Visibility.hpp` | Graphics | stable | Street culling/LOD, island-simulator culling | tests, both scene comparisons, car-simulator branch, Street branch |
| Skinned render items (bone palettes, skinned layouts) | `Graphics/RenderQueue.hpp`, `Graphics/VertexLayouts.hpp` | Graphics | pilot-validated (Street) | Street `SceneRenderer::drawSkinned` | tests, Street scene comparison, Street branch |
| Forward renderer (buckets, passes, state restore, fallbacks) | `Graphics/ForwardRenderer.hpp` | Graphics | stable | Street `SceneRenderer`, car-simulator world renderer | tests, both scene comparisons, car-simulator branch, Street branch |
| Material binders (interface, stock-effect binder) | `Graphics/MaterialBinder.hpp` | Graphics | pilot-validated (stock: car-simulator) | Street `SceneRenderer::applyMaterial` | tests, car-simulator scene comparison, car-simulator branch (stock binder) |
| PBR material binder (CNA `PbrEffect`) | `Enhanced/PbrMaterialBinder.hpp` | EnhancedGraphics | pilot-validated (Street) | Street `SceneRenderer::applyMaterial`, `Material` | tests, Street scene comparison, Street branch |
| Quality presets and capability resolution | `Graphics/Quality.hpp` | Graphics | experimental | car-simulator `QualityTier`, island-simulator quality profiles | tests, street_slice |
| Quality schema (game-declared settings, presets, limits, persistence) | `Graphics/QualitySchema.hpp` | Graphics | stable | car-simulator `QualityTier`, Street `RenderSettings` | tests, pilot probes, car-simulator branch, Street branch |
| Instancing (hardware + per-instance fallback) | `Graphics/Instancing.hpp` | Graphics | pilot-validated (Street) | Street `InstancedMesh` | tests, Street instancing probe and scene comparison, Street branch |
| Asset registry | `Assets/AssetRegistry.hpp` | Graphics | stable | Street `Assets/*Library`, house-simulator caches | tests, car-simulator branch, Street branch |
| World3D screen driven by engine input/camera | `Extended/EngineWorldScreen.hpp` | ExtendedIntegration | experimental | cna-extended `World3DScreenEXT` | tests, consumer probe |
| World3D camera sync | `Extended/WorldCameraSync.hpp` | ExtendedIntegration | experimental | cna-extended `Camera3DEXT` | tests |
