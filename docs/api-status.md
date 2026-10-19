# Public API status

All facilities are `experimental` until both pilots exercise them (see
`conventions.md`). "Evidence" names the existing implementations that shaped
the API; "Consumers" lists code that uses the engine facility today.

| Facility | Header | Target | Status | Evidence | Consumers |
| --- | --- | --- | --- | --- | --- |
| Version constants | `Core/Version.hpp` | Core | experimental | — | tests |
| Input snapshot | `Input/InputSnapshot.hpp` | Core | experimental | car-simulator `InputMapper::Update` | tests |
| Action map (actions, axes, rebinding, conflicts, focus loss, persistence) | `Input/ActionMap.hpp` | Core | experimental | car-simulator `InputMapper` (key names, overrides, pad snapshot) | tests, example |
| Binding names and text format | `Input/InputNames.hpp` | Core | experimental | car-simulator `KeyName`/`KeyFromName` | tests |
| Binding capture (rebinding prompt) | `Input/BindingCapture.hpp` | Core | experimental | — | tests |
| Frame statistics | `Diagnostics/FrameStatistics.hpp` | Core | experimental | Street/car-simulator benchmark overlays | tests, example |
| CPU profiler, scoped timers | `Diagnostics/ScopedTimer.hpp` | Core | experimental | — | tests |
| Camera state | `Graphics/Camera.hpp` | Graphics | experimental | Street `Render/Camera`, car-simulator `Render/Camera`, cna-extended `Camera3DEXT` | tests, example |
| Fly/walk controllers | `Graphics/CameraControllers.hpp` | Graphics | experimental | Street `CameraController`, car-simulator walk mode | tests, example |
| CPU mesh data | `Graphics/MeshData.hpp` | Graphics | experimental | Street `Geometry/MeshData`, car-simulator `Render/MeshData` | tests, example |
| Mesh builder and primitives | `Graphics/MeshBuilder.hpp` | Graphics | experimental | Street `MeshBuilder`, car-simulator `MeshData::Add*` | tests, example |
| Vertex layouts and interleaving | `Graphics/VertexLayouts.hpp` | Graphics | experimental | car-simulator `VertexLayout`, Street tangent vertices | tests |
| GPU mesh | `Graphics/GpuMesh.hpp` | Graphics | experimental | Street `Render/GpuMesh`, car-simulator `Render/GpuMesh` | tests, example |
| Render and GPU memory statistics | `Graphics/RenderStatistics.hpp` | Graphics | experimental | car-simulator `GpuMesh::Submissions` | tests, example |
| Screen capture and screenshot queue | `Graphics/ScreenCapture.hpp` | Graphics | experimental | car-simulator `Render/Screenshot` | tests, example |
| Materials | `Graphics/Material.hpp` | Graphics | experimental | Street material descriptions, car-simulator effect setup | tests |
| Render items and queue (buckets, deterministic sorting) | `Graphics/RenderQueue.hpp` | Graphics | experimental | Street render submission model | tests |
| Visibility policies and LOD selection | `Graphics/Visibility.hpp` | Graphics | experimental | Street culling/LOD, island-simulator culling | tests |
| Forward renderer over stock effects (passes, state restore, fallbacks) | `Graphics/ForwardRenderer.hpp` | Graphics | experimental | Street `SceneRenderer`, car-simulator world renderer | tests |
| Quality presets and capability resolution | `Graphics/Quality.hpp` | Graphics | experimental | car-simulator `QualityTier`, island-simulator quality profiles | tests |
| Instancing (hardware + per-instance fallback) | `Graphics/Instancing.hpp` | Graphics | experimental | Street `InstancedMesh` | tests |
| Asset registry | `Assets/AssetRegistry.hpp` | Graphics | experimental | Street `Assets/*Library`, house-simulator caches | tests |
| World3D screen driven by engine input/camera | `Extended/EngineWorldScreen.hpp` | ExtendedIntegration | experimental | cna-extended `World3DScreenEXT` | tests, consumer probe |
| World3D camera sync | `Extended/WorldCameraSync.hpp` | ExtendedIntegration | experimental | cna-extended `Camera3DEXT` | tests |
