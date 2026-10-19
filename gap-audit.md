# CNA Engine Targeted Gap Audit

Date: 2026-10-14

Status: analysis complete; upstream loader fix verified; no engine
implementation was created

## Executive result

`cna-engine` is feasible, but it should not be written as a replacement for
`cna-extended` and it should not be extracted wholesale from either simulator.
The recommended dependency shape is:

```text
game
  -> cna-engine core and optional integration modules
    -> cna-extended where its facilities are useful
      -> cna
```

The audit found three distinct ownership levels:

1. low-level and generally reusable facilities that already belong to
   `cna-extended`;
2. opinionated game-engine facilities that belong to `cna-engine`;
3. city, traffic, vehicle, and presentation rules that must remain in the
   games.

The most important architectural issue is not graphics implementation. It is
the compatibility boundary between `cna-extended` and the strict-XNA policy of
`cna-car-simulator`. The first design decision must therefore define a small
strict-compatible engine target and a separate optional integration target for
`cna-extended`.

## Scope and baseline

The following working trees were inspected and built where applicable:

| Project | Branch | HEAD observed at close-out | Worktree |
| --- | --- | --- | --- |
| `cna` | `next` | `de310727ce74bcd4b65e6f5147275a5bb1e637a6` | dirty (193 entries at final verification) |
| `cna-extended` | `develop` | `0061e3e17c30b2a3a302bce617e64765883079aa` | clean; matches `origin/develop` |
| `cna-street` | `develop` | `86b4a2e7c8744fbba45cff7a45803dee3cf70757` | clean |
| `cna-car-simulator` | `main` | `523b93b7444aa97437b583d0710d93a2fdf64bc1` | clean |

The CNA worktree was actively changing and dirty during the audit. Results
therefore describe the observed working tree, not a reproducible clean CNA
commit. Before implementation starts, a compatible CNA revision must be pinned
and the baseline rerun.

All post-request build and test commands were limited to three parallel jobs.
The documented reproduction convention is:

```sh
cmake --build <build-directory> --parallel 3
ctest --test-dir <build-directory> --output-on-failure --parallel 3
```

The sandbox could read but not update the shared ccache. Builds used it in
read-only mode with a writable temporary directory. That is an audit-environment
detail rather than a project requirement.

## Build and test findings

### `cna-extended` against the current CNA tree

- Configuration succeeded with C++23, software rendering, headless platform,
  null audio, and tests/examples enabled.
- The library, test executable, and three examples compiled successfully.
- The original audit discovered 2,371 tests: 2,365 passed, four failed, and two
  were skipped. `tiled_demo` failed for the same reason as the four tests.
- Commit `0061e3e17c30b2a3a302bce617e64765883079aa` corrected the filename
  loaders and added regression coverage.
- All 13 directly affected tests passed after the fix.
- The complete suite then passed with 2,369 passes, zero failures, and the same
  two documented skips.
- The minimal, tiled, and World3D examples all ran successfully. The tiled
  example loaded the formerly failing read-only asset; World3D rendered ten
  frames and wrote a capture.

The original four failures were one concrete defect, not four unrelated API
gaps.
Read-only asset loaders construct `System::IO::FileStream(path)`. The current
single-argument constructor opens an existing file with default read/write
access, despite callers using it as an input stream. It consequently fails on
valid assets residing in a read-only tree. At least
`BitmapFontFileReader::Read(path)` and the tiled example exercise this path.

The repair was correctly made in `cna-extended`: loaders that only read data now
open with `FileMode::Open` and `FileAccess::Read`. A source audit found no
remaining single-argument `FileStream` construction in public headers,
implementations, examples, or tests. Other filename loaders use
`File::ReadAllText`, `File::ReadAllBytes`, or `TitleContainer::OpenStream`, all
of which are read-only for these uses. This defect is therefore not a reason to
duplicate font or tilemap loading in `cna-engine`.

The two skipped tests cover known `OrthographicCamera` containment cases. They
are not regressions introduced by this audit, but should remain visible as an
upstream quality task.

### `cna-street`

- Configuration and compilation against the same CNA working tree succeeded.
- All 15 CTest entries passed.
- The project builds with CNA's `CNA_CNAEXT=ON` option.

`CNA_CNAEXT` is CNA's built-in extension surface. It is not the sibling
`cna-extended` library and does not mean that `cna-street` currently consumes
`CNA::Extended`. This naming collision is important when designing dependencies
and documentation.

### Strict-XNA `cna-car-simulator`

- The standalone API checker passed: 235 files were scanned against 544 known
  XNA 4.0 types.
- Configuration succeeded with `CNA_CNAEXT=OFF`, software rendering, headless
  platform, null audio, tools, and tests.
- The complete target set compiled successfully with at most three parallel
  jobs after the explicit concurrency limit was given.
- The CTest run used at most three concurrent tests and finished with four of
  seven CTest entries passing.
- `xna_only_api_check`, map validation, map regeneration, and the asset manifest
  check passed.
- Both display smoke tests hit their 120-second limits under the headless
  software renderer.
- The aggregate unit-test executable started 321 tests from 64 suites and hit
  its 900-second CTest limit while running `TrafficSoak`. Before that timeout,
  one observed assertion failed because sample-map loading took 13.44 seconds
  against a six-second performance threshold.

This is not a clean test pass. The evidence points to execution-time limits in
the constrained software-rendering/audit environment rather than a compile or
API compatibility failure, but that inference must be verified on the project's
supported performance test environment. No functional conclusion is claimed
for tests that had not completed when the aggregate process was terminated.

The checker rejects `CNA/` headers, the `CNA::` namespace, `EXT`-suffixed APIs,
and other non-XNA facilities. As written today, that policy also prevents direct
use of the separately packaged `cna-extended`. Adoption of `cna-engine` by this
project therefore requires a narrowly defined, certified engine boundary. It
must not silently weaken the rule for arbitrary CNA extensions.

## Public API fit for `cna-street`

`cna-street` contains roughly 36,250 lines in 94 C/C++ source files. It is the
best first pilot because it has a mature renderer, explicit subsystem
boundaries, and passing tests.

| Street need | Present in `cna-extended` | Recommended owner |
| --- | --- | --- |
| Screen lifecycle and transitions | Yes: `Screen`, screen management, `World3DScreenEXT` | Reuse `cna-extended`; engine adds composition conventions only |
| ECS world and systems | Yes: ECS plus `World3DEXT` systems | Reuse and adapt; do not create another ECS |
| Basic 3D camera and frustum | Yes: `Camera3DEXT` | General camera data may be improved upstream; controllers and policy belong in engine |
| Model and skinned-model ECS rendering | Yes | Reuse through an optional integration module |
| 3D collision primitives and spatial structures | Substantial support | Reuse where semantics match; keep game response rules outside |
| Particles, debug draw, text, billboards, voxels | Yes | Reuse; expose engine-level coordination only when needed |
| Generic CPU mesh representation and builders | Partial and specialized | `cna-engine` core graphics abstraction |
| GPU mesh ownership and multiple vertex layouts | Not a general public abstraction | `cna-engine` |
| Static and dynamic instancing | Not general enough for Street | `cna-engine` graphics |
| PBR material and texture policy | No equivalent high-level model | `cna-engine` enhanced graphics |
| Render-item submission and ordered pass orchestration | Only concrete systems | `cna-engine` enhanced graphics |
| LOD, shadow distance, cascades, and probe policy | Missing as a coherent layer | `cna-engine` enhanced graphics |
| Capability fallbacks and quality presets | Missing | `cna-engine` |
| Typed asset/model/material registry | Partial content helpers | General registry in engine; format readers remain upstream |
| Frame statistics, overlay hooks, deterministic capture | Partial | `cna-engine` diagnostics |
| Fly, walk, and cinematic camera control | Missing as reusable policy | `cna-engine` input/scene |
| Ambient/spatial voice orchestration | Only lower-level pieces | Optional later engine module |

Useful Street implementations to generalize are its `MeshData`, `GpuMesh`,
`InstancedMesh`, material description, render submission model, culling/LOD,
quality and capability settings, asset registries, frame statistics, and capture
support. They are evidence for an API, not code to copy unchanged.

The following remain Street-specific: city layout, buildings, roads, traffic,
pedestrians, vehicle and character factories, authored material catalogs, and
the artistic behavior of the Street sky and environment.

## Public API fit for strict-XNA `cna-car-simulator`

`cna-car-simulator` contains roughly 45,116 lines in 235 C/C++ source files,
including 51 test source files. It is valuable precisely because its constraints
and architecture differ from Street.

| Car Simulator need | Present in `cna-extended` | Recommended owner |
| --- | --- | --- |
| Named input actions and rebinding | Input helpers exist, but not this policy | `cna-engine` strict-compatible input |
| Camera pose, view, projection, and frustum | Basic facilities exist | Small strict-compatible engine abstraction |
| Chase and cockpit cameras | No generic equivalent | Vehicle-specific behavior stays in the game |
| Flexible CPU mesh data and geometry operations | No single matching abstraction | `cna-engine` strict-compatible graphics |
| GPU mesh upload for several vertex layouts | No matching generic facade | `cna-engine` strict-compatible graphics |
| Screenshot capture | Lower-level CNA support | `cna-engine` diagnostics |
| Quality tiers and settings | No coherent equivalent | `cna-engine` policy |
| Static spatial grid | Related structures exist | Reuse algorithmic ideas; place only a general structure in engine |
| OBB/cylinder contact and vehicle response | Collision primitives overlap | Primitives may be reused; vehicle response stays in game |
| Bitmap fonts | Yes | Use `cna-extended` only in the optional profile; do not duplicate merely for strict mode without evidence |
| World renderer and submission statistics | Not at the required abstraction | General portions in engine; presentation stays in game |

Vehicle dynamics, tire and engine behavior, traffic, map lanes, chase/cockpit
semantics, vehicle audio, and car generation are not engine responsibilities.

## Ownership decisions

### Completed fixes and remaining generalization in `cna-extended`

- Completed at `0061e3e`: open read-only asset inputs explicitly as read-only
  and cover the filename paths with regression tests.
- Completed: audit filename-based readers for the same `FileStream` access
  mismatch; no additional production defect was found.
- Resolve or precisely document the two skipped orthographic-camera containment
  cases.
- Improve existing generic components only when the change is independently
  useful to all `cna-extended` consumers.
- Preserve `Screen`, ECS, animation, particles, tilemaps, fonts, collision
  primitives, and `World3DEXT` in their existing library instead of cloning
  them into the engine.

### Build in `cna-engine`

- a strict-compatible core with input actions, camera data, generic CPU/GPU
  mesh foundations, asset lifetime policy, diagnostics, and capture;
- an optional `cna-extended` integration layer for screens, ECS, World3D,
  animation, particles, and related adapters;
- material and render-item descriptions;
- render queues and explicit pass orchestration;
- culling, LOD, instancing, quality profiles, capability fallbacks, and render
  statistics;
- small conventions that make assembling a game easier without hiding CNA
  resources or preventing custom rendering.

### Keep in each game

- domain simulation and physics;
- map and world content formats unless a second independent consumer proves
  them general;
- traffic, pedestrians, vehicles, roads, and city generation;
- chase/cockpit behavior and other domain cameras;
- game-specific shader tuning, atmosphere art direction, and material catalogs.

## Recommended target boundary

The target names are provisional and must be accepted in an architecture
decision record before code is added:

```text
CnaEngine::Core
  depends on CNA's XNA-compatible API and Sharp Runtime only

CnaEngine::Graphics
  depends on Core and the XNA-compatible graphics surface

CnaEngine::ExtendedIntegration
  depends on Core, Graphics, and CNA::Extended

CnaEngine::Engine
  convenience aggregate for non-strict consumers
```

The strict targets need public headers and namespaces that the Car Simulator
checker can recognize without granting access to the general `CNA::` extension
namespace. The checker should permit only the audited `cna-engine` public
surface and continue rejecting direct non-XNA CNA calls.

This split allows Street to reuse `cna-extended` while giving the Car Simulator
a narrow path to shared facilities. If the team rejects a strict-compatible
target, the Car Simulator should remain a requirements oracle and validation
project rather than a direct engine consumer.

## Conclusion

Do not start `cna-engine` from zero, and do not turn `cna-extended` into a full
opinionated engine. With the concrete upstream read-only loading defect now
repaired and verified, pin a stable CNA baseline, approve the strict/enhanced
boundary, and implement one small vertical slice used by Street. Validate the
same abstractions against the Car Simulator before declaring them stable.

The executable task sequence is maintained in `plan.md`.
