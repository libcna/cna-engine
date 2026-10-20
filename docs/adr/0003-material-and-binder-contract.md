# ADR-003 — What a material describes and what a binder decides

Date: 2026-10-21

Status: accepted (records decisions made while carrying both pilots' scenes
through the engine, 2026-10-19 … 2026-10-21)

Plan items: ENG-031, ENG-032, ENG-035 in `plan.md`

## Context

The forward renderer splits a draw into three parties:

- the **material** (`Graphics/Material.hpp`): a device-independent
  description of a surface, owned by the game;
- the **binder** (`IMaterialBinder`): turns a material into effect parameters
  for one effect family (XNA's stock effects, CNA's `PbrEffect`);
- the **renderer** (`ForwardRenderer`): ordering, buckets and device states.

Carrying Street's and car-simulator's real scenes through the engine forced a
series of decisions about where each piece of rendering state belongs. Each
was taken because a pilot's frame differed from the engine's and the
difference had to be explained, not tuned away. This record collects them so
the next additions follow the same rule.

## Rule

A field belongs on the **material** when it describes the surface and means
the same under every effect: its colors and textures, how it blends with what
is behind it, how its textures are sampled, whether it receives fog or the key
light, whether it discards below an alpha cutoff. A decision belongs to the
**binder** when it depends on what the binder's effect computes. A state
belongs to the **renderer** when it depends on the pass or the view, not on
the surface.

## Decisions

| Concern | Where | Why (the pilot evidence) |
| --- | --- | --- |
| Blend *intent* (`BlendMode`: opaque, alpha-tested, alpha-blended, additive, premultiplied) | material | describes the surface: Street's glass adds its reflection at full strength (`PremultipliedAlpha`), its shop signs filter (`AlphaBlend`) |
| Blend *state* (`IMaterialBinder::BlendStateFor`) | binder | XNA's stock effects multiply their color by alpha, CNA's `PbrEffect` does not; the same intent needs `AlphaBlend` from one and `NonPremultiplied` from the other — the PBR transparent surfaces were too bright until the binder chose |
| Texture sampling (`Material::sampler`, `secondarySampler`) | material, applied by the binder | car-simulator samples terrain anisotropically, its trees with clamp; a renderer default inherited whatever the previous drawer left (SpriteBatch leaves LinearClamp) |
| Fog opt-out (`Material::fog`) | material | car-simulator's street-lamp light is unfogged while everything else is fogged |
| Key-light opt-out (`Material::keyLight`) | material | Street's interiors behind shop windows are not sunlit |
| Discard below cutoff for blended materials (`Material::discardBelowCutoff`) | material | car-simulator relights tree cards additively *and* alpha-tested under its headlights |
| Depth bias (`Material::depthBias`, `slopeScaleDepthBias`) | material, turned into rasterizer states by the renderer | car-simulator's road markings; the bias is a property of the decal, the state object is the renderer's |
| Culling direction for mirrored views (`Camera::SetMirrored`) | camera, applied by the renderer | a mirror flips the winding of every surface in the view, not of any one material (car-simulator's rear-view mirrors) |
| Lighting (key, fills, ambient, fog range and color) | `ForwardLighting`, per renderer | the same for every surface of a pass; a pass with different lighting (car-simulator's headlight fill) is a second renderer run from a custom pass |
| Image-based environment and exposure (`PbrSettings`) | PBR binder | only PBR effects consume them; Street's exposure scales every light and the environment, not emissive or fog |
| Per-item environment (reflection probes) | PBR binder's `EnvironmentSelector`, keyed by `RenderItem::userData` | which probe lights an item is the game's knowledge (Street's nearest probe); the engine never reads `userData` |
| Skinning (`RenderItem::bones`) | render item, consumed by the binder | the palette belongs to one draw (one pedestrian's pose), not to the material shared by many |
| Instancing (`RenderItem::instances`) | render item, drawn by the renderer | a property of the submission; the binder only sees the shared base world |

## Things deliberately left out

- **Art-directed uses of XNA state** that are not surface descriptions —
  car-simulator's wet sheen (fog color as a sky-reflection ramp, varied per
  road batch) and its snow (a premultiplying effect under `NonPremultiplied`,
  alpha counted twice) — stay custom passes that draw with CNA directly, the
  same escape hatch Street's sky uses. Adding fields for them would make the
  material describe one game's tricks rather than surfaces.
- **Per-material blend-state overrides**: the binder owns the translation
  from intent to state; a game that needs a specific XNA state for a special
  effect uses a custom pass or its own binder.

## Consequences

- New material fields need a surface meaning that holds under every binder
  (and a reported fallback where a binder cannot honor it, as the stock
  binder reports premultiplied glass with alpha < 1).
- A binder that adds an effect family answers `BlendStateFor` for what its
  effect writes.
- The pilots' scene comparisons (`docs/pilots/`) are the regression check for
  this contract: they fail on visibility differences and on images outside
  the proposed screenshot tolerance.
