# ADR-002 — CNA compatibility baseline

Date: 2026-10-19

Status: accepted (policy and pin); consumer reruns partially complete — see
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

### Pinned revisions

| Dependency | Branch | Commit | Committed |
| --- | --- | --- | --- |
| CNA | `next` | `705ea232439135654ce93279dfe87c1ab047ef5c` | 2026-10-19 |
| cna-extended | `develop` | `0061e3e17c30b2a3a302bce617e64765883079aa` | 2026-10-14 |
| Sharp Runtime | `next` | `f2aecfefeac171b331787e5cdc4849507363ef0e` | 2026-10-14 |

cna-extended's pin is the read-only loader fix (EXT-001) and matches
`origin/develop`.

### Updating the pin

1. Check out the candidate revisions with clean tracked files.
2. Build and run the engine test suite with `-DCNA_ENGINE_ENFORCE_BASELINE=ON`.
3. Rerun the consumer checks listed below.
4. Update `cmake/CnaEngineBaseline.cmake` and this table in one commit.

### CI reproduction

CI clones the three dependencies at the pinned commits next to the engine
checkout (`../cna`, `../sharp-runtime`, `../cna-extended`) and configures with
`-DCNA_ENGINE_ENFORCE_BASELINE=ON`. No developer worktree is involved. The
build uses at most three parallel jobs in the constrained environment.

## Verification status

| Check | Status |
| --- | --- |
| Engine build + tests at the pin | done in this repository's `build/` (see `docs/verification.md`) |
| cna-extended suite against the pinned CNA | pending |
| street-simulator against the pinned CNA | pending |
| car-simulator against the pinned CNA | pending |

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
