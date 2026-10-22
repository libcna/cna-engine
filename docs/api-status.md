# Public API status

All facilities are `experimental` until both pilots exercise them (see
`conventions.md`). `pilot-validated` means equivalence probes against the
pilots' own code pass (`docs/pilots/`); `stable` additionally requires
adoption in the pilots themselves (PILOT-003). "Evidence" names the existing implementations that shaped
the API; "Consumers" lists code that uses the engine facility today.

Both pilots use the engine in their own code since 2026-10-20: their
`engine-adoption` branches were merged into car-simulator's `main` and
street-simulator's `develop` (`docs/pilots/migration.md`). "car-simulator
branch" and "Street branch" in the Consumers column mark that use. "backrooms
branch" and "school branch" mark backrooms-simulator's and
school-simulator's `engine-adoption` branches (PILOT-005, merged into both
games' `main` and pushed on 2026-10-21). "train branch" marks
train-simulator's `engine-adoption` branch (PILOT-006, merged into its
`develop` and pushed on 2026-10-21). "car shared", "school shared", "train
shared" and "street shared" mark those games' `engine-shared` branches
(SHARE-001 adoption, 2026-10-21, merged and pushed on 2026-10-22). The nine
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
| Action map (actions, axes, rebinding, conflicts, focus loss, persistence) | `Input/ActionMap.hpp` | Core | pilot-validated (both) | car-simulator `InputMapper` (key names, overrides, pad snapshot) | tests, example, car-simulator equivalence probe, car-simulator branch, backrooms branch, school branch, train branch |
| Binding names and text format | `Input/InputNames.hpp` | Core | experimental | car-simulator `KeyName`/`KeyFromName` | tests |
| Binding capture (rebinding prompt) | `Input/BindingCapture.hpp` | Core | experimental | — | tests |
| Frame statistics | `Diagnostics/FrameStatistics.hpp` | Core | experimental | Street/car-simulator benchmark overlays | tests, example, backrooms branch |
| CPU profiler, scoped timers | `Diagnostics/ScopedTimer.hpp` | Core | experimental | — | tests |
| Camera state | `Graphics/Camera.hpp` | Graphics | stable | Street `Render/Camera`, car-simulator `Render/Camera`, cna-extended `Camera3DEXT`, easy-3d `Camera3D` (`SetLookAt`) | tests, example, car-simulator branch, Street branch, easy-3d equivalence probe, galaxy-eggbert branch, backrooms branch, school branch, train branch |
| Fly/walk controllers | `Graphics/CameraControllers.hpp` | Graphics | pilot-validated (fly: Street) | Street `CameraController`, car-simulator walk mode | tests, example |
| Orbit/follow controllers | `Graphics/CameraControllers.hpp` | Graphics | experimental | easy-3d `OrbitCamera`, `FollowCamera` | tests, easy-3d equivalence probe |
| CPU mesh data | `Graphics/MeshData.hpp` | Graphics | pilot-validated (both) | Street `Geometry/MeshData`, car-simulator `Render/MeshData` | tests, example |
| Mesh builder and primitives | `Graphics/MeshBuilder.hpp` | Graphics | pilot-validated (galaxy-eggbert) | Street `MeshBuilder`, car-simulator `MeshData::Add*`, easy-3d `CubeMesh`/`BillboardMesh` (faced boxes, plates, crossed plates, billboards) | tests, example, easy-3d equivalence probe, galaxy-eggbert branch |
| Procedural images and mip chains (box filter, sRGB, alpha-test coverage, upload) | `Graphics/Image.hpp` | Graphics | experimental | car-simulator `Render/Image`, train-simulator `ProceduralImage`/`uploadMipmappedTexture`, school-/museum-simulator `ImageUtil`, room-simulator `Assets/Image::halved`, backrooms- and island-simulator mip loops | tests, image equivalence probe (frozen car, train and school code; room), car shared, school shared, train shared |
| Noise (hash, value, gradient, fBm, ridged, Worley) | `Core/Noise.hpp` | Core | experimental | street-simulator `Assets/Noise` (also car-, room-, backrooms-, island-simulator noise with their own hashes) | tests, image equivalence probe (frozen street code), street shared |
| Sun position (declination, hour angle, elevation, azimuth, direction) | `Core/SunPosition.hpp` | Core | experimental | car-simulator `LightingRig::SetTimeOfDay`, room-simulator `TimeOfDay`, house-simulator `SunPositionAt` | tests, sun equivalence probe (car, room, house) |
| Settings files (key = value, typed clamped reads, atomic save, XDG location) | `Core/SettingsFile.hpp` | Core | experimental | train-simulator `Config::Settings` and its config discovery; island-simulator `UserSettingsStore` (`settings.ini` through XNA storage) | tests, settings equivalence probe (frozen train code), train shared |
| Automated runs (frame and time limits, lockstep, last-frame screenshot) | `Graphics/AutomatedRun.hpp` | Graphics | experimental | car-simulator `--frames`/`--lockstep`/`--screenshot` (`SimulatorGame::Update`, `FinishFrame`), train-simulator `--frames`/`--run-seconds`/`--lockstep`/`--screenshot` (avatar warm-up, capture after Present); school-, museum-, street-, room-, house-, island- and backrooms-simulator capture options | tests, train shared |
| Texture atlas regions | `Graphics/TextureAtlas.hpp` | Graphics | experimental (galaxy-eggbert uses `UvRect` only) | easy-3d `TextureAtlas` | tests, easy-3d equivalence probe |
| Vertex layouts and interleaving | `Graphics/VertexLayouts.hpp` | Graphics | pilot-validated (both) | car-simulator `VertexLayout`, Street tangent vertices | tests |
| GPU mesh | `Graphics/GpuMesh.hpp` | Graphics | stable | Street `Render/GpuMesh`, car-simulator `Render/GpuMesh`, easy-3d mesh renderers (`Draw` with an effect), backrooms-simulator pooled vertex lists (non-indexed `Borrow`) | tests, example, car-simulator branch, Street branch, galaxy-eggbert branch (`Draw` with an effect), backrooms branch (non-indexed `Borrow`), school branch, train branch (streamed segments) |
| GPU memory estimate (meshes, textures, materials, queues) | `Graphics/GpuMemory.hpp` | Graphics | experimental | Street `GpuMesh::gpuBytes`, `MaterialLibrary::textureBytes` | tests, Street scene comparison |
| Render and GPU memory statistics | `Graphics/RenderStatistics.hpp` | Graphics | stable | car-simulator `GpuMesh::Submissions` | tests, example, both scene comparisons (draw calls equal the pilots' own counters), car-simulator branch, Street branch, backrooms branch, school branch |
| Debug lines and box outlines | `Graphics/DebugDraw.hpp` | Graphics | experimental | easy-3d `DebugDraw` (queue only) | tests |
| Screen capture and screenshot queue | `Graphics/ScreenCapture.hpp` | Graphics | pilot-validated (car-simulator) | car-simulator `Render/Screenshot` | tests, example, car-simulator branch, backrooms branch, school branch (opaque), train branch |
| Materials | `Graphics/Material.hpp` | Graphics | stable | Street material descriptions, car-simulator effect setup | tests, both scene comparisons, car-simulator branch, Street branch, backrooms branch, school branch (counter-clockwise faces, straight alpha), train branch (read by its own binder) |
| Render items and queue (buckets, deterministic sorting) | `Graphics/RenderQueue.hpp` | Graphics | stable | Street render submission model, train-simulator's hand-ordered passes (`Sequenced`) | tests, both scene comparisons, car-simulator branch, Street branch, backrooms branch, school branch, train branch (`Sequenced`) |
| Visibility policies and LOD selection | `Graphics/Visibility.hpp` | Graphics | stable | Street culling/LOD, island-simulator culling | tests, both scene comparisons, car-simulator branch, Street branch |
| Skinned render items (bone palettes, skinned layouts) | `Graphics/RenderQueue.hpp`, `Graphics/VertexLayouts.hpp` | Graphics | pilot-validated (Street) | Street `SceneRenderer::drawSkinned` | tests, Street scene comparison, Street branch |
| Forward renderer (buckets, passes, state restore, fallbacks) | `Graphics/ForwardRenderer.hpp` | Graphics | stable | Street `SceneRenderer`, car-simulator world renderer | tests, both scene comparisons, car-simulator branch, Street branch, backrooms branch, school branch, train branch |
| Material binders (interface, stock-effect binder) | `Graphics/MaterialBinder.hpp` | Graphics | pilot-validated (stock: car-simulator) | Street `SceneRenderer::applyMaterial` | tests, car-simulator scene comparison, car-simulator branch (stock binder), backrooms branch (stock binder), school branch (stock binder), train branch (its own binder over its effects) |
| PBR material binder (CNA `PbrEffect`) | `Enhanced/PbrMaterialBinder.hpp` | EnhancedGraphics | pilot-validated (Street) | Street `SceneRenderer::applyMaterial`, `Material` | tests, Street scene comparison, Street branch |
| Quality presets and capability resolution | `Graphics/Quality.hpp` | Graphics | experimental | car-simulator `QualityTier`, island-simulator quality profiles | tests, street_slice |
| Quality schema (game-declared settings, presets, limits, persistence) | `Graphics/QualitySchema.hpp` | Graphics | stable | car-simulator `QualityTier`, Street `RenderSettings` | tests, pilot probes, car-simulator branch, Street branch |
| Instancing (hardware + per-instance fallback) | `Graphics/Instancing.hpp` | Graphics | pilot-validated (Street) | Street `InstancedMesh` | tests, Street instancing probe and scene comparison, Street branch |
| Asset registry | `Assets/AssetRegistry.hpp` | Graphics | stable | Street `Assets/*Library`, house-simulator caches | tests, car-simulator branch, Street branch, school branch |
| World3D screen driven by engine input/camera | `Extended/EngineWorldScreen.hpp` | ExtendedIntegration | experimental | cna-extended `World3DScreenEXT` | tests, consumer probe |
| World3D camera sync | `Extended/WorldCameraSync.hpp` | ExtendedIntegration | experimental | cna-extended `Camera3DEXT` | tests |
| Game build function (desktop and browser) | `cmake/CnaEngineGame.cmake`, `cna_engine_add_game()` | (CMake) | experimental | backrooms-, school- and train-simulator CMake; backrooms, school and house browser builds | example `hello_engine` (desktop and browser) |
| Dependency fetcher | `tools/fetch_dependencies.sh` | (tool) | experimental | the sibling-checkout layout every game expects | dry-run test |
| Game loop already wired | `App/EngineGame.hpp` | App | experimental | `examples/quick_start`; backrooms', school's and train's game classes | tests, example `hello_engine` |
| Asset directory lookup | `App/AssetDirectory.hpp` | App | experimental | backrooms `FindAssetDirectory`, school `AssetPaths`, train's config discovery | tests, `EngineGame` |
| Mouse capture for mouse look | `App/MouseCapture.hpp`, `Enhanced/RelativeMouseCapture.hpp` | App, EnhancedGraphics | experimental | backrooms (relative mode), school (pointer lock in the browser) | `EngineGame` |
| Static collision world (boxes, triangles, cylinders, height field; rays, capsule contacts; grid) | `Collision/StaticWorld.hpp` | Core | experimental | school/museum boxes, car boxes and cylinders in a grid, island cylinders and height field, house triangles and grid | tests, template game |
| Character controller (collide and slide, steps, slopes, gravity, jumps, strides, head bob) | `Collision/CharacterController.hpp` | Core | experimental | house `PlayerStep`, school/museum `PlayerController`, island `PlayerMotion`, car walking mode, backrooms `MoveWithCollision` | tests, template game |
| Models as render items | `Graphics/ModelView.hpp`, `Enhanced/ModelImport.hpp` (PBR reader, skin lookup) | Graphics, EnhancedGraphics | experimental | street, room, museum `ModelLibrary`; house's compiled models | tests (imported glTF drawn through the PBR binder) |
| Skeletal animation (skeleton, clips, animator with cross-fades) | `Animation/Skeleton.hpp`, `Animation/Animator.hpp` | Core | experimental | CNA `AnimationPlayer` users street, museum, island (with its own cross-fade), cna-extended `ModelAnimationComponentEXT` | tests, CNA AnimationPlayer equivalence probe |
| Sky, time of day, sky lighting | `Graphics/Sky.hpp` | Graphics | experimental | car `SkyRenderer`/`LightingRig`, train `SkyGeometry`/`Atmosphere`, school and museum `SkyDome`, house `SkySystem` | tests, template game |
| Sun shadow map (strict producer) and reception (enhanced) | `Graphics/ShadowMap.hpp`, `Enhanced/ShadowReception.hpp`, `EngineGame::EnableShadowMap` | Graphics, App, EnhancedGraphics | experimental | room's vendored `ShadowMap`, island `SunShadowMap`, museum `ShadowLab` | tests (map depths, reception through stock and PBR binders), template game |
| Sound: positional voices, mix categories, voice budget, banks | `Audio/SoundSystem.hpp` | Audio | experimental | school and museum `AudioSystem`, street `SoundScape`, house mix categories and footsteps, island and train | tests (null audio platform), template game |
| Text (built-in font, SpriteFont), HUD painter, toasts | `UI/Font.hpp`, `UI/Painter.hpp` | UI | experimental | island, room, car, train 5x7 tables; school and museum `Hud`; house `TextRenderer` | tests (glyph pixels), template game |
| Menus, menu stack, settings rows, rebinding rows | `UI/Menu.hpp`, `UI/SettingsMenu.hpp` | UI | experimental | house `MenuStack` and settings page, train start menu, galaxy-eggbert menus, car binding overrides | tests, template game |
| Post-processing chain (grade, fade, vignette, custom passes) | `Graphics/PostProcess.hpp`, `EngineGame::EnablePostProcessing` | Graphics, App | experimental | island's time-of-day grade, room's post chain (shader passes) | tests |
| Local lights: reach, strongest lamp, light pools (strict); punctual lights per item (enhanced) | `Graphics/LocalLights.hpp`, `Enhanced/LocalLightBinder.hpp` | Graphics, EnhancedGraphics | experimental | car lamp pools and wet-road glow, island lamp glow, museum `LightingLab`, room point lights | tests |
| cna-extended 3D particles as render items | `Extended/ParticleBillboards.hpp` | ExtendedIntegration | experimental | cna-extended `ParticleRenderSystem3DEXT` | tests |
