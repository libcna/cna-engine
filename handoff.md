# CNA Engine Handoff

Date: 2026-10-20 (first written 2026-10-19; supersedes the 2026-10-14
handoff, which is in git history)

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
  `cna-extended`, the simulators or any other app — with one exception granted
  on 2026-10-20: car-simulator and street-simulator may be changed **on new
  branches** to adopt the engine (one revertible commit per step of
  `docs/pilots/migration.md`, nothing pushed). Defects found elsewhere are
  written to `upstream-issues.md` (UP-001 … UP-012 so far). Pilot code may be
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
| `build-probe/` | baseline build against `~/deps/cna-baseline` and `~/deps/sharp-runtime-baseline` (clean clones at the pins) with `CNA_ENGINE_ENFORCE_BASELINE=ON`, `CNA_ENGINE_CNA_CNAEXT=ON` and `CNA_ENGINE_PILOT_SUITES=ON`; the only tree that builds the Street scene comparison and runs both pilots' own suites (the car-simulator suite alone takes ~15 min; `-E "carsim_suite"` skips it) |
| `build-consumer/` | installed-package consumer probes (`tools/run_consumer_probes.sh`); `build-consumer/cna-extended/` is cna-extended's own suite against the pinned clones (CI's lane configuration, 2.0 GB, kept for the next pin move) |
| `build-asan/` | `CNA_ENGINE_SANITIZE=address,undefined`, pilot probes off, shared CNA worktree; engine targets instrumented only |

`~/deps/cna-baseline` and `~/deps/sharp-runtime-baseline` are local clones of
CNA and Sharp Runtime at the pins (`build-probe/` uses both; cna-extended's
shared worktree is still clean at its pin); move them with
`git -C <clone> fetch && git -C <clone> checkout <rev>` when a pin changes
(ADR-002 "Local reproduction").

## State

All work is committed on `develop`. Latest evidence (`docs/verification.md`):
193 tests pass in `build/`, 208 in the baseline build (`build-probe/`, which
also runs the Street scene comparison and both pilots' own suites; the
car-simulator suite alone takes ~15 min and excludes UP-009's test by name);
2 consumer probes pass.

Both pilots' real scenes render through the engine's `ForwardRenderer`
practically identically to the pilots' own renderers: Street's procedural
city with the PBR binder carrying Street's lighting and its sky as a custom
pass, traffic and pedestrians included (vehicles as dynamic items, people
as skinned render items; visible items, instances, figures and all 1,284
draw calls identical; mean image difference 0.01/255) and car-simulator's
`lipova` map with the strict stock binder (every visibility count and all
draw calls identical by day, at night, with the headlights on and in a
rear-view mirror — additive lamp light, the headlight fill as nested
renderers and the mirrored camera included; 0.05/255 by day). Both
comparisons enforce the agreed screenshot tolerance. See `docs/pilots/`.

car-simulator itself now runs on the engine on its branch `engine-adoption`
(worktree `libcnalab/car-simulator-engine`, build `build/software` there):
its world renderer draws through `ForwardRenderer` with car-simulator's own
matrices (`Camera::SetMatrices`), its suite and checker are green and its
captures stay inside the tolerance. street-simulator does the same on its
branch `engine-adoption` (worktree `libcnalab/street-simulator-engine`,
build `build/` there): `SceneRenderer` draws through `ForwardRenderer` and
the `PbrMaterialBinder`, its suite is green and all its named viewpoints
stay inside the tolerance.

The shared `../cna` worktree is changed by other agents all the time (on
2026-10-20 e.g. XNA-exact `BoundingFrustum` containment and GJK
`Intersects`, CNA `5ab1c7729`, which also resolves UP-004); `build/` results
move with it, `build-probe/` (pinned) is the evidence.

Done (see `plan.md` for per-task results):

- ADR-001 (+ addendum for `EnhancedGraphics`), ADR-002 with a clean,
  enforced baseline run, ADR-003 (what a material describes and what a binder
  decides — read it before adding material fields).
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

1. Pilot adoption (PILOT-001/002), allowed on new branches. car-simulator:
   done on branch `engine-adoption` in the worktree
   `libcnalab/car-simulator-engine` (eight commits; the camera stays the
   game's, see `docs/pilots/migration.md`). street-simulator: done on
   branch `engine-adoption` in the worktree
   `libcnalab/street-simulator-engine` (five commits: build, mesh views,
   `SceneRenderer` through the engine, presets as a `QualitySchema`,
   derived materials in an `AssetRegistry`). What
   is left for both: captures on a hardware GPU (Street's are checked on
   OpenGL via Mesa llvmpipe too) and the owner's merge decision.
2. MILESTONE-2 remainder: the screenshot tolerances are agreed (2026-10-20);
   the performance budget's reference is this machine in a Release build,
   to be measured when it is idle (`docs/pilots/performance-budgets.md`).
3. CI: all three pins are published heads since the move to CNA `688f3f23`
   (2026-10-20); the CI jobs have yet to run on GitHub. The cna-extended
   suite was rerun locally against the new pin (same result as before;
   UP-011 is a test-fixture path artefact of ccache's base directory).
4. ENG-035: shadows and post-processing only when a pilot needs them (Street
   currently draws neither; see `docs/pilots/street-simulator.md`).
   Reflection probes are supported through the PBR binder's selector.
5. Next pin move (ADR-002 procedure) once a published CNA revision contains
   `5ab1c7729` (UP-004's fix); then cna-extended's owner can lift its two
   skipped tests (EXT-002).
6. Upstream, for their owners: UP-001 (no CNA package), UP-005 (cna-extended
   skinned test), UP-006 (CNA umbrella vs module targets), UP-007 (unpushed
   pin), UP-008 (Street's mesh-memory accounting), UP-009 (car-simulator's
   load budget in Debug builds), UP-010 (Street test discards a nodiscard
   result), UP-011 (cna-extended fixture paths under ccache's base
   directory), UP-012 (Street's blended instance groups missing from its
   draw statistics); UP-004 is resolved upstream.

## Pointers

- `README.md`, `docs/guide-strict.md`, `docs/guide-enhanced.md`
- `plan.md` — tasks with `[x]` / `[~]` / `[ ]` and results
- `docs/adr/`, `docs/conventions.md`, `docs/api-status.md`,
  `docs/verification.md`, `docs/verification-matrix.md`, `docs/pilots/`
- `upstream-issues.md`, `NOTICE.md`, `CHANGELOG.md`
- `analysis.md`, `gap-audit.md` — the original study and audit
