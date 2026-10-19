# CNA Engine Handoff

Date: 2026-10-19 (supersedes the 2026-10-14 handoff, which is in git history)

Audience: the next AI agent working on `cna-engine`

## Mission

Build a higher-level C++23 game-engine library above CNA that makes ordinary
game development materially easier than CNA's XNA 4.0-compatible API alone,
without hiding CNA, and reusing `cna-extended` instead of duplicating it.
Pilots: `libcnalab/street-simulator` (enhanced profile) and
`libcnalab/car-simulator` (strict-XNA profile). Both moved from
`libcna/cna-street` and `libcna/cna-car-simulator` after the original audit.

## Owner constraints (binding)

- **Modify only this repository.** Do not edit `cna`, `sharp-runtime`,
  `cna-extended`, the simulators or any other app. Defects found elsewhere are
  written to `upstream-issues.md` (UP-001 … UP-005 so far). Pilot code may be
  *read and compiled read-only* (that is what `tests/pilots/` does).
- Commit regularly on branch `develop`; do not push unless asked.
- At most 3 parallel compile/test jobs. Shared ccache only:
  `CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`, plus `CCACHE_NOHASHDIR=1`
  (UP-003). Never a new cache, never `CCACHE_MAXSIZE`, never build in `/tmp`
  or the scratchpad, only the build directory names of `../CLAUDE.md`.
- Documents stay in English.

## Build trees

| Directory | Content |
| --- | --- |
| `build/` | day-to-day engine build against the shared `../cna` worktree (which other agents edit; not baseline evidence) |
| `build-probe/` | baseline build against `~/deps/cna-baseline` (clean clone at the pinned CNA revision) with `CNA_ENGINE_ENFORCE_BASELINE=ON` |
| `build-consumer/` | installed-package consumer probes (`tools/run_consumer_probes.sh`) |

`~/deps/cna-baseline` is a local clone of CNA at the pin; move it with
`git -C ~/deps/cna-baseline fetch && git -C ~/deps/cna-baseline checkout <rev>`
when the pin changes (ADR-002 "Local reproduction").

## State

All work is committed on `develop`. Latest evidence (`docs/verification.md`):
151 tests pass in `build/`, 150 in the baseline build (`build-probe/`,
before the API-snapshot test existed), 2 consumer probes pass.

Done (see `plan.md` for per-task results):

- ADR-001 (+ addendum for `EnhancedGraphics`), ADR-002 with a clean,
  enforced baseline run.
- MILESTONE-1 (strict foundation): ENG-020 … ENG-025 complete, validated
  against both pilots by read-only equivalence probes (`docs/pilots/`); the
  pilot's own strict checker accepts the strict surface with the ADR-001
  allowance.
- Enhanced work: material binders, PBR binder over `PbrEffect`,
  `EngineWorldScreen`, debug overlay, render queue, culling/LOD, instancing,
  quality schema (ENG-036 complete), `street_slice` example.
- Release readiness: LICENSE, NOTICE, CHANGELOG, API snapshot test, profile
  guides, verification matrix, CI workflow.

## Open, in priority order

1. Pilot adoption (PILOT-001/002) — needs the owner's permission to modify the
   pilots; until then, probes are the evidence.
2. MILESTONE-2 remainder: compare a real Street scene (its `SceneRenderer`)
   with the engine path, CPU/draw-call measurements with agreed tolerances.
3. ENG-035 later steps only when a pilot needs them (Street currently draws no
   shadows; see `docs/pilots/street-simulator.md`).
4. Upstream: UP-001 (no CNA package), UP-004 (BoundingFrustum point
   containment), UP-005 (cna-extended skinned test) — for their owners.

## Pointers

- `README.md`, `docs/guide-strict.md`, `docs/guide-enhanced.md`
- `plan.md` — tasks with `[x]` / `[~]` / `[ ]` and results
- `docs/adr/`, `docs/conventions.md`, `docs/api-status.md`,
  `docs/verification.md`, `docs/verification-matrix.md`, `docs/pilots/`
- `upstream-issues.md`, `NOTICE.md`, `CHANGELOG.md`
- `analysis.md`, `gap-audit.md` — the original study and audit
