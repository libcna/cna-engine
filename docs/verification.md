# Verification record

Newest first. Each entry states the dependency revisions and worktree state,
the configuration, the commands, and the result. Failures and skips are
listed, never folded into a pass.

## 2026-10-21 — static analysis

`clang-tidy` 19 over the engine's 30 source files (and their headers) with
`bugprone-*`, `performance-*` and `clang-analyzer-*` (minus swappable
parameters, narrowing, enum size and implicit widening): 10 findings, none a
defect — an intentional empty catch in a destructor (now marked), optional
values assigned through a dereference after validation (now assigned
directly), identical switch branches (merged), and three string
concatenations in error messages (left). Non-pilot tests 167/167 after the
cleanup.

## 2026-10-21 — the pilots' own suites against the pinned CNA (ADR-002)

`build-probe/` (pinned, baseline enforced and matching) reconfigured with
`-DCNA_ENGINE_PILOT_SUITES=ON`; both pilots' test sources compiled read-only
(1 min 16 s with 3 jobs).

    ctest --test-dir build-probe --parallel 3 -R "carsim_suite|street_suite"

- street-simulator: **13 of 13** test programs pass.
- car-simulator: **320 of 321** tests pass in 18 min; the failure is
  `SampleMap.LipovaLoadsAndIsFullyConnected`'s 6 s production load budget
  (16.0 s in this Debug build on a loaded machine), excluded by name since
  (UP-009).

## 2026-10-21 — Street with traffic

Street comparison with traffic and pedestrians (`build-probe/`, pinned):
first view 1,110 / 192 / 123 visible, 1,284 / 1,284 draw calls, 68 pixels
differ; walk 954 / 157 / 78, 1,087 draws, 884 pixels; close-up 787 / 104 /
42, 869 draws, 781 pixels, none on the 4,112 figure pixels. 176 s, 2.7 GB.

## 2026-10-21 — car-simulator at night, material fog

Sanitizer tree `build-asan/` rebuilt with every change of 2026-10-20/21:
**165 of 165** non-pilot tests pass with ASan, UBSan (fatal) and LSan.
`build/` 188/188, baseline `build-probe/` 189/189.

car-simulator comparison (`build-probe/`, pinned): day 867/867 draw calls
(0.05/255), night at 22:00 674/674 draw calls including 15 additive lamp
lights (800 pixels differ, at most 20/255).

## 2026-10-20 (late night) — car-simulator signs, baseline 188/188

Baseline `build-probe/`: **188 of 188** (before the sign change). The
car-simulator comparison now takes over sign batches as well: 867 of 867 draw
calls, mean image difference 0.05/255.

## 2026-10-20 (late night) — GPU memory estimate, walk view

`build/` **184 of 184**. Street comparison (`build-probe/`, pinned): three
views now — first viewpoint (0.01/255), a walk driven by engine input
(621/157/72 visible, 748 draws, 0.20/255), pedestrian close-up (0.39/255) —
plus GPU resource estimates (Street's mesh accounting is 8 bytes per vertex
high, UP-008).

## 2026-10-20 (late night) — display names, final counts of the day

`build/` (shared CNA worktree): **182 of 182**. Baseline `build-probe/`
(pinned, enforced, CNAEXT on): **183 of 183**, both scene comparisons
included.

## 2026-10-20 (night) — sanitizers

`build-asan/` configured with `-DCNA_ENGINE_SANITIZE=address,undefined
-DCNA_ENGINE_PILOT_PROBES=OFF` (shared CNA worktree at `d77ee1df`, not
baseline evidence; only the engine's libraries, tests and examples are
instrumented). Build 5 min 42 s with 3 jobs.

    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        ctest --test-dir build-asan --parallel 3

Result: **158 of 158 passed** (every non-pilot test, examples included), with
UBSan findings fatal and LeakSanitizer on; also 158/158 with leak detection
off.

## 2026-10-20 (night) — reflection-probe selector, quick-start game

`build/` **180 of 180**, baseline `build-probe/` **181 of 181** (new:
`cna_engine.example.quick_start`, the selector test). Street comparison in
probes mode, run once by hand (4 min 53 s, 2.7 GB peak): first view 75 of
129,600 pixels differ, close-up 785 (2 on figures); visibility and draw
calls identical (`docs/pilots/street-simulator.md`).

## 2026-10-20 (evening) — skinned render items

Baseline `build-probe/` (clean pinned dependencies, baseline enforced and
matching; CNAEXT on), Debug, software renderer: **179 of 179 tests passed**.
Day-to-day `build/` (shared CNA worktree, CNAEXT off): **178 of 178 passed**.

Street scene comparison now runs with Street's pedestrians: first viewpoint
652/652 items, 192/192 instances, 117/117 figures, 820/820 draw calls, mean
image difference 0.01/255 (53 pixels); close-up of a pedestrian: 57 figures
covering 10,412 pixels, 2 of them differ, 772 pixels in the frame
(coplanar ground meshes under a 1.7e-6 view-matrix difference).

## 2026-10-20 (later) — blend states, samplers, Street lighting, diagnostics

Baseline `build-probe/` (clean pinned dependencies, baseline enforced and
matching: CNA `5a44d2c5`, Sharp Runtime `f2aecfef`, cna-extended `0061e3e1`;
`CNA_ENGINE_CNA_CNAEXT=ON`), Debug, software renderer:

    cmake --build build-probe --parallel 3
    ctest --test-dir build-probe --parallel 3

Result: **175 of 175 tests passed**. Scene comparisons in that run:

- Street: visible items 652/652, instances 192/192, draw calls 703/703, mean
  image difference 0.01/255 (53 of 129,600 pixels differ; 49 explained by
  Street's depth writes in its transparent pass).
- car-simulator: terrain 207, roads 77, objects 503, trees 33 — equal under
  both frustum tests; mean image difference 0.1/255.

Day-to-day `build/` (shared CNA worktree at `b0940e0e` with uncommitted math
edits by another agent; CNAEXT off): **174 of 174 passed** (all but the Street
comparison, which needs CNAEXT). In that worktree CNA's
`BoundingFrustum::Intersects(sphere)` is a GJK query (XNA's algorithm), so
car-simulator draws 506 object and 34 tree batches where the engine's plane
test keeps 503 and 33, with the same image; the comparison checks the
engine's rules under car-simulator's frustum test (506/34, equal).

## 2026-10-20 — car-simulator clock probe

Day-to-day `build/` (shared CNA worktree at `b0940e0e`, which other agents
edit — not baseline evidence; CNAEXT off), Debug, software renderer.

    cmake --build build --parallel 3
    ctest --test-dir build --parallel 3

Result: **167 of 167 tests passed**, including the three new
`cna_engine.pilot.CarSimulatorClockProbe.*` tests (car-simulator's
`Vehicle::Update` step loop against `FixedStepClock`; see
`docs/pilots/car-simulator.md`).

## 2026-10-19 (night) — CNAEXT configuration and the Street scene comparison

`build-probe/` reconfigured with `-DCNA_ENGINE_CNA_CNAEXT=ON` (same clean
pinned dependencies, baseline still enforced and matching) so that Street's
whole core, which needs CNA's CNAEXT shader packages, compiles read-only.

- Engine suite in that configuration: **158 of 158 tests passed** (including
  `cna_engine.pilot.street_scene_comparison`), showing the engine also builds
  and runs against CNA with its extension layer enabled.
- Street scene comparison (`docs/pilots/street-simulator.md`): identical
  visible items (652), visible instances (192) and draw calls (703); engine CPU
  frame time lower in every run; mean image difference 19/255 from Street's
  lighting model.
- Found and recorded UP-006: Street's `CNA` umbrella (libcna.so) plus the
  engine's static CNA modules duplicated CNA's statics and crashed at exit;
  the probe links Street against the static modules.

Day-to-day `build/` (shared CNA worktree, CNAEXT off): 157/157 at that
point; later the same night the car-simulator scene comparison was added
there (strict, no CNAEXT needed): terrain 207/207, roads 77/77, objects
503/503, trees 33/33, mean image difference 0.1/255; suite 164/164.

## 2026-10-19 (baseline) — clean pinned dependencies, enforced

The CNA worktree used by the earlier runs carries other agents' uncommitted
edits (by now `modules/math/src/Vector{2,3,4}.cpp`), so it cannot give
baseline evidence. A clean checkout of the pinned CNA revision was created as
a shared dependency (`~/deps/cna-baseline`, a local clone; the CNA repository
itself is untouched).

| Repository | Revision | Worktree |
| --- | --- | --- |
| CNA (`~/deps/cna-baseline`) | `5a44d2c507cd5e49eab9ab35570a8a98c4b7a0f3` | clean |
| Sharp Runtime | `f2aecfefeac171b331787e5cdc4849507363ef0e` | clean |
| cna-extended | `0061e3e17c30b2a3a302bce617e64765883079aa` | clean |

Configuration (`build-probe/`): as CI — Debug, Ninja, ccache,
`CNA_ENGINE_ENFORCE_BASELINE=ON` (all three revisions verified at configure
time), `CNA_ENABLE_SDL=OFF`, `CNA_ENABLE_VIDEO=OFF`, software/headless/null,
pilot probes on.

Result: build 8 min 13 s with 3 jobs; **150 of 150 tests passed** (unit,
boundary, examples, pilot probes). This is the engine's baseline evidence for
ADR-002.

## 2026-10-19 (later) — new CNA pin, pilots, cna-extended rerun

Dependencies: CNA `next` worktree at `5a44d2c507cd5e49eab9ab35570a8a98c4b7a0f3`
(new pin; at the time tracked modifications only in `CNA_DEMOS_REPAIR_*`
documents and `modules/c-api/tests/pure_c/GeometrySmoke.c`, none built here;
the worktree later moved on, see the baseline entry above), Sharp Runtime
`f2aecfef` (clean), cna-extended `0061e3e1` (clean). Pilots read-only:
car-simulator `cd6d948e`, street-simulator `86b4a2e7`.

Engine (`build/`, same configuration as below, plus `CCACHE_NOHASHDIR=1`):
**133 of 133 tests passed** — unit, boundary, example, and pilot probes
(car-simulator: controls, cameras, meshes, screenshots, checker allowance;
street-simulator: camera, controls, tangent-frame meshes).

Installed-package consumer probes (`tools/run_consumer_probes.sh`,
`build-consumer/`): 2 of 2 passed; the strict consumer passes the boundary
checker and its link closure contains no cna-extended target.

cna-extended suite against the new pin (`build-probe/`, reconfigured from the
2026-10-14 audit tree; rebuild 3 min 42 s with 3 jobs):
2,371 tests — **2,368 passed, 1 failed, 2 skipped**.

- Skipped (known): the two `OrthographicCameraTest.Contains*` cases — CNA
  defect UP-004.
- Failed (new since the audit): `AnimationSystem3DEXTTest.DifferentAnimationTimes_ProduceDifferentPixelOutput`
  — a cna-extended test builds a skinned mesh from an undeclared vertex
  buffer, which CNA rejects since `e7bd98e46` (2026-10-15). Test defect,
  UP-005. Not fixed here (cna-extended is not modified from this repository).

Street and car-simulator full suites were not rerun (their own build trees
would be needed); the pilot probes above cover the facilities the engine
takes over.

## 2026-10-19 — first engine build (ENG-010 … ENG-025 initial slice)

Dependencies (configure-time baseline check):

| Repository | Revision | Worktree |
| --- | --- | --- |
| CNA `next` | `705ea232439135654ce93279dfe87c1ab047ef5c` | 3 modified tracked files from concurrent work (`CNA_DEMOS_REPAIR_HANDOFF.md`, `CNA_DEMOS_REPAIR_MATRIX.json`, `modules/c-api/CMakeLists.txt`); none is compiled here (C API off). Configure warned: not baseline evidence. |
| Sharp Runtime `next` | `f2aecfefeac171b331787e5cdc4849507363ef0e` | clean |
| cna-extended `develop` | `0061e3e17c30b2a3a302bce617e64765883079aa` | clean |

Configuration: `build/`, Ninja, Debug, GCC 14.2, ccache (`/rv/cnaccache`,
`CCACHE_BASEDIR=/rv`), CNA renderer `SOFTWARE`, platform `HEADLESS`, audio
`NULL`, `CNA_ENGINE_WITH_EXTENDED=ON`.

Commands:

```sh
cmake --build build --parallel 3
ctest --test-dir build --parallel 3 --timeout 300
```

Result: **88 of 88 tests passed** — 85 unit tests (82 strict, 3 enhanced),
2 boundary tests (`strict_boundary` passes; `strict_boundary_detects_leaks`
fails as required), 1 example (`orbit_scene` renders 30 frames, 3 draws per
frame, writes a PNG and checks the centre pixel). The strict header
self-containment target (`cna_engine_strict_header_check`) compiled every
strict public header on its own.

Notes:

- The first full build compiled 869 steps in 11 min 23 s with 3 jobs. ccache
  hit 1 of 933 cacheable calls: the CNA revision is newer than anything in the
  shared cache, so this run populated it.
- Two first-run failures were correct XNA Reach-profile behaviour (32-bit
  index buffers and `GetBackBufferData` are HiDef-only). `GpuMesh::Upload`
  now rejects 32-bit indices on a Reach device before allocating, and
  `CaptureBackBuffer` reports the Reach failure as a value; tests cover both
  profiles.
- CNA's C++ `Color` has a vtable, so CNA vertex structs are not byte images of
  the GPU layout. Engine interleaving writes the packed color explicitly; the
  GPU draw tests confirm the layouts render correctly.
