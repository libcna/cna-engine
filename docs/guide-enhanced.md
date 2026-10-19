# Guide: enhanced profile

For games that use CNA's graphics extensions and/or cna-extended — the
street-simulator kind. Everything in the strict guide applies; this guide
covers what the enhanced targets add. Link `CnaEngine::Engine` (all targets) or
pick `CnaEngine::EnhancedGraphics` and `CnaEngine::ExtendedIntegration`
individually.

| Target | Depends on | Adds |
| --- | --- | --- |
| `CnaEngine::EnhancedGraphics` | CNA graphics extensions (`PbrEffect`) | `Enhanced::PbrMaterialBinder` |
| `CnaEngine::ExtendedIntegration` | cna-extended | `Extended::EngineWorldScreen`, `DebugOverlay`, World3D camera sync |

## 1. Physically based materials

```cpp
CnaEngine::Graphics::ForwardRenderer renderer(device);
renderer.SetMaterialBinder(std::make_shared<CnaEngine::Enhanced::PbrMaterialBinder>(
    device, renderer.StockBinder()));
renderer.Lighting().defaultLighting = false;          // one key light + ambient
renderer.Lighting().lightDirection = Vector3(0.3f, -0.8f, -0.5f);

CnaEngine::Graphics::Material brick;
brick.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
brick.baseTexture = &albedo;                          // sRGB by default
brick.normalTexture = &normals;                       // needs Tangent in the mesh
brick.occlusionRoughnessMetallicTexture = &orm;       // glTF packing
brick.roughness = 0.8f;
```

Upload PBR meshes with `VertexLayout::PositionNormalTangentTexture()` after
`MeshData::ComputeTangents()` (or with your authored tangents). The binder maps
material fields one-to-one to `PbrEffect` (the same mapping street-simulator's
renderer performs), selects the alpha mode from the render bucket, and:

- draws unlit and vertex-colored materials through the stock binder;
- reports, not hides, a normal map on a mesh without tangents, textures without
  UVs, and meshes without normals.

Image-based lighting, shadows and reflection probes are not managed: derive
your own binder (or use `PbrMaterialBinder::Effect()`) where your game needs
them.

## 2. cna-extended screens and World3D

`EngineWorldScreen` is a `World3DScreenEXT` (so `ScreenManager`,
transitions and World3D systems work unchanged) that adds an `ActionMap`, an
engine `Camera` and an optional camera controller. Per update, in this order:
one input snapshot (`CaptureInput()`, overridable for replays), your
`UpdateScreen()`, the controller, camera sync into `GetCamera3DEXT()`, then the
World3D systems. Games that want another order use `SyncToWorldCamera`
directly with their own screen.

## 3. Debug presentation

`FormatDiagnostics()` gives the metrics as text without drawing. With
cna-extended, `DebugOverlay` draws them: a panel, a frame-time graph (bars over
budget in a warning color) and text lines when you pass a `BitmapFont`.

## 4. Instancing

`InstanceSet` holds per-instance world matrices (64-byte stream,
TextureCoordinate 1–4, the layout street-simulator also uses). With an effect
that reads that stream on a HiDef device use `DrawHardware` (one draw, device
bindings restored); otherwise `DrawPerInstance` with any `IEffectMatrices`
effect (instances relative to the effect's World, which is restored).

## 5. Linking CNA once

Link CNA through its module targets (`CNA::Runtime`, `CNA::GraphicsCore`,
…), as the engine does. With CNA's default `CNA_SHARED_LIBRARY=ON` the `CNA`
umbrella is `libcna.so`; linking it together with the engine puts two CNA
runtimes in one process (`upstream-issues.md` UP-006). Either use the module
targets or configure CNA with `-DCNA_SHARED_LIBRARY=OFF`.

## 6. Boundaries to keep

- Strict code (yours or the engine's) must not include `CnaEngine/Enhanced/`
  or `CnaEngine/Extended/`; the strict boundary test enforces this for the
  engine.
- `EnhancedGraphics` never depends on cna-extended; only
  `ExtendedIntegration` does.
- CNA's own `CNA_CNAEXT` option is a different thing (CNA's built-in extension
  layer); neither enhanced engine target requires it.
