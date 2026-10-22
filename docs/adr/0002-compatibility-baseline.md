# ADR-002 — CNA compatibility baseline

Date: 2026-10-19

Status: accepted (policy and pin); all consumer reruns done (2026-10-20); CI
reproduces the baseline on GitHub (first green run 2026-10-20) — see
"Verification status"

Plan item: `ADR-002` in `plan.md`

## Context

CNA (`next`), Sharp Runtime (`next`) and cna-extended (`develop`) change
continuously and are consumed as source trees (ADR-001). The audit of
2026-10-14 ran against a CNA worktree with 193 uncommitted entries, which is
not reproducible evidence.

## Decision

### Policy: exact revisions

The engine is verified against exact commits, recorded in
`cmake/CnaEngineBaseline.cmake`. Version ranges are not used while every
dependency is at `0.x` and has no release tags.

At configure time the engine compares each dependency checkout's `HEAD` and
tracked-file state with the pin:

- default: a mismatch prints a CMake warning stating that results are not
  baseline evidence;
- `-DCNA_ENGINE_ENFORCE_BASELINE=ON` (CI): a mismatch fails configuration.

Untracked files are ignored (they are not part of any build glob the engine
enables).

The warning also places a differing checkout relative to the pin, which makes
the supported range — the pin and its descendants on the same line —
machine-checked (added 2026-10-20): "N commits after the baseline" or "does
not contain the baseline (older or diverged)".
`-DCNA_ENGINE_REQUIRE_BASELINE_DESCENDANT=ON` turns the second case into an
error, for consumers that want to build only against revisions at least as new
as the verified one. Tests `cna_engine.baseline_range_*` check both reports on
throwaway repositories.

### Pinned revisions

| Dependency | Branch | Commit | Committed |
| --- | --- | --- | --- |
| CNA | `next` | `08c1387eca7f9707d6180ff57d07f0360e059672` | 2026-10-22 |
| cna-extended | `develop` | `86870ad2bf6b3d9a947edc359a5819c83f56f6dc` | 2026-10-22 |
| Sharp Runtime | `next` | `1dc8785b35284b5cf50f3aec361703bfaf090b72` | 2026-10-22 |

cna-extended's pin matches `origin/develop`: on 2026-10-22 it moved from
`0061e3e1` (the read-only loader fix, EXT-001) to `86870ad`, where the
owner-delegated fixes run every cna-extended test with no failure or skip
(UP-004's skips, UP-005, UP-011, UP-015) and cna-extended 0.2.0 installs a
CMake package.

History: the first pin (`705ea232`, 2026-10-19) was replaced the same day by
`5a44d2c5` after CNA `next` advanced by five math-conformance commits; the
engine suite and the cna-extended suite were rerun against the new revision
(`docs/verification.md`). On 2026-10-20 the owner chose to move the pin back
to `688f3f23`, the newest revision published on GitHub (`origin/next`), 22
commits before `5a44d2c5` on the same line: `5a44d2c5` existed only locally,
so CI could not fetch it (UP-007). All three pins are now published heads.
Later on 2026-10-20 CNA published `next` at `900d0db3b` (142 commits after
`688f3f23`), which contains UP-004's fix (`5ab1c7729`) and a regression test
for it, and the pin moved there with the full evidence rerun
(`docs/verification.md`). Sharp Runtime stayed at `f2aecfef`, then its
published `next`; CNA `900d0db3b` builds and passes against it.
On 2026-10-22 both moved to their published `next`: CNA `08c1387ec` (123
commits after `900d0db3b`) contains the fixes for UP-006 (`470b12a5d`) and
UP-019 (`144d9f909`) made at the owner's request by the Codex session
`cna-demos`; since `abbd8a4ea` its video module needs Uri members of Sharp
Runtime `c26614e8` (UP-020), so Sharp Runtime moved with it to `1dc8785b`,
which also begins a Sharp Runtime package for UP-001. The engine's
calibration test now expects the software renderer the right way up.

### Updating the pin

1. Check out the candidate revisions with clean tracked files.
2. Build and run the engine test suite with `-DCNA_ENGINE_ENFORCE_BASELINE=ON`.
3. Rerun the consumer checks listed below.
4. Update `cmake/CnaEngineBaseline.cmake` and this table in one commit.

### Local reproduction

Concurrent agents edit the shared CNA worktree, so local baseline runs use a
clean clone at the pinned revision (`~/deps/cna-baseline`, updated with
`git -C ~/deps/cna-baseline fetch && git -C ~/deps/cna-baseline checkout <pin>`)
selected with `-DCNA_ENGINE_CNA_DIR=$HOME/deps/cna-baseline`, and likewise a
clean clone of Sharp Runtime (`~/deps/sharp-runtime-baseline`, selected with
`-DCNA_ENGINE_SHARP_RUNTIME_DIR=$HOME/deps/sharp-runtime-baseline`; added
2026-10-20 when the shared Sharp Runtime worktree gained uncommitted edits),
plus
`-DCNA_ENGINE_ENFORCE_BASELINE=ON -DCNA_ENABLE_SDL=OFF -DCNA_ENABLE_VIDEO=OFF`
(the vendored SDL is not part of CNA's git tree).

### CI reproduction

CI clones the three dependencies at the pinned commits next to the engine
checkout (`../cna`, `../sharp-runtime`, `../cna-extended`) and configures with
`-DCNA_ENGINE_ENFORCE_BASELINE=ON`. No developer worktree is involved. The
build uses at most three parallel jobs in the constrained environment.

## Verification status

| Check | Status |
| --- | --- |
| Engine build + tests at the pin | done from a clean pinned checkout with the baseline enforced: 150/150 (`docs/verification.md`, baseline entry) |
| cna-extended suite against the pinned CNA | done for `900d0db3b` on 2026-10-20: 2,368 passed, 2 skips (UP-004's tests, still skipped unconditionally in cna-extended), 1 test defect (UP-005); four of the passes need the build root as working directory (UP-011) |
| street-simulator against the pinned CNA | done 2026-10-20: its 13 test programs compiled read-only in `build-probe/` (`CNA_ENGINE_PILOT_SUITES=ON`) all pass |
| car-simulator against the pinned CNA | done 2026-10-20: its suite compiled read-only in `build-probe/`; 320 of 321 tests pass, the remaining one is its production load-time budget, which a Debug build misses (UP-009) |

The two simulators moved from `libcna/cna-street` and
`libcna/cna-car-simulator` to `libcnalab/street-simulator` and
`libcnalab/car-simulator` after the audit, so their audit build trees in this
repository pointed at removed source directories. `build/` (the Street audit
tree) now holds the engine build; `build-consumer/` (the car-simulator audit
tree) is reused for installed-package consumer probes.

During the first engine build, the CNA worktree had three modified tracked
files (`CNA_DEMOS_REPAIR_HANDOFF.md`, `CNA_DEMOS_REPAIR_MATRIX.json`,
`modules/c-api/CMakeLists.txt`) from concurrent work. None is compiled in the
engine's configuration (C API off), but the configure warning correctly marks
such runs as non-baseline evidence.
