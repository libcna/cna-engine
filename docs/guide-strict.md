# Guide: strict profile

For games that stay on the XNA 4.0 surface of CNA (no CNA extensions, no
cna-extended) — the car-simulator kind. Link `CnaEngine::Core` and
`CnaEngine::Graphics` only; include only `CnaEngine/{Core,Input,Diagnostics,
Graphics,Assets}/` headers. Everything here passes car-simulator's own strict
checker with the one ADR-001 include allowance.

## 1. Build integration

```cmake
# CNA (and Sharp Runtime) as source trees, as every CNA game does today.
add_subdirectory(../cna ${CMAKE_BINARY_DIR}/_cna EXCLUDE_FROM_ALL)
# Either the engine sources ...
add_subdirectory(../cna-engine ${CMAKE_BINARY_DIR}/_cna_engine)
# ... or an installed engine:
# find_package(CnaEngine 0.1 REQUIRED COMPONENTS Core Graphics)
target_link_libraries(my_game PRIVATE CnaEngine::Core CnaEngine::Graphics)
```

With the source tree, set `-DCNA_ENGINE_WITH_EXTENDED=OFF
-DCNA_ENGINE_WITH_ENHANCED_GRAPHICS=OFF` to keep enhanced code out of the build
entirely. `tests/consumer/` is a complete, working example of the installed
form.

## 2. Where the engine fits in the XNA game loop

`examples/quick_start/main.cpp` is this section as a complete, running game.

```cpp
class MyGame : public Microsoft::Xna::Framework::Game
{
    CnaEngine::Input::ActionMap actions_;
    CnaEngine::Graphics::CameraInputAxes axes_;
    CnaEngine::Graphics::Camera camera_;
    CnaEngine::Graphics::FlyCameraController fly_;
    std::unique_ptr<CnaEngine::Graphics::ForwardRenderer> renderer_;
    CnaEngine::Graphics::RenderQueue queue_;
    std::vector<CnaEngine::Graphics::RenderItem> scene_;

    void LoadContent() override
    {
        auto& device = getGraphicsDeviceProperty();
        renderer_ = std::make_unique<CnaEngine::Graphics::ForwardRenderer>(device);
        // build meshes, upload GpuMeshes, fill scene_ (see 4.)
    }

    void Update(GameTime& time) override
    {
        const float dt = static_cast<float>(time.getElapsedGameTimeProperty().getTotalSecondsProperty());
        actions_.Update(CnaEngine::Input::InputSnapshot::Capture(getIsActiveProperty()));
        fly_.Update(camera_, CnaEngine::Graphics::ReadCameraInput(actions_, axes_, dt), dt);
        Game::Update(time);
    }

    void Draw(const GameTime& time) override
    {
        auto& device = getGraphicsDeviceProperty();
        device.Clear(Color::CornflowerBlue);
        queue_.Clear();
        CnaEngine::Graphics::CullStatistics culling;
        CnaEngine::Graphics::CullInto(scene_, camera_, {}, queue_, culling);
        queue_.Sort(camera_.Position());
        renderer_->Render(queue_, camera_);
        Game::Draw(time);
    }
};
```

The engine never owns the loop: `Game` stays the application, and every engine
object is a plain member you create, update and draw.

## 3. Input

- Declare actions and axes once (`AddAction`, `AddAxis`); bind keys, mouse
  buttons, pad buttons, analog thresholds (`Binding::ForAxis`) and axes
  (`AxisBinding::ForKeys/ForGamePad/ForMouse`, with dead zone, scale and
  response exponent).
- Call `Update` exactly once per game update with one snapshot. `Pressed` and
  `Released` are action-level edges; focus loss releases everything and
  suppresses keys still held when focus returns.
- Persist with `SaveBindings`/`LoadBindings`; build a rebinding screen with
  `BindingCapture`; warn about shared keys with `FindConflicts`.

## 4. Geometry and drawing

1. Build CPU geometry with `MeshBuilder` (or fill `MeshData` streams yourself;
   winding is XNA's: clockwise front faces).
2. `GpuMesh::Upload(device, mesh, VertexLayout::PositionNormalTexture())`
   converts explicitly; missing attributes throw unless you pass
   `MissingAttributePolicy::UseDefaults`, and `ConversionReport()` lists what
   was filled, dropped or narrowed. On a Reach device a mesh needing 32-bit
   indices is rejected up front.
3. Describe surfaces with `Material` (unlit/lit, blend mode, textures you own
   and the sampler state to read them with — LinearWrap unless you set
   `sampler`, never whatever the device had).
4. Per frame: `RenderItem`s → `CullInto` (layer/distance/frustum) →
   `RenderQueue::Sort` → `ForwardRenderer::Render`. Instanced groups set
   `RenderItem::instances` (an `InstanceSet`), skinned meshes
   `RenderItem::bones` (a borrowed palette of at most 72 matrices, drawn with
   XNA's `SkinnedEffect`; the mesh needs `BlendIndices` and `BlendWeights`,
   e.g. `VertexLayout::PositionNormalTextureSkinned()`).

Ownership: `GpuMesh` owns its buffers (move-only; `Borrow` wraps buffers owned
elsewhere, such as a `ModelMeshPart`). Materials and textures are borrowed by
render items and must outlive the frame. After `Render` the device's blend,
depth and rasterizer states and samplers 0–4 are what they were before.

## 5. Extension points

- **Custom passes:** `ForwardRenderer::AddPass(PassPoint, name, callback)` —
  draw anything with CNA directly before/after the opaque, transparent or
  overlay buckets.
- **Your own effects:** implement `IMaterialBinder` and
  `SetMaterialBinder(...)`; the renderer keeps ordering and states, your
  binder sets the effect and answers `BlendStateFor` (XNA's stock effects
  write premultiplied color; an effect that writes straight alpha wants
  `NonPremultiplied`).
- **A second lighting pass:** a pass with its own lights or fog (car
  headlights relighting what they hit) is another `ForwardRenderer` with its
  own `ForwardLighting`, run from a custom pass of the main one over a queue
  of additive materials.
- **Mirrors and reflections:** render a camera with `SetMirrored(true)` into
  a render target; the renderer flips its culling, and the target can be
  drawn afterwards with `SpriteBatch` (`examples/quick_start`).
- **Your own camera behaviour:** implement `ICameraController` (chase,
  cockpit, cinematic stay in the game).
- **Direct access:** every engine object exposes the CNA objects it wraps
  (`GpuMesh::Vertices()/Indices()`, `ForwardRenderer::LitEffect()`).

## 6. Fallbacks you can rely on

Nothing is silently ignored. The stock binder reports, once per frame and
material, in `ForwardRenderer::LastFallbacks()`: lit material without normals
(drawn unlit), PBR on stock effects (drawn lit), textures without UVs or
disposed, vertex colors without a Color stream, alpha-tested surfaces (unlit
in XNA), premultiplied (glass) blending with alpha below 1 (stock effects
scale the color by alpha). Captures report failure as values (`CaptureResult::error`), asset
loads as `AssetRegistry::Failures()` (or a fallback asset you set).

## 7. Settings, assets, diagnostics

- Quality: declare your settings in a `QualitySchema` (typed, bounded, with
  device limits), define presets, `Resolve` against
  `DeviceCapabilities::FromDevice(device)`; `decisions` explains every change.
- Assets: `AssetRegistry` with per-type loaders (`UseContentManager<T>()`),
  `GetOrCreate` for derived assets, `SetNormalizeKeys(false)` for text keys.
- Simulation: `FixedStepClock` steps game logic at a fixed rate whatever the
  frame rate (frame clamp, step cap, `Alpha()` for interpolation).
- Rebinding screens: `DisplayName(binding)`, `KeyDisplayName`,
  `ButtonDisplayName` give English labels ("Left Shift", "LB"); binding files
  use `FormatBinding`/`ParseBinding`.
- Diagnostics: `FrameStatistics`, `CpuProfiler` + `ScopedTimer` (free when
  disabled), `RenderStatistics`, `CullStatistics` and `LodStatistics`,
  `GpuMemoryEstimate` (what a scene or frame keeps resident, shared buffers
  once), `FormatDiagnostics` for text output, `ScreenshotQueue` for
  deterministic captures.
