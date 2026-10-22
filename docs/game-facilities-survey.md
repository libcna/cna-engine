# What the games wrote themselves (Phase 7 evidence)

A read-only survey of 2026-10-22 for `plan.md` Phase 7: how the simulators
(car, street, train, school, backrooms, room, house in `libcnalab/`; island
and museum in `libcna/`) and galaxy-eggbert do the things every 3D game
needs, what CNA and cna-extended already offer, and what an engine facility
should take over. Line counts are approximate. Nothing was changed in the
games.

## Models and skeletal animation (GAME-010, GAME-011)

| Game | Loading | Drawing | Animation |
| --- | --- | --- | --- |
| street | `.glb` compiled to `.cnb` at build time (`cna_tool_gltf_to_cnb`), `ContentManager::Load<Model>` (`Assets/ModelLibrary.cpp`, 578) | `PbrEffect` getters → own `Material`, each part through `GpuMesh::Borrow` | rigs and clips mostly built in code (`CharacterFactory.cpp`, 1,120); one CNA `AnimationPlayer` per figure; `GetSkinTransforms()` into a skinned render item |
| room | `Load<Model>("x.glb")` at run time, `.cnb` when newer (`Assets/ModelLibrary.cpp`, 472) | `PbrEffect` → own material, textures re-uploaded with mips (run-time import gives one level, GLTF-206) | none |
| museum | `Load<Model>("Models/name")` finds `.gltf` | `Model::Draw` after `EnableDefaultLighting` (PBR effects start unlit) | `ModelAnimationsEXT` (rigid), `SkinningData` + `AnimationPlayer` + `SetBoneTransforms` (`ModelLab.cpp`, 883) |
| house | compiled `Model` | own binder, 72-bone limit | own `.chanim` format, skeleton, clips with foot plants |
| island | skeleton and clips in code into `SkinningData` | custom fur effect | `AnimationPlayer` per monkey; own cross-fade (decompose, lerp/slerp) and palette |
| galaxy-eggbert | `Load<std::shared_ptr<SkinnedModelEXT>>` | manual `DrawIndexedPrimitives` | `ComputeBoneTransformsEXT` |

CNA (all at the baseline):

- `ContentManager::Load<Model>(name)` (XNA API) tries `.cnj`, `.gltf`, `.glb`
  (cgltf) at run time, and XNB models. Imported parts carry `PbrEffect` or
  `SkinnedPbrEffect` (CNA extensions) — or `BasicEffect`/`SkinnedEffect`
  for unlit/non-PBR parts.
- `Model` has XNA members plus extensions: `getSkinsEXTProperty()`
  (`ModelSkinEXT`), `getBoundingSphereEXTProperty()`, import report.
- `AnimationPlayer.hpp` (all extensions): `SkinningData`, `AnimationPlayer`
  (linear translation/scale, slerp rotation, loop or clamp),
  `ModelAnimationsEXT`, `ApplyClipToBonesEXT`,
  `ApplyBindPoseBoneTransformsEXT`. The skin is found in
  `getSkinsEXTProperty()`, else as `SkinningData` in the model's Tag
  (compiled `.cnb` files).
- cna-extended: `ModelAnimationComponentEXT` (ECS; cross-fades by lerping
  skin matrices).

Duplicated: load and cache by name (street, room, museum); `PbrEffect` →
`Material` (street, room); part → `GpuMesh::Borrow` with part and model
bounds (street, room, museum); skin lookup (street, museum); a per-instance
animator with start-if-changed, cross-fade, palette into
`RenderItem::bones` (street, island, museum, cna-extended); clip building
in code (street, island). Game-specific: house's `.chanim`, gait logic,
clip state machines, material tweaks, the Avatar API (train).

Risk: street's imported CesiumMan "draws nothing on the skinned path" —
check run-time imported skinned vertex layouts against
`StockMaterialBinder`'s skinned path.

Fixtures: `cna/tests/assets/gltf/*.glb` (MS-PL, each with
`.expected.json`: skinned without animation, rigid animation);
`cna-demos/content/models/bending_strip.gltf` (skinned and animated,
4.9 KB, MS-PL); `museum-simulator/Content/Models/tentacle.gltf` (6 joints,
clip "wave", MIT); CesiumMan (CC-BY-4.0, 491 KB).

## Walking, collision and picking (GAME-020, GAME-021)

The engine's `WalkCameraController` only moves on the yaw plane and puts
the eye at `ground(x, z) + eyeHeight`: no body, collision, steps, slopes,
gravity or jump.

| Game | Body and world | Resolution | Picking |
| --- | --- | --- | --- |
| house | capsule r 0.30, half-height 0.60, eye 1.68; boxes, triangles, height field; 1 m grid | sweeps, 3 slide iterations, step up 0.22 / down 0.45, slope 46°, gravity 9.81, depenetration; 1/120 s steps (~3,000 lines, ~5,000 lines of tests) | ray (designed) |
| school | cylinder r 0.3 against `BoundingBox`es near the player | per axis, substeps ≤ 0.04 s, step 0.6, no gravity | `BoundingBox` ray hit occluded by the collision raycast, 6 m |
| museum | cylinder r 0.32 against boxes in a feet-height band | per axis | ray against item boxes, 3.2 m, occluded |
| backrooms | circle r 0.31 against wall rectangles of 3×3 cells | per axis, 0.18 m substeps | — |
| train | point against analytic regions | per axis | distance 1.45 m plus facing cone |
| car (on foot) | box 0.48×1.7 against boxes and cylinders in a uniform grid, height field | per axis, 0.08 m substeps, step +0.24/−0.35 | distance 3.2 m |
| street | knee and head points against building plots, cylinders | per axis, step 0.42, gravity 21, escape from solids | — |
| island | circle r 0.34 against cylinders in a 16 m hash grid, height field | 0.14 m substeps, slide, slope 38°, step 0.38, depenetration | — |
| galaxy-eggbert | point against a voxel grid | per axis, gravity 25, jump 12, step one cell | grid cells |

Typical parameters: radius 0.30–0.34, height 1.7–1.9, eye 1.65–1.72, step
up 0.22–0.42, step down 0.35–0.45, slope 38–46°, walk ~3 m/s (1.75–4.3),
run ×1.8–2, gravity 9.81–25, bob amplitude 0.012–0.025, reach ~3 m.

Duplicated: collide-and-slide (seven games, per axis; sweeps only in
house), substeps, overlap recovery, ground snap with a step limit, slope
limit, a grid broad phase, boxes / vertical cylinders / height fields, a
view ray against boxes occluded by the static world, head bob by distance
with footstep events, a third-person camera pulled in by a ray.
Game-specific: world formats, surface kinds, swim, vehicles, elevators,
interaction semantics.

CNA (strict): `Ray::Intersects` with boxes, spheres, planes and frustums
(distance only; no normals, triangles, capsules, sweeps or broad phase).
cna-extended `CollisionWorld3DEXT` (boxes and spheres, spatial hash or
octree; no rays, sweeps or capsules) is not usable from strict code.

Tests worth porting as behaviour: house's sweep, step-assist, ground probe,
depenetration and determinism tests (same end position at 30, 60 and 144
fps), wall contact that does not store speed, kerb up and drop down,
diagonal no faster; museum's "an upper-floor wall does not block the floor
below".

## Sound (GAME-040)

| Game | Loading | Positioning | Other |
| --- | --- | --- | --- |
| train | ~35 WAVs, `SoundEffect(path)` (CNA extension), ~1,220 lines | `Apply3D` for two doors only | loops, footsteps with cooldown and two variants, ducking, every call in `try` |
| school, museum | `Content.Load<SoundEffect>("Audio/…")`, the same `AudioSystem` class copied (260–284 lines) | `Voice3D` (instance, emitter, radius, edge fade), `DistanceScale` 4, no Doppler | `PlayUi`, `PlayAt`, `CreateLoop`, master volume, mute; silent without a device (museum catches `NoAudioHardwareException`) |
| house | `Content.Load` through a cache, silent fallback, ~1,730 lines | own pan through `Play(volume, pitch, pan)` | Waiting/Ready/Silent states (device opened on the first gesture, for the browser), mix categories, `FootstepDirector` by stride (0.75/0.95 m), seeded variants; CNA accepts 512 voices, so a limit is the game's |
| car | own RIFF reader, synthesis through `DynamicSoundEffectInstance`, ~1,740 lines | own gain and pan from the listener's right vector | — |
| street | `SoundEffect(path)`, ~740 lines | `Apply3D` with velocity, `DistanceScale` 3.5, 55 m reach | 6 engine and 6 one-shot voices, footsteps every 0.72 m |
| island | procedural PCM through XNA's buffer constructor | `Apply3D` | 6 voices, footsteps by distance and surface |
| backrooms | `SoundEffect(path)`, ~100 lines | own pan and attenuation | hum loop, steps |
| galaxy-eggbert | 93 WAVs, `SoundEffect(path)` | — | one instance each |

`SoundEffect` reads WAV only (path, `FromStream`, content); no game plays
music. Duplicated: an owner that is safe without a device (all), master
volume and mute (6), looping helper (5), positional voices with a radius
fade on top of `Apply3D` (school, museum, street, island), manual pan
(backrooms, car, house), distance-cadence footsteps with surface variants
(6), a one-shot pool that retires finished voices (school, museum, street),
a voice cap of ~6 (street, island), failures logged once (3).

## Text, menus and HUD (GAME-050)

Fonts: house loads a compiled `SpriteFont`; school and museum share a
`FontLoader` (PNG atlas + metrics from a Pillow script) into CNA's
extension `SpriteFont` constructor; island and street build an atlas in
code into the same constructor; room draws a 5×7 table as quads; train and
car have their own bitmap fonts (car also a built-in 5×7); galaxy-eggbert a
fixed-cell PNG. With the XNA API alone, `ContentManager::Load<SpriteFont>`
is the only way; CNA resolves it from XNB, CNB or a loose `.cnj` (JSON
glyph table plus atlas texture). Enhanced alternatives: the extension
constructor, `RasterizeFontDescription` (FreeType), cna-extended
`BitmapFont`.

Menus: house's `MenuStack` (~1,300 lines: top screen updates, several
draw, pause flag, Main/Pause/Credits/Confirm/Settings, device-independent
navigation, anchored text on a 1600×900 virtual layout with shadows);
school's and museum's near-identical `Hud` (text, centred text, panel,
toast, pause and help, crosshair); train's start menu with mouse hit tests;
galaxy-eggbert's menus. No game has a rebinding screen; car keeps binding
overrides in its save.

Duplicated: 5×7 glyph tables (island, room, car, train), atlases built in
code (island, street, car), shadowed text / panel / toast / centring
(school, museum, house, car), a pause and help overlay toggling mouse
capture (school, train, house).

## Saving (GAME-063)

train: `SettingsFile`. car: versioned JSON (newer versions read-only),
tmp-and-rename, `$CARSIM_SAVE_DIR`/XDG/APPDATA/HOME. house and island: XNA
`StorageContainer`, which has no rename, so both write tmp, copy to
`.bak`, delete, write, delete tmp, and recover from `.bak` (nearly line for
line); house plans checksums and migrations. galaxy-eggbert: key = value,
not atomic. Duplicated: the storage-container write sequence with backup
recovery (house, island), schema versions (car, island, house), save
directory resolution, bindings inside the save (car).

## Sky, fog, shadows, local lights, post-processing (GAME-030, GAME-031, GAME-060, GAME-061)

Sky:

- **Vertex-coloured dome, unlit `BasicEffect`:** car (`SkyRenderer`, palette from `LightingRig::SetTimeOfDay`: zenith/horizon/fog mixed night → dusk → day by sun elevation, low-sun glow, sun quad, CPU cloud layer, stars fading in at twilight, moon), train (`SkyGeometry`, palette from `Atmosphere`), school and museum (`SkyDome`, the same file; rebuilt each frame, sun and moon billboards, stars, scrolling clouds), house (authored dome coloured from a zenith/horizon table by sun elevation, star catalogue, sun and moon discs).
- **Shader skies (`ShaderEffect`, GLSL):** island, with a vertex-coloured dome as fallback; street and room, procedural atmosphere plus IBL bake.
- backrooms clears to the level's fog colour.

Fog: every strict game uses the stock linear fog (the engine's `ForwardLighting` already carries it), coloured by the sky's horizon colour (car, train, house) or by hand (school); backrooms per level and clear colour.

Shadows:

| Game | Technique |
| --- | --- |
| car | none from a shadow map: CPU-baked ground shadows; car hulls projected along the sun into a fan with a penumbra rim |
| train | box hulls projected on the ground, fading with a low sun; a contact disc |
| backrooms | baked into vertex colours, contact quads |
| house | light maps (`DualTextureEffect`) |
| island | own shadow map (shader caster, own receivers), blob shadows |
| museum | `Matrix::CreateShadow` with stencil; CNA's receiver API in a lab |
| room | vendored copy of CNA's removed shadow-map producers |
| street, school | none |

CNA at the baseline keeps only the receiver side: `IShadowReceiverEXT`
(BasicEffect, SkinnedEffect, PbrEffect, SkinnedPbrEffect: map, light
view-projection, bias, PCF), `ShadowCascadeStateEXT` (four cascades, filled
by the application), `GraphicsDevice::SupportsShadowSamplingEXT()` (true on
EasyGL, SDL-GPU, FNA3D, software, DX9). A caster can be a stock
`BasicEffect` writing light-space depth as vertex colour (museum's
`ShadowLab`). All of this is extension API: enhanced only.

Local lights: `PunctualLightEXT` (one point or spot light per draw, also
on `BasicEffect`); more lights by additive passes (museum). Strict games
use additive glow quads and light pools (car, island), light maps (house),
baked vertex colours (backrooms) or per-room levels (school).

Post-processing: room has a vendored full chain (shader effects); island a
time-of-day grade as a full-screen multiplied `BasicEffect` quad; the
others none. CNA keeps only `CRTEffect`, `DepthEffect` and
`AsciiPostProcessEffect` (CNA_CNAEXT). Custom effects: `ShaderEffect`
(GLSL source at run time, extension; check the shader dialect, WebGPU wants
WGSL) or XNA `Effect` from `fxc` bytecode where the renderer has compiled
effects (off by default on EasyGL and SDL-GPU).

Strict candidates: a sky dome with a time-of-day palette from
`SunPosition` feeding `ForwardLighting` and fog (car, train, school,
museum, house); projected hull and blob shadows (car, train, island,
backrooms); a full-screen grade quad (island); additive light pools (car,
island). Enhanced: a shadow-map producer for `IShadowReceiverEXT`, the
punctual light in the PBR binder, a post chain on `ShaderEffect`.
