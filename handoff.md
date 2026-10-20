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
  written to `upstream-issues.md` (UP-001 … UP-008 so far). Pilot code may be
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
| `build-probe/` | baseline build against `~/deps/cna-baseline` (clean clone at the pinned CNA revision) with `CNA_ENGINE_ENFORCE_BASELINE=ON`, `CNA_ENGINE_CNA_CNAEXT=ON` and `CNA_ENGINE_PILOT_SUITES=ON`; the only tree that builds the Street scene comparison and runs both pilots' own suites (the car-simulator suite alone takes ~18 min; `-E "carsim_suite"` skips it) |
| `build-consumer/` | installed-package consumer probes (`tools/run_consumer_probes.sh`) |
| `build-asan/` | `CNA_ENGINE_SANITIZE=address,undefined`, pilot probes off, shared CNA worktree; engine targets instrumented only |

`~/deps/cna-baseline` is a local clone of CNA at the pin; move it with
`git -C ~/deps/cna-baseline fetch && git -C ~/deps/cna-baseline checkout <rev>`
when the pin changes (ADR-002 "Local reproduction").

## State

All work is committed on `develop`. Latest evidence (`docs/verification.md`):
188 tests pass in `build/`, 189 in the baseline build (`build-probe/`, which
also runs the Street scene comparison), 2 consumer probes pass.

Both pilots' real scenes render through the engine's `ForwardRenderer`
practically identically to the pilots' own renderers: Street's procedural
city with the PBR binder carrying Street's lighting and its sky as a custom
pass, traffic and pedestrians included (vehicles as dynamic items, people
as skinned render items; visible items, instances, figures and all 1,284
draw calls identical; mean image difference 0.01/255) and car-simulator's `lipova` map with the strict stock binder
(every visibility count and all draw calls identical by day and at night,
additive lamp light included; 0.05/255 by day). See
`docs/pilots/`.

The shared `../cna` worktree is changed by other agents all the time (on
2026-10-20 e.g. XNA-exact `BoundingFrustum` containment and GJK
`Intersects`, CNA `5ab1c7729`, which also resolves UP-004); `build/` results
move with it, `build-probe/` (pinned) is the evidence.

Done (see `plan.md` for per-task results):

- ADR-001 (+ addendum for `EnhancedGraphics`), ADR-002 with a clean,
  enforced baseline run.
- MILESTONE-1 (strict foundation): ENG-020 … ENG-025 complete, validated
  against both pilots by read-only equivalence probes (`docs/pilots/`); the
  pilot's own strict checker accepts the strict surface with the ADR-001
  allowance.
- Rendering (ENG-031 … ENG-034 complete): materials with per-material
  samplers and premultiplied/straight blending chosen by the binder, the
  stock binder (BasicEffect, AlphaTestEffect, DualTextureEffect,
  SkinnedEffect) and the PBR binder (PbrEffect, SkinnedPbrEffect, image-based
  environment, exposure, per-item environment selector for reflection
  probes), render queue and state contract, culling/LOD with diagnostics,
  instancing, skinned render items.
- Enhanced integration: `EngineWorldScreen`, debug overlay, quality schema
  (ENG-036), `street_slice` example.
- Release readiness: LICENSE, NOTICE, CHANGELOG, API snapshot of the 21
  pilot-validated headers, profile guides, `quick_start` game example,
  verification matrix, CI workflow with the EXT-003 upstream lane and canary
  (blocked by UP-007 until the CNA pin is pushed).

## Open, in priority order

1. Pilot adoption (PILOT-001/002) — needs the owner's permission to modify the
   pilots; until then, probes are the evidence. The plan, step by step with
   known differences and reversal: `docs/pilots/migration.md`.
2. MILESTONE-2 remainder: screenshot tolerances and performance budgets must
   be agreed with the owner (the scene comparisons give the measured
   differences to start from; the scaling checks guard the algorithms).
3. CI: the pinned CNA revision must be pushed (UP-007) before the CI jobs,
   the upstream lane and the canary can run; ADR-002's consumer checks are
   otherwise complete.
4. ENG-035: shadows and post-processing only when a pilot needs them (Street
   currently draws neither; see `docs/pilots/street-simulator.md`).
   Reflection probes are supported through the PBR binder's selector.
5. Pin move (ADR-002 procedure) once a published CNA revision contains
   `5ab1c7729` (UP-004's fix); then cna-extended's owner can lift its two
   skipped tests (EXT-002).
6. Upstream, for their owners: UP-001 (no CNA package), UP-005 (cna-extended
   skinned test), UP-006 (CNA umbrella vs module targets), UP-007 (unpushed
   pin), UP-008 (Street's mesh-memory accounting), UP-009 (car-simulator's
   load budget in Debug builds); UP-004 is resolved upstream.

## Pointers

- `README.md`, `docs/guide-strict.md`, `docs/guide-enhanced.md`
- `plan.md` — tasks with `[x]` / `[~]` / `[ ]` and results
- `docs/adr/`, `docs/conventions.md`, `docs/api-status.md`,
  `docs/verification.md`, `docs/verification-matrix.md`, `docs/pilots/`
- `upstream-issues.md`, `NOTICE.md`, `CHANGELOG.md`
- `analysis.md`, `gap-audit.md` — the original study and audit
