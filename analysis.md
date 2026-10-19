# CNA Engine Feasibility Analysis

## Executive conclusion

Developing a standalone `cna-engine` library on top of `../cna` is technically feasible and justified by the existing projects. It should not be created by copying one simulator wholesale. The sound approach is to identify and gradually extract shared mechanisms while leaving game-specific rules in their respective projects.

The reviewed code already contains multiple working implementations of nearly every foundational subsystem expected from a higher-level game engine. The main challenge is therefore not proving that the functionality can be built, but defining stable boundaries that do not overfit one game or duplicate CNA itself.

## Follow-up status

The later targeted audit is recorded in `gap-audit.md`, and its executable task
sequence is maintained in `plan.md`. The CNA Extended read-only filename-loader
defect discovered by that audit was fixed upstream in commit
`0061e3e17c30b2a3a302bce617e64765883079aa` and independently verified: all 13
affected tests passed, the complete 2,371-test suite finished with 2,369 passes
and two known skips, and all three examples ran successfully.

The next implementation gates are therefore the strict/enhanced architecture
decision and a clean, pinned CNA compatibility baseline, not another loader
workaround in `cna-engine`.

## Scope of the review

The following sibling projects were inspected:

- `cna-car-simulator`
- `house-simulator`
- `island-simulator`
- `living-room-simulator`
- `museum-simulator`
- `school-simulator`
- `ship-simulator`
- `train-simulator`
- `cna-street`

Together, excluding build trees, generated content, Android build products, and vendored Node modules, they contain approximately 428,000 lines of C and C++ source. This is a sufficiently broad practical base from which to derive an engine API.

The current `cna-engine` directory is effectively an empty placeholder. It does not yet contain an initialized working repository or implementation.

## Evidence of a shared engine layer

The projects repeatedly implement the following facilities:

- application lifecycle on top of `Game`;
- fixed-step timing and separation of simulation from presentation;
- first-person, free-flight, scripted, and cinematic cameras;
- input action mapping and mouse capture;
- CPU mesh data, procedural mesh builders, and GPU mesh ownership;
- material, model, texture, and audio caches;
- render queues, render passes, and graphics-state management;
- frustum culling, spatial indices, LOD, and world streaming;
- screenshots, diagnostic overlays, benchmarks, and deterministic capture modes;
- time of day, weather, and simple particle systems;
- spatial audio and bounded sound-instance management.

This is more than superficial similarity.

`cna-street` and `living-room-simulator` have twelve correspondingly named central abstractions, including `Camera`, `CameraController`, `MeshBuilder`, `GpuMesh`, `MaterialLibrary`, `SceneRenderer`, and `SkySystem`. Their responsibilities are documented in `../cna-street/docs/DEVELOPING.md` and `../living-room-simulator/docs/ARCHITECTURE.md`.

The relationship between `museum-simulator` and `school-simulator` is even more direct. At least fifteen source files are byte-for-byte identical, including implementations or declarations of `StaticMesh`, `LitMesh`, `EffectBank`, `Gizmos`, `LiveScreen`, `FirstPersonCamera`, `ImageUtil`, and `RenderStates`. This is direct evidence of a copied project-local engine layer.

Several larger projects already demonstrate useful architectural boundaries:

- `cna-car-simulator` separates renderer-independent `carsim_core` from `carsim_render`. Its executable is a thin `Game` subclass. This is described in `../cna-car-simulator/README.md` under Architecture.
- `house-simulator` contains a service container, ordered system scheduler, fixed-step support, asset caches, first-person control, collision, visibility, LOD, weather, audio, and debug infrastructure. Its project-owned subsystems are summarized in `../house-simulator/docs/xna-deviations.md`.
- `island-simulator` contains quality profiles, culling, instancing, renderer fallbacks, touch input, procedural world rendering, and wildlife activation tiers. Its boundaries and frame flow are documented in `../island-simulator/docs/architecture.md`.
- `ship-simulator` clearly separates deterministic simulation from CNA presentation, as described in `../ship-simulator/docs/ARCHITECTURE.md`.
- `train-simulator` provides experience with large-coordinate handling, deterministic world streaming, simulation/presentation separation, procedural content, and renderer-independent testing.

## Relationship to CNA

CNA is a suitable lower-level foundation. It already provides:

- the XNA `Game` lifecycle;
- math and bounding volumes;
- graphics resources and stock effects;
- input, audio, media, storage, and content management;
- the content pipeline;
- platform abstraction and multiple renderers.

`cna-engine` should build on those facilities instead of duplicating them.

CNA previously contained a much larger engine layer: HDR rendering, shadows, sky, image-based lighting, materials, render passes, culling and LOD, particles, probes, and other systems. That layer was deliberately removed during a scope reduction. The decision and its dependency analysis are recorded in `../cna/docs/graphics-ext-scope-reduction.md`. The remaining `graphics-ext` module is intentionally small and is documented in `../cna/docs/cnaext-engine-layer.md`.

This does not argue against `cna-engine`. It confirms that a separate library is the correct ownership boundary. Higher-level game policies should not be restored inside the XNA-compatible CNA runtime.

The intended dependency direction should be:

```text
game or simulator
        |
        v
    cna-engine       game, scene, and rendering policies
        |
        v
       cna           XNA API, GPU resources, content, audio, platform
        |
        v
renderer / SDL / operating system
```

## Recommended modular architecture

The library should be modular rather than a single monolithic target.

### `CnaEngine::Core`

- clocks and fixed-step scheduling;
- ordered system lifecycle;
- events and lightweight service wiring;
- diagnostics and frame timing;
- no GPU dependency.

### `CnaEngine::Input`

- named actions and axes;
- keyboard, mouse, gamepad, and touch adapters;
- mouse capture;
- reusable first-person and free-camera controllers.

### `CnaEngine::Scene`

- transforms and simple scene objects;
- stable object identifiers;
- spatial indexing;
- frustum culling, LOD selection, and streaming hooks;
- no general-purpose ECS in the initial design.

### `CnaEngine::Graphics`

- parameterized CPU-side mesh data and mesh building;
- RAII GPU mesh ownership;
- cameras and view volumes;
- materials and render items;
- opaque, alpha-tested, transparent, and UI queues;
- render-pass scheduling and state restoration;
- screenshots and debug rendering.

### `CnaEngine::Content`

- typed asset registry and caches over `ContentManager`;
- content manifests and lifetime management;
- fallback and validation policies;
- offline-tool integration contracts without embedding tools into the runtime.

### Later optional modules

- simple kinematic collision;
- particles and environment systems;
- audio voices, zones, and ambience;
- animation playback and blending;
- reusable UI foundations.

## Strict-XNA and enhanced profiles

The reviewed projects use two materially different policies.

`cna-car-simulator` and `house-simulator` deliberately restrict themselves to the XNA 4.0-compatible surface and project-owned code. Other projects, including `cna-street`, `living-room-simulator`, and `island-simulator`, use CNA extensions, portable shader packages, PBR effects, and capability queries.

The engine should therefore separate these profiles:

1. A strict-XNA baseline that depends only on `Microsoft::Xna::Framework::*`, Sharp Runtime, and the C++ standard library. This maximizes portability and keeps the fundamental engine independent of optional CNA extensions.
2. An optional enhanced graphics module that enables `CNAEXT`, portable shader packages, PBR facilities, and explicit capability handling.

This split prevents every engine consumer from being forced into the most demanding renderer and shader requirements.

Existing strict-XNA projects would still need an explicit policy decision before consuming an external engine library. Their current rules permit only XNA, Sharp Runtime, the standard library, and code owned by the project itself. Even a strict-XNA implementation inside `cna-engine` would no longer be project-owned code under those definitions.

## What should remain outside the engine

The following belong to individual games and should not be moved into the shared library:

- car, train, ship, or aircraft physics;
- traffic rules and route-driving behavior;
- passengers, wildlife, or game-specific NPC behavior;
- project-specific map and world formats;
- the authored layout of a house, city, island, school, museum, train route, or ship;
- game-specific weather and material policies;
- renderer implementation details from `CNA::Internal`;
- a comprehensive ECS, editor, or full physics engine in the first phase.

Similar names also do not imply interchangeable implementations. Existing `MeshBuilder` classes use different vertex formats and assumptions: PBR tangent frames, colored vertices, multiple UV sets, or several selectable vertex layouts. The engine needs a parameterized common foundation rather than selecting one existing implementation as the universal version.

## Recommended initial vertical slice

The first usable version should remain deliberately small:

1. camera and input actions;
2. generic CPU mesh types and GPU mesh ownership;
3. typed asset cache;
4. a simple scene with frustum culling;
5. render queues for opaque, transparent, and UI passes;
6. diagnostics and screenshot capture;
7. one small example game or scene.

`cna-street` is the strongest candidate for the first real consumer. It already builds most application code as a testable `CnaStreet::Core` library, has a clean subsystem layout, and includes a useful test suite. Its engine-like code is less coupled to one specialized simulation than the car, train, or ship projects.

The second pilot should be architecturally different, preferably a strict-XNA project such as `cna-car-simulator`. Only abstractions that work for both pilots should be treated as stable engine API. This two-consumer rule would substantially reduce overfitting.

## Risks

### CNA API evolution

CNA is currently version `0.1.0` and is under active development. `cna-engine` will need pinned compatible revisions, explicit version checks, and continuous integration against the intended CNA branch or release.

### Renderer differences

The renderer backends do not support identical feature sets. Engine systems must define fallbacks and capability contracts instead of assuming one OpenGL configuration.

### Premature generalization

The largest architectural risk is publishing a broad API before it has been exercised by at least two independent games. Existing implementations should be treated as evidence and prototypes, not automatically as final interfaces.

### Licensing and provenance

Most reviewed repositories use MIT-licensed project code, while CNA uses the Microsoft Public License. Some reviewed projects do not expose an unambiguous top-level license, and parts adapted from CNA's retired engine retain Ms-PL notices. Every extracted implementation needs a provenance and license review before being moved.

### Migration cost

The existing projects have different naming, ownership, vertex layouts, renderer policies, and test maturity. Immediate migration of all projects to one API is unrealistic. Compatibility adapters and staged migrations will be necessary.

### Scope growth

A reusable application, scene, and rendering foundation is a manageable project. A complete engine with an editor, general physics, navigation, animation authoring, asset database, and mature tooling would be a much larger, multi-phase effort.

## Final assessment

The project is technically viable. The sibling repositories demonstrate both the need for a shared layer and multiple proven implementations of its likely subsystems.

A realistic path is:

- first, build a small foundation and rendering library;
- next, validate it through two substantially different games;
- then stabilize the common API and add optional modules incrementally;
- only later consider broader facilities such as animation, collision, audio orchestration, or editor tooling.

The unrealistic approach would be to assume that all nine existing projects can immediately converge on one large API without adapters, redesign, and staged migration.

No implementation, build, or source modification was performed as part of the original feasibility investigation. This document records analysis only.
