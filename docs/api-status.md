# Public API status

All facilities are `experimental` until both pilots exercise them (see
`conventions.md`). `pilot-validated` means equivalence probes against the
pilots' own code pass (`docs/pilots/`); `stable` additionally requires
adoption in the pilots themselves (PILOT-003). "Evidence" names the existing implementations that shaped
the API; "Consumers" lists code that uses the engine facility today.

| Facility | Header | Target | Status | Evidence | Consumers |
| --- | --- | --- | --- | --- | --- |
| Version constants | `Core/Version.hpp` | Core | experimental | — | tests |
| Fixed-step clock | `Core/FixedStepClock.hpp` | Core | pilot-validated (car-simulator) | car-simulator `Vehicle::Update` step loop | tests, car-simulator equivalence probe |
| Input snapshot | `Input/InputSnapshot.hpp` | Core | experimental | car-simulator `InputMapper::Update` | tests |
| Action map (actions, axes, rebinding, conflicts, focus loss, persistence) | `Input/ActionMap.hpp` | Core | pilot-validated (both) | car-simulator `InputMapper` (key names, overrides, pad snapshot) | tests, example, car-simulator equivalence probe |
| Binding names and text format | `Input/InputNames.hpp` | Core | experimental | car-simulator `KeyName`/`KeyFromName` | tests |
| Binding capture (rebinding prompt) | `Input/BindingCapture.hpp` | Core | experimental | — | tests |
| Frame statistics | `Diagnostics/FrameStatistics.hpp` | Core | experimental | Street/car-simulator benchmark overlays | tests, example |
| CPU profiler, scoped timers | `Diagnostics/ScopedTimer.hpp` | Core | experimental | — | tests |
| Camera state | `Graphics/Camera.hpp` | Graphics | pilot-validated (both) | Street `Render/Camera`, car-simulator `Render/Camera`, cna-extended `Camera3DEXT` | tests, example |
| Fly/walk controllers | `Graphics/CameraControllers.hpp` | Graphics | pilot-validated (fly: Street) | Street `CameraController`, car-simulator walk mode | tests, example |
| CPU mesh data | `Graphics/MeshData.hpp` | Graphics | pilot-validated (both) | Street `Geometry/MeshData`, car-simulator `Render/MeshData` | tests, example |
| Mesh builder and primitives | `Graphics/MeshBuilder.hpp` | Graphics | experimental | Street `MeshBuilder`, car-simulator `MeshData::Add*` | tests, example |
| Vertex layouts and interleaving | `Graphics/VertexLayouts.hpp` | Graphics | pilot-validated (both) | car-simulator `VertexLayout`, Street tangent vertices | tests |
| GPU mesh | `Graphics/GpuMesh.hpp` | Graphics | pilot-validated (both) | Street `Render/GpuMesh`, car-simulator `Render/GpuMesh` | tests, example |
| GPU memory estimate (meshes, textures, materials, queues) | `Graphics/GpuMemory.hpp` | Graphics | experimental | Street `GpuMesh::gpuBytes`, `MaterialLibrary::textureBytes` | tests, Street scene comparison |
| Render and GPU memory statistics | `Graphics/RenderStatistics.hpp` | Graphics | pilot-validated (both) | car-simulator `GpuMesh::Submissions` | tests, example, both scene comparisons (draw calls equal the pilots' own counters) |
| Screen capture and screenshot queue | `Graphics/ScreenCapture.hpp` | Graphics | pilot-validated (car-simulator) | car-simulator `Render/Screenshot` | tests, example |
| Materials | `Graphics/Material.hpp` | Graphics | pilot-validated (both) | Street material descriptions, car-simulator effect setup | tests, both scene comparisons |
| Render items and queue (buckets, deterministic sorting) | `Graphics/RenderQueue.hpp` | Graphics | pilot-validated (both) | Street render submission model | tests, both scene comparisons |
| Visibility policies and LOD selection | `Graphics/Visibility.hpp` | Graphics | pilot-validated (both) | Street culling/LOD, island-simulator culling | tests, both scene comparisons |
| Skinned render items (bone palettes, skinned layouts) | `Graphics/RenderQueue.hpp`, `Graphics/VertexLayouts.hpp` | Graphics | pilot-validated (Street) | Street `SceneRenderer::drawSkinned` | tests, Street scene comparison |
| Forward renderer (buckets, passes, state restore, fallbacks) | `Graphics/ForwardRenderer.hpp` | Graphics | pilot-validated (both) | Street `SceneRenderer`, car-simulator world renderer | tests, both scene comparisons |
| Material binders (interface, stock-effect binder) | `Graphics/MaterialBinder.hpp` | Graphics | pilot-validated (stock: car-simulator) | Street `SceneRenderer::applyMaterial` | tests, car-simulator scene comparison |
| PBR material binder (CNA `PbrEffect`) | `Enhanced/PbrMaterialBinder.hpp` | EnhancedGraphics | pilot-validated (Street) | Street `SceneRenderer::applyMaterial`, `Material` | tests, Street scene comparison |
| Quality presets and capability resolution | `Graphics/Quality.hpp` | Graphics | experimental | car-simulator `QualityTier`, island-simulator quality profiles | tests, street_slice |
| Quality schema (game-declared settings, presets, limits, persistence) | `Graphics/QualitySchema.hpp` | Graphics | pilot-validated (both) | car-simulator `QualityTier`, Street `RenderSettings` | tests, pilot probes |
| Instancing (hardware + per-instance fallback) | `Graphics/Instancing.hpp` | Graphics | pilot-validated (Street) | Street `InstancedMesh` | tests, Street instancing probe and scene comparison |
| Asset registry | `Assets/AssetRegistry.hpp` | Graphics | pilot-validated (both) | Street `Assets/*Library`, house-simulator caches | tests |
| World3D screen driven by engine input/camera | `Extended/EngineWorldScreen.hpp` | ExtendedIntegration | experimental | cna-extended `World3DScreenEXT` | tests, consumer probe |
| World3D camera sync | `Extended/WorldCameraSync.hpp` | ExtendedIntegration | experimental | cna-extended `Camera3DEXT` | tests |
