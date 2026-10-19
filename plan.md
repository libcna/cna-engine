# CNA Engine Implementation Plan

Date: 2026-10-14

Status: proposed

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

- `[x]` completed by the analysis/audit phase
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

### ADR-001 — Approve the strict/enhanced dependency boundary `[ ]`

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

### ADR-002 — Select and pin the CNA compatibility baseline `[ ]`

Acceptance:

- CNA and `cna-extended` revisions are clean and explicitly recorded;
- compatibility policy states whether exact revisions or version ranges apply;
- all three audited consumers are rerun against the pinned baseline;
- CI can reproduce the baseline without relying on a developer worktree.

Blocked by: none. Complete before publishing engine API.

## Phase 1 — upstream prerequisites

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

### EXT-003 — Establish an upstream compatibility lane `[ ]`

Acceptance:

- CI builds `cna-extended` against the pinned CNA revision with a maximum of
  three jobs in the constrained environment;
- examples and tests are separate reported jobs;
- an engine integration probe detects an upstream API break early.

Blocked by: ADR-002.

## Phase 2 — repository and boundary skeleton

### ENG-010 — Create the C++23 CMake project skeleton `[ ]`

Acceptance:

- targets approved by ADR-001 configure, build, install, and export;
- build-tree and installed-package consumers both work;
- warning policy does not mutate dependency flags globally;
- tests can be enabled or disabled without changing public behavior.

Blocked by: ADR-001 and ADR-002.

### ENG-011 — Add public dependency-boundary checks `[ ]`

Acceptance:

- strict public headers are scanned and compile-tested without
  `cna-extended`, CNA internal headers, or CNA extension APIs;
- enhanced headers compile with only their declared dependencies;
- forbidden transitive includes produce a clear CI failure.

Blocked by: ENG-010.

### ENG-012 — Add minimal strict and enhanced consumer projects `[ ]`

Acceptance:

- each consumer uses the installed package rather than repository-relative
  private paths;
- strict consumer links only strict targets;
- enhanced consumer creates a minimal `CNA::Extended` screen or world adapter;
- both compile and run in the software/headless/null configuration.

Blocked by: ENG-010 and ENG-011.

### ENG-013 — Define API stability and ownership conventions `[ ]`

Acceptance:

- public/private header policy, namespace rules, error handling, resource
  ownership, nullable handles, and ABI expectations are documented;
- experimental APIs are visibly marked;
- contribution checklist requires two-consumer evidence for generalization.

Blocked by: ADR-001.

## Phase 3 — first strict-compatible vertical slice

### ENG-020 — Implement input actions and rebinding `[ ]`

Generalize the proven Car Simulator action map without importing its domain
commands.

Acceptance:

- named digital actions and analog axes support keyboard, mouse, and gamepad;
- held, pressed, and released state is deterministic per update;
- mappings serialize without exposing platform internals;
- unit tests cover rebinding, conflicting bindings, dead zones, and focus loss;
- both pilot projects can express their existing core controls.

Blocked by: ENG-012.

### ENG-021 — Implement camera data and controller interfaces `[ ]`

Acceptance:

- view, projection, combined matrix, basis vectors, and frustum have explicit
  invalidation behavior;
- perspective and orthographic projections are tested;
- controller policy is separate from camera state;
- fly/walk can be implemented by the engine while chase/cockpit remains in the
  Car Simulator;
- no duplicate scene/ECS ownership is introduced.

Blocked by: ENG-012. Coordinate with EXT-002.

### ENG-022 — Define generic CPU mesh data `[ ]`

Acceptance:

- the design supports Street's tangent-frame PBR vertices and the Car
  Simulator's multiple strict-XNA layouts without one giant mandatory vertex;
- 32-bit indices, bounds, append, transform, winding, and normal/tangent
  generation have tests;
- conversion is explicit and detects unsupported or lossy attributes;
- procedural builders depend on the generic representation, not on a game.

Blocked by: ENG-013.

### ENG-023 — Add RAII GPU mesh upload and drawing `[ ]`

Acceptance:

- resource ownership, borrowed resources, device reset/loss, and disposal are
  defined;
- supported vertex layouts upload and draw through XNA-compatible APIs;
- submission and memory statistics are observable;
- tests cover validation and lifecycle; a headless/software integration scene
  covers actual drawing.

Blocked by: ENG-022.

### ENG-024 — Add typed asset registry and lifetime policy `[ ]`

Acceptance:

- cache keys, ownership, invalidation, fallback, and failure behavior are
  documented and tested;
- `ContentManager` remains the underlying content owner where appropriate;
- engine code does not duplicate `cna-extended` format readers;
- both pilots can replace at least one local cache without behavior changes.

Blocked by: ENG-012. EXT-001 is complete.

### ENG-025 — Add diagnostics and screenshot capture `[ ]`

Acceptance:

- frame counters and scoped CPU timings have negligible disabled overhead;
- draw/submission statistics can be supplied without a hard dependency on a UI;
- deterministic screenshot requests report success and failure explicitly;
- software/headless tests verify output metadata and image dimensions.

Blocked by: ENG-012.

### MILESTONE-1 — Strict-compatible foundation `[ ]`

Exit criteria:

- ENG-020 through ENG-025 meet acceptance criteria;
- Car Simulator uses at least two facilities in a branch or integration probe;
- its strict checker remains green with only the narrow ADR-approved allowance;
- no engine domain depends on `cna-extended` through a strict target.

Estimated active AI-agent effort: 90–150 hours including Phase 2 and pilot
integration feedback.

## Phase 4 — enhanced scene and rendering layer

### ENG-030 — Integrate `cna-extended` lifecycle and World3D `[ ]`

Acceptance:

- adapters compose with `Screen`, ECS, `World3DScreenEXT`, animation, and
  particles without wrapping every upstream type;
- applications may opt out and use engine graphics directly;
- ownership and update/draw order are documented and tested.

Blocked by: ENG-012. EXT-001 is complete.

### ENG-031 — Define materials and shader-facing parameters `[ ]`

Acceptance:

- opaque, alpha-tested, transparent, unlit, and PBR-oriented material needs are
  representable;
- material descriptions are separate from per-device GPU state;
- missing textures and unsupported capabilities have deterministic fallbacks;
- one Street material family migrates without visual regression.

Blocked by: ENG-023 and ENG-024.

### ENG-032 — Define render items, queues, and state contracts `[ ]`

Acceptance:

- static, dynamic, skinned, and instanced submissions share a small common
  render-item contract;
- sorting rules for opaque, alpha-tested, transparent, and UI work are tested;
- custom passes and direct CNA drawing remain possible;
- graphics state before and after engine passes is explicit.

Blocked by: ENG-023 and ENG-031.

### ENG-033 — Add visibility and LOD policy `[ ]`

Acceptance:

- frustum, distance, and layer filtering are independent policies;
- bounds updates and conservative culling are tested;
- LOD thresholds are data-driven and stable under small camera movements;
- Street can report culling/LOD counts through engine diagnostics.

Blocked by: ENG-021 and ENG-032.

### ENG-034 — Add instanced rendering `[ ]`

Acceptance:

- instance data layout and update frequency are not hard-coded to Street;
- capability detection selects an explicit instanced or fallback path;
- batching preserves material/pass ordering;
- a representative Street group matches the original output and improves or
  preserves measured frame cost.

Blocked by: ENG-032 and ENG-033.

### ENG-035 — Add pass orchestration incrementally `[ ]`

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

### ENG-036 — Add quality profiles and capability resolution `[ ]`

Acceptance:

- user intent, detected capability, and resolved runtime setting are separate;
- profiles are serializable and validated;
- fallback decisions are inspectable through diagnostics;
- both pilots can map existing quality tiers without silently changing them.

Blocked by: ENG-035.

### ENG-037 — Add optional debug presentation `[ ]`

Acceptance:

- metrics are available without rendering an overlay;
- an enhanced adapter can present them using existing font/debug facilities;
- game-specific benchmark UI remains outside the engine.

Blocked by: ENG-025 and ENG-030.

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

### PILOT-001 — Validate `cna-street` `[ ]`

Acceptance:

- migrate one complete vertical slice before moving broad utility code;
- document every adapter and game-specific facility intentionally retained;
- all existing Street tests stay green;
- performance and visual baselines show no unexplained regression.

Blocked by: MILESTONE-2.

### PILOT-002 — Validate strict-XNA Car Simulator `[ ]`

Migrate or probe input actions, camera data, mesh data/upload, diagnostics, and
screenshot capture. Do not migrate vehicle physics or domain rendering policy.

Acceptance:

- strict API checker remains green;
- all non-display tests pass;
- display smoke tests run in an environment with an appropriate renderer budget;
- direct CNA extension use remains forbidden;
- at least two engine abstractions are exercised by real application code.

Blocked by: MILESTONE-1 and ADR-001.

### PILOT-003 — Stabilize only proven APIs `[ ]`

Acceptance:

- APIs exercised by both pilots receive compatibility tests and stability
  status;
- single-consumer abstractions remain experimental;
- duplicated game code is removed only after equivalent behavior is measured;
- migration notes include reversibility and known differences.

Blocked by: PILOT-001 and PILOT-002.

## Phase 6 — release readiness

### REL-001 — Package and version the library `[ ]`

Acceptance:

- installed CMake package, semantic-version policy, license inventory, and
  dependency notices are complete;
- source and binary consumers use only public artifacts;
- CNA and `cna-extended` compatibility ranges are machine-checked.

### REL-002 — Complete documentation and examples `[ ]`

Acceptance:

- quick start demonstrates a small game without requiring Street knowledge;
- strict and enhanced profiles have separate guides;
- lifecycle, ownership, rendering extension points, and fallback behavior are
  documented;
- examples run under the supported headless/software audit profile.

### REL-003 — Establish the verification matrix `[ ]`

Acceptance:

- unit, boundary, installed-consumer, visual, and performance checks are
  separated;
- supported platform/renderer combinations are explicit;
- CI and local documentation cap compilation at three jobs for this workspace;
- known skips and environmental timeouts cannot appear as silent success.

Blocked by: PILOT-003.

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
