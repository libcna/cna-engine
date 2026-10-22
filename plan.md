# CNA Engine Implementation Plan

Date: 2026-10-14

Status: in progress (implementation started 2026-10-19; see the per-task results)

This plan translates the targeted gap audit into implementation tasks. It does
not authorize implementation by itself. The work begins only after the initial
architecture decision and upstream baseline are accepted.

## Objective

Create a modular C++23 game-engine library above CNA that materially reduces
the work required to build games, reuses `cna-extended` instead of duplicating
it, supports an enhanced 3D path for `cna-street`, and preserves a narrowly
audited strict-XNA path for `cna-car-simulator`.

## Constraints and working rules

- CNA remains the owner of the XNA-compatible runtime, platform, content, audio,
  and GPU-resource APIs.
- `cna-extended` remains the owner of its existing screens, ECS, animation,
  particles, fonts, tilemaps, collision helpers, and `World3DEXT` facilities.
- Engine APIs must be validated by at least two materially different consumers
  before being declared stable.
- Game-domain rules do not move into the engine merely because only one project
  has implemented them well.
- Builds and tests use no more than three parallel jobs:
  `cmake --build <dir> --parallel 3` and
  `ctest --test-dir <dir> --parallel 3`.
- Public APIs use XNA-compatible CNA types where they are already sufficient.
- Internal CNA APIs are not dependencies.
- Each extraction records source provenance and license compatibility.
- Performance changes require a baseline and a repeatable measurement.

## Proposed module shape

The exact names are a decision item, not a completed API commitment.

```text
CnaEngine::Core
  lifecycle helpers, input actions, diagnostics, no cna-extended dependency

CnaEngine::Graphics
  camera data, mesh data/upload, assets, render submissions and capture

CnaEngine::ExtendedIntegration
  adapters for CNA::Extended screens, ECS, World3DEXT and reusable systems

CnaEngine::Engine
  convenience aggregate for enhanced consumers
```

`Core` and `Graphics` should be capable of passing a dedicated strict-surface
check. `ExtendedIntegration` is explicitly outside that boundary.

## Status legend

- `[x]` completed (acceptance criteria met)
- `[~]` implemented in the engine; acceptance criteria that need pilot
  projects or other repositories are still open (named in the result)
- `[ ]` not started
- `Blocked by` names hard dependencies, not merely preferred ordering

## Phase 0 — evidence and decisions

### AUD-001 — Build `cna-extended` against current CNA `[x]`

Acceptance:

- library, examples, and tests configure and compile;
- failures and skips are classified rather than hidden;
- commands and CNA worktree state are recorded.

Result: the initial build passed but 2,371 tests exposed one read-only
file-opening defect and two existing skipped camera cases. The defect was later
fixed at CNA Extended commit `0061e3e17c30b2a3a302bce617e64765883079aa`;
the full verification now has zero failures and the same two skips. See
`gap-audit.md`.

### AUD-002 — Compare `cna-extended` with `cna-street` `[x]`

Acceptance:

- public API needs are mapped to reuse, upstream generalization, engine work, or
  game-owned code;
- Street builds and its test suite passes against the audited CNA tree.

Result: build passed and all 15 test entries passed.

### AUD-003 — Compare against strict-XNA Car Simulator `[x]`

Acceptance:

- strict checker result, build result, and test result are recorded;
- reusable and vehicle-specific facilities are separated;
- policy conflicts with `cna-extended` are explicit.

Result: strict checking and compilation passed. Four of seven CTest entries
passed; two display smoke tests reached 120-second limits, and the 321-test
aggregate reached its 900-second limit during `TrafficSoak`. One preceding
sample-map performance assertion exceeded its six-second budget. These results
are retained as environmental/performance follow-up rather than hidden or
misreported as a clean pass.

### ADR-001 — Approve the strict/enhanced dependency boundary `[x]`

Decide:

- whether `Core` and `Graphics` may depend only on CNA's XNA-compatible API;
- whether `ExtendedIntegration` is the sole dependency on `CNA::Extended`;
- public include prefix, namespace, exported CMake targets, and versioning;
- how the Car Simulator checker narrowly permits certified engine headers while
  continuing to reject direct CNA extensions.

Acceptance:

- a committed architecture decision record contains the dependency graph;
- minimal strict and enhanced consumer probes both configure;
- the strict probe fails deliberately if an `EXT` or `CNA::` API leaks into its
  public boundary.

Blocked by: none. This is the first implementation gate.

Result: accepted in `docs/adr/0001-strict-enhanced-boundary.md` (targets,
prefixes, namespaces, versioning, car-simulator checker contract). Strict and
enhanced consumer probes configure (`tests/consumer`); the strict boundary test
fails deliberately on `tests/boundary/violations/LeakyHeader.hpp`.

### ADR-002 — Select and pin the CNA compatibility baseline `[~]`

Acceptance:

- CNA and `cna-extended` revisions are clean and explicitly recorded;
- compatibility policy states whether exact revisions or version ranges apply;
- all three audited consumers are rerun against the pinned baseline;
- CI can reproduce the baseline without relying on a developer worktree.

Blocked by: none. Complete before publishing engine API.

## Phase 1 — upstream prerequisites

Result: policy and pin accepted in `docs/adr/0002-compatibility-baseline.md`;
`cmake/CnaEngineBaseline.cmake` checks the revisions at configure time
(`CNA_ENGINE_ENFORCE_BASELINE=ON` for CI; `.github/workflows/ci.yml`). The
cna-extended suite was rerun against the pin (one new test-defect failure,
UP-005). street-simulator's and car-simulator's own suites were run against the
pin on 2026-10-20 (`CNA_ENGINE_PILOT_SUITES`): 13/13 and 320/321, the one
failure a Release-only load budget (UP-009). Open: CI cannot fetch the pinned
CNA revision until it is pushed (UP-007), so the last acceptance point waits
for the owner.

### EXT-001 — Correct read-only asset loading in `cna-extended` `[x]`

Open filename-based input streams with `FileMode::Open` and
`FileAccess::Read`. Audit equivalent readers rather than fixing only the four
observed BMFont tests.

Acceptance:

- assets load from a read-only source tree;
- all 2,371 tests pass except explicitly accepted skips;
- `tiled_demo`, minimal, and `world3d_demo` run successfully;
- regression tests cover read-only BMFont and tilemap-related assets.

Result: completed at CNA Extended commit
`0061e3e17c30b2a3a302bce617e64765883079aa`. The filename-loader audit found no
remaining single-argument `FileStream` construction. All 13 targeted regression
tests passed; the full suite passed with 2,369 passes, zero failures, and two
known skips; and the minimal, tiled, and World3D examples all ran successfully.
This fix was verified against the observed CNA working tree and does not replace
the reproducible baseline work in ADR-002.

### EXT-002 — Resolve the orthographic-camera test disposition `[ ]`

Determine whether each containment case is an implementation defect, a floating
point boundary ambiguity, or an invalid expectation.

Acceptance:

- both tests run and pass, or a documented numerical contract replaces the
  ambiguous assertions;
- no unexplained disabled camera tests remain.

Blocked by: ADR-002. Not a blocker for the first engine skeleton.

Disposition (2026-10-19, analysis only; cna-extended and CNA are not modified
from this repository): both skipped cases are an implementation defect in CNA's
`BoundingFrustum::Contains(Vector3)`, which deviates from the XNA 4.0 reference
(no `1E-05f` tolerance, `Intersects` for on-plane points, early `break`). See
`upstream-issues.md` UP-004. CNA fixed it in `5ab1c7729` (2026-10-20) and
published it with a regression test in `900d0db3b`; the engine's pin moved
there the same day (ADR-002). Remaining, for cna-extended's owner: remove the
two unconditional `GTEST_SKIP()`s in `tests/CNA/Extended/OrthographicCameraTests.cpp`;
the CI lane reports when the skips are gone.

### EXT-003 — Establish an upstream compatibility lane `[x]`

Acceptance:

- CI builds `cna-extended` against the pinned CNA revision with a maximum of
  three jobs in the constrained environment;
- examples and tests are separate reported jobs;
- an engine integration probe detects an upstream API break early.

Blocked by: ADR-002.

Result (2026-10-20): `.github/workflows/ci.yml` job `cna-extended` builds
cna-extended standalone at its pin against the pinned CNA with three jobs,
tests and examples as separately reported steps; the known upstream defects
(UP-004 skips, UP-005 failure) are allowed by exact name only and reported
when they disappear. Job `upstream-canary` (weekly and on demand) builds and
tests the engine and its installed-package consumers against CNA `next`,
cna-extended `develop` and Sharp Runtime `next`. Open: neither has run yet —
the pinned CNA revision is not on the remote (UP-007); the skip filter was
checked locally against a synthetic ctest log, the lane's build commands
mirror the 2026-10-19 local cna-extended run. Update (2026-10-20): after the
pin moved to the published CNA and the engine was pushed, all three jobs ran
green on GitHub (run 39409759296: engine, cna-extended lane with UP-005 and
UP-011 as named known failures and UP-004's skips, canary against the
upstream heads). The first runs found four workflow defects, all fixed:
Sharp Runtime's GoogleTest submodule, the skip check matching test names,
the consumer probes' relative prefix and their hard-coded shared cache.

## Phase 2 — repository and boundary skeleton

### ENG-010 — Create the C++23 CMake project skeleton `[x]`

Acceptance:

- targets approved by ADR-001 configure, build, install, and export;
- build-tree and installed-package consumers both work;
- warning policy does not mutate dependency flags globally;
- tests can be enabled or disabled without changing public behavior.

Blocked by: ADR-001 and ADR-002.

Result: `CMakeLists.txt` and `cmake/`; four targets with aliases, install and
`CnaEngineConfig.cmake` export; warnings only on engine targets; tests and
examples optional. Build-tree and installed-package consumers both build
(`tools/run_consumer_probes.sh`).

### ENG-011 — Add public dependency-boundary checks `[x]`

Acceptance:

- strict public headers are scanned and compile-tested without
  `cna-extended`, CNA internal headers, or CNA extension APIs;
- enhanced headers compile with only their declared dependencies;
- forbidden transitive includes produce a clear CI failure.

Blocked by: ENG-010.

Result: `tools/check_strict_boundary.py` (car-simulator rules plus "no
enhanced engine headers") as CTest tests, a deliberate-leak fixture, and
`cna_engine_strict_header_check`, which compiles every strict header alone
against strict targets only.

### ENG-012 — Add minimal strict and enhanced consumer projects `[x]`

Acceptance:

- each consumer uses the installed package rather than repository-relative
  private paths;
- strict consumer links only strict targets;
- enhanced consumer creates a minimal `CNA::Extended` screen or world adapter;
- both compile and run in the software/headless/null configuration.

Blocked by: ENG-010 and ENG-011.

Result: `tests/consumer` is a separate project using the installed package;
the strict consumer links only strict targets (its link closure is checked for
cna-extended targets) and passes the boundary checker; the enhanced consumer
runs an `EngineWorldScreen`. Both run in the software/headless/null
configuration.

### ENG-013 — Define API stability and ownership conventions `[x]`

Acceptance:

- public/private header policy, namespace rules, error handling, resource
  ownership, nullable handles, and ABI expectations are documented;
- experimental APIs are visibly marked;
- contribution checklist requires two-consumer evidence for generalization.

Blocked by: ADR-001.

## Phase 3 — first strict-compatible vertical slice

Result: `docs/conventions.md` and `docs/api-status.md`.

### ENG-020 — Implement input actions and rebinding `[x]`

Generalize the proven Car Simulator action map without importing its domain
commands.

Acceptance:

- named digital actions and analog axes support keyboard, mouse, and gamepad;
- held, pressed, and released state is deterministic per update;
- mappings serialize without exposing platform internals;
- unit tests cover rebinding, conflicting bindings, dead zones, and focus loss;
- both pilot projects can express their existing core controls.

Blocked by: ENG-012.

Result: `Input/ActionMap.hpp`, `Bindings.hpp`, `InputNames.hpp`,
`BindingCapture.hpp`, `InputSnapshot.hpp` with unit tests for rebinding,
conflicts, dead zones, focus loss and persistence. Open: express both pilots'
core controls: car-simulator's InputMapper and Street's fly controls are
reproduced exactly by engine actions, axes and the fly controller
(`docs/pilots/`).

### ENG-021 — Implement camera data and controller interfaces `[x]`

Acceptance:

- view, projection, combined matrix, basis vectors, and frustum have explicit
  invalidation behavior;
- perspective and orthographic projections are tested;
- controller policy is separate from camera state;
- fly/walk can be implemented by the engine while chase/cockpit remains in the
  Car Simulator;
- no duplicate scene/ECS ownership is introduced.

Blocked by: ENG-012. Coordinate with EXT-002.

Result: `Graphics/Camera.hpp` (lazy derived values, `Revision()`),
`CameraControllers.hpp` (fly, walk, `ICameraController`). Open: coordinate
with EXT-002. Both pilots validated: car-simulator chase-camera poses and
Street's fly controller drive the engine camera with matching matrices
(`docs/pilots/`).

### ENG-022 — Define generic CPU mesh data `[x]`

Acceptance:

- the design supports Street's tangent-frame PBR vertices and the Car
  Simulator's multiple strict-XNA layouts without one giant mandatory vertex;
- 32-bit indices, bounds, append, transform, winding, and normal/tangent
  generation have tests;
- conversion is explicit and detects unsupported or lossy attributes;
- procedural builders depend on the generic representation, not on a game.

Blocked by: ENG-013.

Result: `Graphics/MeshData.hpp` (attribute streams, 32-bit indices, bounds,
append, transform, winding, angle-weighted normals, tangents),
`MeshBuilder.hpp`, `VertexLayouts.hpp` (explicit conversion with
missing/dropped/narrowed report). Both pilots validated: car-simulator's
multi-layout meshes and Street's tangent-frame meshes convert without loss
(`docs/pilots/`).

### ENG-023 — Add RAII GPU mesh upload and drawing `[x]`

Acceptance:

- resource ownership, borrowed resources, device reset/loss, and disposal are
  defined;
- supported vertex layouts upload and draw through XNA-compatible APIs;
- submission and memory statistics are observable;
- tests cover validation and lifecycle; a headless/software integration scene
  covers actual drawing.

Blocked by: ENG-022.

Result: `Graphics/GpuMesh.hpp` (owned/borrowed buffers, disposal handling,
16/32-bit indices with Reach-profile checks, statistics and GPU memory tally),
software-renderer pixel tests. Both pilots' meshes render pixel-identically
through the engine upload and the pilot's own upload (`docs/pilots/`).

### ENG-024 — Add typed asset registry and lifetime policy `[x]`

Acceptance:

- cache keys, ownership, invalidation, fallback, and failure behavior are
  documented and tested;
- `ContentManager` remains the underlying content owner where appropriate;
- engine code does not duplicate `cna-extended` format readers;
- both pilots can replace at least one local cache without behavior changes.

Blocked by: ENG-012. EXT-001 is complete.

Result: `Assets/AssetRegistry.hpp` (normalized keys, typed loaders,
ContentManager loader, fallbacks, failure records, invalidation, exact text
keys, `GetOrCreate` for recipes not derivable from the key). Car-simulator's
plate-texture cache and Street's derived-material cache are reproduced
without behaviour change (`docs/pilots/`).

### ENG-025 — Add diagnostics and screenshot capture `[x]`

Acceptance:

- frame counters and scoped CPU timings have negligible disabled overhead;
- draw/submission statistics can be supplied without a hard dependency on a UI;
- deterministic screenshot requests report success and failure explicitly;
- software/headless tests verify output metadata and image dimensions.

Blocked by: ENG-012.

Result: `Diagnostics/FrameStatistics.hpp`, `ScopedTimer.hpp` (no clock
read when disabled), `Graphics/RenderStatistics.hpp`, `ScreenCapture.hpp`
(explicit results, deterministic names, Reach handling), and
`Graphics/DiagnosticsReport.hpp` (metrics as text, locale-independent). The
car-simulator probe shows engine captures are identical to the pilot's own
screenshot files.

### MILESTONE-1 — Strict-compatible foundation `[x]`

Exit criteria:

- ENG-020 through ENG-025 meet acceptance criteria;
- Car Simulator uses at least two facilities in a branch or integration probe;
- its strict checker remains green with only the narrow ADR-approved allowance;
- no engine domain depends on `cna-extended` through a strict target.

Estimated active AI-agent effort: 90–150 hours including Phase 2 and pilot
integration feedback.

## Phase 4 — enhanced scene and rendering layer

Result (2026-10-19): ENG-020 … ENG-025 meet their acceptance criteria.
Car-simulator exercises input actions, camera data, mesh data and upload,
screenshots and the asset registry through integration probes that compile
its sources read-only (`tests/pilots/`); its own strict checker accepts the
engine's strict surface with only the ADR-001 include allowance
(`cna_engine.pilot.car_simulator_checker_allowance`); no strict target
reaches cna-extended (boundary tests and the consumer probe's link-closure
check).

### ENG-030 — Integrate `cna-extended` lifecycle and World3D `[~]`

Acceptance:

- adapters compose with `Screen`, ECS, `World3DScreenEXT`, animation, and
  particles without wrapping every upstream type;
- applications may opt out and use engine graphics directly;
- ownership and update/draw order are documented and tested.

Blocked by: ENG-012. EXT-001 is complete.

Result: `Extended/EngineWorldScreen.hpp` (a World3DScreenEXT whose update
order is documented and tested) and `Extended/WorldCameraSync.hpp` for games
that opt out. Open: particles/animation adapters once a pilot needs them.

### ENG-031 — Define materials and shader-facing parameters `[x]`

Acceptance:

- opaque, alpha-tested, transparent, unlit, and PBR-oriented material needs are
  representable;
- material descriptions are separate from per-device GPU state;
- missing textures and unsupported capabilities have deterministic fallbacks;
- one Street material family migrates without visual regression.

Blocked by: ENG-023 and ENG-024.

Result: `Graphics/Material.hpp` (unlit/lit/PBR description, blend modes,
borrowed textures, no GPU state, glTF metallic-roughness fields matching
Street's `Material`); `IMaterialBinder` with a strict stock-effect binder and
an enhanced `PbrMaterialBinder` over CNA's `PbrEffect`, both with reported
fallbacks. Street's whole material set (opaque, alpha-tested, blended,
premultiplied glass, unlit interiors) renders through the `PbrMaterialBinder`
with Street's lighting (exposure, sky ambient, image-based environment, fog)
practically identically to Street's own `applyMaterial`: mean difference
0.01/255, 53 of 129,600 pixels differ, 49 of them from Street's depth writes in
its transparent pass (`docs/pilots/street-simulator.md`). Generalizations:
binder-chosen blend states (`PbrEffect` writes straight alpha),
`BlendMode::PremultipliedAlpha`, per-material samplers, PBR environment and
exposure.

### ENG-032 — Define render items, queues, and state contracts `[x]`

Acceptance:

- static, dynamic, skinned, and instanced submissions share a small common
  render-item contract;
- sorting rules for opaque, alpha-tested, transparent, and UI work are tested;
- custom passes and direct CNA drawing remain possible;
- graphics state before and after engine passes is explicit.

Blocked by: ENG-023 and ENG-031.

Result: `Graphics/RenderQueue.hpp` (buckets, deterministic material-grouped
front-to-back, back-to-front transparent, stable overlay) and the
`ForwardRenderer` state contract (states set per bucket, restored after
`Render`, samplers included). Static, dynamic, instanced (`instances`) and
skinned (`bones`: XNA `SkinnedEffect` in the stock binder, CNA
`SkinnedPbrEffect` in the PBR binder) submissions share `RenderItem`. Street's
pedestrians draw through it: in a close-up 3.5 m from a figure, 57 skinned
figures cover 10,412 pixels and 2 of them differ from Street's own frame
(`docs/pilots/street-simulator.md`). cna-extended's World3D skinning stays
available to games that use World3D.

### ENG-033 — Add visibility and LOD policy `[x]`

Acceptance:

- frustum, distance, and layer filtering are independent policies;
- bounds updates and conservative culling are tested;
- LOD thresholds are data-driven and stable under small camera movements;
- Street can report culling/LOD counts through engine diagnostics.

Blocked by: ENG-021 and ENG-032.

Result: `Graphics/Visibility.hpp` (independent layer/distance/frustum
policies, conservative classification, per-reason statistics, LOD selection
with hysteresis). Street's scene reports its culling (per reason, items and
instances) and group LOD counts through `CullStatistics::Record`,
`LodStatistics` and `FormatDiagnostics`; the visible sets equal Street's and
car-simulator's own.

### ENG-034 — Add instanced rendering `[x]`

Acceptance:

- instance data layout and update frequency are not hard-coded to Street;
- capability detection selects an explicit instanced or fallback path;
- batching preserves material/pass ordering;
- a representative Street group matches the original output and improves or
  preserves measured frame cost.

Blocked by: ENG-032 and ENG-033.

Result: `Graphics/Instancing.hpp` (per-instance world stream, HiDef hardware
path via `DrawInstancedPrimitives`, per-instance fallback, culling,
upload-on-change, binding restore). Street's `InstancedMesh` draws
pixel-identically on the hardware path (`docs/pilots/street-simulator.md`).
In Street's real scene all 51 instanced draws (93 groups, 192 visible
instances) go through engine instanced render items with Street's `PbrEffect`
(which reads the instance stream) and produce the same image; the frame's CPU
time is equal within run-to-run noise (engine 7.4–9.9 s, Street 7.6–10.8 s
over the same runs, software renderer, Debug).

### ENG-035 — Add pass orchestration incrementally `[~]`

Deliver in this order:

1. opaque and alpha-tested geometry;
2. transparent geometry;
3. shadow submission and cascades;
4. post-processing hooks;
5. reflection-probe hooks only after a pilot requires them.

Acceptance:

- pass dependencies and resource lifetimes are explicit;
- unsupported features degrade through documented fallbacks;
- a custom game pass can be inserted without modifying engine internals;
- visual captures cover each supported renderer profile.

Blocked by: ENG-032. Later substeps depend on ENG-033 and ENG-034 as needed.

Result: substeps 1–2 (opaque, alpha-tested, transparent) plus overlay and
custom passes at four points in `ForwardRenderer`; material binders make the
effect side replaceable. Shadows/cascades are deferred: Street currently draws
no shadows (CNA's cascaded shadow map was removed upstream; see
`docs/pilots/street-simulator.md`). Post-processing hooks likewise wait for
a pilot that needs them (Street's renderer has no post pass at present).
Step 5 (reflection probes): `PbrMaterialBinder::EnvironmentSelector` with
`RenderItem::userData`; Street's probe-lit frame is reproduced to 75 differing
pixels of 129,600 (probes change 82 % of Street's own frame).

### ENG-036 — Add quality profiles and capability resolution `[x]`

Acceptance:

- user intent, detected capability, and resolved runtime setting are separate;
- profiles are serializable and validated;
- fallback decisions are inspectable through diagnostics;
- both pilots can map existing quality tiers without silently changing them.

Blocked by: ENG-035.

Result: `Graphics/Quality.hpp` (intent, capabilities from the XNA profile,
resolved settings with recorded decisions, validated text persistence) and
`Graphics/QualitySchema.hpp`, where a game declares its own typed settings,
bounds and device limits and defines presets as data. Both pilots' tiers map
exactly (car-simulator's 3 tiers × 4 settings, Street's 4 presets × 26
settings); on a limited device only declared limits change values, each
recorded (`docs/pilots/`).

### ENG-037 — Add optional debug presentation `[x]`

Acceptance:

- metrics are available without rendering an overlay;
- an enhanced adapter can present them using existing font/debug facilities;
- game-specific benchmark UI remains outside the engine.

Blocked by: ENG-025 and ENG-030.

Result: `Graphics/FormatDiagnostics()` makes every metric available without an
overlay; `Extended/DebugOverlay.hpp` draws a panel and frame-time graph with
cna-extended's shape drawing and text with a game-supplied `BitmapFont`.
Pixel tests cover the panel, the budget-coloured graph and the text path
(with a font built in code).

### MILESTONE-2 — Street rendering vertical slice `[x]`

Exit criteria:

- one representative Street scene uses engine camera/input, assets, generic
  meshes, materials, render queues, culling/LOD, and instancing;
- screenshot comparisons have agreed tolerances;
- CPU time, draw calls, submissions, culled objects, and GPU resource estimates
  are compared with the original implementation;
- direct CNA escape hatches are demonstrated and documented.

Result (2026-10-20): Street's own `SceneRenderer` draws its city through the
engine on Street's adoption branch — engine camera state (Street's
matrices), generic meshes (borrowed), materials, render queue, culling and
LOD, instancing, an `AssetRegistry` for its derived materials, a
`QualitySchema` for its presets — and the scene comparison reaches a view by
engine input. Screenshots stay inside the agreed tolerance on the software
renderer and on OpenGL; CPU time, draw calls, submissions, culled objects and
GPU resource estimates are compared with Street's own implementation
(`docs/pilots/street-simulator.md`, `docs/pilots/migration.md`); the sky
pass and `ForwardRenderer::LitEffect()` are the documented escape hatches.
Enforcing the performance budget waits for the idle-machine measurement the
owner chose (REL-003, `docs/pilots/performance-budgets.md`).

Estimated active AI-agent effort from project start: 180–300 hours for a useful
MVP; 320–500 hours for the complete Phase 4 scope with robust tests.

## Phase 5 — two-consumer validation and staged migration

Progress (2026-10-19): `examples/street_slice` renders a procedural street
block through engine camera/input, generic meshes, PBR materials
(`EnhancedGraphics`), render queues, frustum/distance culling, LOD with
hysteresis and instanced lamps, with diagnostics and a PNG capture; it runs as
a CTest example. Street's own camera, controls and tangent-frame meshes are
validated by equivalence probes. A real Street scene (Street's own `CityScene`) is drawn by Street's
`SceneRenderer` and by the engine over the same buffers, with traffic and
pedestrians: identical visibility (1,110 items, 192 instances, 123 skinned
figures), identical draw calls (1,284, instanced groups as instanced render
items), CPU frame time equal within noise. With Street's lighting carried by
the engine binder and Street's sky drawn as a custom pass (the direct-CNA
escape hatch), the images are practically identical: mean difference
0.01/255, 68 of 129,600 pixels differ (`docs/pilots/street-simulator.md`).
Screenshot tolerances (mean ≤ 0.5/255, ≤ 1 % of pixels visibly different)
are enforced by both scene comparisons and were agreed by the owner on
2026-10-20 (`docs/pilots/screenshot-tolerances.md`). Open: the performance
budget measurement (reference: this machine, Release, when idle). Street
itself draws through the engine on its adoption branch since 2026-10-20
(PILOT-001). Engine input
drives the slice's camera since 2026-10-20 (a third view reached by an
`ActionMap` and a `FlyCameraController`: visibility and draw calls identical,
0.20/255); Street's own asset libraries stay (the engine's `AssetRegistry` is
validated on Street's derived-material cache separately). GPU resource estimates are compared since 2026-10-20
(`GpuMemoryEstimate`; Street's own figure overstates meshes by 8 bytes per
vertex, UP-008).

### PILOT-001 — Validate `cna-street` `[x]`

Acceptance:

- migrate one complete vertical slice before moving broad utility code;
- document every adapter and game-specific facility intentionally retained;
- all existing Street tests stay green;
- performance and visual baselines show no unexplained regression.

Blocked by: MILESTONE-2.

Progress: equivalence probes against Street's own code (camera, fly
controls, tangent-frame meshes and upload, instancing, quality presets,
derived-material cache) — `docs/pilots/street-simulator.md`. Adoption (allowed
by the owner on 2026-10-20): Street's branch `engine-adoption` draws the whole
city through the engine — `SceneRenderer`'s culling and frame through
`ForwardRenderer` with the `PbrMaterialBinder`, instance groups as
`InstanceSet`s, skinned figures, probes through the environment selector —
its quality presets are a `QualitySchema` and its derived materials live in
an `AssetRegistry`; its suite (15/15) stays green,
all 18 named viewpoints and a night view stay inside the agreed tolerance,
draw calls are identical and the frame is no slower
(`docs/pilots/migration.md`, "street-simulator adoption"). Open: captures on
a GPU renderer.

### PILOT-002 — Validate strict-XNA Car Simulator `[x]`

Migrate or probe input actions, camera data, mesh data/upload, diagnostics, and
screenshot capture. Do not migrate vehicle physics or domain rendering policy.

Acceptance:

- strict API checker remains green;
- all non-display tests pass;
- display smoke tests run in an environment with an appropriate renderer budget;
- direct CNA extension use remains forbidden;
- at least two engine abstractions are exercised by real application code.

Blocked by: MILESTONE-1 and ADR-001.

Progress: car-simulator's real map drawn through the engine's strict
`ForwardRenderer` matches its own `WorldRenderer` in every visibility count
and to 0.1/255 in the image; equivalence probes against car-simulator's own
code (controls, chase camera, meshes and upload, screenshots, plate cache,
quality tiers) and
its unmodified strict checker accepting the engine's strict surface with the
ADR-001 allowance — `docs/pilots/car-simulator.md`. Adoption (allowed by
the owner on 2026-10-20): car-simulator's branch `engine-adoption` runs on
the engine — input actions, fixed-step physics, screenshots, the plate
cache, quality tiers and its whole world renderer (terrain, roads, objects,
trees, signs, headlight fill, lamp light through `ForwardRenderer`); its
suite (7/7) and strict checker stay green and captures in seven scenarios
stay inside the agreed tolerance (largest difference 2/255)
(`docs/pilots/migration.md`, "car-simulator adoption"). Open: its display
smoke tests on a GPU renderer.

### PILOT-003 — Stabilize only proven APIs `[x]`

Acceptance:

- APIs exercised by both pilots receive compatibility tests and stability
  status;
- single-consumer abstractions remain experimental;
- duplicated game code is removed only after equivalent behavior is measured;
- migration notes include reversibility and known differences.

Blocked by: PILOT-001 and PILOT-002.

Progress (2026-10-20): both pilots' adoption branches replace their own code
step by step, each step measured before the pilot's code was removed
(`docs/pilots/migration.md`: suites, captures against the agreed tolerance,
benchmarks); every step is one revertible commit with its known differences
listed. `docs/api-status.md` records which facilities each branch uses; the
nine both use are the candidates for `stable`, which waits for the owner to
merge the branches. The API snapshot test is the compatibility check for
them. Result (2026-10-20): both branches were merged into the pilots' main
branches; the nine facilities both pilots use are `stable`
(`docs/api-status.md`), single-consumer facilities keep their
`pilot-validated`/`experimental` status.

### PILOT-004 — Replace easy-3d in galaxy-eggbert `[x]`

easy-3d (`openeggbert/easy-3d`, namespace `Easy3D`) is a small helper library
over CNA — position/target camera, orbit and follow cameras, texture atlas,
cube/plate/billboard mesh builders with their renderers, a debug-line queue —
whose only consumer is galaxy-eggbert. The owner decided on 2026-10-20 to
move what is useful into the engine (namespace `CnaEngine`, no `Easy3D`
compatibility layer) and to migrate galaxy-eggbert onto the engine.

Acceptance:

- what the engine takes over produces exactly easy-3d's vertices, indices and
  matrices (read-only probe `Easy3DEquivalenceTests`);
- galaxy-eggbert builds without easy-3d on its branch `engine-adoption`, its
  suite stays green and its golden captures stay inside the agreed tolerance;
- what is not taken over (the CPU batches, the pyramid tip) is listed with
  where it went.

Progress (2026-10-20): the engine took over `Camera3D` (`Camera::SetLookAt`),
`OrbitCamera`/`FollowCamera` (controllers), `TextureAtlas`/`UvRect`, the
cube/plate/triple-cross/billboard meshes (`MeshBuilder`), the mesh renderers
(`GpuMesh::Draw` with an effect) and `DebugDraw` (now with a renderer); the
probe compares all of them with easy-3d's sources bit for bit. Not taken over:
`CubeBatch`/`BillboardBatch` (a vector of items; the builder is the batch) and
the pyramid tip (one game's shape, built in galaxy-eggbert with `AddVertex`/
`AddTriangle`).

Result (2026-10-20): galaxy-eggbert runs on the engine on its branch
`engine-adoption` (worktree `openeggbert/galaxy-eggbert-engine`, not
pushed): build, camera, geometry and drawing, removal of easy-3d, docs —
one commit each. After every step its 93 CTest cases pass and its golden
frames, behavioral trace and startup screenshots are byte-identical to the
easy-3d build, under llvmpipe and on the GPU; the editor runs in an
`OPENGL33` build (`docs/pilots/migration.md`, "galaxy-eggbert"). What is
left: the owner's decision to merge the branch (and push it), and archiving
easy-3d, which nothing uses any more. The owner merged and pushed it the same
day (galaxy-eggbert `develop` at `5c5b727`).

Audit (2026-10-20, owner's question whether everything useful moved): every
public declaration of easy-3d's 13 headers, its tests, its example and its
documentation's six recipes checked against the engine. Everything has an
engine counterpart except, deliberately: the CPU batches (`CubeBatch`,
`BillboardBatch`: the builder is the batch), the pyramid tip (galaxy-eggbert's
hanging teleporter spike with its own texture mapping, built in the game),
`DebugDraw`'s box list (the engine stores boxes as their twelve edges and
draws them) and easy-3d's version constants. One gap was found and closed:
`FollowCamera::SetPosition`/`ApplyTo` became
`FollowCameraController::SetPosition`/`Apply` (equivalence probe extended).

Closure (2026-10-21): the equivalence oracle was reduced to a test-only,
MIT-attributed fixture under `tests/fixtures/easy3d_legacy` pinned to easy-3d
`ad63bf9`; the engine no longer reads an easy-3d checkout and the five tests
can no longer disappear conditionally. `NOTICE.md` records the origin. The
repository has no remaining product, build, CI or test dependency and may be
archived or removed.

### PILOT-005 — Adopt the engine in backrooms-simulator and school-simulator `[x]`

A read-only survey of the seven other simulators (2026-10-21) found two
good candidates: backrooms-simulator (one unlit BasicEffect pass over
streamed chunks, small) and school-simulator (a streamed interior world,
dormant since 2026-10-15); island, museum, house, room and train were not
recommended (museum is CNA's coverage instrument, island and house are
active or bound to XNA-only rules, room's renderer needs CNA extensions
throughout, train's renderer is hand-ordered custom passes). The owner
allowed both migrations on 2026-10-21.

Acceptance:

- each game builds against the engine's strict modules and a static CNA on
  a branch `engine-adoption`, one revertible commit per step;
- every step is checked against deterministic captures of the step before
  it (a capture mode is added first where the game has none) and the
  game's own suites;
- what the engine lacks for them is added to the engine only when a step
  needs it, with tests, and listed in `CHANGELOG.md`.

Progress (2026-10-21): backrooms-simulator runs on the engine on its branch
`engine-adoption` (worktree `libcnalab/backrooms-simulator-engine`, not
pushed): deterministic captures, build, capture writing, chunk drawing
through `ForwardRenderer`, keys as actions, title statistics, camera — 30
views byte-identical after every step on llvmpipe and on the GPU, its
controls tool passes (`docs/pilots/migration.md`, "backrooms-simulator").
The engine gained borrowed non-indexed meshes (`GpuMesh::Borrow` of a plain
vertex list) for it.

Result (2026-10-21): school-simulator runs on the engine on its branch
`engine-adoption` (worktree `libcnalab/school-simulator-engine`, not
pushed): deterministic fixed-step pictures, build, screenshots, camera, mesh
views, the world geometry through `ForwardRenderer`, system keys as actions,
the texture cache as an `AssetRegistry` — 19 views identical after every
step on llvmpipe and on the GPU, its whole check battery green. The engine
gained `CaptureOptions::opaque`, `Material::frontFace` and
`Material::straightAlpha` for it. Both branches were fast-forwarded into
the games' `main` and pushed at the owner's request the same day, and this
repository's `develop` with them.

### PILOT-006 — Adopt the engine in train-simulator `[x]`

The owner allowed it on 2026-10-21 after a second read-only survey of the
remaining simulators: train-simulator is dormant, uses only XNA 4.0 types
and has repeatable audit views; island and museum are under active work,
house's ADR-0001 admits XNA types only, and room's renderer is built on CNA
extensions (shader packages, render targets) throughout.

Acceptance: as PILOT-005 — a branch `engine-adoption` with one revertible
commit per step, each checked against deterministic captures of the step
before and the game's own suites; engine additions only where a step needs
them, with tests, in `CHANGELOG.md`.

Result (2026-10-21): train-simulator runs on the engine on its branch
`engine-adoption` (worktree `libcnalab/train-simulator-engine`): repeatable `--lockstep` captures, build, screenshots, camera, mesh
segments, system keys as actions, and the world and train through
`ForwardRenderer` with the game's own binder over its effects and shaders —
54 views pixel-identical after every step (llvmpipe; the last step also on
the GPU), report counters unchanged, CTest 73/75 with both failures predating
the migration or due to the build's system SDL (`docs/pilots/migration.md`,
"train-simulator"). The owner's first desktop run found a crash the game has
with any CNA since 2026-10-19 (sit/stand bone blends that XNA's exact
`Matrix::Decompose` refuses); fixed on the branch. The engine gained
`RenderBucket::Sequenced` (hand-ordered scenes) and byte-based screen
captures for it. At the owner's request the branch was fast-forwarded into
the game's `develop` and pushed the same day, after this repository's
`develop`.

### SHARE-001 — Facilities the simulators each wrote `[~]`

A read-only survey (2026-10-21) of the nine simulators and galaxy-eggbert
for work each repeats. The owner asked for the four strongest candidates.

Acceptance: each is an engine facility with unit tests and, where a game
has the same code, a read-only probe matching it (bit for bit where the
games compute the same way); differences found in the games go to
`upstream-issues.md`.

Result (2026-10-21), four experimental facilities:

- `Graphics/Image.hpp` and `Core/Noise.hpp` (`6f33311`): CPU RGBA8 images,
  mip chains (truncating or rounding box filter, sRGB, alpha-test coverage),
  upload; street-simulator's noise. The image probe matches car-, train-,
  school- and room-simulator and street's noise bit for bit; backrooms' and
  island's loops are inline in their renderers and were read, not compiled.
- `Core/SunPosition.hpp` (`9a4377d`): Cooper's declination, the hour angle,
  elevation, azimuth, direction. The games compute differently (single
  precision, an ephemeris), so the sun probe compares car, room and house
  within tolerances; it found UP-016. train- and island-simulator draw an
  artistic sun path and street only scatters light for a given sun, so they
  have no solar geometry to share.
- `Core/SettingsFile.hpp` (`7e95498`): key = value preferences, typed
  clamped reads, atomic save, the XDG location. The settings probe reads
  train-simulator's files as the same values and writes the same bytes;
  island keeps its settings through XNA storage and house as JSON.
- `Graphics/AutomatedRun.hpp` (`49a20be`): the shared --frames,
  --run-seconds, --screenshot and --lockstep options and their driver
  (unit tests; car's and train's code as evidence).

Adoption (2026-10-21, the owner: "převeď je: vlak, škola, car a street"),
each game on a branch `engine-shared` in its `libcnalab/*-engine` worktree,
fast-forwarded into car's and school's `main` and train's and street's
`develop` and pushed on 2026-10-22:

- car-simulator (`eec91c2`, `77c36ee`): `Image::Downsampled`,
  `UploadTexture` and `UpdateTexture` delegate to the engine; the road-shadow
  worker builds its levels with `BuildMipChain`. The eleven reference
  captures are byte-identical; CTest 7/7.
- school-simulator (`c48b243`, `0eccf53`): the texture library uploads
  through `UploadTexture`; `ImageUtil` is gone. The 19 tour captures are
  pixel-identical; CTest 5/5.
- train-simulator (`87da297`, `b9d743c`, `683b694`, `2fd191d`): procedural
  textures through `Image`; preferences through `SettingsFile` (strictness
  kept on top with `Line` and `TryGetFloat`, added for it in `69cf3ea`) in a
  game-only library; `--frames`, `--run-seconds`, `--screenshot` and
  `--lockstep` through `AutomatedRun`. 54 of the 56 lockstep views
  pixel-identical (the two onboard avatar views vary between runs anyway),
  counters unchanged, CTest 73 of 75 with the two known failures.
- street-simulator (`2e40f0c`): `CnaStreet::Noise` forwards to
  `CnaEngine::Noise` under its old names. All 215 surfaces `bake-assets
  --content` writes are byte-identical; CTest 15/15. A full `--capture`
  could not be run: it was OOM-killed at 23 GB while importing the models,
  with the unchanged game too (UP-017).

The image and settings probes now compare the engine with frozen copies of
the replaced code (`tests/fixtures/simulator_oracles`, `dd8970b`), so they
stay meaningful once the games call the engine. Not adopted in car: the sun
(car computes it in single precision and mirrors the azimuth before 01:00,
UP-016, so the engine's would change its pictures slightly rather than
reproduce them) and the automated runs (car's also end once a route has been
driven, save the instrument cluster's texture and print the benchmark in the
same frame, and read the screenshot before Present; its options are parsed
in `carsim_core`, which links only `CnaEngine::Core`, while
`ParseAutomatedRunOption` is in Graphics). room and house, which also compute
the sun, were not part of the request.

## Phase 6 — release readiness

Progress: facilities validated by both pilots are marked `pilot-validated`
in `docs/api-status.md`, their headers are fingerprinted
(`docs/api-snapshot.txt`, test `cna_engine.api_snapshot`) and changes go to
`CHANGELOG.md`. `stable` status waits for adoption in the pilots.
`docs/pilots/migration.md` is the step-by-step migration plan for both pilots
with the evidence, known differences and reversal of every step.

### REL-001 — Package and version the library `[~]`

Acceptance:

- installed CMake package, semantic-version policy, license inventory, and
  dependency notices are complete;
- source and binary consumers use only public artifacts;
- CNA and `cna-extended` compatibility ranges are machine-checked.

Progress: installed CMake package with `SameMinorVersion`, MIT `LICENSE`,
`NOTICE.md` license/provenance inventory (installed with the package),
`CHANGELOG.md`, API snapshot of pilot-validated headers, and baseline revisions
machine-checked at configure time, including where a differing checkout
stands relative to the pin (descendant by N commits, or older/diverged;
`CNA_ENGINE_REQUIRE_BASELINE_DESCENDANT` enforces the range). Open:
CNA/cna-extended packages (UP-001) and version ranges once dependencies have
releases.

### REL-002 — Complete documentation and examples `[~]`

Acceptance:

- quick start demonstrates a small game without requiring Street knowledge;
- strict and enhanced profiles have separate guides;
- lifecycle, ownership, rendering extension points, and fallback behavior are
  documented;
- examples run under the supported headless/software audit profile.

Progress: `README.md` quick look, `docs/guide-strict.md`,
`docs/guide-enhanced.md`, `docs/conventions.md`, and three headless examples
that run as tests, among them `examples/quick_start` — a small complete game
as an XNA `Game` (input actions, fixed-step simulation, materials and
samplers, culling, instancing, the forward renderer, diagnostics,
screenshot), scripted so the test is deterministic. Revisited after both
pilots adopted the engine (2026-10-20): the quick start keeps the
engine-owned camera as the pattern for new games; what existing games
needed — their own camera matrices (`Camera::SetMatrices`), borrowed meshes,
game passes between engine renders, overlay presets — is in the strict and
enhanced guides.

### REL-003 — Establish the verification matrix `[~]`

Acceptance:

- unit, boundary, installed-consumer, visual, and performance checks are
  separated;
- supported platform/renderer combinations are explicit;
- CI and local documentation cap compilation at three jobs for this workspace;
- known skips and environmental timeouts cannot appear as silent success.

Blocked by: PILOT-003.

Progress: `docs/verification-matrix.md` separates unit, boundary, example,
installed-consumer, pilot, baseline, upstream and visual checks and lists the
verified combinations; CI caps jobs at three and fails on skipped tests.
Performance scaling checks run under their own label since 2026-10-20 (no
budgets: they fail on super-linear growth). The pilots' adoption is verified
on the software renderer, OpenGL on Mesa llvmpipe and the machine's AMD GPU
(`docs/verification-matrix.md`). Open: enforcing the agreed frame-time
budget, which waits for its idle-machine measurement
(`docs/pilots/performance-budgets.md`).

## Phase 7 — a new game from CNA and the engine alone

Asked by the owner on 2026-10-22 ("udělej to vše, jak navrhuješ … pokračuj,
dokud nebudeš mít hotový celý plán"). Phases 1–6 extracted what existing
games had; someone starting a game from nothing still has to write the
build, the game loop's wiring and every common game system themselves.
The simulators show which systems those are: each wrote its own.

Rules for this phase:

- Every facility names the games whose code is its evidence (at least
  two, `conventions.md`); where the games compute the same thing, a
  read-only probe compares them, as in SHARE-001. Games are read, never
  changed: adopting a facility in a game needs the owner's request.
- New facilities are `experimental` until a game adopts them.
- Strict placement (XNA 4.0 types only) unless the facility needs a CNA
  extension; then it goes to the enhanced profile, with a strict fallback
  where one is possible.
- Each facility has unit tests; GPU-facing ones a software-renderer test
  that checks pixels; each new example runs headless as a test.
- Nothing from "Deferred work and non-goals" below: no editor, no general
  rigid-body physics, no navmesh/AI framework, no own ECS, no shader graph.

### Part A — starting a game

### GAME-001 — One CMake call builds a game for the desktop and the browser `[x]`

Evidence: the CMake of backrooms-, school- and train-simulator (115, 188
and 905 lines) and the browser builds of backrooms, school and house, which
repeat the same Emscripten recipe and shell page.

Acceptance:

- `include(<engine>/cmake/CnaEngineGame.cmake)` adds CNA and the engine
  with game defaults (desktop renderer, SDL3 platform and audio, static
  CNA, dependency tests off; WebGL 2 under Emscripten);
- `cna_engine_add_game(<target> SOURCES … [ASSETS <dir>] [PROFILE
  strict|enhanced] [WEB_SHELL <file>] [TITLE <text>])` builds the
  executable, copies the assets beside it on the desktop, preloads them
  under Emscripten (exception ABI, Asyncify, memory, the renderer link
  contract), and packages `web-package/` (page, script, wasm, data,
  `index.html`);
- the engine ships a default shell page (loading bar, pointer lock, options
  from the address);
- a test configures a game through the function in the engine's build.

Result (2026-10-22): `cmake/CnaEngineGame.cmake`,
`cmake/CnaEngineGameFunctions.cmake`, `cmake/CnaEngineWebPackage.cmake`,
`cmake/web/shell.html`. `examples/hello_engine` is built with the function
(desktop test with a screenshot; an Emscripten build of the same example in
`build-probe/` ran in headless Chrome: WebGL 2, the preloaded texture, the
page's hint). An empty renderer/platform/audio value leaves the choice to
CNA. With a shared CNA (the engine's own default build) the function links
`CNA::Runtime` instead of the `CNA` umbrella (UP-006).

### GAME-002 — Fetch the dependencies at the pinned revisions `[x]`

Acceptance: `tools/fetch_dependencies.sh [dir]` clones CNA, Sharp Runtime
and easy-gl at the baseline revisions (ADR-002) beside the engine, or
reuses checkouts that are already there, and initialises only the
submodules a game build needs; `--dry-run` prints the plan and is tested.

Result (2026-10-22): `tools/fetch_dependencies.sh` (`--dry-run`,
`--with-extended`, `--no-sdl`, `CNA_ENGINE_GIT_BASE`); all five repositories
are public on GitHub (`libcna/…`). easy-gl `f4d6afa` (0.1.1) and meta-gl
`dd3b300` (0.4.1) pinned in `cmake/CnaEngineBaseline.cmake`. Test
`cna_engine.fetch_dependencies_dry_run` reads the expected pins from that file.

### GAME-003 — `EngineGame`: the game loop already wired `[x]`

Evidence: `examples/quick_start` (425 lines, most of them wiring) and the
game classes of backrooms, school and train, which each wire the same
facilities by hand.

Acceptance:

- a strict target `CnaEngine::App` with `EngineGame`, an XNA `Game` that
  owns the device manager, the action map (with default bindings for quit,
  screenshot, statistics), a fixed-step clock, a camera kept at the
  window's aspect, the forward renderer, frame and render statistics, the
  screenshot queue, automated runs from the command line and the
  preferences file;
- a game overrides `LoadGame`, `FixedUpdate`, `Update` and `Submit` (render
  items into the frame) and may draw over the frame; every part can be
  replaced or left out;
- an asset-directory lookup that works beside the executable, from the
  working directory and in the browser's preload;
- `examples/hello_engine` shows a lit, textured, moving scene in well under
  100 lines and runs headless as a test.

Result (2026-10-22): strict target `CnaEngine::App` (`EngineGame`,
`RunGame`, `Frame`, `AssetDirectory`, `MouseCapture`; `App` added to the
boundary checker's strict modules) and `Enhanced/RelativeMouseCapture` for
browser mouse look. `hello_engine` is 81 lines. Found on the way: CNA
creates the device when `Run()` starts, before `Initialize`, so the window's
settings are applied in the constructor and `ParseCommandLine` (preferences
read in `Initialize` are applied with `ApplyChanges`).

### GAME-004 — A project template and `tools/new_game.sh` `[x]`

Acceptance: `tools/new_game.sh <dir> <Name>` writes a working game project
(CMake through GAME-001, a game class on `EngineGame`, assets, README,
`tools/build_web.sh`); the template's game is also built and run by the
engine's tests, so it cannot rot.

Result (2026-10-22): `templates/game/` (a first-person walk among crates on
`EngineGame`, PROFILE enhanced for the browser's pointer lock; CC0 asset)
and `tools/new_game.sh` (target, class and engine path from the name;
refuses a non-empty directory and names with `"` or `\`). Tests:
`cna_engine.new_game` writes a project and checks it;
`cna_engine.example.template_game` builds and runs the template's sources.
Verified as a new user would: a project written into `build-consumer/new_game`
built on its own through `CnaEngineGame.cmake` (CNA's default OpenGL ES 3,
SDL3), ran on the GPU offscreen (Mesa radeonsi), and its `tools/build_web.sh`
build walked in headless Chrome. That run found that the function's
directory variable was invisible from a game's own directory (now a global
property).

### GAME-005 — Tutorial and examples `[ ]`

Acceptance: `docs/tutorial.md` builds a small 3D game step by step with
the facilities of this phase; examples for a first-person walk (collision,
interaction), a third-person animated character and the sky and lights
run headless as tests.

### Part B — what every game wrote itself

### GAME-010 — Models into render items `[x]`

Acceptance: an XNA `Model` (from the content manager or CNA's runtime glTF
import) becomes render items with materials and bone transforms in a few
lines; strict for XNA `Model`, enhanced where CNA's importer is an
extension.

Result (2026-10-22): strict `Graphics/ModelView.hpp` (every part borrowed,
its material read from the effect -- `StockMaterialFromEffect` for
BasicEffect, SkinnedEffect and AlphaTestEffect, or a game's reader; rigid
parts on their bone chain, `SetBoneTransforms` for node animation; skinned
parts posed by a palette) and `Frame::Add(ModelView, world, palette)`.
Enhanced `Enhanced/ModelImport.hpp`: `PbrMaterialFromEffect` (street's and
room's PbrEffect translation), `FindSkinningData` (skins, else Tag). Fixture
`tests/fixtures/models/skinned_strip.gltf`, made by the engine's own
`make_skinned_strip.py` (MIT): CNA imports it at run time and the PBR
binder draws its skinned part, bent and straight (the risk street's
imported figure showed -- nothing drawn -- does not occur here).

### GAME-011 — Skeletal animation `[x]`

Acceptance: an animation player samples clips (interpolated keyframes,
bone hierarchy, cross-fade), fills the bone palette that skinned render
items already take, and is tested against hand-computed poses.

Result (2026-10-22): strict `Animation/Skeleton.hpp` (`BonePose`,
`Skeleton` with `FromBindPose`, `BoneTrack`, `AnimationClip`) and
`Animation/Animator.hpp` (play, loop or once, speed, cross-fade, stop to
the bind pose, palette, bone transforms for attachments); `Animation` added
to the strict modules. Enhanced `SkeletonFromSkinningData` and
`ClipsFromSkinningData`. Tests: hand-computed poses (a 90 degree bend, the
45 degree midpoint, a cross-fade that cancels two opposite bends) and a
probe against CNA's own `AnimationPlayer` on the imported fixture: every
palette element of both clips at six times within 2e-4.

### GAME-020 — Static collision world and raycasts `[x]`

Acceptance: boxes, triangles and height fields in a spatial grid; ray,
sphere and capsule queries with hit point, normal and distance; strict.

Result (2026-10-22): `Collision/StaticWorld.hpp` in `CnaEngine::Core`
(`Collision` added to the strict modules): boxes, two-sided triangles
(`AddTriangles` for meshes), vertical cylinders and one height field; a
uniform grid over X and Z, so floors above one another share cells; rays
walk the grid nearest cell first; capsule contacts give the way out
(normal and depth, deepest first). Closest points after Ericson (cited in
the private `CollisionMath.hpp`). Not a physics engine: no moving or
reacting shapes. 12 unit tests, the games' cases among them (nearest hit,
eye inside a wall, ray in a triangle's plane, an upper floor that does not
touch the floor below).

### GAME-021 — Character controller `[x]`

Acceptance: a capsule that walks, slides along walls, climbs steps, keeps
to slopes up to a limit, falls under gravity and jumps, deterministic for
a fixed step; drives the walk camera.

Result (2026-10-22): `Collision/CharacterController.hpp`: collide and slide
in substeps of half the radius; steep ground acts as a wall (pushes stay
horizontal); a grounded body rides up edges below its lower sphere's
centre when walkable ground lies behind them (a short ray decides: kerbs,
stair treads, low posts), and lifts by the step height and on by at least
its radius for ledges above that centre; it follows the ground down by
`stepDown`, falls, jumps, counts strides (footsteps) and gives a head bob.
`WishVelocity` turns camera-relative input into a velocity on
`WalkCameraController`'s basis. 13 tests: walls and sliding, a kerb but
not a block, stairs up (0.17 m risers) and down, a 0.33 m step, a ledge
fallen off, 20 degree ramp walked and 60 degree ramp refused, jump height,
bit-identical positions at 30, 60 and 144 fps, strides, height fields. The
template game walks on it (stairs to a platform, jumping).

### GAME-030 — Sky, fog and time of day `[x]`

Acceptance: a sky dome with a gradient, sun disc and colour ramps over the
day (with `SunPosition`), fog matched to the horizon, and a lighting rig
that follows the time of day; strict.

Result (2026-10-22): strict `Graphics/Sky.hpp`: `SkyColors`,
`DayNightPalette` (night below -18 degrees, twilight, sunrise, golden light,
day; overcast greys it), `TimeOfDay` (SunPosition's clock), `ApplySkyLighting`
(sun, or the moon from the other side at night; sky fill from above;
ambient; fog in the horizon's colour) and `SkyDome` (vertex colours through
BasicEffect, drawn with `DrawUserPrimitives` as the renderer's first pass:
horizon-to-zenith gradient, ground colour below and closed underneath, a
glow round a low sun, the sun's disc, stars that fade in). Evidence: the
domes of car, train, school, museum and house. Tests: palette keys and
interpolation, overcast, the noon sun at 50 degrees north, sun and moon
lighting, and pixels of the dome (gradient, disc, stars). The template's
day passes in 48 minutes.

### GAME-031 — Shadows `[x]`

Acceptance: decided by what CNA offers at the baseline: shadow maps where
the effects can receive them, otherwise planar and contact shadows for
strict games; the choice and fallbacks documented.

Result (2026-10-22): CNA at the baseline keeps only the receiving side
(`IShadowReceiverEXT` on BasicEffect, SkinnedEffect, PbrEffect and
SkinnedPbrEffect; `SupportsShadowSamplingEXT`); its producers were removed
upstream. The engine's producer is strict: `Graphics/ShadowMap.hpp` draws
the light-space depth with XNA's own BasicEffect (SkinnedEffect for skinned
casters) -- the linear fog of an orthographic light view is exactly the
receivers' `z/w * 0.5 + 0.5` -- into a Single target (else HalfSingle, else
Color), drawn upside down because the receivers read `v = y * 0.5 + 0.5`;
the view moves in whole texels of a world-fixed grid; the bias follows the
precision the target really keeps, measured when it is made (UP-018: the
software renderer stores Single as 8-bit). `EngineGame::EnableShadowMap`
renders it from each frame's opaque items before the frame; reception is
enhanced: `Enhanced/ShadowReception.hpp` (`ApplyShadowReception`,
`EnableShadows`), through the binders' effects (additive accessors
`StockMaterialBinder::SkinnedLitEffect`, `PbrMaterialBinder::SkinnedEffect`).
Tests: the stored depths of a floor and a box; a box's shadow on the floor
on the side away from the light, through the stock and the PBR binder, with
mirrored places unshadowed. The template casts shadows. Not built: planar
(stencil) and blob shadows for strict games; strict games can draw blob
discs with ordinary blended items. The receivers read the map the other
way up on the software renderer than on EasyGL (UP-019): the producer's
orientation is a setting, measured once by `Enhanced::CalibrateShadowMap`;
checked on the software renderer (tests) and on the GPU (OpenGL ES 3, Mesa
radeonsi: the template's crates and platform cast their shadows away from
the sun).

### GAME-040 — 3D sound `[x]`

Acceptance: a strict `CnaEngine::Audio` target: a listener that follows the
camera, positional one-shots and loops with a voice limit, ambience, and
silent operation without an audio device.

Result (2026-10-22): strict target `CnaEngine::Audio` (`Audio` added to the
strict modules), `Audio/SoundSystem.hpp`: `Spatialize` (reference
distance, rolloff, a fade over the last part of the reach, pan from the
listener's right; hand-computed like backrooms, car and house rather than
Apply3D, so it is the same on every platform), `ListenerFrom(Camera)`,
`SoundBank` (seeded variants), and `SoundSystem`: one-shots and loops,
positional or stereo, category volumes under a master and mute, a voice
budget by priority then loudness (loops wait silently, one-shots are
dropped), finished one-shots retired, and on any audio failure a Silent
state with the reason instead of exceptions. Instances come from XNA's
`CreateInstance`, built in place (no use of CNA's move constructor, an
extension). Tests on CNA's null audio platform. The template plays
footsteps (two generated CC0 WAVs) on the character's strides;
`cna_engine_add_game` links `CnaEngine::Audio`.

### GAME-050 — Text, menus and HUD `[x]`

Acceptance: text with a font a game can get without the XNA content
pipeline, a menu navigated by actions, HUD anchoring, and a settings screen
built from a `QualitySchema` and the action map's rebinding.

Result (2026-10-22): strict target `CnaEngine::UI` (`UI` added to the strict
modules). `UI/Font.hpp`: `IFont`, `BuiltInFont` (the engine's own 5 x 7
glyphs of the 95 printable ASCII characters, built into a texture: text with
no file at all; others draw as '?') and `SpriteFontText` (a content
SpriteFont). `UI/Painter.hpp`: a virtual resolution (720 units tall by
default), anchors, panels, text with a shadow, `Toasts`. `UI/Menu.hpp`:
`Menu` (buttons, choices, sliders, labels; selection skips labels and
wraps; the pointer selects and clicks), `MenuStack`, `MenuActions`
(declared, rebindable actions with keys and pad buttons), `ReadMenuInput`.
`UI/SettingsMenu.hpp`: `AddQualitySettings` (preset choice, on/off and
bounded sliders writing a `QualitySelection`) and `RebindRows` (a row
listens through `BindingCapture`; the captured key, still held, does not
act -- the ActionMap's focus-loss suppression). `EngineGame::CurrentInput`
for menus and capture. Tests: glyph pixels where the text is placed,
measuring, anchors, toasts, navigation, choices, sliders, pointer, stack,
quality rows, rebinding with a cancel. The template has a crosshair, a hint,
messages and a pause menu (resume, settings with the volume and the Jump
and Run keys saved in the preferences, quit). Czech or other non-ASCII text
needs a SpriteFont (content): the built-in font is ASCII.

### Part C — later in the same phase

### GAME-060 — Post-processing hook `[x]` (ENG-035 step 4)

Acceptance: the world drawn into an off-screen target and through a chain
of full-screen passes to the screen, the HUD on top untouched; a game can
add its own passes (any effect); built-in passes that need no custom
shader, so they run on every renderer.

Result (2026-10-22): strict `Graphics/PostProcess.hpp`: `PostProcessChain`
(scene target sized like the viewport, ping-pong between two targets, the
last pass into the back buffer), `PostPass`, and `ColorGradePass` (island's
time-of-day multiply), `FadePass`, `VignettePass` through SpriteBatch only;
`EngineGame::EnablePostProcessing`. Tests through an EngineGame: a plain
copy, a grade and a vignette chained, a fade, a pass removed. Bloom,
tonemapping and FXAA need custom shaders (room's vendored chain); a game
adds them as its own passes.
### GAME-061 — Local lights `[ ]`
### GAME-062 — cna-extended particles and animation adapters `[ ]` (ENG-030)
### GAME-063 — Save slots `[ ]`

Acceptance for each is written when its part B evidence is in.

## Deferred work and non-goals for 1.0

- editor and authoring suite;
- general rigid-body physics engine;
- navigation mesh generation and general AI framework;
- game-specific traffic, vehicle, train, ship, wildlife, or pedestrian systems;
- replacement ECS;
- replacement content pipeline;
- universal shader graph;
- wholesale migration of every simulator before two pilots stabilize the API;
- compatibility wrappers for every historical project-local class name.

## Effort envelope

These are active AI-agent hours for implementation, tests, integration, and
documentation. They are planning ranges, not elapsed wall-clock promises.

| Scope | Estimated hours |
| --- | ---: |
| Remaining upstream disposition and pinned baseline | 8–16 |
| Architecture and project skeleton | 20–35 |
| Strict-compatible foundation | 70–115 |
| Enhanced rendering vertical slice | 90–150 |
| Full enhanced Phase 4 | additional 140–220 |
| Two-pilot stabilization and migration | 65–105 |
| Packaging, documentation, and release checks | 35–60 |

A useful MVP is approximately 180–300 hours. A production-oriented first
release validated by both pilots is approximately 400–650 hours after normal
integration risk. A broader Street-class renderer with extensive migration and
polish can reach 650–1,000 hours.

## Definition of done for the first stable release

- CNA and `cna-extended` baselines are pinned and reproducible.
- The upstream read-only asset-loading defect remains resolved at or after CNA
  Extended commit `0061e3e17c30b2a3a302bce617e64765883079aa`.
- Strict and enhanced dependency checks prevent accidental API leakage.
- Street and Car Simulator each exercise multiple real engine facilities.
- Tests, examples, installed consumers, and supported smoke configurations pass.
- Visual and performance differences are measured and explained.
- Public ownership, error, lifetime, extension, and compatibility contracts are
  documented.
- No game-domain subsystem was moved solely to increase engine feature count.
