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
UP-005). Open: full street-simulator and car-simulator suites against the pin.

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
`upstream-issues.md` UP-004. Remaining: CNA fix, then re-enable the tests.

### EXT-003 — Establish an upstream compatibility lane `[~]`

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
mirror the 2026-10-19 local cna-extended run.

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

### MILESTONE-2 — Street rendering vertical slice `[ ]`

Exit criteria:

- one representative Street scene uses engine camera/input, assets, generic
  meshes, materials, render queues, culling/LOD, and instancing;
- screenshot comparisons have agreed tolerances;
- CPU time, draw calls, submissions, culled objects, and GPU resource estimates
  are compared with the original implementation;
- direct CNA escape hatches are demonstrated and documented.

Estimated active AI-agent effort from project start: 180–300 hours for a useful
MVP; 320–500 hours for the complete Phase 4 scope with robust tests.

## Phase 5 — two-consumer validation and staged migration

Progress (2026-10-19): `examples/street_slice` renders a procedural street
block through engine camera/input, generic meshes, PBR materials
(`EnhancedGraphics`), render queues, frustum/distance culling, LOD with
hysteresis and instanced lamps, with diagnostics and a PNG capture; it runs as
a CTest example. Street's own camera, controls and tangent-frame meshes are
validated by equivalence probes. A real Street scene (Street's own `CityScene`) is drawn by Street's
`SceneRenderer` and by the engine over the same buffers: identical visibility
(652 items, 192 instances), identical draw calls (703, instanced groups as
instanced render items), CPU frame time equal within noise. With Street's
lighting carried by the engine binder and Street's sky drawn as a custom pass
(the direct-CNA escape hatch), the images are practically identical: mean
difference 0.01/255, 53 of 129,600 pixels differ
(`docs/pilots/street-simulator.md`). Open: agreed screenshot tolerances with
the owner; engine input and assets in the slice (the comparison uses one fixed
viewpoint and Street's asset libraries); GPU resource estimates; adoption
inside street-simulator (needs permission).

### PILOT-001 — Validate `cna-street` `[~]`

Acceptance:

- migrate one complete vertical slice before moving broad utility code;
- document every adapter and game-specific facility intentionally retained;
- all existing Street tests stay green;
- performance and visual baselines show no unexplained regression.

Blocked by: MILESTONE-2.

Progress: equivalence probes against Street's own code (camera, fly
controls, tangent-frame meshes and upload, instancing, quality presets,
derived-material cache) — `docs/pilots/street-simulator.md`. Open: migrating a
vertical slice inside street-simulator itself, which needs the owner's
permission to modify that repository.

### PILOT-002 — Validate strict-XNA Car Simulator `[~]`

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
ADR-001 allowance — `docs/pilots/car-simulator.md`. Open: an integration
branch of car-simulator (owner's permission needed); its display smoke tests
and full suite in an environment with a suitable renderer budget.

### PILOT-003 — Stabilize only proven APIs `[~]`

Acceptance:

- APIs exercised by both pilots receive compatibility tests and stability
  status;
- single-consumer abstractions remain experimental;
- duplicated game code is removed only after equivalent behavior is measured;
- migration notes include reversibility and known differences.

Blocked by: PILOT-001 and PILOT-002.

## Phase 6 — release readiness

Progress: facilities validated by both pilots are marked `pilot-validated`
in `docs/api-status.md`, their headers are fingerprinted
(`docs/api-snapshot.txt`, test `cna_engine.api_snapshot`) and changes go to
`CHANGELOG.md`. `stable` status waits for adoption in the pilots.

### REL-001 — Package and version the library `[~]`

Acceptance:

- installed CMake package, semantic-version policy, license inventory, and
  dependency notices are complete;
- source and binary consumers use only public artifacts;
- CNA and `cna-extended` compatibility ranges are machine-checked.

Progress: installed CMake package with `SameMinorVersion`, MIT `LICENSE`,
`NOTICE.md` license/provenance inventory (installed with the package),
`CHANGELOG.md`, API snapshot of pilot-validated headers, and baseline revisions
machine-checked at configure time. Open: CNA/cna-extended packages (UP-001)
and version ranges once dependencies have releases.

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
screenshot), scripted so the test is deterministic. Open: revisit the
template once a pilot has adopted the engine.

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
Open: performance checks with agreed budgets (MILESTONE-2).

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
