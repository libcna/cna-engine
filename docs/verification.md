# Verification record

Newest first. Each entry states the dependency revisions and worktree state,
the configuration, the commands, and the result. Failures and skips are
listed, never folded into a pass.

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

Day-to-day `build/` (shared CNA worktree, CNAEXT off): 157/157.

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
